#include "WebAssets.h"

namespace timeprint {

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TimePrint</title>
<style>
:root{color-scheme:light;--ink:#222;--muted:#68707a;--line:#d8dde3;--paper:#f7f8fa;--panel:#fff;--red:#e23d3d;--accent:#176f8f;--ok:#2f7d51}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:var(--paper);color:var(--ink)}
main{width:min(960px,100%);margin:0 auto;padding:20px;display:grid;grid-template-columns:minmax(280px,360px) 1fr;gap:22px;align-items:start}
h1{font-size:24px;margin:0 0 4px}.sub{color:var(--muted);font-size:14px;margin:0 0 18px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:16px}
.dial{width:100%;aspect-ratio:1;display:block}.dial-bg{fill:#fff;stroke:#c8ced6;stroke-width:2}.dial-rem{fill:var(--red)}.dial-cover{fill:#fff}.dial-center{fill:#fff;stroke:#c8ced6;stroke-width:2}
.time{font-size:34px;font-weight:700;margin:14px 0 4px}.state{font-size:15px;color:var(--muted);text-transform:uppercase}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;margin-top:14px}
label{display:block;font-size:13px;color:var(--muted);margin-bottom:6px}input{width:100%;height:42px;border:1px solid var(--line);border-radius:6px;padding:0 10px;font-size:16px;background:#fff;color:var(--ink)}
button{height:42px;border:1px solid #bfc8d2;border-radius:6px;background:#fff;color:var(--ink);font-size:15px;cursor:pointer}button.primary{background:var(--accent);border-color:var(--accent);color:#fff}button:disabled{opacity:.5;cursor:not-allowed}
.row{display:flex;gap:10px;margin-top:10px}.row>*{flex:1}.statusline{font-size:13px;color:var(--muted);margin-top:12px;min-height:20px}.setup{margin-top:16px;border-top:1px solid var(--line);padding-top:16px}.metric{display:flex;justify-content:space-between;border-bottom:1px solid var(--line);padding:9px 0;font-size:14px}.metric:last-child{border-bottom:0}
@media(max-width:760px){main{grid-template-columns:1fr;padding:14px}.time{font-size:30px}}
</style>
</head>
<body>
<main>
  <section class="panel">
    <h1>TimePrint</h1>
    <p class="sub" id="net">Connecting</p>
    <svg class="dial" viewBox="0 0 100 100" aria-label="timer dial">
      <circle class="dial-bg" cx="50" cy="50" r="47"></circle>
      <path id="wedge" class="dial-rem" d=""></path>
      <circle class="dial-center" cx="50" cy="50" r="12"></circle>
    </svg>
    <div class="time" id="clock">00:00</div>
    <div class="state" id="state">idle</div>
  </section>
  <section class="panel">
    <label for="minutes">Minutes</label>
    <input id="minutes" type="number" min="1" max="240" value="25" inputmode="numeric">
    <div class="grid">
      <button class="primary" data-cmd="set-start">Set + Start</button>
      <button data-cmd="pause">Pause</button>
      <button data-cmd="resume">Resume</button>
      <button data-cmd="stop">Stop</button>
      <button data-cmd="reset">Reset</button>
      <button data-cmd="status">Refresh</button>
    </div>
    <div class="statusline" id="msg"></div>
    <div class="setup">
      <label for="ssid">WiFi SSID</label>
      <input id="ssid" autocomplete="off">
      <label for="pass" style="margin-top:10px">WiFi Password</label>
      <input id="pass" type="password" autocomplete="off">
      <div class="row">
        <button data-cmd="savewifi">Save WiFi</button>
      </div>
    </div>
    <div class="setup">
      <div class="metric"><span>Planned</span><span id="planned">0s</span></div>
      <div class="metric"><span>Elapsed</span><span id="elapsed">0s</span></div>
      <div class="metric"><span>Remaining</span><span id="remaining">0s</span></div>
      <div class="metric"><span>Overtime</span><span id="overtime">0s</span></div>
    </div>
  </section>
</main>
<script>
const els={net:document.getElementById('net'),clock:document.getElementById('clock'),state:document.getElementById('state'),msg:document.getElementById('msg'),minutes:document.getElementById('minutes'),ssid:document.getElementById('ssid'),pass:document.getElementById('pass'),planned:document.getElementById('planned'),elapsed:document.getElementById('elapsed'),remaining:document.getElementById('remaining'),overtime:document.getElementById('overtime'),wedge:document.getElementById('wedge')};
let ws;
function mmss(s){s=Math.max(0,Number(s)||0);return String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0')}
function polar(cx,cy,r,a){const rad=(a-90)*Math.PI/180;return [cx+r*Math.cos(rad),cy+r*Math.sin(rad)]}
function wedgePath(frac,state){if(state==='overtime'||frac<=0)return '';if(frac>=.999)return 'M50 50 m-47 0 a47 47 0 1 0 94 0 a47 47 0 1 0 -94 0';const start=polar(50,50,47,0),end=polar(50,50,47,360*frac),large=frac>.5?1:0;return `M50 50 L${start[0]} ${start[1]} A47 47 0 ${large} 1 ${end[0]} ${end[1]} Z`}
function render(s){const planned=s.planned_s||0,remaining=s.remaining_s||0,elapsed=s.elapsed_s||0,overtime=s.overtime_s||0;els.clock.textContent=s.state==='overtime'?('+'+mmss(overtime)):mmss(remaining||planned);els.state.textContent=s.state||'idle';els.planned.textContent=planned+'s';els.elapsed.textContent=elapsed+'s';els.remaining.textContent=remaining+'s';els.overtime.textContent=overtime+'s';els.wedge.setAttribute('d',wedgePath(planned?remaining/planned:0,s.state))}
async function post(obj){const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(obj)});if(!r.ok)throw new Error(await r.text());return r.json()}
async function status(){const r=await fetch('/api/status');render(await r.json())}
async function command(kind){try{els.msg.textContent='';if(kind==='set-start'){await post({cmd:'reset'});await post({cmd:'set',minutes:Number(els.minutes.value)||1});await post({cmd:'start'})}else if(kind==='savewifi'){await post({cmd:'config',data:{ssid:els.ssid.value,pass:els.pass.value}});els.msg.textContent='WiFi saved. Device restarting.'}else if(kind==='status'){await status()}else{await post({cmd:kind})}}catch(e){els.msg.textContent=e.message}}
document.querySelectorAll('button[data-cmd]').forEach(b=>b.addEventListener('click',()=>command(b.dataset.cmd)));
function connect(){ws=new WebSocket(`ws://${location.host}/ws`);ws.onopen=()=>{els.net.textContent='WebSocket connected'};ws.onclose=()=>{els.net.textContent='WebSocket closed';setTimeout(connect,1500)};ws.onerror=()=>{els.net.textContent='WebSocket error'};ws.onmessage=e=>{try{render(JSON.parse(e.data))}catch(_){}}}
connect();status();
</script>
</body>
</html>
)HTML";

}  // namespace timeprint
