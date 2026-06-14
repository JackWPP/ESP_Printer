#include "WebAssets.h"

namespace timeprint {

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>时光小印</title>
<style>
:root{color-scheme:light;--ink:#222;--muted:#68707a;--line:#d8dde3;--paper:#f7f8fa;--panel:#fff;--red:#e23d3d;--accent:#176f8f;--ok:#2f7d51;--accent-light:#e8f4f8;--danger:#e23d3d;--danger-bg:#fef2f2}
*{box-sizing:border-box;margin:0;padding:0}body{font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:var(--paper);color:var(--ink);padding-bottom:20px}
.wrap{max-width:600px;margin:0 auto;padding:12px}
.header{text-align:center;padding:16px 0 12px}
.header h1{font-size:22px;margin:0 0 4px}
.header .wifi-status{font-size:13px;color:var(--muted);margin:0 0 10px}
.dial-wrap{display:flex;justify-content:center;margin:6px 0}
.dial{width:200px;height:200px;display:block}
.dial-bg{fill:#fff;stroke:#c8ced6;stroke-width:2}
.dial-rem{fill:var(--accent)}
.dial-cover{fill:#fff}
.dial-center{fill:#fff;stroke:#c8ced6;stroke-width:2}
.clock.done{color:var(--ok)}
.state-label{font-size:14px;color:var(--muted);text-transform:uppercase;margin:0 0 6px}
.tabs{display:flex;border-bottom:2px solid var(--line);margin:0 -12px;padding:0 12px;position:sticky;top:0;background:var(--paper);z-index:10}
.tab{flex:1;text-align:center;padding:10px 4px;font-size:14px;font-weight:500;color:var(--muted);cursor:pointer;border-bottom:2px solid transparent;margin-bottom:-2px;transition:color .2s,border-color .2s}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}
.tab:hover{color:var(--ink)}
.tab-content{display:none;padding:14px 0}
.tab-content.active{display:block}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:12px}
.panel h2{font-size:15px;margin:0 0 10px;font-weight:600}
label{display:block;font-size:13px;color:var(--muted);margin-bottom:5px}
input[type=text],input[type=password],input[type=number]{width:100%;height:40px;border:1px solid var(--line);border-radius:8px;padding:0 10px;font-size:15px;background:#fff;color:var(--ink);outline:none}
input:focus{border-color:var(--accent)}
button{height:40px;border:1px solid var(--line);border-radius:8px;background:#fff;color:var(--ink);font-size:14px;cursor:pointer;padding:0 14px;transition:background .15s,color .15s}
button:hover{background:#f0f0f0}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.primary:hover{background:#145d78}
button:disabled{opacity:.5;cursor:not-allowed}
button.danger{background:var(--danger-bg);border-color:var(--danger);color:var(--danger)}
button.danger:hover{background:#fde2e2}
.btn-row{display:flex;gap:8px;margin-top:10px}
.btn-row>*{flex:1}
.presets{display:flex;gap:8px;margin:8px 0}
.preset{flex:1;height:34px;border-radius:17px;border:1px solid var(--line);background:#fff;font-size:13px;cursor:pointer;text-align:center;padding:0;transition:background .15s,color .15s}
.preset:hover,.preset.active{background:var(--accent);color:#fff;border-color:var(--accent)}
input[type=range]{-webkit-appearance:none;width:100%;height:6px;border-radius:3px;background:var(--line);outline:none;margin:8px 0;padding:0;border:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:var(--accent);cursor:pointer;border:none;box-shadow:0 1px 4px rgba(0,0,0,.2)}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:var(--accent);cursor:pointer;border:none}
.statusline{font-size:13px;color:var(--muted);min-height:18px;margin-top:8px;word-break:break-all}
.slip{font-family:monospace;background:#fafafa;padding:12px;border:1px dashed var(--line);border-radius:8px;white-space:pre-line;font-size:13px;line-height:1.6;text-align:center;margin:10px 0}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:4px 12px;margin-top:6px}
.metric{display:flex;justify-content:space-between;font-size:13px;padding:5px 0;border-bottom:1px solid var(--line)}
.metric:last-child,.metric:nth-last-child(2):nth-child(odd)~*{border-bottom:none}
.msg-card{border:1px solid var(--line);border-radius:8px;padding:10px 12px;margin-bottom:8px;cursor:pointer;transition:border-color .2s,background .2s;display:flex;align-items:center;gap:8px}
.msg-card:hover{background:#f8f9fa}
.msg-card.active{border-color:var(--accent);background:var(--accent-light)}
.msg-card .msg-text{flex:1;font-size:14px;line-height:1.4}
.msg-card .msg-actions{display:flex;gap:4px;flex-shrink:0}
.msg-card .msg-actions button{width:32px;height:32px;padding:0;font-size:16px;border-radius:6px;display:flex;align-items:center;justify-content:center}
.msg-add{display:flex;gap:8px;margin-top:10px}
.msg-add input{flex:1}
.msg-add button{flex-shrink:0}
.mode-toggle{display:flex;gap:0;border:1px solid var(--line);border-radius:8px;overflow:hidden;margin:8px 0}
.mode-btn{flex:1;height:36px;border:none;border-radius:0;font-size:13px;background:#fff;color:var(--muted);cursor:pointer;transition:background .15s,color .15s}
.mode-btn.active{background:var(--accent);color:#fff}
.preview-box{font-family:monospace;background:#fafafa;padding:12px;border:1px dashed var(--line);border-radius:8px;white-space:pre-line;font-size:13px;line-height:1.5;min-height:60px;margin-top:10px}
.info-grid{display:grid;grid-template-columns:auto 1fr;gap:6px 12px;font-size:14px}
.info-grid .k{color:var(--muted)}
.net-list{margin-top:10px}
.net-item{display:flex;align-items:center;gap:10px;padding:10px 12px;border:1px solid var(--line);border-radius:8px;margin-bottom:6px;cursor:pointer;transition:background .15s}
.net-item:hover{background:#f0f8fb}
.net-item.selected{border-color:var(--accent);background:var(--accent-light)}
.net-item .net-info{flex:1}
.net-item .net-ssid{font-size:14px;font-weight:500}
.net-item .net-detail{font-size:12px;color:var(--muted);margin-top:2px}
.signal-bars{display:flex;gap:2px;align-items:flex-end;height:16px}
.signal-bars span{display:block;width:4px;background:var(--line);border-radius:1px}
.signal-bars span.on{background:var(--ok)}
.signal-bars span:nth-child(1){height:4px}
.signal-bars span:nth-child(2){height:8px}
.signal-bars span:nth-child(3){height:12px}
.signal-bars span:nth-child(4){height:16px}
.connect-box{margin-top:10px;padding:12px;border:1px solid var(--line);border-radius:8px;background:#f8f9fa}
.connect-box label{margin-top:8px}
.connect-box label:first-child{margin-top:0}
.saved-item{display:flex;align-items:center;justify-content:space-between;padding:8px 12px;border:1px solid var(--line);border-radius:8px;margin-bottom:6px}
.saved-item .ssid-text{font-size:14px}
.saved-item button{width:32px;height:32px;padding:0;font-size:16px;border-radius:6px;color:var(--danger);border-color:transparent}
.saved-item button:hover{background:var(--danger-bg)}
.spinner{display:inline-block;width:18px;height:18px;border:2px solid var(--line);border-top-color:var(--accent);border-radius:50%;animation:spin .6s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.empty{color:var(--muted);font-size:13px;text-align:center;padding:16px}
@media(max-width:480px){
.wrap{padding:8px}
.dial{width:170px;height:170px}
.clock{font-size:30px}
.tab{font-size:13px;padding:8px 2px}
}
</style>
</head>
<body>
<div class="wrap">

<div class="header">
<h1>时光小印</h1>
<p class="wifi-status" id="net">正在连接…</p>
<div class="dial-wrap">
<svg class="dial" viewBox="0 0 100 100" aria-label="计时表盘">
<circle class="dial-bg" cx="50" cy="50" r="47"></circle>
<path id="wedge" class="dial-rem" d=""></path>
<circle class="dial-center" cx="50" cy="50" r="12"></circle>
</svg>
</div>
<div class="clock" id="clock">00:00</div>
<div class="state-label" id="state">待机</div>
</div>

<div class="tabs">
<div class="tab active" data-tab="timer">计时</div>
<div class="tab" data-tab="messages">话语</div>
<div class="tab" data-tab="wifi">WiFi 设置</div>
<div class="tab" data-tab="printer">打印机</div>
</div>

<div id="tab-timer" class="tab-content active">
<div class="panel">
<h2>专注时间</h2>
<label for="minutes">分钟</label>
<input id="minutes" type="number" min="1" max="240" value="25" inputmode="numeric">
<div class="presets">
<button class="preset" data-min="5">5</button>
<button class="preset" data-min="15">15</button>
<button class="preset" data-min="25">25</button>
<button class="preset" data-min="45">45</button>
</div>
<input id="slider" type="range" min="1" max="120" value="25">
<div class="btn-row">
<button class="primary" data-cmd="set-start">设置并开始</button>
</div>
<div class="btn-row">
<button data-cmd="pause">暂停</button>
<button data-cmd="resume">继续</button>
</div>
<div class="btn-row">
<button data-cmd="stop">停止</button>
<button data-cmd="reset">重置</button>
</div>
<div class="statusline" id="msg"></div>
</div>

<div class="panel">
<h2>便签预览</h2>
<div class="slip" id="preview"></div>
</div>

<div class="panel">
<h2>计时数据</h2>
<div class="metrics">
<div class="metric"><span>计划</span><span id="planned">0 秒</span></div>
<div class="metric"><span>已用</span><span id="elapsed">0 秒</span></div>
<div class="metric"><span>剩余</span><span id="remaining">0 秒</span></div>
</div>
</div>
</div>

<div id="tab-messages" class="tab-content">
<div class="panel">
<h2>话语库</h2>
<div class="mode-toggle">
<button class="mode-btn active" data-mode="random">随机</button>
<button class="mode-btn" data-mode="fixed">固定</button>
</div>
<div id="msgList"></div>
<div class="msg-add">
<input id="newMsg" type="text" placeholder="输入新话语…">
<button id="addMsgBtn" class="primary">添加</button>
</div>
</div>
<div class="panel">
<h2>打印预览</h2>
<div class="preview-box" id="msgPreview"></div>
</div>
</div>

<div id="tab-wifi" class="tab-content">
<div class="panel">
<h2>WiFi 状态</h2>
<div class="info-grid">
<span class="k">模式</span><span id="wifiMode">—</span>
<span class="k">IP</span><span id="wifiIp">—</span>
<span class="k">已保存</span><span id="wifiCount">—</span>
</div>
</div>
<div class="panel">
<h2>添加 WiFi</h2>
<label for="scanSsid">WiFi 名称</label>
<input id="scanSsid" type="text" placeholder="输入 WiFi 名称">
<label for="scanPass" style="margin-top:8px">密码</label>
<input id="scanPass" type="password" autocomplete="off" placeholder="输入密码">
<div class="btn-row">
<button class="primary" id="connectBtn">保存并连接</button>
</div>
<div id="connectMsg" class="statusline"></div>
</div>
<div class="panel" id="scanPanel">
<h2>扫描网络 <button id="scanBtn" style="float:right;height:28px;font-size:12px">扫描</button></h2>
<div id="scanStatus" class="statusline"></div>
<div id="scanList" class="net-list"></div>
</div>
<div class="panel">
<h2>已保存网络</h2>
<div id="savedList"></div>
<div class="btn-row" style="margin-top:10px">
<button class="danger" id="resetWifiBtn" style="flex:none;width:auto;padding:0 20px">重置全部</button>
</div>
<div id="wifiMsg" class="statusline"></div>
</div>
</div>

<div id="tab-printer" class="tab-content">
<div class="panel">
<h2>打印机信息</h2>
<div class="info-grid">
<span class="k">型号</span><span id="prName">—</span>
<span class="k">RX 引脚</span><span id="prRx">—</span>
<span class="k">TX 引脚</span><span id="prTx">—</span>
<span class="k">波特率</span><span id="prBaud">—</span>
</div>
</div>
<div class="panel">
<button class="primary" id="testPrintBtn" style="width:100%">打印测试页</button>
<div id="prMsg" class="statusline"></div>
</div>
</div>

</div>
<script>
const $=id=>document.getElementById(id);
const els={net:$('net'),clock:$('clock'),state:$('state'),msg:$('msg'),minutes:$('minutes'),slider:$('slider'),planned:$('planned'),elapsed:$('elapsed'),remaining:$('remaining'),wedge:$('wedge'),preview:$('preview'),msgList:$('msgList'),newMsg:$('newMsg'),msgPreview:$('msgPreview'),wifiMode:$('wifiMode'),wifiIp:$('wifiIp'),wifiCount:$('wifiCount'),scanBtn:$('scanBtn'),scanStatus:$('scanStatus'),scanList:$('scanList'),scanSsid:$('scanSsid'),scanPass:$('scanPass'),connectBtn:$('connectBtn'),connectMsg:$('connectMsg'),savedList:$('savedList'),wifiMsg:$('wifiMsg'),resetWifiBtn:$('resetWifiBtn'),prName:$('prName'),prRx:$('prRx'),prTx:$('prTx'),prBaud:$('prBaud'),testPrintBtn:$('testPrintBtn'),prMsg:$('prMsg')};

let ws;
let templates=[];
let selectedTemplate='default';
let selectionMode='random';
let wifiData=null;
let scanResults=[];

document.querySelectorAll('.tab').forEach(t=>t.addEventListener('click',()=>{
document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));
document.querySelectorAll('.tab-content').forEach(x=>x.classList.remove('active'));
t.classList.add('active');
const target=$('tab-'+t.dataset.tab);
if(target)target.classList.add('active');
if(t.dataset.tab==='wifi'){loadWifiInfo();loadCachedScan()}
if(t.dataset.tab==='printer')loadPrinterInfo();
if(t.dataset.tab==='messages'){loadTemplates();updateMsgPreview()}
}));

const LERP_SPEED=0.08;
const animState={currentFrac:0,targetFrac:0,currentState:'idle',remaining:0,planned:0,elapsed:0};

function mmss(s){s=Math.max(0,Number(s)||0);return String(Math.floor(s/60)).padStart(2,'0')+':'+String(Math.floor(s%60)).padStart(2,'0')}
function polar(cx,cy,r,a){const rad=(a-90)*Math.PI/180;return[cx+r*Math.cos(rad),cy+r*Math.sin(rad)]}
function wedgePath(frac,state){if(frac<=0)return'';if(frac>=.999)return'M50 50 m-47 0 a47 47 0 1 0 94 0 a47 47 0 1 0 -94 0';const start=polar(50,50,47,0),end=polar(50,50,47,360*frac),large=frac>.5?1:0;return`M50 50 L${start[0]} ${start[1]} A47 47 0 ${large} 1 ${end[0]} ${end[1]} Z`}
function stateText(s){return{idle:'待机',running:'计时中',paused:'已暂停',finished:'已完成'}[s]||'未知'}
function secondsText(s){return(Number(s)||0)+' 秒'}

function nowStr(){
const d=new Date();
const y=d.getFullYear();
const m=d.getMonth()+1;
const day=d.getDate();
const h=String(d.getHours()).padStart(2,'0');
const min=String(d.getMinutes()).padStart(2,'0');
return y+'年'+m+'月'+day+'日 '+h+':'+min;
}

function renderSlip(){
const a=animState;
const msg=templates.find(t=>t.id===selectedTemplate);
const line='━━━━━━━━━━━━━━━━━━━━━━';
const sessionNum=Math.floor(Date.now()/86400000)%1000;
let txt=line+'\n    ✦ 时光小印 ✦\n  「'+(msg?msg.message:'专注的每一刻都值得记录')+'」\n'+line+'\n  '+nowStr()+'\n  第 '+sessionNum+' 次专注\n'+line;
els.preview.textContent=txt;
}

function renderMsgPreview(){
const msg=templates.find(t=>t.id===selectedTemplate);
const line='━━━━━━━━━━━━━━━━━━━━━━';
const a=animState;
let inner='';
if(a.currentState==='running'){
inner='  预计 : '+mmss(a.planned)+'\n  实际 : '+mmss(a.elapsed);
}else{
inner='  时长 : '+mmss(a.planned);
}
els.msgPreview.textContent=line+'\n    ✦ 时光小印 ✦\n  「'+(msg?msg.message:'—')+'」\n'+line+'\n'+inner+'\n'+line;
}
function updateMsgPreview(){renderMsgPreview()}

function renderFrame(){
const{currentFrac:frac,currentState:state,remaining,planned,elapsed}=animState;
if(state==='finished'){
els.clock.textContent=mmss(planned);
els.clock.classList.add('done');
els.wedge.setAttribute('class','dial-rem');
}else{
els.clock.textContent=mmss(remaining||planned);
els.clock.classList.remove('done');
els.wedge.setAttribute('class','dial-rem');
}
els.state.textContent=stateText(state);
els.planned.textContent=secondsText(planned);
els.elapsed.textContent=secondsText(elapsed);
els.remaining.textContent=secondsText(remaining);
els.wedge.setAttribute('d',wedgePath(frac,state));
}

function animLoop(){
animState.currentFrac+=(animState.targetFrac-animState.currentFrac)*LERP_SPEED;
renderFrame();
renderSlip();
renderMsgPreview();
requestAnimationFrame(animLoop);
}

function applyState(s){
const planned=s.planned_s||0,remaining=s.remaining_s||0;
animState.targetFrac=planned?Math.min(1,remaining/planned):0;
animState.currentState=s.state||'idle';
animState.remaining=remaining;
animState.planned=planned;
animState.elapsed=s.elapsed_s||0;
if(s.wifi_mode){
els.wifiMode.textContent=s.wifi_mode==='ap'?'AP 热点':'STA 客户端';
els.wifiIp.textContent=s.ip||'—';
}
const wsText=s.wifi_mode?(s.wifi_mode==='ap'?'AP':'STA')+' '+(s.ip||''):'已连接';
els.net.textContent=wsText;
}

async function post(obj){try{const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(obj)});if(!r.ok)throw new Error(await r.text());return r.json()}catch(e){return null}}
async function fetchStatus(){try{const r=await fetch('/api/status',{signal:AbortSignal.timeout(3000)});applyState(await r.json());els.net.textContent='已连接'}catch(_){els.net.textContent='连接断开'}}

async function command(kind){
try{
els.msg.textContent='';
if(kind==='set-start'){
await post({cmd:'reset'});
await post({cmd:'set',minutes:Number(els.minutes.value)||1});
await post({cmd:'start'});
}else if(kind==='status'){
await fetchStatus();
}else{
await post({cmd:kind});
}
}catch(e){els.msg.textContent=e.message}
}

document.querySelectorAll('button[data-cmd]').forEach(b=>b.addEventListener('click',()=>command(b.dataset.cmd)));

function syncPreset(){
const v=Number(els.minutes.value);
document.querySelectorAll('.preset').forEach(b=>b.classList.toggle('active',Number(b.dataset.min)===v));
}
document.querySelectorAll('.preset').forEach(b=>b.addEventListener('click',()=>{els.minutes.value=b.dataset.min;els.slider.value=b.dataset.min;syncPreset()}));
els.slider.addEventListener('input',()=>{els.minutes.value=els.slider.value;syncPreset()});
els.minutes.addEventListener('input',()=>{els.slider.value=els.minutes.value;syncPreset()});
syncPreset();

function loadTemplates(){
fetch('/api/templates').then(r=>r.json()).then(d=>{templates=d;renderMessageCards();updateMsgPreview()}).catch(()=>{});
}

function renderMessageCards(){
els.msgList.innerHTML='';
if(!templates.length){els.msgList.innerHTML='<div class="empty">暂无话语</div>';return}
templates.forEach(t=>{
const card=document.createElement('div');
card.className='msg-card'+(t.id===selectedTemplate?' active':'');
card.innerHTML='<div class="msg-text">'+t.message+'</div><div class="msg-actions"><button class="edit-btn" title="选用">✎</button><button class="del-btn" title="删除">×</button></div>';
card.addEventListener('click',async e=>{
if(e.target.closest('.del-btn')){
if(t.id.startsWith('custom_')){
const idx=templates.filter(x=>x.id.startsWith('custom_')).indexOf(t);
if(idx>=0)try{await fetch('/api/messages/'+idx,{method:'DELETE'})}catch(_){}
}
templates=templates.filter(x=>x.id!==t.id);
if(selectedTemplate===t.id&&templates.length)selectedTemplate=templates[0].id;
renderMessageCards();
updateMsgPreview();
return;
}
selectedTemplate=t.id;
post({cmd:'template',message:t.message});
renderMessageCards();
updateMsgPreview();
});
els.msgList.appendChild(card);
});
}

$('addMsgBtn').addEventListener('click',async()=>{
const text=els.newMsg.value.trim();
if(!text)return;
try{
await fetch('/api/messages',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:text})});
els.newMsg.value='';
loadTemplates();
post({cmd:'template',message:text});
}catch(e){}
});

els.newMsg.addEventListener('keydown',e=>{if(e.key==='Enter')$('addMsgBtn').click()});

document.querySelectorAll('.mode-btn').forEach(b=>b.addEventListener('click',()=>{
document.querySelectorAll('.mode-btn').forEach(x=>x.classList.remove('active'));
b.classList.add('active');
selectionMode=b.dataset.mode;
}));

async function loadWifiInfo(){
try{
const r=await fetch('/api/wifi');
const d=await r.json();
wifiData=d;
els.wifiCount.textContent=d.items?d.items.length+' / '+d.max:'—';
renderSavedList(d);
}catch(e){
els.wifiMsg.textContent='加载失败: '+e.message;
}
}

function renderSavedList(d){
els.savedList.innerHTML='';
if(!d||!d.items||!d.items.length){els.savedList.innerHTML='<div class="empty">暂无已保存网络</div>';return}
d.items.forEach((item,i)=>{
const div=document.createElement('div');
div.className='saved-item';
div.innerHTML='<span class="ssid-text">'+item.ssid+'</span>';
const btn=document.createElement('button');
btn.textContent='×';
btn.addEventListener('click',async()=>{
if(!confirm('删除 "'+item.ssid+'"？'))return;
try{
await fetch('/api/wifi/'+i,{method:'DELETE'});
els.wifiMsg.textContent='已删除 '+item.ssid;
setTimeout(loadWifiInfo,1000);
}catch(e){els.wifiMsg.textContent='删除失败: '+e.message}
});
div.appendChild(btn);
els.savedList.appendChild(div);
});
}

els.resetWifiBtn.addEventListener('click',async()=>{
if(!confirm('确定要重置全部已保存的 WiFi 网络吗？'))return;
try{
if(!wifiData||!wifiData.items)return;
for(let i=wifiData.items.length-1;i>=0;i--){
await fetch('/api/wifi/'+i,{method:'DELETE'});
}
els.wifiMsg.textContent='已重置全部，设备即将重启。';
setTimeout(loadWifiInfo,2000);
}catch(e){els.wifiMsg.textContent='重置失败: '+e.message}
});

els.scanBtn.addEventListener('click',async()=>{refreshScan()});
async function refreshScan(){
els.scanBtn.disabled=true;
els.scanStatus.innerHTML='<span class="spinner"></span> 扫描中…';
els.scanList.innerHTML='';
for(let i=0;i<30;i++){
await new Promise(r=>setTimeout(r,2000));
try{
const r=await fetch('/api/wifi/scan',{signal:AbortSignal.timeout(3000)});
const d=await r.json();
if(!d.scanning&&Array.isArray(d.aps)&&d.aps.length>0){
scanResults=d.aps;
scanResults.sort((a,b)=>b.rssi-a.rssi);
renderScanList();
els.scanStatus.textContent='发现 '+scanResults.length+' 个网络';
els.scanBtn.textContent='刷新列表';
els.scanBtn.disabled=false;
return;
}
}catch(_){}
}
els.scanStatus.textContent='未发现网络';
els.scanBtn.disabled=false;
}
async function loadCachedScan(){
try{
const r=await fetch('/api/wifi/scan',{signal:AbortSignal.timeout(3000)});
const d=await r.json();
if(!d.scanning&&Array.isArray(d.aps)&&d.aps.length>0){
scanResults=d.aps;
scanResults.sort((a,b)=>b.rssi-a.rssi);
renderScanList();
els.scanStatus.textContent='发现 '+scanResults.length+' 个网络';
els.scanBtn.textContent='刷新列表';
}
}catch(_){}
}

function signalBars(rssi){
let level=0;
if(rssi>-50)level=4;else if(rssi>-60)level=3;else if(rssi>-70)level=2;else if(rssi>-80)level=1;
let html='<div class="signal-bars">';
for(let i=0;i<4;i++)html+='<span'+(i<level?' class="on"':'')+'></span>';
html+='</div>';
return html;
}

function renderScanList(){
els.scanList.innerHTML='';
if(!scanResults.length){els.scanList.innerHTML='<div class="empty">未发现网络</div>';return}
scanResults.forEach(net=>{
const div=document.createElement('div');
div.className='net-item';
div.innerHTML=signalBars(net.rssi)+'<div class="net-info"><div class="net-ssid">'+net.ssid+'</div><div class="net-detail">'+(net.secure?'🔒 ':'')+'CH '+(net.channel||'?')+' · '+net.rssi+' dBm</div></div>';
div.addEventListener('click',()=>{
document.querySelectorAll('.net-item').forEach(x=>x.classList.remove('selected'));
div.classList.add('selected');
els.scanSsid.value=net.ssid;
els.scanPass.value='';
els.scanPass.focus();
});
els.scanList.appendChild(div);
});
}

els.connectBtn.addEventListener('click',async()=>{
const ssid=els.scanSsid.value;
const pass=els.scanPass.value;
if(!ssid)return;
els.connectBtn.disabled=true;
els.connectMsg.textContent='正在连接…';
try{
const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,pass:pass})});
const d=await r.json();
if(d.ok){
els.connectMsg.textContent='已保存，设备正在重启…';
}else{
els.connectMsg.textContent=d.error||'保存失败';
}
}catch(e){els.connectMsg.textContent=e.message}
finally{els.connectBtn.disabled=false}
});

async function loadPrinterInfo(){
try{
const r=await fetch('/api/printer');
const d=await r.json();
els.prName.textContent=d.active||'—';
els.prRx.textContent=d.rx_pin||'—';
els.prTx.textContent=d.tx_pin||'—';
els.prBaud.textContent=d.baud||'—';
}catch(e){els.prMsg.textContent='加载失败: '+e.message}
}

els.testPrintBtn.addEventListener('click',async()=>{
els.testPrintBtn.disabled=true;
els.prMsg.textContent='正在打印…';
try{
const r=await fetch('/api/printer/test',{method:'POST'});
const d=await r.json();
els.prMsg.textContent=d.ok?'测试页已发送':(d.error||'打印失败');
}catch(e){els.prMsg.textContent=e.message}
finally{els.testPrintBtn.disabled=false}
});

function connectWs(){}

setInterval(fetchStatus,2000);
fetchStatus();
requestAnimationFrame(animLoop);
loadTemplates();
</script>
</body>
</html>
)HTML";

}  // namespace timeprint
