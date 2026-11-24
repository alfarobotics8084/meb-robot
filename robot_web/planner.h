// Yol Planlayıcı (Sürücü İstasyonu sekmesi). robot_web.ino bunu /planner.js adresinde sunar.
// Saha çizimi aşağıdaki field() fonksiyonundadır (6x6 karo, karo ölçüsü ayarlanabilir).
#pragma once

const char PLANNER_JS[] = R"PPJS((()=>{
'use strict';
const $=id=>document.getElementById(id);
const root=$('ppRoot');if(!root)return;
document.head.insertAdjacentHTML('beforeend',`<style>
.pp{display:grid;grid-template-columns:minmax(0,1fr) 340px;gap:16px;align-items:start}
.ppc canvas{display:block;width:100%;aspect-ratio:1/1;border-radius:8px;touch-action:none;cursor:crosshair;background:#1b1f24}
.ppbar{display:flex;align-items:center;gap:10px;margin-top:10px}.ppbar input{flex:1;min-width:0}
.ppside{display:flex;flex-direction:column;gap:16px}
.kv{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-top:8px}.kv>label:first-child{color:var(--dim)}
.kv input[type=number],.kv input[type=text],.kv select{width:140px}
.kv input[type=text]{background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:7px 10px;font:inherit}
.ppbtns{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}.ppbtns button{padding:6px 10px;font-size:13px}
button.pri{background:var(--acc);border-color:var(--acc);color:#fff}
.ppinfo{margin-top:10px}#ppMsg{min-height:18px}#ppMsg.err{color:var(--bad)}.ppbad{color:var(--bad)}
@media(max-width:900px){.pp{grid-template-columns:1fr}}
</style>`);
const kv=(l,id,t,a)=>`<div class="kv"><label for="${id}">${l}</label><input id="${id}" type="${t||'number'}" ${a||''}></div>`;
const sw=(l,id,c)=>`<div class="kv"><label for="${id}">${l}</label><label class="sw"><input id="${id}" type="checkbox" ${c?'checked':''}><span></span></label></div>`;
root.innerHTML=`<div class="pp">
<div class="card ppc"><canvas id="ppCv"></canvas>
<div class="ppbar"><button id="ppPlay">&#9654; Önizle</button><input id="ppT" type="range" min="0" max="1000" value="0"><span id="ppTT">0.0 sn</span></div>
<div class="hint">Boş alana tıkla: nokta ekle &middot; Yolun üstüne Shift+tıkla: araya nokta ekle &middot; Noktaları ve &#9675; tutamaçları sürükleyerek eğriyi bük &middot; Sarı topu sürükleyerek robotu çevir &middot; Sağ tık / Del: noktayı sil &middot; Sürüklerken Shift: hizalamadan<br>(0,0) = sol alt köşe &middot; X sağa, Y yukarı &middot; 0&deg; = sağa bakış, açılar saat yönünün tersine artar &middot; turuncu = gerçek robot (Hazırla'dan sonra)</div></div>
<div class="ppside">
<div class="card"><h2>Yol</h2>
<div class="kv"><label for="ppPre">Hazır rota</label><select id="ppPre"></select></div>
<div class="ppbtns"><button id="ppPreOpen">Hazır rotayı aç</button></div>
<div class="kv"><label for="ppSlot">Slot</label><select id="ppSlot"></select></div>
${kv('İsim','ppName','text','maxlength="32"')}
<div class="ppbtns"><button id="ppLoad">Yükle</button><button id="ppSave" class="pri">Robota gönder</button><button id="ppUse">Otonomda kullan</button></div>
<div class="ppbtns"><button id="ppNew">Yeni</button><button id="ppMirY">Ayna &#8597;</button><button id="ppMirX">Ayna &#8596;</button><button id="ppDel">Slotu sil</button></div>
<div class="ppbtns"><button id="ppExp">Dosyaya kaydet</button><button id="ppImp">Dosyadan aç</button><button id="ppTrail">İzi temizle</button><input id="ppFile" type="file" accept=".json,application/json" hidden></div>
<table class="ppinfo"><tr><td>Uzunluk</td><td id="ppLen">-</td></tr><tr><td>Tahmini süre</td><td id="ppTime">-</td></tr><tr><td>Robot noktası</td><td id="ppPts">-</td></tr><tr><td>Otonomda çalışan</td><td id="ppAct">-</td></tr></table>
<div class="hint" id="ppMsg"></div></div>
<div class="card"><h2>Nokta <span id="ppIdx"></span></h2>
${kv('X (cm)','wX','number','step="1"')}${kv('Y (cm)','wY','number','step="1"')}${kv('Açı (&deg;)','wH','number','step="5"')}
${kv('Hız sınırı (cm/s, 0 = en yüksek)','wS','number','min="0" step="5"')}
${sw('Burada dur','wSt')}${sw('Şişe bekle (sensör)','wC')}${kv('Bekle / en fazla (ms)','wW','number','min="0" step="100"')}${kv('Olay no (0 = yok)','wE','number','min="0" max="255" step="1"')}
<div class="ppbtns"><button id="wPrev">&#9664; Önceki</button><button id="wNext">Sonraki &#9654;</button><button id="wDel">Noktayı sil</button></div></div>
<div class="card"><h2>Robot ve Sınırlar</h2>
${kv('Karo ölçüsü (cm)','cTile','number','min="20" step="1"')}${kv('Robot genişliği (cm)','cRw','number','min="5"')}${kv('Robot uzunluğu (cm)','cRl','number','min="5"')}
${kv('En yüksek hız (cm/s)','cMaxV','number','min="5" step="5"')}${kv('İvme (cm/s&sup2;)','cAcc','number','min="10" step="10"')}
${kv('En yüksek dönüş (&deg;/s)','cTurn','number','min="10" step="10"')}${kv('Viraj ivmesi (cm/s&sup2;)','cAlat','number','min="10" step="10"')}
${kv('%100 güçte hız (cm/s)','cV100','number','min="10" step="5"')}
${sw('Hizala (5 cm / 15&deg;)','cSnap',1)}${sw('Gerçek robotu göster','cLive',1)}
<div class="hint">%100 güçte hız: robotu tam güçte sürüp saniyede kaç cm gittiğini ölçün. Robot, yoldaki hızı bu değere bölerek ne kadar güç vereceğini bulur.</div></div>
</div></div>`;

// ---------- durum ----------
const R=Math.PI/180,MAXPTS=400,DS=5;
const DEF={v:1,name:'Yol',tile:61,rw:46,rl:46,maxV:80,acc:120,turn:180,alat:200,v100:300,wp:[]};   // v100: 740 rpm motor, 9.6 cm teker
let P=null,slot=1,sel=0,smp=[],tot=0,plen=0,drag=null,playing=false,playT=0,lastF=0,trail=[],lastSt=-1,slots=[],act=-1,saveTm=0;
const wrap=a=>{a=((a+180)%360+360)%360;return a-180};
const cl=(v,a,b)=>Math.max(a,Math.min(b,v));
const escH=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const F=()=>6*P.tile,MG=()=>P.tile;
const mkW=(x,y,h,d,l)=>({x,y,h,d,li:l,lo:l,st:false,w:0,e:0,s:0,c:0});   // c: şişe sensörünü bekle
const robotS=()=>typeof S!=='undefined'?S:null;
function fresh(){P=JSON.parse(JSON.stringify(DEF));const T=P.tile;P.wp=[mkW(1.5*T,0.5*T,90,90,40),mkW(1.5*T,2.5*T,90,90,40)];sel=0;playT=0}
function fix(o){const p=Object.assign(JSON.parse(JSON.stringify(DEF)),o||{});
 p.wp=(Array.isArray(p.wp)?p.wp:[]).filter(w=>w&&isFinite(w.x)&&isFinite(w.y)).map(w=>Object.assign(mkW(0,0,0,0,30),w));
 if(!p.wp.length)p.wp=[mkW(90,30,90,90,40)];return p}
function draft(){clearTimeout(saveTm);saveTm=setTimeout(()=>{try{localStorage.setItem('pp_draft',JSON.stringify({P,slot}))}catch(e){}},300)}
try{const d=JSON.parse(localStorage.getItem('pp_draft')||'null');if(d&&d.P){P=fix(d.P);slot=d.slot||1;if(P.v100===140)P.v100=300}}catch(e){}
if(!P)fresh();
function msg(t,err){const m=$('ppMsg');m.textContent=t;m.className='hint'+(err?' err':'')}

// ---------- geometri ----------
const U=d=>[Math.cos(d*R),Math.sin(d*R)];
function ctrl(i){const a=P.wp[i],b=P.wp[i+1],ua=U(a.d),ub=U(b.d);
 return[[a.x,a.y],[a.x+ua[0]*a.lo,a.y+ua[1]*a.lo],[b.x-ub[0]*b.li,b.y-ub[1]*b.li],[b.x,b.y]]}
function bez(c,t){const m=1-t,a=m*m*m,b=3*m*m*t,d=3*m*t*t,e=t*t*t;
 return[a*c[0][0]+b*c[1][0]+d*c[2][0]+e*c[3][0],a*c[0][1]+b*c[1][1]+d*c[2][1]+e*c[3][1]]}
function curv(a,b,c){const abx=b.x-a.x,aby=b.y-a.y,acx=c.x-a.x,acy=c.y-a.y;
 const den=Math.hypot(abx,aby)*Math.hypot(c.x-b.x,c.y-b.y)*Math.hypot(acx,acy);return den>1e-6?2*Math.abs(abx*acy-aby*acx)/den:0}

// Yolu 5 cm aralıklı noktalara böl, hız profili ve zamanı hesapla
function build(){
 const W=P.wp,out=[];
 for(let i=0;i<W.length-1;i++){
  const c=ctrl(i),N=120,pts=[bez(c,0)],L=[0];
  for(let k=1;k<=N;k++){const p=bez(c,k/N);L.push(L[k-1]+Math.hypot(p[0]-pts[k-1][0],p[1]-pts[k-1][1]));pts.push(p)}
  const len=L[N],n=Math.max(1,Math.round(len/DS)),dh=wrap(W[i+1].h-W[i].h);
  let k=0;
  for(let j=0;j<n;j++){const s=j*len/n;while(k<N-1&&L[k+1]<s)k++;
   const f=L[k+1]>L[k]?(s-L[k])/(L[k+1]-L[k]):0,fr=len>0?s/len:0;
   out.push({x:pts[k][0]+(pts[k+1][0]-pts[k][0])*f,y:pts[k][1]+(pts[k+1][1]-pts[k][1])*f,h:wrap(W[i].h+dh*fr),cap:W[i].s>0?W[i].s:P.maxV,wi:j?-1:i,seg:i})}
 }
 const l=W[W.length-1];out.push({x:l.x,y:l.y,h:l.h,cap:P.maxV,wi:W.length-1,seg:Math.max(0,W.length-2)});
 const n=out.length;
 for(let i=0;i<n;i++){const p=out[i],w=p.wi>=0?W[p.wi]:null;
  p.ds=i?Math.hypot(p.x-out[i-1].x,p.y-out[i-1].y):0;
  p.stop=i===0||i===n-1||!!(w&&w.st);
  let lim=Math.min(p.cap,P.maxV);
  if(i>0&&i<n-1){const k=curv(out[i-1],p,out[i+1]);if(k>1e-4)lim=Math.min(lim,Math.sqrt(P.alat/k))}
  if(i>0){const dh=Math.abs(wrap(p.h-out[i-1].h));if(dh>.01&&p.ds>.01)lim=Math.min(lim,P.turn*p.ds/dh)}
  p.lim=p.stop?0:lim}
 out[0].v=0;
 for(let i=1;i<n;i++)out[i].v=Math.min(out[i].lim,Math.sqrt(out[i-1].v**2+2*P.acc*out[i].ds));
 for(let i=n-2;i>=0;i--)out[i].v=Math.min(out[i].v,Math.sqrt(out[i+1].v**2+2*P.acc*out[i+1].ds));
 let t=0,len=0;
 for(let i=0;i<n;i++){const p=out[i];
  if(i){const a=out[i-1],vs=a.v+p.v;let dt=p.ds>.01?(vs>1?2*p.ds/vs:2*Math.sqrt(p.ds/P.acc)):0;
   if(p.ds<.5)dt+=Math.abs(wrap(p.h-a.h))/P.turn;t+=dt;len+=p.ds}
  p.t0=t;
  if(p.stop&&i>0)t+=.3;   // yerleşme
  if(p.wi>=0&&W[p.wi].st&&i<n-1)t+=(W[p.wi].w||0)/1000;
  p.t1=t}
 smp=out;tot=t;plen=len}

// Robotun dosyası: noktalar robotun başlangıcına göre (X ileri, Y sol)
// Sütunlar: x mm, y mm, açı*10, güç %*10, bekleme ms, olay, zaman ms (son duruştan kalkıştan beri)
function exportTxt(){
 const W0=P.wp[0],c=Math.cos(W0.h*R),s=Math.sin(W0.h*R),last=P.wp.length-1;
 const lines=[`H,${smp.length},${String(P.name||'Yol').replace(/[\r\n]/g,' ').slice(0,32)}`];
 let dep=0;
 smp.forEach((p,i)=>{const dx=p.x-W0.x,dy=p.y-W0.y,w=p.wi>=0?P.wp[p.wi]:null;
  const wait=w&&w.st&&p.wi<last?Math.max(1,Math.round(w.w||0)):0;
  const tr=i?Math.max(0,Math.round((p.t0-dep)*1000)):0;
  if(p.stop)dep=p.t1;
  lines.push([Math.round((dx*c+dy*s)*10),Math.round((-dx*s+dy*c)*10),Math.round(wrap(p.h-W0.h)*10),
   Math.round(cl(p.v/P.v100*100,0,100)*10),wait,w?cl(w.e|0,0,255):0,tr,wait&&w.c?1:0].join(','))});
 lines.push('J'+JSON.stringify(P));
 return lines.join('\n')+'\n'}
function toField(x,y,h){const w=P.wp[0],c=Math.cos(w.h*R),s=Math.sin(w.h*R);return{x:w.x+x*c-y*s,y:w.y+x*s+y*c,h:wrap(w.h+h)}}

// ---------- çizim ----------
const cv=$('ppCv'),g=cv.getContext('2d');let dpr=1,sc=1;
function tp(x,y){const m=MG();return[(x+m)*sc,(F()+m-y)*sc]}
function fp(px,py){const m=MG();return[px/sc-m,F()+m-py/sc]}
function size(){dpr=window.devicePixelRatio||1;const W=Math.round(Math.max(200,cv.clientWidth)*dpr);if(cv.width!==W){cv.width=W;cv.height=W}sc=cv.width/(F()+2*MG())}

// Saha: karo birimiyle, sol üst köşe (0,0), aşağı +  (mavi üstte, kırmızı altta)
function field(){
 const T=P.tile,k=sc*T,o=MG()*sc,px=1/k;
 g.setTransform(1,0,0,1,0,0);g.fillStyle='#1b1f24';g.fillRect(0,0,cv.width,cv.height);
 g.setTransform(k,0,0,k,o,o);
 const BL='#1f5fae',RD='#c3272e',BLD='#123d73',RDD='#7d1519';
 const rr=(x,y,w,h,f,s)=>{if(f){g.fillStyle=f;g.fillRect(x,y,w,h)}if(s){g.strokeStyle=s;g.lineWidth=2*px;g.strokeRect(x,y,w,h)}};
 const dots=(x,y,w,h,nx,ny,c,r)=>{g.fillStyle=c;for(let i=0;i<nx;i++)for(let j=0;j<ny;j++){g.beginPath();g.arc(x+(i+.5)*w/nx,y+(j+.5)*h/ny,r,0,7);g.fill()}};
 const ln=(c,a)=>{g.strokeStyle=c;g.lineWidth=3*px;g.beginPath();for(const s of a){g.moveTo(s[0],s[1]);g.lineTo(s[2],s[3])}g.stroke()};
 rr(2.3,-.72,1.4,.66,BL,BLD);dots(2.3,-.72,1.4,.66,8,4,BLD,.05);
 rr(2.3,6.06,1.4,.66,RD,RDD);dots(2.3,6.06,1.4,.66,8,4,RDD,.05);
 dots(-.95,1.3,.8,.45,4,2,'#3b82f6',.09);dots(-.95,4.3,.8,.45,4,2,'#ef4444',.09);
 rr(0,0,6,6,'#8a8a8a');
 g.strokeStyle='#6e6e6e';g.lineWidth=2*px;g.beginPath();for(let i=1;i<6;i++){g.moveTo(i,0);g.lineTo(i,6);g.moveTo(0,i);g.lineTo(6,i)}g.stroke();
 g.strokeStyle='#f5f5f5';g.lineWidth=4*px;g.setLineDash([.12,.08]);g.beginPath();g.moveTo(0,3);g.lineTo(6,3);g.stroke();g.setLineDash([]);
 ln(BL,[[0,1,2,1],[0,2,2,2],[4,1,6,1],[4,2,6,2],[1,0,1,1],[2,0,2,1],[4,0,4,1],[5,0,5,1],[2,.375,4,.375]]);
 ln(RD,[[0,4,2,4],[0,5,2,5],[4,4,6,4],[4,5,6,5],[1,5,1,6],[2,5,2,6],[4,5,4,6],[5,5,5,6],[2,5.625,4,5.625]]);
 const sq=(c,r,f,s)=>rr(c+.1,r+.1,.8,.8,f,s);
 sq(1,0,BL,BLD);sq(4,0,BL,BLD);sq(0,1,BL,BLD);sq(5,1,BL,BLD);sq(0,4,RD,RDD);sq(5,4,RD,RDD);sq(1,5,RD,RDD);sq(4,5,RD,RDD);
 for(const[x,y,c]of[[1,2,BL],[4,2,BL],[1,3.7,RD],[4,3.7,RD]]){rr(x+.05,y+.02,.9,.26,'#a0a0a0',c);dots(x+.05,y+.02,.9,.26,7,2,'#333',.045)}
 g.beginPath();g.moveTo(2.25,3);g.lineTo(3,2.25);g.lineTo(3.75,3);g.closePath();g.fillStyle=BL;g.fill();
 g.beginPath();g.moveTo(2.25,3);g.lineTo(3,3.75);g.lineTo(3.75,3);g.closePath();g.fillStyle=RD;g.fill();
 g.beginPath();g.moveTo(3,2.25);g.lineTo(3.75,3);g.lineTo(3,3.75);g.lineTo(2.25,3);g.closePath();g.strokeStyle='#111';g.lineWidth=3*px;g.stroke();
 g.fillStyle='#bdbdbd';for(const y of[2.62,3.38]){g.beginPath();g.arc(3,y,.12,0,7);g.fill()}
 g.strokeStyle='#222';g.lineWidth=3*px;g.strokeRect(0,0,6,6);
 g.setTransform(1,0,0,1,0,0);g.font=`700 ${Math.round(k*.5)}px system-ui,sans-serif`;g.textAlign='center';g.textBaseline='middle';
 const L=(c,r,t,col,rot)=>{g.save();g.translate(o+(c+.5)*k,o+(r+.5)*k);g.rotate(rot);g.fillStyle=col;g.fillText(t,0,0);g.restore()};
 L(1,0,'B',BLD,Math.PI);L(4,0,'B',BLD,Math.PI);L(0,1,'P',BLD,Math.PI);L(5,1,'P',BLD,Math.PI);
 L(0,4,'P',RDD,0);L(5,4,'P',RDD,0);L(1,5,'B',RDD,0);L(4,5,'B',RDD,0);
 g.font=`700 ${Math.max(9,Math.round(k*.13))}px system-ui,sans-serif`;g.fillStyle='#fff';g.textAlign='left';
 g.fillText('ORTA ÇİZGİ - otonomda geçilmez',o+.08*k,o+2.88*k);
 g.textAlign='center';g.fillText('GDT (mavi)',o+3*k,o-.82*k);g.fillText('GDT (kırmızı)',o+3*k,o+6.84*k)}

function circ(p,r,f,s){g.beginPath();g.arc(p[0],p[1],r,0,7);if(f){g.fillStyle=f;g.fill()}if(s){g.strokeStyle=s;g.lineWidth=2*dpr;g.stroke()}}
function box(x,y,h,s,f){const c=Math.cos(h*R),sn=Math.sin(h*R),L=P.rl/2,W=P.rw/2;
 const q=[[L,W],[L,-W],[-L,-W],[-L,W]].map(([a,b])=>tp(x+a*c-b*sn,y+a*sn+b*c));
 g.beginPath();q.forEach((p,i)=>i?g.lineTo(p[0],p[1]):g.moveTo(p[0],p[1]));g.closePath();
 if(f){g.fillStyle=f;g.fill()}g.strokeStyle=s;g.lineWidth=1.5*dpr;g.stroke();
 g.lineWidth=4*dpr;g.beginPath();g.moveTo(q[0][0],q[0][1]);g.lineTo(q[1][0],q[1][1]);g.stroke();
 // ön tarafı gösteren ok
 const A=(a,b)=>tp(x+a*c-b*sn,y+a*sn+b*c),t1=A(L*.85,0),t2=A(L*.2,W*.45),t3=A(L*.2,-W*.45);
 g.beginPath();g.moveTo(t1[0],t1[1]);g.lineTo(t2[0],t2[1]);g.lineTo(t3[0],t3[1]);g.closePath();g.fillStyle=s;g.fill()}
function handles(i){const w=P.wp[i],u=U(w.d),r=[];
 if(i>0)r.push({t:'hin',x:w.x-u[0]*w.li,y:w.y-u[1]*w.li});
 if(i<P.wp.length-1)r.push({t:'hout',x:w.x+u[0]*w.lo,y:w.y+u[1]*w.lo});return r}
function knob(i){const w=P.wp[i],u=U(w.h),d=Math.max(P.rl/2,15)+15;return[w.x+u[0]*d,w.y+u[1]*d]}

function poseAt(t){if(!smp.length)return null;let i=1;while(i<smp.length&&smp[i].t0<t)i++;
 if(i>=smp.length)return smp[smp.length-1];
 const a=smp[i-1],b=smp[i];if(t<=a.t1)return a;
 const f=cl((t-a.t1)/Math.max(1e-6,b.t0-a.t1),0,1);return{x:a.x+(b.x-a.x)*f,y:a.y+(b.y-a.y)*f,h:a.h+wrap(b.h-a.h)*f}}

function draw(){
 size();field();g.setTransform(1,0,0,1,0,0);
 const W=P.wp,last=W.length-1;
 W.forEach((w,i)=>box(w.x,w.y,w.h,i===sel?'rgba(255,255,255,.9)':'rgba(255,255,255,.35)',null));
 g.lineCap='round';g.lineWidth=Math.max(2,3.5*dpr);
 for(let i=1;i<smp.length;i++){const a=smp[i-1],b=smp[i],f=cl((a.v+b.v)/2/P.maxV,0,1),p=tp(a.x,a.y),q=tp(b.x,b.y);
  g.strokeStyle=`hsl(${Math.round(f*120)},85%,50%)`;g.beginPath();g.moveTo(p[0],p[1]);g.lineTo(q[0],q[1]);g.stroke()}
 if(trail.length>1){g.strokeStyle='#f97316';g.lineWidth=2*dpr;g.beginPath();trail.forEach((p,i)=>{const q=tp(p.x,p.y);i?g.lineTo(q[0],q[1]):g.moveTo(q[0],q[1])});g.stroke()}
 const w=W[sel];
 if(w){const p=tp(w.x,w.y);g.lineWidth=1.5*dpr;
  for(const hd of handles(sel)){const q=tp(hd.x,hd.y);g.strokeStyle='#fff';g.beginPath();g.moveTo(p[0],p[1]);g.lineTo(q[0],q[1]);g.stroke();circ(q,5*dpr,'#1c232c','#fff')}
  const kn=knob(sel),q=tp(kn[0],kn[1]);g.strokeStyle='#facc15';g.lineWidth=1.5*dpr;g.beginPath();g.moveTo(p[0],p[1]);g.lineTo(q[0],q[1]);g.stroke();circ(q,6*dpr,'#facc15','#000')}
 g.font=`600 ${Math.round(11*dpr)}px system-ui,sans-serif`;g.textAlign='center';g.textBaseline='bottom';
 W.forEach((w,i)=>{const p=tp(w.x,w.y),r=(i===sel?8:6)*dpr,col=i===0?'#22c55e':i===last?'#ef4444':'#fff';
  if(w.st&&i<last){g.fillStyle=col;g.fillRect(p[0]-r,p[1]-r,2*r,2*r);g.strokeStyle='#000';g.lineWidth=2*dpr;g.strokeRect(p[0]-r,p[1]-r,2*r,2*r)}else circ(p,r,col,'#000');
  if(i===sel)circ(p,r+4*dpr,null,'#3b82f6');
  const t=(i===0?'BAŞLANGIÇ ':'')+String(i+1)+(w.e?` O${w.e}`:'')+(w.c&&w.st&&i<last?' ŞİŞE':'')+(w.st&&w.w&&i<last?` ${w.w}ms`:'');
  g.lineWidth=3*dpr;g.strokeStyle='rgba(0,0,0,.7)';g.strokeText(t,p[0],p[1]-r-4*dpr);g.fillStyle='#fff';g.fillText(t,p[0],p[1]-r-4*dpr)});
 if(playing||playT>0){const p=poseAt(playT);if(p)box(p.x,p.y,p.h,'#38bdf8','rgba(56,189,248,.3)')}
 const s=robotS();if($('cLive').checked&&s&&s.x!==undefined&&s.st>=1){const p=toField(s.x,s.y,s.h);box(p.x,p.y,p.h,'#f97316','rgba(249,115,22,.3)')}
 if(document.activeElement!==$('ppT'))$('ppT').value=tot?Math.round(playT/tot*1000):0;
 $('ppTT').textContent=`${playT.toFixed(1)} / ${tot.toFixed(1)} sn`}

// ---------- düzenleme ----------
function changed(noSync){build();if(!noSync)sync();info();draft()}
function info(){$('ppLen').textContent=(plen/100).toFixed(2)+' m';
 const dur=$('dur')?+$('dur').value||30:30,tt=$('ppTime');tt.textContent=tot.toFixed(1)+' sn'+(tot>dur?` (otonom ${dur} sn'yi aşıyor!)`:'');tt.className=tot>dur?'ppbad':'';
 const pp=$('ppPts');pp.textContent=`${smp.length} / ${MAXPTS}`;pp.className=smp.length>MAXPTS?'ppbad':''}
const WF=[['wX','x'],['wY','y'],['wH','h'],['wS','s'],['wW','w'],['wE','e']];
const CF=[['cTile','tile',20],['cRw','rw',5],['cRl','rl',5],['cMaxV','maxV',5],['cAcc','acc',10],['cTurn','turn',10],['cAlat','alat',10],['cV100','v100',10]];
function sync(){sel=cl(sel,0,P.wp.length-1);const w=P.wp[sel],last=P.wp.length-1;
 $('ppIdx').textContent=`${sel+1} / ${P.wp.length}`;
 for(const[id,k]of WF){const el=$(id);if(document.activeElement!==el)el.value=Math.round((w[k]||0)*10)/10}
 $('wSt').checked=sel===last||!!w.st;$('wSt').disabled=sel===last;$('wW').disabled=!w.st||sel===last;
 $('wC').checked=!!w.c&&sel!==last;$('wC').disabled=sel===last;
 for(const[id,k]of CF){const el=$(id);if(document.activeElement!==el)el.value=P[k]}
 if(document.activeElement!==$('ppName'))$('ppName').value=P.name||''}
const snapOn=e=>$('cSnap').checked&&!(e&&e.shiftKey);
const sn=(v,e)=>snapOn(e)?Math.round(v/5)*5:Math.round(v*10)/10;
function addW(x,y,e){const W=P.wp,l=W[W.length-1];x=sn(x,e);y=sn(y,e);
 const d=Math.atan2(y-l.y,x-l.x)/R,dist=Math.hypot(x-l.x,y-l.y);if(dist<2)return;
 W.push(mkW(x,y,l.h,d,dist/3));
 l.d=W.length>=3?Math.atan2(y-W[W.length-3].y,x-W[W.length-3].x)/R:d;l.lo=dist/3;
 sel=W.length-1;changed()}
function insertNear(px,py){let bi=-1,bd=1e9;
 smp.forEach((p,i)=>{const q=tp(p.x,p.y),d=Math.hypot(q[0]-px,q[1]-py);if(d<bd){bd=d;bi=i}});
 if(bi<0||bd>14*dpr){msg('Araya nokta eklemek için yolun üstüne daha yakın Shift+tıklayın');return}
 const p=smp[bi],a=smp[Math.max(0,bi-1)],b=smp[Math.min(smp.length-1,bi+1)],seg=p.seg;
 const d=Math.atan2(b.y-a.y,b.x-a.x)/R,A=P.wp[seg],B=P.wp[seg+1];
 const l=Math.min(Math.hypot(p.x-A.x,p.y-A.y),Math.hypot(p.x-B.x,p.y-B.y))/3;
 P.wp.splice(seg+1,0,mkW(Math.round(p.x),Math.round(p.y),Math.round(p.h),d,l));sel=seg+1;changed()}
function delW(i){if(P.wp.length<=1)return;P.wp.splice(i,1);sel=cl(i>0?i-1:0,0,P.wp.length-1);changed()}
function mirror(ax){const f=F();for(const w of P.wp){if(ax==='y'){w.y=f-w.y;w.h=wrap(-w.h);w.d=wrap(-w.d)}else{w.x=f-w.x;w.h=wrap(180-w.h);w.d=wrap(180-w.d)}}changed()}

function hit(px,py){const D=(q)=>Math.hypot(q[0]-px,q[1]-py),r0=11*dpr;
 if(P.wp[sel]){const k=knob(sel);if(D(tp(k[0],k[1]))<r0)return{t:'head',i:sel};
  for(const h of handles(sel))if(D(tp(h.x,h.y))<r0)return{t:h.t,i:sel}}
 for(let i=P.wp.length-1;i>=0;i--)if(D(tp(P.wp[i].x,P.wp[i].y))<r0)return{t:'wp',i};
 return null}
const evp=e=>{const r=cv.getBoundingClientRect();return[(e.clientX-r.left)*cv.width/r.width,(e.clientY-r.top)*cv.height/r.height]};
cv.addEventListener('contextmenu',e=>e.preventDefault());
cv.addEventListener('pointerdown',e=>{const[px,py]=evp(e),h=hit(px,py);
 if(e.button===2){if(h&&h.t==='wp')delW(h.i);return}
 if(e.button!==0)return;
 if(h){sel=h.i;drag=h;try{cv.setPointerCapture(e.pointerId)}catch(x){}sync();return}
 const[x,y]=fp(px,py);if(e.shiftKey)insertNear(px,py);else addW(x,y,e)});
cv.addEventListener('pointermove',e=>{
 if(!drag){const[px,py]=evp(e);cv.style.cursor=hit(px,py)?'grab':'crosshair';return}
 const[px,py]=evp(e);let[x,y]=fp(px,py);const w=P.wp[drag.i],m=MG()*.95;x=cl(x,-m,F()+m);y=cl(y,-m,F()+m);
 if(drag.t==='wp'){w.x=sn(x,e);w.y=sn(y,e)}
 else if(drag.t==='head'){const a=Math.atan2(y-w.y,x-w.x)/R;w.h=wrap(snapOn(e)?Math.round(a/15)*15:Math.round(a))}
 else{const dx=x-w.x,dy=y-w.y,l=Math.hypot(dx,dy);if(l<1)return;let a=Math.atan2(dy,dx)/R;if(drag.t==='hin')a+=180;w.d=wrap(a);if(drag.t==='hin')w.li=l;else w.lo=l}
 changed()});
const up=()=>{drag=null};cv.addEventListener('pointerup',up);cv.addEventListener('pointercancel',up);
document.addEventListener('keydown',e=>{if(!$('p-plan')||!$('p-plan').classList.contains('on'))return;
 const t=document.activeElement&&document.activeElement.tagName;if(t==='INPUT'||t==='SELECT'||t==='TEXTAREA')return;
 if(e.key==='Delete'||e.key==='Backspace'){delW(sel);e.preventDefault()}});

for(const[id,k]of WF)$(id).addEventListener('input',e=>{const v=parseFloat(e.target.value);if(!isFinite(v))return;const w=P.wp[sel];
 w[k]=k==='h'?wrap(v):k==='e'?cl(Math.round(v),0,255):(k==='w'||k==='s')?Math.max(0,v):v;changed(true)});
$('wSt').onchange=e=>{const w=P.wp[sel];w.st=e.target.checked;if(!w.st)w.c=0;changed()};
$('wC').onchange=e=>{const w=P.wp[sel];w.c=e.target.checked?1:0;if(w.c){w.st=true;if(!(w.w>=1000))w.w=8000}changed()};
for(const[id,k,mn]of CF)$(id).addEventListener('input',e=>{const v=parseFloat(e.target.value);if(isFinite(v)&&v>=mn){P[k]=v;changed(true)}});
$('ppName').oninput=e=>{P.name=e.target.value;draft()};
$('wPrev').onclick=()=>{sel=Math.max(0,sel-1);sync()};
$('wNext').onclick=()=>{sel=Math.min(P.wp.length-1,sel+1);sync()};
$('wDel').onclick=()=>delW(sel);
$('ppNew').onclick=()=>{if(confirm('Yeni yol başlatılsın mı? Kaydedilmemiş değişiklikler kaybolur.')){fresh();changed()}};
$('ppMirY').onclick=()=>mirror('y');$('ppMirX').onclick=()=>mirror('x');
$('ppTrail').onclick=()=>{trail=[]};
$('ppPlay').onclick=()=>{playing=!playing;if(playing&&playT>=tot)playT=0;setPlayBtn()};
function setPlayBtn(){$('ppPlay').innerHTML=playing?'&#10074;&#10074; Duraklat':'&#9654; Önizle'}
$('ppT').oninput=e=>{playT=e.target.value/1000*tot;playing=false;setPlayBtn()};
$('ppExp').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(P,null,1)],{type:'application/json'}));
 a.download=(P.name||'yol').replace(/[^\w-]+/g,'_')+'.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)};
$('ppImp').onclick=()=>$('ppFile').click();
$('ppFile').onchange=async e=>{const f=e.target.files[0];if(!f)return;try{P=fix(JSON.parse(await f.text()));sel=0;playT=0;changed();msg(f.name+' açıldı')}catch(x){msg('Geçerli bir yol dosyası değil',1)}e.target.value=''};

// ---------- hazır rotalar (GERİ DÖNÜŞTÜR kılavuzuna göre) ----------
// Saha 6x6 karo, karo 61 cm. Kırmızı yarı altta (y < 183), mavi üstte. ORTA ÇİZGİ y = 183: otonomda geçilmez (+5 rakibe).
// B = başlangıç alanı (ATM), P = park alanı. Kırmızı B: (1,5) (4,5) karoları, P: (0,4) (5,4). GDT penceresi alt duvar ortasında.
// Otonom puanı: B'den tamamen çık +4, P'ye park +4, GDT'ye her şişe +4.
// Olay 1 = şişeyi GDT penceresine bırak, Olay 2 = şişe almaya hazırlan (raf ya da insan oyuncu)  ->  robot_web.ino pathEvent()
// "Şişe bekle" noktası: robot şişe sensörü (GPIO 5 limit switch) basılana kadar bekler, en fazla "Bekle" ms.
// pw(x, y, açı, eğri yönü, giriş tutamacı, çıkış tutamacı, ek ayarlar)
const pw=(x,y,h,d,li,lo,o)=>Object.assign(mkW(x,y,h,d,0),{li,lo},o||{});
const STOP=(w,e)=>({st:true,w,e:e||0});
const RB=[[91.5,30.5],[274.5,30.5]],RP=[[30.5,91.5],[335.5,91.5]],GDT=[183,26],RACK=[[91.5,99],[274.5,99]];
const RED=[];
[0,1].forEach(s=>{const side=s?'sağ':'sol',B=RB[s],Pk=RP[s],K=RACK[s],toG=s?180:0,toP=s?30:150;
 // En hızlı: B'den çapraz komşu P'ye dümdüz (yaklaşık 2 sn)
 RED.push({name:`Kırmızı ${side}: B → P  (+8, en hızlı)`,maxV:100,acc:150,
  wp:[pw(B[0],B[1],90,s?45:135,0,0),pw(Pk[0],Pk[1],90,s?45:135,0,0)]});
 // Ön yüklü şişeyi GDT'ye at, park et (robot yüzü duvara dönük başlar)
 RED.push({name:`Kırmızı ${side}: şişe → GDT → P  (+12, mekanizma)`,maxV:80,acc:130,
  wp:[pw(B[0],B[1],-90,toG,0,30),pw(GDT[0],GDT[1],-90,toG,30,3,STOP(600,1)),pw(Pk[0],Pk[1],-90,toP,30,0)]});
 // Ön yüklü şişe + raftan 1 şişe
 RED.push({name:`Kırmızı ${side}: 2 şişe → GDT → P  (+16, mekanizma)`,maxV:80,acc:130,
  wp:[pw(B[0],B[1],-90,toG,0,30),pw(GDT[0],GDT[1],-90,toG,30,3,STOP(600,1)),
   pw(K[0],K[1],90,90,25,3,STOP(800,2)),pw(GDT[0],GDT[1],-90,-90,25,3,STOP(600,1)),pw(Pk[0],Pk[1],-90,toP,30,0)]});
 // İnsan oyuncu sol P'ye şişe bırakır: ön yüklü şişeyi at, P'de sensörle şişeyi bekle, onu da at, P'ye park et
 if(s===0)RED.push({name:`Kırmızı sol: şişe → GDT → P'de şişe al → GDT → P  (+16, sensör)`,maxV:80,acc:130,
  wp:[pw(B[0],B[1],-90,toG,0,30),pw(GDT[0],GDT[1],-90,toG,30,3,STOP(600,1)),
   pw(Pk[0],Pk[1],-90,toP,30,3,Object.assign(STOP(8000,2),{c:1})),
   pw(GDT[0],GDT[1],-90,-30,30,3,STOP(600,1)),pw(Pk[0],Pk[1],-90,toP,30,0)]});
});
const F0=6*61,toBlue=p=>Object.assign(JSON.parse(JSON.stringify(p)),{name:p.name.replace('Kırmızı','Mavi'),
 wp:p.wp.map(w=>Object.assign({},w,{y:F0-w.y,h:wrap(-w.h),d:wrap(-w.d)}))});
const PRESETS=[...RED,...RED.map(toBlue),
 {name:'Test: 80 cm ileri',maxV:50,acc:100,wp:[pw(183,30.5,90,90,0,25),pw(183,110.5,90,90,25,0)]},
 {name:'Test: 61 cm kare (yana kayarak)',maxV:40,acc:100,wp:[pw(152.5,30.5,90,90,0,20),pw(152.5,91.5,90,0,20,20,STOP(300)),
  pw(213.5,91.5,90,-90,20,20,STOP(300)),pw(213.5,30.5,90,180,20,20,STOP(300)),pw(152.5,30.5,90,180,20,0)]},
 {name:'Test: 90° dönerek ileri',maxV:40,acc:100,wp:[pw(183,30.5,90,90,0,25),pw(183,110.5,180,90,25,0)]},
];
$('ppPre').innerHTML=PRESETS.map((p,i)=>`<option value="${i}">${escH(p.name)}</option>`).join('');
$('ppPreOpen').onclick=()=>{const src=PRESETS[+$('ppPre').value];if(!src)return;
 if(!confirm(`"${src.name}" açılsın mı? Ekrandaki kaydedilmemiş yol kaybolur.`))return;
 const keep={rw:P.rw,rl:P.rl,turn:P.turn,alat:P.alat,v100:P.v100===140?300:P.v100};   // 140 = eski yanlış varsayılan
 P=fix(Object.assign(JSON.parse(JSON.stringify(src)),{tile:61},keep));sel=0;playT=0;trail=[];changed();
 msg(`"${src.name}" açıldı - robotu BAŞLANGIÇ kutusuna ok yönünde koyun, sonra slot seçip "Robota gönder"`)};

// ---------- robot ile ----------
async function api(url,opt){const r=await fetch(url,Object.assign({cache:'no-store'},opt||{}));let j={};try{j=await r.json()}catch(e){}
 if(!r.ok)throw new Error(j.err||('HTTP '+r.status));return j}
async function loadSlots(){try{const j=await api('/paths');slots=j.s||[];act=j.act;
 if(!j.fs)msg('Robotta dosya sistemi yok - Tools > Partition Scheme ayarını Default yapıp tekrar yükleyin',1);renderSlots()}catch(e){}}
const slotName=i=>{const x=slots[i-1];return x&&x.n?`${x.name||'Yol'} (${x.n} nokta)`:'(boş)'};
function renderSlots(){const s=$('ppSlot');
 s.innerHTML=[1,2,3,4].map(i=>`<option value="${i}">Slot ${i}: ${escH(slotName(i))}</option>`).join('');s.value=slot;
 const a=$('autoSel');if(a&&document.activeElement!==a){a.innerHTML='<option value="0">Kod rotası (AUTO_ROUTE)</option>'+[1,2,3,4].map(i=>`<option value="${i}">Yol ${i}: ${escH(slotName(i))}</option>`).join('');a.value=act>0?act:0}
 $('ppAct').textContent=act>0?`Slot ${act}: ${slotName(act)}`:act===0?'Kod rotası (AUTO_ROUTE)':'-'}
$('ppSlot').onchange=e=>{slot=+e.target.value;draft();msg(`Slot ${slot} seçildi - "Yükle" onu açar, "Robota gönder" ekrandaki yolu oraya kaydeder`)};
$('ppLoad').onclick=async()=>{try{const r=await fetch('/path?s='+slot,{cache:'no-store'});if(!r.ok){msg(`Slot ${slot} boş`,1);return}
 const line=(await r.text()).split('\n').find(l=>l[0]==='J');if(!line){msg('Bu slotta düzenlenebilir veri yok',1);return}
 P=fix(JSON.parse(line.slice(1)));sel=0;playT=0;trail=[];changed();msg(`Slot ${slot} yüklendi`)}catch(e){msg('Yükleme başarısız: '+e.message,1)}};
$('ppSave').onclick=async()=>{if(smp.length>MAXPTS){msg(`Yol çok uzun (${smp.length} > ${MAXPTS} nokta)`,1);return}
 if(P.maxV>P.v100)msg('Uyarı: en yüksek hız, robotun %100 güçteki hızından fazla',1);
 try{const j=await api('/path?s='+slot,{method:'POST',headers:{'Content-Type':'text/plain'},body:exportTxt()});
  msg(`Robota gönderildi: slot ${slot} (${j.n} nokta)`+(act!==slot?' - çalıştırmak için "Otonomda kullan"a basın':''));loadSlots()}catch(e){msg('Gönderme başarısız: '+e.message,1)}};
async function setAct(s){try{await api('/pathact?s='+s);act=s;renderSlots();msg(s?`Otonomda slot ${s} çalışacak`:'Otonomda kod rotası çalışacak');setTimeout(loadSlots,400)}catch(e){msg('Başarısız: '+e.message,1);loadSlots()}}
$('ppUse').onclick=()=>setAct(slot);
const aSel=$('autoSel');if(aSel)aSel.onchange=e=>setAct(+e.target.value);
$('ppDel').onclick=async()=>{if(!confirm(`Robottaki slot ${slot} yolu silinsin mi?`))return;
 try{await api('/pathdel?s='+slot);msg(`Slot ${slot} silindi`);loadSlots()}catch(e){msg('Silme başarısız: '+e.message,1)}};
document.querySelectorAll('.tab[data-p="plan"]').forEach(t=>t.addEventListener('click',loadSlots));

function live(){const s=robotS();if(!s||s.x===undefined)return;
 if(s.pa!==undefined&&s.pa!==act&&act>=0){act=s.pa;renderSlots()}
 if(aSel)aSel.disabled=s.st===2;
 if(s.st===2&&lastSt!==2)trail=[];lastSt=s.st;
 if(s.st===2){const p=toField(s.x,s.y,s.h),l=trail[trail.length-1];if(!l||Math.hypot(p.x-l.x,p.y-l.y)>1)trail.push(p);if(trail.length>4000)trail.shift()}}
function frame(ts){requestAnimationFrame(frame);const dt=lastF?Math.min(.1,(ts-lastF)/1000):0;lastF=ts;
 if(playing){playT+=dt;if(playT>=tot){playT=tot;playing=false;setPlayBtn()}}
 live();if($('p-plan')&&$('p-plan').classList.contains('on'))draw()}
changed();renderSlots();loadSlots();requestAnimationFrame(frame);
})();
)PPJS";
