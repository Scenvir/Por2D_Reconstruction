'use strict';
const M = MapModel, $ = id => document.getElementById(id);
const canvas = $('map'), ctx = canvas.getContext('2d');
let map = M.blank(), tool = '1', zoom = 1, dirty = false, history = [], future = [], stroke = null;
let selection = {tiles:[], bodies:[]};
const clearSelection = () => { selection={tiles:[],bodies:[]}; };
function selectedAt(c) {
  return selection.tiles.some(t=>t.x===c.x&&t.y===c.y)||selection.bodies.some(key=>{
    const r=M.bounds(map[key]);
    return c.x*20<r.x+r.w&&(c.x+1)*20>r.x&&c.y*20<r.y+r.h&&(c.y+1)*20>r.y;
  });
}
const snapshot = () => JSON.stringify(map);
function status(text) { $('status').textContent = text; }
function markChanged() { dirty = true; document.title = '* Por2D 地图编辑器'; $('validation').textContent = ''; }
function record(before) {
  if (before === snapshot()) return;
  history.push(before); if(history.length > 100) history.shift(); future=[]; markChanged(); draw();
}
function controls() { clearSelection(); $('name').value = map.name; $('commentary').value = map.commentary; if(map[tool]) $('direction').value = map[tool].direction; }
function selectTool(value) {
  cancel();clearSelection();
  tool=value; document.querySelectorAll('[data-tool]').forEach(b => b.classList.toggle('active',b.dataset.tool===tool));
  if(map[tool]) $('direction').value=map[tool].direction;
  draw();
}
function draw() {
  const cell=20*zoom, margin=32;
  canvas.width = margin + M.W*cell+1; canvas.height = margin+M.H*cell+1;
  ctx.fillStyle='#121923'; ctx.fillRect(0,0,canvas.width,canvas.height);
  ctx.font='10px monospace'; ctx.textAlign='center'; ctx.textBaseline='middle';
  for(let x=0;x<M.W;x++){ctx.fillStyle='#a8bfdc';ctx.fillText(x,margin+(x+.5)*cell,16);}
  for(let y=0;y<M.H;y++){
    ctx.fillStyle='#a8bfdc';ctx.fillText(y,14,margin+(y+.5)*cell);
    for(let x=0;x<M.W;x++){
      ctx.fillStyle=['#18212e','#e5ebf2','#596579'][map.tiles[y][x]];
      ctx.fillRect(margin+x*cell,margin+y*cell,cell,cell);
    }
  }
  ctx.lineWidth=1;
  for(let x=0;x<=M.W;x++){ctx.strokeStyle=x%5?'#344154':'#62718a';ctx.beginPath();ctx.moveTo(margin+x*cell+.5,margin);ctx.lineTo(margin+x*cell+.5,canvas.height);ctx.stroke();}
  for(let y=0;y<=M.H;y++){ctx.strokeStyle=y%5?'#344154':'#62718a';ctx.beginPath();ctx.moveTo(margin,margin+y*cell+.5);ctx.lineTo(canvas.width,margin+y*cell+.5);ctx.stroke();}
  for(const key of ['exit','spawn']) {
    const b=map[key];if(!b)continue;
    const r=M.bounds(b), x=margin+r.x*zoom,y=margin+r.y*zoom,w=r.w*zoom,h=r.h*zoom;
    ctx.fillStyle=key==='spawn'?'#ffe699aa':'#70d9c599';ctx.fillRect(x,y,w,h);
    ctx.strokeStyle=key==='spawn'?'#ffe699':'#70d9c5';ctx.lineWidth=2;ctx.strokeRect(x,y,w,h);
    if(key==='exit') {
      ctx.save();ctx.translate(x+w/2,y+h/2);ctx.rotate(b.direction*Math.PI/2);ctx.scale(zoom,zoom);
      ctx.strokeStyle='#e6fff7';ctx.fillStyle='#e6fff7';ctx.lineWidth=2;
      ctx.beginPath();ctx.moveTo(-7,-24);ctx.lineTo(7,-24);ctx.stroke();
      ctx.beginPath();ctx.moveTo(0,17);ctx.lineTo(0,-9);ctx.stroke();
      ctx.beginPath();ctx.moveTo(0,-19);ctx.lineTo(-6,-7);ctx.lineTo(6,-7);ctx.closePath();ctx.fill();ctx.restore();
    } else {ctx.fillStyle='#fff';ctx.font=`bold ${14*zoom}px sans-serif`;ctx.fillText(['↑','→','↓','←'][b.direction],x+w/2,y+h/2);}
    ctx.fillStyle='#fff';ctx.font='11px sans-serif';ctx.fillText(key==='spawn'?'出生':'出口 · 头朝'+['上','右','下','左'][b.direction],x+w/2,y-7);
  }
  ctx.strokeStyle='#5cc8ff';ctx.fillStyle='#5cc8ff55';ctx.lineWidth=2;
  for(const c of selection.tiles){ctx.fillRect(margin+c.x*cell,margin+c.y*cell,cell,cell);ctx.strokeRect(margin+c.x*cell+2,margin+c.y*cell+2,cell-4,cell-4);}
  for(const key of selection.bodies){const r=M.bounds(map[key]);ctx.strokeRect(margin+r.x*zoom,margin+r.y*zoom,r.w*zoom,r.h*zoom);}
  if(stroke && (stroke.rectangle||stroke.selecting) && stroke.last) {
    const a=stroke.start,b=stroke.last;
    const x=margin+Math.min(a.x,b.x)*cell,y=margin+Math.min(a.y,b.y)*cell;
    const w=(Math.abs(a.x-b.x)+1)*cell,h=(Math.abs(a.y-b.y)+1)*cell;
    ctx.fillStyle=stroke.erase?'#ff997755':'#7be4cd55';ctx.fillRect(x,y,w,h);
    ctx.strokeStyle='#7be4cd';ctx.setLineDash([5,3]);ctx.strokeRect(x,y,w,h);ctx.setLineDash([]);
  }
  $('undo').disabled=!history.length; $('redo').disabled=!future.length;
}
function cellAt(e) {
  const r=canvas.getBoundingClientRect();
  const x=Math.floor(((e.clientX-r.left)*canvas.width/r.width-32)/(20*zoom));
  const y=Math.floor(((e.clientY-r.top)*canvas.height/r.height-32)/(20*zoom));
  return x>=0&&x<M.W&&y>=0&&y<M.H?{x,y}:null;
}
function paint(c,erase) {
  if(erase){
    let removed=false;
    for(const key of ['spawn','exit']){const b=map[key];if(b){const r=M.bounds(b);if(c.x*20<r.x+r.w&&(c.x+1)*20>r.x&&c.y*20<r.y+r.h&&(c.y+1)*20>r.y){map[key]=null;removed=true;}}}
    if(!removed)map.tiles[c.y][c.x]=0;
  }else if(tool==='spawn'||tool==='exit')map[tool]={...c,direction:Number($('direction').value)};
  else map.tiles[c.y][c.x]=Number(tool);
}
function move(e) {
  const c=cellAt(e);
  $('coords').textContent=c?`格坐标 (${c.x}, ${c.y}) · 左上像素 (${c.x*20}, ${c.y*20})`:'鼠标位于网格外';
  if(!stroke)return;
  if(!c){if(stroke.moving)stroke.valid=false;return;}
  if(stroke.selecting){stroke.last=c;selection=M.selectRectangle(map,stroke.start,c);draw();return;}
  if(stroke.moving){
    const result=M.moveSelection(M.parse(stroke.before),stroke.selection,c.x-stroke.start.x,c.y-stroke.start.y);
    stroke.valid=!!result;
    if(result){map=result.map;selection=result.selection;}
    status(result?'松开鼠标完成整体移动；Esc 取消。':'无法放置：超出地图边界或覆盖其他方块。松开将取消本次移动。');
    draw();return;
  }
  if(stroke.rectangle){stroke.last=c;draw();return;}
  const a=stroke.last||c, steps=Math.max(Math.abs(c.x-a.x),Math.abs(c.y-a.y),1);
  for(let i=1;i<=steps;i++)paint({x:Math.round(a.x+(c.x-a.x)*i/steps),y:Math.round(a.y+(c.y-a.y)*i/steps)},stroke.erase);
  stroke.last=c;draw();
}
function finish(){if(stroke){const before=stroke.before;
  if(stroke.moving&&!stroke.valid){cancel();return;}
  if(stroke.selecting)status(`已选中 ${selection.tiles.length} 个方块、${selection.bodies.length} 个标记。拖动选中元素移动；在其他位置拖动重新框选。`);
  if(stroke.rectangle && stroke.last) M.fillRectangle(map,stroke.start,stroke.last,stroke.erase?0:Number(stroke.tool));
  stroke=null;record(before);draw();}}
function cancel(){if(stroke){map=M.parse(stroke.before);if(stroke.selection)selection=stroke.selection;stroke=null;draw();}}
canvas.addEventListener('contextmenu',e=>e.preventDefault());
canvas.addEventListener('pointerdown',e=>{
  const c=cellAt(e);if(stroke||e.button!==0&&e.button!==2||!c)return;e.preventDefault();
  if(tool==='select'){
    if(e.button!==0)return;
    stroke={before:snapshot(),start:c,last:c,selection, moving:selectedAt(c),valid:true};
    stroke.selecting=!stroke.moving;
  }else stroke={before:snapshot(),erase:e.button===2,last:null,start:c,tool,rectangle:$('shape').value==='rectangle'&&['0','1','2'].includes(tool)};
  canvas.setPointerCapture(e.pointerId);move(e);
});
canvas.addEventListener('pointermove',move);
canvas.addEventListener('pointerup',e=>{move(e);finish();});canvas.addEventListener('pointercancel',cancel);canvas.addEventListener('lostpointercapture',cancel);
document.querySelectorAll('[data-tool]').forEach(b=>b.onclick=()=>selectTool(b.dataset.tool));
$('direction').onchange=()=>{if(map[tool]){const before=snapshot();map[tool].direction=Number($('direction').value);record(before);}};
for(const key of ['name','commentary'])$(key).oninput=()=>{const before=snapshot();map[key]=$(key).value;record(before);};
$('zoom').onchange=()=>{zoom=Number($('zoom').value);draw();};
function travel(from,to){finish();if(!from.length)return;to.push(snapshot());map=M.parse(from.pop());clearSelection();markChanged();controls();draw();}
$('undo').onclick=()=>travel(history,future);$('redo').onclick=()=>travel(future,history);
function download(text,extension,type){
  const url=URL.createObjectURL(new Blob([text],{type})),a=document.createElement('a');
  a.href=url;a.download=(map.name.trim().replace(/[<>:"/\\|?*\u0000-\u001f]/g,'_')||'map')+extension;
  a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
}
$('save').onclick=()=>{finish();download(JSON.stringify(map,null,2),'.json','application/json');dirty=false;document.title='Por2D 地图编辑器';status('已发起地图下载，请确认浏览器已保存文件。可用“打开地图”继续编辑。');};
$('export').onclick=()=>{try{download(M.cpp(map),'.cpp','text/plain');status('已导出 makeCustomLevel()。请接入游戏关卡选择并重新编译；JSON 文件仍需单独保存。');}catch(e){status('不能导出：'+e.message);}};
$('validate').onclick=()=>{const errors=M.validate(map);$('validation').textContent=errors.join('；');status(errors.length?'请检查以下问题（仍可保存 JSON 草稿）。':'检查通过：出生点和出口有效且未与墙重叠。尚未验证关卡可解性。');};
$('new').onclick=()=>{if(dirty&&!confirm('当前地图尚未保存，确定新建？'))return;map=M.blank();history=[];future=[];dirty=false;document.title='Por2D 地图编辑器';controls();draw();$('validation').textContent='';status('已新建空白地图。');};
$('open').onclick=()=>{if(dirty&&!confirm('当前地图尚未保存，确定打开其他地图？'))return;$('file').click();};
$('file').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  try{if(file.size>1024*1024)throw Error('地图文件不能超过 1 MB');const next=M.parse(await file.text());map=next;history=[];future=[];dirty=false;document.title='Por2D 地图编辑器';controls();draw();$('validation').textContent='';status('已打开 '+file.name);}
  catch(err){status('打开失败：'+err.message);}finally{e.target.value='';}
};
window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
window.addEventListener('keydown',e=>{
  if(e.key==='Escape'&&stroke){e.preventDefault();cancel();return;}
  if(e.key==='Escape'){clearSelection();draw();return;}
  if(e.ctrlKey||e.metaKey){if(e.key.toLowerCase()==='s'){e.preventDefault();$('save').click();return;}}
  if(['INPUT','SELECT','TEXTAREA'].includes(document.activeElement.tagName))return;
  if(e.ctrlKey||e.metaKey){const k=e.key.toLowerCase();if(k==='z'||k==='y'){e.preventDefault();(k==='y'||e.shiftKey?$('redo'):$('undo')).click();}}
  else if(/^[1-5]$/.test(e.key))selectTool(['0','1','2','spawn','exit'][Number(e.key)-1]);
  else if(e.key.toLowerCase()==='v')selectTool('select');
});
controls();draw();
