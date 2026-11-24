// Sürücü İstasyonu web sayfası (robot_web.ino tarafından http://192.168.4.1 adresinde sunulur)
#pragma once

const char DS_PAGE[] = R"rawliteral(<!doctype html>
<html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MEB Robot Sürücü İstasyonu</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#1c232c;--line:#2a333e;--text:#e6edf3;--dim:#8b98a5;--acc:#3b82f6;--ok:#22c55e;--warn:#eab308;--bad:#ef4444;--org:#f97316}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:14px/1.45 system-ui,-apple-system,Segoe UI,sans-serif}
header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:16px;padding:8px 16px;background:var(--panel);border-bottom:1px solid var(--line);flex-wrap:wrap}
.brand{font-weight:700;font-size:16px;white-space:nowrap}
nav{display:flex;gap:4px;flex-wrap:wrap}
.tab{background:none;border:0;color:var(--dim);font:600 14px inherit;padding:8px 14px;border-radius:8px;cursor:pointer}
.tab.on{background:var(--panel2);color:var(--text)}
.link{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--dim);margin-left:auto;white-space:nowrap}
.dot{width:10px;height:10px;border-radius:50%;background:var(--bad)}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px}
.bars b{width:4px;background:var(--line);border-radius:1px}
.bars b:nth-child(1){height:4px}.bars b:nth-child(2){height:7px}.bars b:nth-child(3){height:10px}.bars b:nth-child(4){height:14px}
.batt{--c:var(--dim);margin-top:16px;padding:16px;border-radius:12px;background:linear-gradient(160deg,color-mix(in srgb,var(--c) 12%,var(--panel2)),var(--panel2) 60%);border:1px solid color-mix(in srgb,var(--c) 35%,var(--line));transition:border-color .4s}
.batt.ok{--c:var(--ok)}.batt.warn{--c:var(--warn)}.batt.bad{--c:var(--bad)}
.bTop{display:flex;align-items:center;gap:18px}
.bIco{position:relative;width:64px;height:30px;border:3px solid var(--c);border-radius:6px;flex:none;color:var(--c)}
.bIco:after{content:"";position:absolute;right:-9px;top:7px;width:5px;height:10px;background:var(--c);border-radius:0 2px 2px 0}
.bIco b{position:absolute;left:3px;top:3px;bottom:3px;width:0;background:var(--c);border-radius:2px;transition:width .5s}
.batt.bad .bIco{animation:bl 1s steps(2) infinite}
@keyframes bl{50%{opacity:.3}}
.bVal{display:flex;flex-direction:column;line-height:1.05}
.bVal span{font:700 34px ui-monospace,Consolas,monospace;color:var(--c)}
.bVal em{font-style:normal;font-size:12px;color:var(--dim);margin-top:4px}
.bSt{margin-left:auto;padding:4px 12px;border-radius:99px;font-size:12px;font-weight:700;letter-spacing:.04em;background:color-mix(in srgb,var(--c) 18%,transparent);color:var(--c);white-space:nowrap}
.bBar{position:relative;height:8px;margin:16px 0 4px;border-radius:4px;background:linear-gradient(90deg,var(--bad) 0 22%,var(--warn) 22% 43%,var(--ok) 43%);opacity:.85}
.bBar i{position:absolute;top:-4px;width:4px;height:16px;margin-left:-2px;border-radius:2px;background:var(--text);box-shadow:0 0 0 2px var(--panel2);left:0;transition:left .5s}
.bScale{position:relative;height:14px;font:11px ui-monospace,Consolas,monospace;color:var(--dim)}
.bScale span{position:absolute;white-space:nowrap;transform:translateX(-50%)}.bScale span:first-child{transform:none}.bScale span:last-child{transform:translateX(-100%)}
.bCv{display:block;width:100%;height:70px;margin-top:12px;border-radius:8px;background:#0a0e13;border:1px solid var(--line)}
.bStats{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:12px}
.bStats div{background:rgba(255,255,255,.03);border:1px solid var(--line);border-radius:8px;padding:7px 8px;min-width:0}
.bStats small{display:block;font-size:10px;color:var(--dim);text-transform:uppercase;letter-spacing:.05em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bStats b{font:700 14px ui-monospace,Consolas,monospace}
@media (max-width:420px){.bStats{grid-template-columns:repeat(2,1fr)}}
.status{display:flex;flex-direction:column;line-height:1.2;min-width:150px}
.status b{font-size:14px}.status span{font-size:12px;color:var(--dim)}
main{padding:16px;max-width:1200px;margin:0 auto}
.page{display:none}.page.on{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px}
.card h2{margin:0 0 12px;font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--dim)}
.wide{grid-column:1/-1}
button{font:600 14px inherit;color:var(--text);background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:8px 14px;cursor:pointer}
button:disabled{opacity:.4;cursor:not-allowed}
#mainBtn{width:100%;font-size:22px;padding:18px;border:0;color:#fff;background:var(--acc)}
#mainBtn.start{background:var(--ok);color:#05200e}#mainBtn.stop{background:var(--org)}
#estop{width:100%;margin-top:12px;font-size:18px;padding:16px;background:var(--bad);border:0;color:#fff;letter-spacing:.05em}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:12px}
input[type=number],select{background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:7px 10px;font:inherit}
input[type=number]{width:90px}
.sw{position:relative;width:44px;height:24px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:var(--line);border-radius:12px;cursor:pointer;transition:.15s}
.sw span:before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.15s}
.sw input:checked+span{background:var(--ok)}.sw input:checked+span:before{transform:translateX(20px)}
.sw input:disabled+span{opacity:.4;cursor:not-allowed}
.cd{margin-top:14px;display:none}.cd.on{display:block}
.cdbar{height:8px;background:var(--panel2);border-radius:4px;overflow:hidden;margin-top:6px}
.cdbar i{display:block;height:100%;background:var(--org);width:100%}
.stick{position:relative;width:150px;height:150px;border-radius:50%;background:var(--panel2);border:2px solid var(--line);flex:none}
.stick:before,.stick:after{content:"";position:absolute;background:var(--line)}
.stick:before{left:50%;top:8px;bottom:8px;width:1px}.stick:after{top:50%;left:8px;right:8px;height:1px}
.stick i{position:absolute;width:18px;height:18px;margin:-9px 0 0 -9px;border-radius:50%;background:var(--acc);left:50%;top:50%;z-index:1}
.joyrow{display:flex;align-items:center;gap:20px;flex-wrap:wrap}
.pill{display:inline-block;padding:3px 10px;border-radius:99px;font-size:12px;font-weight:700;background:var(--bad);color:#fff}
.pill.ok{background:var(--ok);color:#05200e}
.hint{color:var(--dim);font-size:12px;margin-top:6px}
#pidGrid .row,#pidGrid h2{position:relative}
#pidGrid [data-tip] label,#pidGrid h2[data-tip]{text-decoration:underline dotted var(--dim);text-underline-offset:3px;cursor:help}
#pidGrid [data-tip]:hover::after,#pidGrid .row[data-tip]:focus-within::after{content:attr(data-tip);position:absolute;left:0;bottom:calc(100% + 8px);z-index:20;width:min(330px,88vw);padding:10px 12px;border-radius:10px;background:#0a0e13;border:1px solid var(--acc);box-shadow:0 6px 20px rgba(0,0,0,.5);color:var(--text);font:400 12px/1.5 system-ui,-apple-system,Segoe UI,sans-serif;letter-spacing:0;text-transform:none;white-space:normal;text-align:left;pointer-events:none}
pre{margin:0;background:#0a0e13;border:1px solid var(--line);border-radius:8px;padding:10px;height:220px;overflow:auto;font:12px/1.5 ui-monospace,Consolas,monospace;white-space:pre-wrap;word-break:break-word}
table{width:100%;border-collapse:collapse}
td{padding:6px 0;border-bottom:1px solid var(--line)}
td:first-child{color:var(--dim);width:45%}
td:last-child{font-family:ui-monospace,Consolas,monospace}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin-top:14px}
.btns span{width:42px;height:42px;border-radius:50%;border:2px solid var(--line);display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:700;color:var(--dim)}
.btns span.on{background:var(--acc);border-color:var(--acc);color:#fff}
.lists{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:14px;font:12px/1.6 ui-monospace,Consolas,monospace}
.robot td:last-child{text-align:right}
.ovl{position:fixed;inset:0;z-index:10;background:rgba(5,8,12,.88);display:none;flex-direction:column;align-items:center;justify-content:center;gap:12px;text-align:center;padding:16px}
.ovl.on{display:flex}
.ovl h1{margin:0;font-size:34px;letter-spacing:.05em}
.ovl p{margin:0;color:var(--dim)}
.ovl.es h1{color:var(--bad)}
@media (max-width:900px){.brand,.status{display:none}.link{margin-left:auto}}
@media (max-width:600px){header{padding:8px 12px}main{padding:12px}.tab{padding:8px 10px}.lists{grid-template-columns:1fr}}
</style></head><body>
<header>
  <div class="brand">MEB Robot Sürücü İstasyonu</div>
  <nav><button class="tab on" data-p="dash">Panel</button><button class="tab" data-p="joy">Kol</button><button class="tab" data-p="plan">Yol Planlayıcı</button><button class="tab" data-p="pid">PID Ayarları</button><button class="tab" data-p="logs">Sistem</button></nav>
  <div class="link" title="Sinyal, gecikme, boş / toplam bellek"><i class="dot" id="dot"></i><span class="bars" id="bars"><b></b><b></b><b></b><b></b></span><span id="lat">-- ms</span><span id="mem">-- / -- KB</span></div>
  <div class="status"><b id="stName">Bekleme</b><span id="stDesc">Komut bekleniyor</span></div>
</header>

<main>
<section class="page on" id="p-dash"><div class="grid">
  <div class="card">
    <h2>Maç Kontrolü</h2>
    <button id="mainBtn">Hazırla</button>
    <div class="row"><label for="dur">Otonom Süresi (sn)</label><input id="dur" type="number" min="1" max="600" value="30"></div>
    <div class="row"><span>Otonom</span><label class="sw"><input id="autoSw" type="checkbox" checked><span></span></label></div>
    <div class="row"><label for="autoSel">Otonom Rutini</label><select id="autoSel" style="max-width:200px"><option value="0">Kod rotası (AUTO_ROUTE)</option></select></div>
    <div class="cd" id="cd"><div class="row" style="margin:0"><span>Otonom Geri Sayım</span><b id="cdTxt">0.0 sn</b></div><div class="cdbar"><i id="cdBar"></i></div></div>
    <button id="estop">ACİL DURDUR</button>
  </div>
  <div class="card">
    <h2>Kol</h2>
    <div class="joyrow"><div class="stick"><i id="dStick"></i></div>
      <div><span class="pill" id="dJoy">Bağlı Değil</span><div class="hint" id="dName">Kol yok</div>
      <button style="margin-top:12px" data-go="joy">Kol sayfasını aç</button></div></div>
    <div class="batt" id="batt">
      <div class="bTop"><i class="bIco"><b id="battFill"></b></i>
        <div class="bVal"><span id="battV">--.- V</span><em id="battPct">Akü · --%</em></div>
        <span class="bSt" id="battSt">BEKLENİYOR</span></div>
      <div class="bBar"><i id="battMk"></i></div>
      <div class="bScale"><span style="left:0">10.5</span><span id="bsL">11.0</span><span id="bsW">11.5</span><span style="left:100%">12.8 V</span></div>
      <canvas class="bCv" id="battCv"></canvas>
      <div class="bStats"><div><small>En düşük</small><b id="bMin">-</b></div><div><small>En yüksek</small><b id="bMax">-</b></div>
        <div><small>Değişim /dk</small><b id="bRate">-</b></div><div><small>Yük</small><b id="bLoad">-</b></div></div>
    </div>
  </div>
  <div class="card robot">
    <h2>Robot</h2>
    <table><tbody id="robotTbl"></tbody></table>
  </div>
  <div class="card wide">
    <h2>Telemetri</h2>
    <pre id="tel"></pre>
    <div class="btns" style="margin-top:10px"><button id="telClear">Temizle</button><button id="telAuto">Otomatik kaydır: AÇIK</button></div>
  </div>
</div></section>

<section class="page" id="p-joy"><div class="grid">
  <div class="card">
    <h2>Kol Kontrolü</h2>
    <div class="joyrow"><div class="stick"><i id="jStick"></i></div>
      <div><span class="pill" id="jJoy">Bağlı Değil</span><div class="hint" id="jHint">Etkinleştirmek için kolda herhangi bir tuşa basın.</div></div></div>
    <div class="btns" id="jBtns"></div>
  </div>
  <div class="card">
    <h2>Kumanda</h2>
    <select id="padSel" style="width:100%"><option value="-1">Kol yok</option></select>
    <div class="hint" id="padInfo">-</div>
    <div class="lists"><div id="axList"></div><div id="btList"></div></div>
  </div>
</div></section>

<section class="page" id="p-plan"><div id="ppRoot"></div></section>

<section class="page" id="p-pid"><div class="hint" style="margin:0 0 12px">İpucu: ayar adlarının ve başlıkların üstüne gelince (telefonda kutuya dokununca) ne işe yaradığı ve değiştirince ne olacağı balon olarak çıkar.</div><div class="grid" id="pidGrid"></div>
  <div class="card" style="margin-top:16px">
    <div class="row" style="margin-top:0"><button id="pidSave">Kalıcı kaydet</button><button id="pidDef">Varsayılana dön</button><button id="pidReload">Robottan yeniden oku</button></div>
    <div class="hint" id="pidMsg">Değer değişince robota hemen uygulanır. Kalıcı olması için "Kalıcı kaydet"e basın (yoksa reset'te kaybolur).</div>
  </div>
</section>

<section class="page" id="p-logs"><div class="grid">
  <div class="card">
    <h2>WiFi Ayarları</h2>
    <table><tr><td>Ağ adı (SSID)</td><td id="iSsid">-</td></tr><tr><td>Kanal</td><td id="iCh">-</td></tr><tr><td>IP Adresi</td><td id="iIp">-</td></tr></table>
    <div class="row"><select id="chSel"></select><button id="chApply">Uygula</button></div>
    <div class="hint" id="chRes"></div>
  </div>
  <div class="card">
    <h2>Ağ Durumu</h2>
    <table>
      <tr><td>Sinyal (RSSI)</td><td id="nRssi">-</td></tr><tr><td>Ping</td><td id="nPing">-</td></tr>
      <tr><td>WebSocket</td><td id="nWs">Bağlı değil</td></tr><tr><td>Gecikme aşımı</td><td id="nDm">-</td></tr>
      <tr><td>Kol verisi yaşı</td><td id="nJa">-</td></tr><tr><td>Bağlı cihaz</td><td id="nCl">-</td></tr>
      <tr><td>Son kopma</td><td id="nLd">-</td></tr>
    </table>
  </div>
  <div class="card">
    <h2>ESP32 Sistemi</h2>
    <table>
      <tr><td>Çip</td><td id="sChip">-</td></tr><tr><td>İşlemci</td><td id="sCpu">-</td></tr><tr><td>SDK</td><td id="sSdk">-</td></tr>
      <tr><td>Çalışma süresi</td><td id="sUp">-</td></tr><tr><td>Bellek (Boş / Toplam)</td><td id="sHeap">-</td></tr>
      <tr><td>PSRAM</td><td id="sPs">-</td></tr><tr><td>Flash (Kod / Toplam)</td><td id="sFl">-</td></tr>
      <tr><td>Boş kod alanı</td><td id="sFree">-</td></tr>
    </table>
  </div>
</div></section>
</main>

<div class="ovl" id="ovl"><h1 id="oTitle">BAĞLANTI YOK</h1><p id="oMsg">Yeniden bağlanılıyor...</p><button id="reboot" style="display:none">Robotu Yeniden Başlat</button></div>

<script>
const $=id=>document.getElementById(id);
const ST=[['Bekleme','Komut bekleniyor'],['Hazır','Sistemler hazır'],['Otonom',''],['Sürücü Kontrolü','Sürücüler kontrolde'],['Durduruldu','Motorlar güvende']];
const BN=['A','B','X','Y','LB','RB','LT','RT','Back','Start','L3','R3'];
const REBOOTING='Yeniden başlatılıyor...';
let ws=null,wsOk=false,lastRx=0,busy=false,S=null,telSeq=0,autoScroll=true,ping=null,padIdx=-1,padKeys='',everPad=false;

// Sekmeler
function showPage(p){document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('on',t.dataset.p===p));
 document.querySelectorAll('.page').forEach(s=>s.classList.toggle('on',s.id==='p-'+p));if(p==='logs')loadInfo()}
document.querySelectorAll('.tab').forEach(t=>t.onclick=()=>showPage(t.dataset.p));
document.querySelectorAll('[data-go]').forEach(b=>b.onclick=()=>showPage(b.dataset.go));

// WebSocket
function connect(){
 try{ws=new WebSocket(`ws://${location.host}/ws`)}catch(e){setTimeout(connect,1000);return}
 ws.onopen=()=>{wsOk=true};
 ws.onmessage=e=>{let d;try{d=JSON.parse(e.data)}catch(x){return}lastRx=Date.now();
  if(d.busy){busy=true;return}busy=false;
  if(d.t){const p=performance.now()-d.t;ping=ping==null?p:ping*.7+p*.3}
  if(d.tel!==undefined){telSeq=d.ts;setTel(d.tel)}
  S=d;render()};
 ws.onclose=()=>{wsOk=false;setTimeout(connect,1000)};
 ws.onerror=()=>{try{ws.close()}catch(e){}};
}
connect();
const now=()=>performance.now().toFixed(1);
function cmd(n,a=0,b=0){if(ws&&ws.readyState===1)ws.send(`C,${now()},${telSeq},${n},${a},${b}`)}

// Kol
function pads(){return navigator.getGamepads?[...navigator.getGamepads()].filter(Boolean):[]}
function curPad(){const ps=pads();if(!ps.length)return null;let g=ps.find(p=>p.index===padIdx);if(!g){g=ps[0];padIdx=g.index}return g}
function refreshPadList(){const ps=pads();const k=ps.map(p=>p.index+p.id).join('|');if(k===padKeys)return;padKeys=k;
 const sel=$('padSel');sel.innerHTML=ps.length?ps.map(p=>`<option value="${p.index}">${esc(p.id)} (no ${p.index})</option>`).join(''):'<option value="-1">Kol yok</option>';
 if(ps.length)sel.value=curPad().index}
$('padSel').onchange=e=>padIdx=+e.target.value;
addEventListener('gamepadconnected',()=>{everPad=true;refreshPadList()});
addEventListener('gamepaddisconnected',refreshPadList);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

setInterval(()=>{
 if(!ws||ws.readyState!==1||ws.bufferedAmount>512)return;
 const g=curPad();let ax=[0,0,0,0],m=0,lt=0,rt=0,c=0;
 if(g){c=1;everPad=true;for(let i=0;i<4;i++)ax[i]=+(g.axes[i]||0).toFixed(3);
  g.buttons.forEach((b,i)=>{if(b.pressed&&i<32)m|=(1<<i)});m=m>>>0;
  lt=+((g.buttons[6]||{}).value||0).toFixed(3);rt=+((g.buttons[7]||{}).value||0).toFixed(3)}
 ws.send(`J,${now()},${telSeq},${ax.join(',')},${m},${lt},${rt},${c}`);
},50);

// Ekran
function setTel(t){const p=$('tel');p.textContent=t;if(autoScroll)p.scrollTop=p.scrollHeight}
$('telClear').onclick=()=>$('tel').textContent='';
$('telAuto').onclick=()=>{autoScroll=!autoScroll;$('telAuto').textContent='Otomatik kaydır: '+(autoScroll?'AÇIK':'KAPALI')};

$('mainBtn').onclick=()=>{if(!S)return;const st=S.st;
 if(st===0||st===4)cmd('init');else if(st===1)cmd('start',Math.max(1,+$('dur').value||30),$('autoSw').checked?1:0);else cmd('stop')};
$('estop').onclick=()=>{cmd('estop');cmd('estop')};
$('autoSw').onchange=e=>cmd('auto',e.target.checked?1:0);
$('reboot').onclick=()=>{cmd('reboot');$('oMsg').textContent=REBOOTING};

function render(){const d=S;
 const st=d.st;$('stName').textContent=d.es?'ACİL DURDURULDU':ST[st][0];
 $('stDesc').textContent=d.es?'Yeniden başlatma gerekli':(st===2?`Otonom çalışıyor - ${d.ar.toFixed(1)} sn`:ST[st][1]+(d.lk&&st!==1?' (kilitli)':''));
 const mb=$('mainBtn');const k=(st===0||st===4)?0:st===1?1:2;mb.textContent=['Hazırla','Başlat','Durdur'][k];
 mb.className=k===1?'start':k===2?'stop':'';mb.disabled=!!d.es;
 const sw=$('autoSw');sw.disabled=st===3;if(document.activeElement!==sw)sw.checked=!!d.ae;
 $('cd').classList.toggle('on',st===2);
 if(st===2){$('cdTxt').textContent=d.ar.toFixed(1)+' sn';$('cdBar').style.width=Math.max(0,Math.min(100,d.ar/Math.max(1,d.ad)*100))+'%'}
 $('mem').textContent=`${Math.round(d.heap/1024)} / ${Math.round(d.heapT/1024)} KB`;
 const bv=d.bv,bOn=bv!=null&&bv>=0,bOk=bOn&&bv>3;
 $('batt').style.display=bOn?'':'none';
 if(bOn)battUI(d,bv,bOk);
 const r=[['X',d.x.toFixed(1)+' cm'],['Y',d.y.toFixed(1)+' cm'],['Açı',d.h.toFixed(1)+'°'],['BNO055',d.bno?'Tamam':'Yok'],
  ['Akü',bOn?(bOk?bv.toFixed(2)+' V':'Bağlı değil'):'Kapalı'],['Saha-yönlü sürüş',d.fo?'AÇIK':'KAPALI'],['Güvenlik kilidi',d.lk?'KİLİTLİ':'Açık'],
  ['Şişe sensörü',d.sw===undefined||d.sw<0?'-':(d.sw?'ŞİŞE VAR':'Boş')]].concat(['FL','FR','RL','RR'].map((n,i)=>[n,`${d.d?d.d[i].toFixed(1)+' cm · ':''}${Math.round(d.rpm[i])} rpm · %${Math.round(d.p[i])}`]));
 $('robotTbl').innerHTML=r.map(x=>`<tr><td>${x[0]}</td><td>${x[1]}</td></tr>`).join('');
 $('nRssi').textContent=d.rssi?d.rssi+' dBm':'-';$('nDm').textContent=d.dm?'EVET':'Yok';
 $('nJa').textContent=d.ja+' ms';$('nCl').textContent=d.cl;$('nLd').textContent=d.ld<0?'-':'neden '+d.ld;
}

// Akü paneli
const BV0=10.5,BV1=12.8,bHist=[];let bMinV=null,bMaxV=null,bLastPush=0;
const bFrac=v=>Math.max(0,Math.min(1,(v-BV0)/(BV1-BV0)));
function battUI(d,bv,ok){
 const st=!ok?['','BAĞLI DEĞİL']:bv<d.bl?[' bad','KRİTİK']:bv<d.bw?[' warn','ZAYIF']:[' ok','İYİ'];
 $('batt').className='batt'+st[0];$('battSt').textContent=st[1];
 $('battV').textContent=ok?bv.toFixed(2)+' V':'--.- V';
 const f=ok?bFrac(bv):0;$('battPct').textContent=ok?`Akü · %${Math.round(f*100)} dolu`:'Akü · voltaj bölücüyü kontrol edin';
 $('battFill').style.width=f*52+'px';$('battMk').style.left=f*100+'%';
 const pl=bFrac(d.bl)*100,pw=bFrac(d.bw)*100;
 $('bsL').textContent=d.bl.toFixed(1);$('bsL').style.left=pl+'%';$('bsW').textContent=d.bw.toFixed(1);$('bsW').style.left=pw+'%';
 $('batt').querySelector('.bBar').style.background=`linear-gradient(90deg,var(--bad) 0 ${pl}%,var(--warn) ${pl}% ${pw}%,var(--ok) ${pw}%)`;
 const load=d.p?d.p.reduce((a,b)=>a+Math.abs(b),0)/4:0;$('bLoad').textContent='%'+Math.round(load);
 if(!ok)return;
 bMinV=bMinV==null?bv:Math.min(bMinV,bv);bMaxV=bMaxV==null?bv:Math.max(bMaxV,bv);
 $('bMin').textContent=bMinV.toFixed(2);$('bMax').textContent=bMaxV.toFixed(2);
 const t=Date.now();if(t-bLastPush>=1000){bLastPush=t;bHist.push(bv);if(bHist.length>180)bHist.shift();battChart(d)}
 if(bHist.length>=10){const n=Math.min(60,bHist.length-1),r=(bv-bHist[bHist.length-1-n])/n*60;
  $('bRate').textContent=(r>=0?'+':'')+r.toFixed(2)}
}
function battChart(d){const c=$('battCv'),w=c.clientWidth,h=c.clientHeight;if(!w)return;
 const k=devicePixelRatio||1;c.width=w*k;c.height=h*k;const g=c.getContext('2d');g.scale(k,k);
 const col=getComputedStyle($('batt')).getPropertyValue('--c').trim()||'#22c55e';
 const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim();
 let lo=Math.min(d.bl-0.3,...bHist),hi=Math.max(BV1,...bHist);lo=Math.floor(lo*2)/2;hi=Math.ceil(hi*2)/2;
 const y=v=>h-4-(v-lo)/(hi-lo)*(h-8),N=180,x=i=>(i+N-bHist.length)/(N-1)*w;
 g.setLineDash([3,4]);g.lineWidth=1;
 [[d.bw,css('--warn')],[d.bl,css('--bad')]].forEach(([v,cl])=>{g.strokeStyle=cl;g.globalAlpha=.5;g.beginPath();g.moveTo(0,y(v));g.lineTo(w,y(v));g.stroke()});
 g.setLineDash([]);g.globalAlpha=1;g.fillStyle=css('--dim');g.font='10px ui-monospace,Consolas,monospace';
 g.fillText(hi.toFixed(1)+' V',4,11);g.fillText(lo.toFixed(1)+' V',4,h-5);g.fillText('son 3 dk',w-50,11);
 if(bHist.length<2)return;
 g.beginPath();bHist.forEach((v,i)=>i?g.lineTo(x(i),y(v)):g.moveTo(x(i),y(v)));
 g.strokeStyle=col;g.lineWidth=2;g.lineJoin='round';g.stroke();
 g.lineTo(x(bHist.length-1),h);g.lineTo(x(0),h);g.closePath();g.globalAlpha=.15;g.fillStyle=col;g.fill();g.globalAlpha=1;
 const lv=bHist[bHist.length-1];g.beginPath();g.arc(x(bHist.length-1)-3,y(lv),3,0,7);g.fillStyle=col;g.fill()}

function bars(){let n=0;
 if(wsOk&&S&&Date.now()-lastRx<2000){const r=S.rssi||-50;n=r>-60?4:r>-70?3:r>-80?2:1;if(ping>150)n=Math.min(n,2);if(ping>400)n=1}
 [...$('bars').children].forEach((b,i)=>b.style.background=i<n?(n>=3?'var(--ok)':n===2?'var(--warn)':'var(--bad)'):'');
 $('dot').style.background=n>=3?'var(--ok)':n===2?'var(--warn)':'var(--bad)';
 $('lat').textContent=ping==null||!wsOk?'-- ms':Math.round(ping)+' ms';
 $('nPing').textContent=ping==null||!wsOk?'-':Math.round(ping)+' ms';$('nWs').textContent=wsOk?'Bağlı':'Bağlı değil'}

function overlay(){const o=$('ovl');let show=false,t='',m='',es=false;
 if(S&&S.es&&Date.now()-lastRx<6000){show=true;es=true;t='ACİL DURDURULDU';m='Robot yeniden başlatılana kadar kilitli.'}
 else if(busy&&Date.now()-lastRx<6000){show=true;t='MEŞGUL';m='Başka bir cihaz zaten bağlı.'}
 else if(Date.now()-lastRx>6000){show=true;t='BAĞLANTI YOK';m='Yeniden bağlanılıyor...'}
 o.classList.toggle('on',show);o.classList.toggle('es',es);$('oTitle').textContent=t;
 if($('oMsg').textContent!==REBOOTING||!es)$('oMsg').textContent=m;$('reboot').style.display=es?'':'none'}

function joyUI(){refreshPadList();const g=curPad();
 const x=g?(g.axes[0]||0):0,y=g?(g.axes[1]||0):0,pos=`${50+x*42}%`,posy=`${50+y*42}%`;
 for(const id of ['dStick','jStick']){$(id).style.left=pos;$(id).style.top=posy}
 for(const id of ['dJoy','jJoy']){$(id).textContent=g?'Bağlı':'Bağlı Değil';$(id).className='pill'+(g?' ok':'')}
 $('dName').textContent=g?g.id:'Kol yok';$('jHint').style.display=(g||everPad)?'none':'';
 if(!$('p-joy').classList.contains('on'))return;
 if(!g){$('padInfo').textContent='-';$('axList').innerHTML='';$('btList').innerHTML='';$('jBtns').innerHTML='';return}
 $('padInfo').textContent=`${g.id}  ·  Eksen:${g.axes.length} Tuş:${g.buttons.length}`;
 $('axList').innerHTML=g.axes.map((a,i)=>`Eksen ${i}: ${a.toFixed(2)}`).join('<br>');
 $('btList').innerHTML=g.buttons.map((b,i)=>`${i} ${b.pressed?'●':'○'}${b.value&&b.value<1?' '+b.value.toFixed(2):''}`).join('<br>');
 $('jBtns').innerHTML=g.buttons.map((b,i)=>`<span class="${b.pressed?'on':''}">${BN[i]||'P'+(i+1)}</span>`).join('')}

setInterval(()=>{bars();overlay();joyUI()},100);

// Sistem bilgisi
const chSel=$('chSel');chSel.innerHTML=[...Array(13)].map((_,i)=>`<option value="${i+1}">Kanal ${i+1}</option>`).join('')+'<option value="0">Varsayılan (açılışta)</option>';
$('chApply').onclick=()=>{cmd('chan',+chSel.value);$('chRes').textContent='Uygulanıyor... (robotun WiFi\'ı yeniden başlar, gerekirse tekrar bağlanın)';setTimeout(loadInfo,2500)};
function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return`${h}sa ${m}dk ${s%60}sn`}
const CHS={macro:'koddan',nvs:'kayıtlı'};
async function loadInfo(){try{const r=await fetch('/info',{cache:'no-store'});const i=await r.json();
 $('iSsid').textContent=i.ssid;$('iCh').textContent=`${i.ch} (${CHS[i.chs]||i.chs})`;$('iIp').textContent=i.ip;if(i.chr)$('chRes').textContent=i.chr;
 if(document.activeElement!==chSel)chSel.value=i.ch;
 $('sChip').textContent=i.chip;$('sCpu').textContent=i.cpu+' MHz';$('sSdk').textContent=i.sdk;$('sUp').textContent=fmtUp(i.up);
 $('sHeap').textContent=`${Math.round(i.heap/1024)} / ${Math.round(i.heapT/1024)} KB`;$('sPs').textContent=i.psram?Math.round(i.psram/1024)+' KB':'Yok';
 $('sFl').textContent=`${Math.round(i.sketch/1024)} / ${Math.round(i.flash/1024)} KB`;$('sFree').textContent=Math.round(i.free/1024)+' KB'}catch(e){}}
setInterval(()=>{if($('p-logs').classList.contains('on'))loadInfo()},2000);

// PID ayarları
// Açıklama balonları: ayarın üstüne gelince (telefonda kutuya dokununca) çıkar
const TIP_KP=u=>"Hata başına verilen güç ("+u+"). Artırırsan robot hedefe/çizgiye daha sert ve hızlı döner; çok artırırsan hedefin iki yanına gidip gelir (salınım). Azaltırsan yumuşak ama gevşek olur, hedefe yetişemeyebilir.";
const TIP_KI="Küçük kalıcı hatayı yavaş yavaş kapatır (robot hedefin birkaç cm önünde duruyorsa işe yarar). Artırırsan hata daha çabuk kapanır ama çok artırırsan hedefi aşar ve sallanır. 0 = kapalı. Önce Ki ve Kd 0 iken Kp'yi ayarla.";
const TIP_KD="Hata hızla değişirken frenleme yapar. Artırırsan robot hedefe yaklaşırken daha yumuşak yavaşlar ve hedefi daha az geçer; çok artırırsan titrer. 0 = kapalı.";
const PIDG=[
 ["Otonom: konum (cm hata -> % güç)","Adım adım otonomda (Forward, GoTo gibi adımlar) robotun hedefe kalan mesafeye göre ne kadar güç vereceğini belirler; hedefe yaklaştıkça yavaşlatır.",
  [["dKp","Kp",TIP_KP("% güç / cm")],["dKi","Ki",TIP_KI],["dKd","Kd",TIP_KD]]],
 ["Otonom: açı (derece hata -> % güç)","Adım adım otonomda dönüşleri ve açıyı tutmayı belirler: robotun kalan açıya göre ne kadar dönme gücü vereceği.",
  [["tKp","Kp",TIP_KP("% güç / derece")],["tKi","Ki",TIP_KI],["tKd","Kd",TIP_KD]]],
 ["Yol takibi: konum sapması","Yol Planlayıcı rotalarında robot çizilen yoldan saptığında onu yola geri çeken düzeltmeyi belirler.",
  [["pKp","Kp",TIP_KP("% güç / cm sapma")],["pKi","Ki",TIP_KI],["pKd","Kd",TIP_KD]]],
 ["Yol takibi: açı","Yol Planlayıcı rotalarında robotun bakış açısını planlanan açıda tutar.",
  [["qKp","Kp",TIP_KP("% güç / derece")],["qKi","Ki",TIP_KI],["qKd","Kd",TIP_KD]]],
 ["Motor hız PID (enkoder rpm)","Her tekerin gerçek hızını (enkoder rpm) hedef hıza eşitler. Pil boşalsa da robot aynı hızda gider ve tekerler arasındaki farklar azalır. Önce robot takozdayken dene.",
  [["vOn","Açık (1) / Kapalı (0)","1 = her tekerin hızı enkoderden okunup hedefe eşitlenir. 0 = güç doğrudan motora gider (eski, güvenli davranış). Açmadan önce robotu takoza koy; ayar yanlışsa tekerler sallanabilir."],
   ["vKp","Kp (% / rpm)","rpm hatası başına eklenen güç. Artırırsan tekerler hedef hıza daha çabuk ulaşır; çok artırırsan teker sallanır ve ses çıkarır. Azaltırsan tepki yavaşlar."],
   ["vKi","Ki","Hız farkını yavaş yavaş kapatır; pil boşalınca da aynı hızda gitmeni sağlar. Çok artırırsan hız ileri geri dalgalanır. 0 olursa pil düştükçe robot yavaşlar."],
   ["vKd","Kd","Hız ani değişirken sertliği azaltır. Genelde 0 bırak; artırırsan teker titreyebilir."],
   ["vFF","İleri besleme","İstenen gücün ne kadarı doğrudan motora verilir (1 = hepsi); PID sadece kalan farkı düzeltir. Azaltırsan PID daha çok iş yapar ve tepki yavaşlar. Normalde 1 bırak."],
   ["vMax","%100 = rpm","Motorun %100 güçte gerçek rpm değeri. Hedef güç bu değerle rpm'e çevrilir. Yanlışsa teker hedeflenenden hızlı ya da yavaş döner: tam güçte Panel'deki rpm'i okuyup buraya yaz."]]]];
$('pidGrid').innerHTML=PIDG.map(g=>`<div class="card"><h2 data-tip="${esc(g[1])}">${g[0]}</h2>${g[2].map(p=>`<div class="row" data-tip="${esc(p[2])}"><label for="pid_${p[0]}">${p[1]}</label><input type="number" step="any" id="pid_${p[0]}"></div>`).join('')}</div>`).join('');
const pidMsg=t=>$('pidMsg').textContent=t;
async function pidLoad(){try{const j=await (await fetch('/pid',{cache:'no-store'})).json();
 for(const k in j){const el=$('pid_'+k);if(el&&document.activeElement!==el)el.value=j[k]}pidMsg('Robottan okundu')}catch(e){pidMsg('Okunamadı: '+e.message)}}
document.querySelectorAll('#pidGrid input').forEach(el=>el.addEventListener('change',async()=>{const k=el.id.slice(4),v=parseFloat(el.value);if(!isFinite(v))return;
 try{const r=await fetch(`/pidset?n=${k}&v=${v}`,{cache:'no-store'});const j=await r.json();if(j.err)pidMsg('Hata: '+j.err);else{el.value=j.v;pidMsg(k+' = '+j.v+' (uygulandı, kaydedilmedi)')}}catch(e){pidMsg('Gönderilemedi')}}));
$('pidSave').onclick=async()=>{try{await fetch('/pidsave',{cache:'no-store'});pidMsg('Kalıcı olarak kaydedildi')}catch(e){pidMsg('Kaydedilemedi')}};
$('pidDef').onclick=async()=>{if(!confirm('Tüm PID değerleri varsayılana dönsün mü?'))return;try{await fetch('/pidreset',{cache:'no-store'});pidLoad()}catch(e){pidMsg('Başarısız')}};
$('pidReload').onclick=pidLoad;
document.querySelector('.tab[data-p="pid"]').addEventListener('click',pidLoad);
</script><script src="/planner.js"></script></body></html>)rawliteral";
