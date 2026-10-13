// Exercise the actual embedded script with DOM and transport boundaries stubbed.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert/strict');
const html = fs.readFileSync('src/web_ui.h', 'utf8');
const ids = new Set([...html.matchAll(/\bid="([^"]+)"/g)].map(m => m[1]));
const elements = new Map();
function element() {
  return {value:'',textContent:'',hidden:false,disabled:false,children:[],files:[],
    classList:{toggle(){}},replaceChildren(){this.children=[]},
    append(...items){this.children.push(...items)}};
}
for (const id of ids) elements.set(id, element());
let sent = 0, responseCode = 201;
const posts = [];
let failStatus=false;
const animationSettings = {text:'SELAMAT DATANG',text_effect:'scroll-left',image_effect:'fade',
  text_scale:1,text_color:65535,background:0,duration_ms:3000,speed:30,loop:true,image_visible:true};
async function fetchStub(path, options={}) {
  if(path==='/api/status' && failStatus) throw Error('Temporary transport failure');
  if(options.method==='POST') {
    posts.push({path,body:Object.fromEntries(options.body)});
    return {ok:true,json:async()=>({message:'Saved'})};
  }
  if(path==='/api/panel') return {ok:true,json:async()=>({scan:32,mapping:4})};
  if(path==='/api/animation') return {ok:true,json:async()=>animationSettings};
  if(path==='/api/media') return {ok:true,json:async()=>({ready:true,used:0,total:9437184,max_files:64,files:[]})};
  if(path==='/api/status') return {ok:true,json:async()=>({memory_ok:true,storage_ok:true,
    uptime_ms:1000,sta_connected:false,sta_connecting:false,ap_ip:'192.168.4.1',ap_ssid:'test',
    ap_ready:true,target_ssid:'',width:64,height:32,generation:0,layout:0,animation_active:true,animation_paused:false})};
  if(path==='/api/frame') {
    const bytes=new Uint8Array(4+64*32*2);bytes[0]=64;bytes[2]=32;
    return {ok:true,arrayBuffer:async()=>bytes.buffer};
  }
  throw Error('Unexpected request '+path);
}
elements.get('preview').getContext=()=>({createImageData(w,h){return {data:new Uint8ClampedArray(w*h*4)}},putImageData(){}});
class Xhr {
  upload = {};
  open(method, url) { assert.equal(method,'POST'); assert.equal(url,'/api/media/upload'); }
  send(form) {
    sent++;
    assert(form.get('file') instanceof File);
    this.status=responseCode;
    this.responseText=JSON.stringify(responseCode===201?{message:'Saved'}:{error:'Duplicate'});
    this.upload.onprogress({lengthComputable:true,loaded:1,total:2});
    assert.equal(elements.get('uploadProgress').textContent,'50% terkirim');
    this.upload.onload();
    this.onload();
  }
}
const context = vm.createContext({
  document:{hidden:true,getElementById(id){assert(ids.has(id),`Missing HTML element ${id}`);return elements.get(id)},
    querySelectorAll(){return []},createElement:element},
  setInterval(){},setTimeout,FormData,File,XMLHttpRequest:Xhr,URLSearchParams,AbortSignal,TextEncoder,
  fetch:fetchStub,
  confirm(){return true},console,
});
vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],context);
context.file = new File(['BM'], 'image.bmp');
(async()=>{
  await vm.runInContext('upload(file)',context);
  assert.equal(sent,1);
  assert.equal(elements.get('uploadProgress').textContent,'Selesai');
  for (const file of [null,new File([], 'empty.bmp'),new File(['x'],'../escape.bmp'),
    new File(['x'],'spaced name.bmp'),new File(['x'],'wrong.png'),new File(['x'],'UPPER.JPG'),
    new File([new Uint8Array(1048577)],'large.bmp')]) {
    context.file=file;
    await assert.rejects(vm.runInContext('upload(file)',context));
  }
  assert.equal(sent,1,'Invalid selections must not start a network upload');
  context.file=new File(['x'],'valid.jpg');responseCode=409;
  await assert.rejects(vm.runInContext('upload(file)',context),/Duplicate/);
  assert.equal(elements.get('uploadProgress').textContent,'Gagal');
  assert.equal(typeof elements.get('uploadForm').onsubmit,'function');
  assert.equal(typeof elements.get('refreshMedia').onclick,'function');
  assert.equal(elements.get('imageEffect').children.length,11);
  assert.equal(elements.get('textEffect').children.length,11);
  assert.equal(vm.runInContext("color565('#ff0000')",context),0xf800);
  assert.equal(vm.runInContext("colorHex(65535)",context),'#ffffff');
  await vm.runInContext('loadAnimation()',context);
  assert.equal(elements.get('textContent').value,'SELAMAT DATANG');
  elements.get('animationForm').onsubmit({preventDefault(){}});
  await new Promise(resolve=>setTimeout(resolve,30));
  assert.equal(posts[0].path,'/api/animation');
  assert.deepEqual(JSON.parse(posts[0].body.settings),animationSettings);
  elements.get('textContent').value='\u00e9'.repeat(97);
  elements.get('animationForm').onsubmit({preventDefault(){}});
  await new Promise(resolve=>setTimeout(resolve,30));
  assert.equal(posts.length,1,'UTF-8 byte limit must be enforced before sending');
  assert.match(elements.get('notice').textContent,/192 byte/);
  await vm.runInContext('loadPanel()',context);
  assert.equal(elements.get('panelScan').value,'32');
  assert.equal(elements.get('panelMapping').disabled,true);
  for (const [scan,mapping] of [['32','4'],['16','2'],['8','5'],['4','5']]) {
    elements.get('panelScan').value=scan;
    elements.get('panelMapping').value='2';
    elements.get('panelScan').onchange();
    assert.equal(elements.get('panelMapping').disabled,scan!=='16');
    elements.get('panelForm').onsubmit({preventDefault(){}});
    await new Promise(resolve=>setTimeout(resolve,30));
    assert.deepEqual(posts.at(-1),{path:'/api/panel',body:{scan,command:'preview',mapping}});
    elements.get('savePanel').onclick();
    await new Promise(resolve=>setTimeout(resolve,30));
    assert.deepEqual(posts.at(-1),{path:'/api/panel',body:{scan,command:'save',mapping}});
  }
  vm.runInContext('showPanelPreview({preview_active:true,preview_remaining_ms:15001})',context);
  assert.equal(elements.get('panelPreviewActions').hidden,false);
  assert.match(elements.get('panelPreviewState').textContent,/16 detik/);
  elements.get('confirmPanel').onclick();
  await new Promise(resolve=>setTimeout(resolve,30));
  assert.deepEqual(posts.at(-1),{path:'/api/panel',body:{command:'confirm'}});
  elements.get('cancelPanel').onclick();
  await new Promise(resolve=>setTimeout(resolve,30));
  assert.deepEqual(posts.at(-1),{path:'/api/panel',body:{command:'cancel'}});
  vm.runInContext('showPanelPreview({preview_active:false})',context);
  assert.equal(elements.get('panelPreviewActions').hidden,true);
  context.document.hidden=false;
  failStatus=true;
  for (let attempt=1;attempt<=3;++attempt) {
    await vm.runInContext('nextPoll=0;poll()',context);
    assert.equal(elements.get('connection').textContent,attempt<3?'Mencoba kembali...':'Koneksi terputus');
  }
  failStatus=false;
  await vm.runInContext('nextPoll=0;poll()',context);
  assert.equal(elements.get('connection').textContent,'Perangkat terhubung');
  assert.equal(vm.runInContext('pollFailures',context),0);
  console.log('PASS: temporary HTTP failures retry and connection status recovers');
  console.log('PASS: panel scan selection and mapping normalization');
  console.log('PASS: UI upload/progress/errors, animation form/settings, RGB565 colors, and UTF-8 byte limit');
})().catch(error=>{console.error(error);process.exitCode=1});
