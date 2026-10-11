import {ESPLoader, Transport} from 'esptool-js';
import SparkMD5 from 'spark-md5';
import {loadReleaseIndex, createReleaseSelection} from './releases.js';
import {createInstaller} from './installer.js';
import {useVerifiedFlashReads} from './flash-read.js';
import './style.css';
const $ = id => document.getElementById(id);
let catalog, busy=false, selection;
const supported=isSecureContext && 'serial' in navigator;
const md5=data=>SparkMD5.ArrayBuffer.hash(data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength));
function log(message) {$('log').textContent+=message+'\n';$('log').scrollTop=$('log').scrollHeight;}
function status(message,state='') {$('status').textContent=message;$('status').dataset.state=state;}
function resetConsent() {$('install-confirm').checked=false;$('recovery-confirm').checked=false;}
function updateControls() {
  const mode=$('mode').value;
  $('flash').disabled=busy||!supported||!catalog||!$('board-confirm').checked||(mode==='install'&&!$('install-confirm').checked)||(mode==='recovery'&&!$('recovery-confirm').checked);
  $('install-warning').hidden=mode!=='install';$('recovery-warning').hidden=mode!=='recovery';
  for (const id of ['release','mode','board-confirm','install-confirm','recovery-confirm']) $(id).disabled=busy||(id==='release'&&!selection);
  $('flash').textContent=busy?'Checking / installing…':mode==='install'?'Connect & install':mode==='recovery'?'Connect & repair':'Connect & update';
}
for (const id of ['board-confirm','install-confirm','recovery-confirm']) $(id).addEventListener('change',updateControls);
$('mode').addEventListener('change',()=>{resetConsent();updateControls();});
$('release').addEventListener('change',()=>{if(!busy&&selection){resetConsent();selection.select($('release').value);}});
if(!supported){$('browser-note').hidden=false;status('Open this page in desktop Chrome or Edge to install over USB.');}
updateControls();
loadReleaseIndex(document.baseURI).then(index=>{
  $('release').replaceChildren(...index.releases.map(release=>{
    const option=document.createElement('option');
    option.value=release.version;option.textContent='v'+release.version+(release.version===index.latest?' · updater default':' · previous release');
    return option;
  }));
  selection=createReleaseSelection({index,baseUrl:document.baseURI,onChange:state=>{
    catalog=state.catalog;
    $('version').textContent=catalog?'v'+catalog.version:'Loading…';
    $('checksum').textContent=catalog?catalog.files[3].sha256:'';
    $('target-version').textContent=catalog?'v'+catalog.version:'the selected version';
    $('installed').textContent='Installed version is checked after connecting.';
    $('device').textContent='Chip and flash capacity are checked after connecting; confirm K150 from the device label.';
    if(state.error) status(state.error.message,'error');
    else if(supported) status(state.loading?'Loading selected release…':'Ready. Update blocks downgrades and unknown applications.');
    updateControls();
  }});
  $('release').value=index.latest;
  return selection.select(index.latest);
}).catch(error=>{catalog=null;status(error.message,'error');updateControls();});
const installer=createInstaller({
  baseUrl:document.baseURI,
  requestPort:()=>navigator.serial.requestPort({filters:[{usbVendorId:0x303a}]}),
  openConnection:async port=>{
    const transport=new Transport(port,true);
    const loader=new ESPLoader({transport,baudrate:115200,romBaudrate:115200,terminal:{clean(){},writeLine:log,write:log}});
    useVerifiedFlashReads(loader,md5);
    return {transport,loader};
  },
  md5,
  onBusy:value=>{busy=value;if(!busy)resetConsent();updateControls();},
  onStatus:status,onLog:log,onProgress:value=>{$('progress').value=value;},
  onDevice:device=>{$('device').textContent=device.chip+' · '+device.flashSize+' detected. K150 model confirmation is still yours.';log($('device').textContent);},
  onRecovery:failure=>{$('recovery-help').hidden=!failure;$('recovery-message').textContent=failure?.advice||'';},
  onInspect:installed=>{
    $('installed').textContent=installed.application?'Installed v'+installed.application.version+(installed.state==='verified'?' · complete image verified':' · image incomplete or changed'):'Installed application is unknown.';
    log($('installed').textContent);
  }
});
window.addEventListener('beforeunload',event=>{if(busy){event.preventDefault();event.returnValue='';}});
$('flash').addEventListener('click',()=>{
  if($('flash').disabled||busy)return;
  $('log').textContent='';$('details').open=true;$('progress').value=0;
  installer.run({catalog,mode:$('mode').value,installConfirmed:$('install-confirm').checked,recoveryConfirmed:$('recovery-confirm').checked});
});
