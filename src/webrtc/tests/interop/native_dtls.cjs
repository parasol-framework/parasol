'use strict';

const { spawn } = require('node:child_process');
const { chromium } = require('playwright');

async function main() {
   const [probePath, chromePath] = process.argv.slice(2);
   if (!probePath || !chromePath) throw new Error('Expected native probe and Chrome paths');
   const browser = await chromium.launch({
      executablePath: chromePath,
      headless: true,
      args: ['--no-sandbox', '--allow-loopback-in-peer-connection',
         '--disable-features=WebRtcHideLocalIpsWithMdns'],
   });
   const page = await browser.newPage();
   const probe = spawn(probePath, [], { stdio: ['pipe', 'pipe', 'pipe'] });
   let output = '';
   let errors = '';
   let waiter = null;
   probe.stdout.setEncoding('utf8');
   probe.stderr.setEncoding('utf8');
   probe.stdout.on('data', (chunk) => {
      output += chunk;
      if (waiter) { waiter(); waiter = null; }
   });
   probe.stderr.on('data', (chunk) => { errors += chunk; });
   const waitFor = async (predicate) => {
      const deadline = Date.now() + 15000;
      while (!predicate()) {
         if (Date.now() >= deadline) throw new Error(`Native probe timed out: ${errors}`);
         await Promise.race([
            new Promise((resolve) => { waiter = resolve; }),
            new Promise((resolve) => setTimeout(resolve, 100)),
         ]);
      }
   };

   try {
      const offer = await page.evaluate(async () => {
         window.pc = new RTCPeerConnection();
         window.pc.createDataChannel('probe');
         const description = await window.pc.createOffer();
         await window.pc.setLocalDescription(description);
         return description.sdp;
      });
      probe.stdin.end(offer);
      await waitFor(() => output.includes('\n'));
      const headerEnd = output.indexOf('\n');
      const answerLength = Number(output.slice(0, headerEnd));
      if (!Number.isInteger(answerLength) || answerLength < 100 || answerLength > 65536) {
         throw new Error(`Invalid native SDP length: ${output.slice(0, 100)} ${errors}`);
      }
      await waitFor(() => output.length >= headerEnd + 1 + answerLength);
      const answer = output.slice(headerEnd + 1, headerEnd + 1 + answerLength);
      await page.evaluate(async (sdp) => {
         await window.pc.setRemoteDescription({ type: 'answer', sdp });
      }, answer);
      await waitFor(() => output.includes('CONNECTED\n', headerEnd + 1 + answerLength));
      const state = await page.evaluate(() => window.pc.iceConnectionState);
      if (state !== 'connected' && state !== 'completed') {
         throw new Error(`Browser ICE state is ${state}; native output: ${output}; errors: ${errors}`);
      }
      const dtlsDeadline = Date.now() + 5000;
      let dtlsConnected = false;
      while (Date.now() < dtlsDeadline && !dtlsConnected) {
         dtlsConnected = await page.evaluate(async () => {
            const stats = await window.pc.getStats();
            return [...stats.values()].some((entry) => entry.type === 'transport' && entry.dtlsState === 'connected');
         });
         if (!dtlsConnected) await new Promise((resolve) => setTimeout(resolve, 100));
      }
      if (!dtlsConnected) throw new Error(`Browser DTLS did not connect; native errors: ${errors}`);
      console.log('Browser and native ICE/DTLS handshake completed');
   } finally {
      probe.kill();
      await page.close();
      await browser.close();
   }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
