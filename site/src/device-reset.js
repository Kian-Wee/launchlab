const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));

// StickS3 K150 uses the ESP32-S3 USB Serial/JTAG interface. Pulse EN via
// RTS with GPIO0 released; deasserting RTS alone leaves the flasher stub running.
export async function restartK150(transport,wait=sleep) {
  await transport.setDTR(false);
  await transport.setRTS(true);
  try {await wait(200);} finally {await transport.setRTS(false);}
  await wait(200);
}
