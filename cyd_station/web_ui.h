// web_ui.h - the image upload page, served from flash.
#pragma once
#include <Arduino.h>

static const char UPLOAD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD 情報ステーション</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:#12161c;color:#e8edf2;
     font-family:system-ui,-apple-system,"Segoe UI","Hiragino Sans","Noto Sans JP",sans-serif}
h1{font-size:18px;margin:0 0 4px}
p.sub{margin:0 0 16px;color:#8b98a8;font-size:13px}
.card{background:#1a2029;border:1px solid #2b3542;border-radius:10px;padding:14px;margin-bottom:14px}
label{display:block;font-size:13px;color:#a8b4c2;margin-bottom:6px}
input[type=file]{width:100%;font-size:14px}
input[type=range]{width:100%}
.frame{position:relative;width:100%;max-width:480px;margin:0 auto;
       aspect-ratio:4/3;background:#000;border:1px solid #3a4756;border-radius:6px;overflow:hidden;touch-action:none}
canvas{display:block;position:absolute;left:0;top:0;width:100%;height:100%}
#ov{pointer-events:none}
.hint{font-size:12px;color:#7b8796;margin-top:8px;text-align:center}
.hint b{color:#a8b4c2;font-weight:600}
.row{display:flex;gap:10px;align-items:center;margin-top:10px}
.row span{font-size:12px;color:#8b98a8;min-width:74px}
button{width:100%;padding:12px;font-size:15px;font-weight:600;border:0;border-radius:8px;
       background:#2f7fd0;color:#fff;margin-top:12px}
button:disabled{background:#39434f;color:#7b8796}
button.ghost{background:#39434f;color:#dfe6ee}
#msg{margin-top:10px;font-size:13px;min-height:18px}
.ok{color:#59d07a}.ng{color:#e26a6a}
small{color:#66727f}
</style>
</head>
<body>
<h1>CYD 情報ステーション</h1>
<p class="sub">画像を選んで位置と大きさを合わせ、320x240 で送信します。</p>

<div class="card">
  <label for="file">1. 画像を選ぶ</label>
  <input id="file" type="file" accept="image/*">
</div>

<div class="card">
  <label>2. アスペクト比を保ったままクロップ</label>
  <div class="frame" id="frame">
    <canvas id="cv" width="320" height="240"></canvas>
    <canvas id="ov" width="320" height="240"></canvas>
  </div>
  <div class="hint">枠の中をドラッグして位置を調整<br>
    <b>薄く塗られた範囲にデバイスの時計が重なります</b>（送信する画像には入りません）</div>
  <div class="row"><span>拡大 </span><input id="zoom" type="range" min="100" max="400" value="100"></div>
  <div class="row"><span>画質 <b id="qv">85</b></span><input id="q" type="range" min="40" max="95" value="85"></div>
  <button id="send" disabled>3. デバイスへ送信</button>
  <button id="clock" class="ghost">時計表示に戻す</button>
  <div id="msg"></div>
</div>

<div class="card">
  <small id="info">状態を取得中...</small>
</div>

<script>
const cv = document.getElementById('cv');
const ctx = cv.getContext('2d');
const W = 320, H = 240;
let img = null, base = 1, zoom = 1, ox = 0, oy = 0;

// Where the device paints its clock box - keep in sync with drawClockOverlay()
// in ui.cpp. Drawn on a separate canvas so it never ends up in the JPEG.
const CLOCK_BOX = { x: 6, y: 6, w: 168, h: 48 };
function drawGuide() {
  const g = document.getElementById('ov').getContext('2d');
  g.clearRect(0, 0, W, H);
  g.fillStyle = 'rgba(255,255,255,0.28)';
  g.fillRect(CLOCK_BOX.x, CLOCK_BOX.y, CLOCK_BOX.w, CLOCK_BOX.h);
  g.strokeStyle = 'rgba(255,255,255,0.75)';
  g.lineWidth = 1;
  g.setLineDash([5, 4]);
  g.strokeRect(CLOCK_BOX.x + 0.5, CLOCK_BOX.y + 0.5, CLOCK_BOX.w - 1, CLOCK_BOX.h - 1);
  g.setLineDash([]);
  g.fillStyle = 'rgba(0,0,0,0.65)';
  g.font = '11px sans-serif';
  g.fillText('時計', CLOCK_BOX.x + 6, CLOCK_BOX.y + CLOCK_BOX.h - 6);
}

function clampOffsets() {
  const w = img.width * base * zoom, h = img.height * base * zoom;
  ox = Math.min(0, Math.max(W - w, ox));
  oy = Math.min(0, Math.max(H - h, oy));
}
function render() {
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, W, H);
  if (!img) return;
  clampOffsets();
  ctx.drawImage(img, ox, oy, img.width * base * zoom, img.height * base * zoom);
}
document.getElementById('file').addEventListener('change', e => {
  const f = e.target.files[0];
  if (!f) return;
  const url = URL.createObjectURL(f);
  const im = new Image();
  im.onload = () => {
    img = im;
    base = Math.max(W / im.width, H / im.height);   // cover: aspect ratio kept
    zoom = 1;
    document.getElementById('zoom').value = 100;
    ox = (W - im.width * base) / 2;
    oy = (H - im.height * base) / 2;
    document.getElementById('send').disabled = false;
    render();
    URL.revokeObjectURL(url);
  };
  im.src = url;
});
document.getElementById('zoom').addEventListener('input', e => {
  if (!img) return;
  const nz = e.target.value / 100;
  const cx = W / 2, cy = H / 2;
  ox = cx - (cx - ox) * (nz / zoom);
  oy = cy - (cy - oy) * (nz / zoom);
  zoom = nz;
  render();
});
const qEl = document.getElementById('q');
qEl.addEventListener('input', () => document.getElementById('qv').textContent = qEl.value);

let drag = null;
const frame = document.getElementById('frame');
frame.addEventListener('pointerdown', e => {
  if (!img) return;
  frame.setPointerCapture(e.pointerId);
  drag = { x: e.clientX, y: e.clientY, ox: ox, oy: oy };
});
frame.addEventListener('pointermove', e => {
  if (!drag) return;
  const k = W / frame.clientWidth;
  ox = drag.ox + (e.clientX - drag.x) * k;
  oy = drag.oy + (e.clientY - drag.y) * k;
  render();
});
frame.addEventListener('pointerup', () => { drag = null; });
frame.addEventListener('pointercancel', () => { drag = null; });

function msg(text, cls) {
  const m = document.getElementById('msg');
  m.textContent = text;
  m.className = cls || '';
}
document.getElementById('send').addEventListener('click', () => {
  if (!img) return;
  msg('送信中...');
  cv.toBlob(blob => {
    if (!blob) { msg('JPEGへの変換に失敗しました', 'ng'); return; }
    const fd = new FormData();
    fd.append('image', blob, 'upload.jpg');
    fetch('/upload', { method: 'POST', body: fd })
      .then(r => r.text().then(t => {
        if (r.ok) { msg('送信しました (' + Math.round(blob.size / 1024) + ' KB)', 'ok'); status(); }
        else      { msg('エラー: ' + t, 'ng'); }
      }))
      .catch(err => msg('通信エラー: ' + err, 'ng'));
  }, 'image/jpeg', qEl.value / 100);
});
document.getElementById('clock').addEventListener('click', () => {
  fetch('/mode?m=clock', { method: 'POST' }).then(status);
});

function status() {
  fetch('/status').then(r => r.json()).then(s => {
    document.getElementById('info').textContent =
      'IP ' + s.ip + ' / ' + s.hostname + '.local / NTP ' + (s.ntp ? '同期済' : '未同期') +
      ' / SD ' + (s.sd ? ('あり (' + s.album + '枚)') : 'なし') +
      ' / 空きメモリ ' + Math.round(s.heap / 1024) + 'KB';
  }).catch(() => {});
}
status();
setInterval(status, 10000);
render();
drawGuide();
</script>
</body>
</html>
)rawliteral";
