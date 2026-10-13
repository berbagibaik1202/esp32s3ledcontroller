"""Package host-rendered animation frames as a standalone interactive HTML file."""
import base64
from pathlib import Path

folder = Path(".build-temp/previews")
frames = (folder / "animation-frames.rgb").read_bytes()
assert len(frames) == 60 * 64 * 32 * 3, "Run tools/test-host.ps1 first"
html = '''<!doctype html><html lang="id"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Preview animasi ESP32</title>
<style>body{background:#10151e;color:#eef3fa;font:16px system-ui;max-width:850px;margin:40px auto;padding:20px}canvas{display:block;width:100%;image-rendering:pixelated;background:black;border:1px solid #354152;border-radius:10px;margin:24px 0}button,input{font:inherit;padding:10px;background:#223344;color:white;border:1px solid #415771;border-radius:8px}input{width:100%;padding:0}p{color:#a5b3c8;line-height:1.6}</style>
<h1>Preview teks & gambar beranimasi</h1><p>Frame dibuat oleh mesin render C++ yang sama dengan firmware: gambar RGB memakai fade, teks memakai scroll ke kiri. Canvas 64x32, preview 10 FPS. Ini preview software; output panel fisik belum diuji.</p>
<canvas id="frame" width="64" height="32"></canvas><input id="seek" type="range" min="0" max="59" value="0" aria-label="Pilih frame"><p><button id="play">Pause</button> <button id="restart">Mulai ulang</button> <span id="time"></span></p>
<script>const raw=Uint8Array.from(atob('PAYLOAD'),c=>c.charCodeAt(0));let index=0,playing=true;const c=document.getElementById('frame'),ctx=c.getContext('2d');function draw(){const image=ctx.createImageData(64,32);for(let i=0;i<64*32;i++){image.data[i*4]=raw[index*6144+i*3];image.data[i*4+1]=raw[index*6144+i*3+1];image.data[i*4+2]=raw[index*6144+i*3+2];image.data[i*4+3]=255}ctx.putImageData(image,0,0);document.getElementById('seek').value=index;document.getElementById('time').textContent=(index/10).toFixed(1)+' detik'}function toggle(){playing=!playing;document.getElementById('play').textContent=playing?'Pause':'Play'}document.getElementById('play').onclick=toggle;document.getElementById('restart').onclick=()=>{index=0;draw()};document.getElementById('seek').oninput=e=>{index=Number(e.target.value);if(playing)toggle();draw()};draw();setInterval(()=>{if(playing){index=(index+1)%60;draw()}},100);</script></html>'''
target = folder / "animation-preview.html"
target.write_text(html.replace("PAYLOAD", base64.b64encode(frames).decode()), encoding="utf-8")
print(target.resolve())
