#include "touch_xpt2046.h"
#include "config_store.h"
#include "LGFX_CYD.hpp"
#include "ui.h"

// Bit-bang clock half period.  Raise it if readings turn out to be noisy on a
// particular board; the XPT2046 tolerates anything up to ~2MHz.
static const uint32_t TP_CLK_US = 1;

static const uint8_t CMD_X = 0xD0;  // PD=00 -> PENIRQ stays enabled
static const uint8_t CMD_Y = 0x90;

static bool     s_pressed = false;
static uint32_t s_lastEdge = 0;
static const uint32_t DEBOUNCE_MS = 50;

static inline void clk_pulse_out() {
  delayMicroseconds(TP_CLK_US);
  digitalWrite(CYD_TP_CLK, HIGH);
  delayMicroseconds(TP_CLK_US);
  digitalWrite(CYD_TP_CLK, LOW);
}

// One XPT2046 conversion: 8 command bits out, 1 busy clock, 12 data bits in.
static uint16_t tp_xfer(uint8_t cmd) {
  for (int i = 7; i >= 0; --i) {
    digitalWrite(CYD_TP_MOSI, (cmd >> i) & 1);
    clk_pulse_out();
  }
  digitalWrite(CYD_TP_MOSI, LOW);
  clk_pulse_out();                       // busy / conversion clock

  uint16_t v = 0;
  for (int i = 0; i < 12; ++i) {
    delayMicroseconds(TP_CLK_US);
    digitalWrite(CYD_TP_CLK, HIGH);
    delayMicroseconds(TP_CLK_US);
    v = (v << 1) | (digitalRead(CYD_TP_MISO) ? 1 : 0);
    digitalWrite(CYD_TP_CLK, LOW);
  }
  for (int i = 0; i < 3; ++i) clk_pulse_out();   // flush the remaining bits
  return v;
}

void touch_begin() {
  pinMode(CYD_TP_CLK,  OUTPUT);
  pinMode(CYD_TP_MOSI, OUTPUT);
  pinMode(CYD_TP_CS,   OUTPUT);
  pinMode(CYD_TP_MISO, INPUT);
  pinMode(CYD_TP_IRQ,  INPUT);
  digitalWrite(CYD_TP_CS,  HIGH);
  digitalWrite(CYD_TP_CLK, LOW);
}

static int16_t median3(int16_t a, int16_t b, int16_t c) {
  if (a > b) { int16_t t = a; a = b; b = t; }
  if (b > c) { int16_t t = b; b = c; c = t; }
  if (a > b) { int16_t t = a; a = b; b = t; }
  return b;
}

bool touch_readRaw(int16_t& rx, int16_t& ry) {
  // PENIRQ is low while the panel is pressed.  Gate on it so the bit-bang loop
  // costs nothing when nobody is touching the screen.
  if (digitalRead(CYD_TP_IRQ) != LOW) return false;

  int16_t xs[3], ys[3];
  digitalWrite(CYD_TP_CS, LOW);
  for (int i = 0; i < 3; ++i) {
    ys[i] = (int16_t)tp_xfer(CMD_Y);
    xs[i] = (int16_t)tp_xfer(CMD_X);
  }
  digitalWrite(CYD_TP_CS, HIGH);

  int16_t x = median3(xs[0], xs[1], xs[2]);
  int16_t y = median3(ys[0], ys[1], ys[2]);

  // A released panel reads as a rail value; reject those.
  if (x < 100 || x > 4000 || y < 100 || y > 4000) return false;
  rx = x;
  ry = y;
  return true;
}

static int16_t mapClamp(int16_t v, int16_t inMin, int16_t inMax, int16_t outMax) {
  if (inMax == inMin) return 0;
  long r = (long)(v - inMin) * outMax / (inMax - inMin);
  if (r < 0) r = 0;
  if (r > outMax) r = outMax;
  return (int16_t)r;
}

bool touch_read(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!touch_readRaw(rx, ry)) return false;

  const TouchCalib& c = g_cfg.calib;
  int16_t ax = c.swapxy ? ry : rx;   // raw axis feeding the screen X axis
  int16_t ay = c.swapxy ? rx : ry;

  x = mapClamp(ax, c.xmin, c.xmax, lcd.width()  - 1);
  y = mapClamp(ay, c.ymin, c.ymax, lcd.height() - 1);
  return true;
}

bool touch_tapped(int16_t& x, int16_t& y) {
  int16_t tx, ty;
  bool now = touch_read(tx, ty);
  uint32_t ms = millis();

  if (now && !s_pressed && (ms - s_lastEdge) > DEBOUNCE_MS) {
    s_pressed  = true;
    s_lastEdge = ms;
    x = tx;
    y = ty;
    return true;
  }
  if (!now && s_pressed && (ms - s_lastEdge) > DEBOUNCE_MS) {
    s_pressed  = false;
    s_lastEdge = ms;
  }
  return false;
}

void touch_calibrateFrom(int16_t rx0, int16_t ry0, int16_t sx0, int16_t sy0,
                         int16_t rx1, int16_t ry1, int16_t sx1,
                         int16_t rx2, int16_t ry2, int16_t sy2,
                         int16_t screenW, int16_t screenH) {
  // P0 = (sx0,sy0)  P1 = (sx1,sy0)  P2 = (sx0,sy2)
  // Whichever raw axis moves most between P0 and P1 is the one driving screen X.
  long dxx = labs((long)rx1 - rx0);
  long dxy = labs((long)ry1 - ry0);
  bool swapxy = (dxy > dxx);

  int16_t ax0 = swapxy ? ry0 : rx0, ax1 = swapxy ? ry1 : rx1;
  int16_t ay0 = swapxy ? rx0 : ry0, ay2 = swapxy ? rx2 : ry2;

  float spanX = (float)(sx1 - sx0);
  float spanY = (float)(sy2 - sy0);
  if (spanX == 0 || spanY == 0) return;

  float kx = (float)(ax1 - ax0) / spanX;   // raw units per screen pixel
  float ky = (float)(ay2 - ay0) / spanY;
  if (kx == 0 || ky == 0) return;

  g_cfg.calib.swapxy = swapxy;
  g_cfg.calib.xmin = (int16_t)(ax0 - kx * sx0);
  g_cfg.calib.xmax = (int16_t)(ax0 + kx * ((screenW - 1) - sx0));
  g_cfg.calib.ymin = (int16_t)(ay0 - ky * sy0);
  g_cfg.calib.ymax = (int16_t)(ay0 + ky * ((screenH - 1) - sy0));
}
