// Run with: node tests/editor_save_test.js
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../editor/editor.js'),'utf8');
const saveCode=source.slice(source.indexOf('async function saveExport('),source.indexOf("$('validate').onclick"));
function editor(fetch,confirm=()=>true){
  const buttons={save:{},export:{}},messages=[];
  const context=vm.createContext({URL,fetch,confirm,location:{protocol:'http:',href:'http://127.0.0.1:1234/session/index.html'},
    map:{name:'中文地图'},dirty:true,document:{title:'*'},window:{},M:{cpp:()=> '// C++'},
    $:id=>buttons[id],finish:()=>{},status:message=>messages.push(message)});
  vm.runInContext('function snapshot(){return JSON.stringify(map)}\n'+saveCode,context);
  return {context,buttons,messages,save:()=>buttons.save.onclick(),cpp:()=>buttons.export.onclick()};
}
const response=status=>({status,ok:status===200,text:async()=> 'write failed'});
(async()=>{
  const calls=[];
  const success=editor(async(url,options)=>{calls.push([url.toString(),options]);return response(200);});
  await success.save();assert.equal(success.context.dirty,false);
  assert.equal(calls[0][0],'http://127.0.0.1:1234/session/save/%E4%B8%AD%E6%96%87%E5%9C%B0%E5%9B%BE.json');
  assert.equal(calls[0][1].method,'POST');assert.match(calls[0][1].body,/中文地图/);
  let count=0;
  const cancel=editor(async()=>{++count;return response(409);},()=>false);
  await cancel.save();assert.equal(count,1);assert.equal(cancel.context.dirty,true);
  count=0;
  const overwrite=editor(async url=>{++count;if(count===1)return response(409);assert.equal(url.search,'?replace=1');return response(200);});
  await overwrite.save();assert.equal(count,2);assert.equal(overwrite.context.dirty,false);
  const failed=editor(async()=>response(500));await failed.save();
  assert.equal(failed.context.dirty,true);assert.equal(failed.buttons.save.disabled,false);assert.match(failed.messages[0],/保存失败/);
  const disconnected=editor(async()=>{throw Error('offline');});await disconnected.save();
  assert.equal(disconnected.context.dirty,true);assert.match(disconnected.messages[0],/保持游戏运行/);
  let finish;
  const delayed=editor(()=>new Promise(resolve=>{finish=resolve;}));
  const pending=delayed.save();delayed.context.map.name='new edits';finish(response(200));await pending;
  assert.equal(delayed.context.dirty,true);
  const cpp=editor(async url=>{assert.match(url.pathname,/\.cpp$/);return response(200);});
  await cpp.cpp();assert.equal(cpp.context.dirty,true);
  const standalone=editor(()=>{throw Error('must not download');});standalone.context.location.protocol='file:';
  await standalone.save();assert.equal(standalone.context.dirty,true);assert.match(standalone.messages[0],/Ctrl\+E/);
  console.log('PASS editor save: direct path, confirmation, write/network failures, concurrent edits and C++ exports');
})().catch(error=>{console.error(error);process.exitCode=1;});
