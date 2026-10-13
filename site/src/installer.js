import {validateCatalog, verifiedDownload, flashVerified} from './flasher.js';
import {restartK150} from './device-reset.js';

// One attempt owns its connection. Always release it before allowing a retry.
// Callbacks keep this flow testable without opening a browser or a USB port.
export function recoveryAdvice({phase, mode, code, writeStarted}) {
  if (writeStarted) return mode==='install'
    ? 'Installation or verification did not finish. The bootloader, layout or application may be incomplete. Re-enter download mode with USB connected. Retry First install only for the same intended LaunchLab bundle after backup and explicit confirmation. If it still fails, use the M5Stack software reinstall guide below or ask for support. Never switch modes to bypass a refusal.'
    : 'The application write or verification did not finish. Re-enter download mode with USB connected, then deliberately select Repair / rollback and the intended version. Repair will recheck the layout before writing. If that check fails, stop and use the recovery guide below; do not switch to First install to bypass it.';
  if (code==='layout') return 'No flash was written by this attempt. A different partition layout or boot selection needs a backup and a reviewed recovery choice. First install is not an automatic fix. See the recovery guide below.';
  if (code==='device') return 'No flash was written by this attempt. Check the printed model/SKU: this bundle requires StickS3 K150 with 8 MB flash. Chip and USB identity alone cannot prove the board model. Do not bypass this check.';
  if (code==='bundle-layout') return 'No flash was written by this attempt. The downloaded bundle is outside the reviewed LaunchLab layout. Save the installation log and ask for support; do not retry through another mode.';
  if (code==='application') return 'No flash was written by this attempt. If this is an interrupted LaunchLab installation, deliberately choose Repair / rollback after backup. For M5Stack software, use the vendor reinstall guide below.';
  if (phase==='connect') return 'No flash was written by this attempt. Automatic download-mode connection failed. Close other serial tools and use a USB data cable. With USB connected, hold the side Power/Reset button until the green LED flashes, then release it. Retry and select the download-mode USB device, which may appear as a new port. Use the recovery guide below if it stays unavailable.';
  if (phase==='preflight') return 'No flash was written by this attempt. Close other serial tools, check the data cable and connect USB before holding the side reset button until the green LED flashes. Reconnect and repeat the checks. Use the recovery guide below if the device stays unavailable.';
  return 'No flash was written by this attempt. Correct the reported problem before trying again. Keep important backups private.';
}
export function createInstaller({baseUrl, requestPort, openConnection, md5, fetchFn=fetch, onBusy=()=>{}, onStatus=()=>{}, onLog=()=>{}, onProgress=()=>{}, onInspect=()=>{}, onDevice=()=>{}, onRecovery=()=>{}}) {
  let busy=false;
  return {
    get busy(){return busy;},
    async run({catalog,mode,recoveryConfirmed=false,installConfirmed=false}) {
      if (busy) return {status:'busy'};
      busy=true;onBusy(true);
      let connection, phase='validate', writeStarted=false;
      onRecovery(null);
      try {
        validateCatalog(catalog);
        if (!['update','install','recovery'].includes(mode)) throw new Error('Unknown install mode.');
        if (mode==='install' && installConfirmed!==true) throw new Error('Confirm first install and backup before continuing.');
        if (mode==='recovery' && recoveryConfirmed!==true) throw new Error('Confirm intentional repair or downgrade and backup before continuing.');
        // The chooser is requested directly during the initiating click.
        phase='chooser';
        const port=await requestPort();
        phase='downloads';
        onStatus('Checking v'+catalog.version+' downloads…');
        const images=await Promise.all(catalog.files.map(file=>verifiedDownload(file,baseUrl,fetchFn)));
        onLog('SHA-256 verified for every firmware file. Selected v'+catalog.version+'.');
        onStatus('Connecting and checking the installed application…');
        phase='connect';
        connection=await openConnection(port);
        // Let esptool select the native USB-Serial/JTAG reset for StickS3.
        // Skipping reset only connects when the user has already entered ROM download mode.
        onLog('Requesting automatic download mode. No flash has been written.');
        await connection.loader.main('default_reset');
        phase='preflight';
        const totalBytes=mode==='install'?images.reduce((sum,f)=>sum+f.size,0):images[3].size;
        const previous=[];
        const result=await flashVerified({loader:connection.loader,catalog,images,mode,recoveryConfirmed,installConfirmed,md5,onInspect,onDevice,onWriteStart:()=>{writeStarted=true;phase='write';},reportProgress:(index,bytes)=>{
          previous[index]=bytes;
          const percent=Math.min(100,previous.reduce((sum,n)=>sum+(n||0),0)/totalBytes*100);
          onProgress(percent);onStatus('Installing v'+catalog.version+' · '+Math.round(percent)+'%. Keep USB connected.');
        }});
        onProgress(100);
        onLog(result.status==='up-to-date'?'Installed v'+catalog.version+' matches the complete archived application. No write needed.':'Application write completed and device MD5 matched.');
        let restarted=false;
        try {await restartK150(connection.transport);restarted=true;} catch { /* Verified bytes; manual restart is sufficient. */ }
        const message=result.status==='up-to-date'?'Already on verified v'+catalog.version+'. No flash was written.':'v'+catalog.version+' installed and verified.';
        onStatus(message+(restarted?' Restarting.':' Press the side Power/Reset button to restart.'),'success');
        return {...result,restarted};
      } catch(error) {
        if (phase==='chooser' && error.name==='NotFoundError') {onStatus('No device selected. You can try again.');return {status:'cancelled'};}
        const advice=recoveryAdvice({phase,mode,code:error.code,writeStarted});
        onStatus(error.message,'error');onLog(error.stack||error.message);
        onRecovery({phase,mode,writeStarted,advice});
        return {status:'error',error,phase,writeStarted,advice};
      } finally {
        if (connection) {try {await connection.transport.disconnect();} catch {}}
        busy=false;onBusy(false);
      }
    }
  };
}
