'use strict';
const { spawn } = require('node:child_process');
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const { pathToFileURL } = require('node:url');
const args = ['--no-sandbox', '--allow-loopback-in-peer-connection',
   '--disable-features=WebRtcHideLocalIpsWithMdns'];

async function ready(port, child) {
   const deadline = Date.now() + 15000;
   while (Date.now() < deadline) {
      if (child.exitCode !== null || child.signalCode !== null) throw new Error('Signalling server exited');
      if (await new Promise(resolve => {
         const socket = net.connect(port, '127.0.0.1');
         socket.on('connect', () => { socket.destroy(); resolve(true); });
         socket.on('error', () => resolve(false));
      })) return;
      await new Promise(resolve => setTimeout(resolve, 100));
   }
   throw new Error('Signalling startup timed out');
}
async function exercise({ port, nativeOffer }) {
   const pc = new RTCPeerConnection({ iceServers: [] });
   const ws = new WebSocket(`ws://127.0.0.1:${port}/`);
   const channels = new Map(), failures = [], closed = new Set();
   const rtts = [], bulkRtts = [];
   let signalling = Promise.resolve();
   const pendingCandidates = [];
   let greeting = false, nativeBuffer = null, bulkReceived = 0;
   let nativeStats = null, pendingPayload = null;
   const send = message => ws.send(JSON.stringify(message));
   const wait = async (condition, label) => {
      const deadline = performance.now() + 25000;
      while (!condition()) {
         if (failures.length) throw new Error(failures.join('; '));
         if (performance.now() > deadline) throw new Error(`Timed out: ${label}, ` +
            `bulk replies=${channels.get("negotiated")?.replies.length}, ` +
            `buffered=${channels.get("negotiated")?.channel.bufferedAmount}`);
         await new Promise(resolve => setTimeout(resolve, 5));
      }
   };
   function setup(channel) {
      channels.set(channel.label, { channel, replies: [] });
      channel.binaryType = 'arraybuffer';
      channel.onmessage = ({ data }) => {
         if (channel.label === 'chat' && data === 'Hello from native Kōtuku') greeting = true;
         else {
            const entry = channels.get(channel.label);
            entry.replies.push(data);
            if (channel.label === 'negotiated' && entry.replies.length === 8) bulkReceived = performance.now();
         }
      };
      channel.onclose = () => closed.add(channel.label);
      channel.onerror = () => failures.push(`Channel error: ${channel.label}`);
      if (channel.label.startsWith('native-')) {
         const mode = Number(channel.label.slice(7));
         if (channel.ordered !== (mode < 3) || channel.protocol !== 'echo' ||
             channel.maxRetransmits !== (mode % 3 === 1 ? 0 : null) ||
             channel.maxPacketLifeTime !== (mode % 3 === 2 ? 5000 : null)) failures.push('DCEP options differ');
      }
   }
   pc.ondatachannel = ({ channel }) => setup(channel);
   for (let mode = 0; mode < 6; ++mode) {
      const options = { ordered: mode < 3, protocol: 'echo' };
      if (mode % 3 === 1) options.maxRetransmits = 0;
      if (mode % 3 === 2) options.maxPacketLifeTime = 5000;
      setup(pc.createDataChannel(`browser-${mode}`, options));
   }
   setup(pc.createDataChannel('negotiated', { negotiated: true, id: 100 }));
   pc.onicecandidate = ({ candidate }) => {
      if (candidate) send({ type: 'candidate', candidate: candidate.candidate });
   };
   ws.onmessage = ({ data }) => {
      signalling = signalling.then(async () => {
         const message = JSON.parse(data);
         if (message.type === 'ready') {
            await pc.setLocalDescription(await pc.createOffer());
            send({ type: 'offer', sdp: pc.localDescription.sdp });
         } else if (message.type === 'offer') {
            await pc.setRemoteDescription({ type: 'offer', sdp: message.sdp });
            await pc.setLocalDescription(await pc.createAnswer());
            send({ type: 'answer', sdp: pc.localDescription.sdp });
         } else if (message.type === 'answer') {
            await pc.setRemoteDescription({ type: 'answer', sdp: message.sdp });
         } else if (message.type === 'candidate') {
            const candidate = { candidate: message.candidate, sdpMid: '0', sdpMLineIndex: 0 };
            if (pc.remoteDescription) await pc.addIceCandidate(candidate);
            else pendingCandidates.push(candidate);
         } else if (message.type === 'buffer-report') nativeBuffer = message;
         else if (message.type === 'stats') nativeStats = message;
         else if (message.type === 'error') failures.push(message.message);
         if (pc.remoteDescription) {
            for (const candidate of pendingCandidates.splice(0)) await pc.addIceCandidate(candidate);
         }
      }).catch(error => failures.push(error.message));
   };
   try {
      await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
      send({ type: 'join', nativeOffer });
      await wait(() => channels.size === 14 && [...channels.values()].every(x => x.channel.readyState === 'open'),
         '14 public channels open');
      await wait(() => greeting, 'native greeting');
      // Complete candidate exchange before asserting exact delivery on zero-retransmission channels.
      // Route migration has separate coverage; an open channel alone does not mean ICE selection has finished.
      await wait(() => pc.iceGatheringState === 'complete', 'browser gathering complete');
      send({ type: 'candidate', candidate: '' });
      const gatheringDeadline = performance.now() + 25000;
      do {
         nativeStats = null;
         send({ type: 'stats' });
         await wait(() => nativeStats, 'native gathering report');
         if (nativeStats.gatheringComplete) break;
         if (performance.now() > gatheringDeadline) throw new Error('Native gathering did not complete');
         await new Promise(resolve => setTimeout(resolve, 50));
      } while (true);
      await pc.addIceCandidate(null);
      // Chromium can keep ICE "connected" while checking unused interfaces after end-of-candidates.
      // Require a stable, validated selected pair instead of waiting for those unused checks to expire.
      let selectedPair = null, stableSince = performance.now();
      const selectionDeadline = performance.now() + 25000;
      while (true) {
         const stats = await pc.getStats();
         const transport = [...stats.values()].find(value => value.type === 'transport');
         const selected = transport?.selectedCandidatePairId;
         if (selected !== selectedPair) { selectedPair = selected; stableSince = performance.now(); }
         if (selected && stats.get(selected)?.state === 'succeeded' && performance.now() - stableSince >= 1000) break;
         if (performance.now() > selectionDeadline) throw new Error('Selected ICE pair did not settle');
         await new Promise(resolve => setTimeout(resolve, 50));
      }
      if (channels.get('negotiated').channel.id !== 100) throw new Error('Negotiated ID changed');
      const binary = new Uint8Array([0, 255, 128, 1, 0]);
      for (const [label, entry] of channels) {
         for (const payload of ['hello € \0', binary, '', new ArrayBuffer(0)]) {
            pendingPayload = { label, type: typeof payload === 'string' ? 'text' : 'binary',
               bytes: typeof payload === 'string' ? new TextEncoder().encode(payload).length : payload.byteLength };
            entry.channel.send(payload);
            await wait(() => entry.replies.length > 0, `${label} echo`);
            const received = entry.replies.shift();
            if (typeof payload === 'string' ? received !== payload :
                !(received instanceof ArrayBuffer) || String([...new Uint8Array(received)]) !==
                String([...new Uint8Array(payload.buffer || payload)])) throw new Error(`${label}: bytes differ`);
         }
      }
      const settledStats = [...(await pc.getStats()).values()];
      if (settledStats.find(value => value.type === 'transport')?.selectedCandidatePairId !== selectedPair) {
         throw new Error('Selected ICE pair changed during steady-state payload checks');
      }
      // Same public path supplies repeatable RTT/bulk evidence. No hardware-dependent speed assertion.
      const ping = channels.get('browser-0'), bulk = channels.get('negotiated');
      for (let i = 0; i < 100; ++i) {
         const start = performance.now(); ping.channel.send(`ping-${i}`);
         await wait(() => ping.replies.length, 'RTT echo');
         if (ping.replies.shift() !== `ping-${i}`) throw new Error('RTT payload differs');
         rtts.push(performance.now() - start);
      }
      const size = Math.min(pc.sctp.maxMessageSize, 262144);
      const payload = Uint8Array.from({ length: size }, (_, i) => (i * 13 + Math.floor(i / 113)) % 256);
      const bulkStart = performance.now();
      let highWater = 0, lowEvents = 0;
      bulk.channel.bufferedAmountLowThreshold = 65536;
      bulk.channel.onbufferedamountlow = () => lowEvents++;
      for (let i = 0; i < 8; ++i) {
         bulk.channel.send(payload);
         highWater = Math.max(highWater, bulk.channel.bufferedAmount);
      }
      for (let i = 0; i < 10; ++i) {
         const start = performance.now(); ping.channel.send(`during-${i}`);
         await wait(() => ping.replies.length, 'small channel during bulk');
         if (ping.replies.shift() !== `during-${i}`) throw new Error('Concurrent ping differs');
         bulkRtts.push(performance.now() - start);
      }
      await wait(() => bulk.replies.length === 8 && bulk.channel.bufferedAmount === 0, 'bulk drain');
      const elapsed = bulkReceived - bulkStart;
      for (const received of bulk.replies) {
         if (!(received instanceof ArrayBuffer) || received.byteLength !== size ||
             new Uint8Array(received).some((value, index) => value !== payload[index])) {
            throw new Error('Near-limit bulk bytes differ');
         }
      }
      if (highWater <= 65536 || lowEvents < 1) throw new Error('Back-pressure high/low crossing was not exercised');
      send({ type: 'buffer-report' });
      await wait(() => nativeBuffer, 'native buffering report');
      if (nativeBuffer.highWater < size || nativeBuffer.lowEvents < 1) {
         throw new Error('Native buffering not exercised');
      }
      for (const [label, entry] of channels) {
         if (label.startsWith('native-') || label === 'chat') send({ type: 'close-channel', label });
         else entry.channel.close();
      }
      await wait(() => closed.size === 14, 'both closure directions');
      const percentile = (values, p) => [...values].sort((a, b) => a - b)[Math.ceil(values.length * p) - 1];
      const summary = values => ({ samples: values.length, p50_ms: percentile(values, .5),
         p95_ms: percentile(values, .95), p99_ms: percentile(values, .99) });
      const stats = [...(await pc.getStats()).values()];
      return { nativeOffer, channels: channels.size, closed: closed.size, rtt: summary(rtts),
         concurrent_rtt: summary(bulkRtts), bulk_bytes: size * 8, echo_goodput_mib_s: size * 8 / elapsed / 1048.576,
         browser_queue_high_water: highWater, browser_low_events: lowEvents,
         native_buffer: nativeBuffer,
         conditions: { one_way_delay_ms: 0, injected_loss: 0, transport: 'localhost ICE/DTLS/SCTP' },
         stats, limitations: ['RTT polling has a 5 ms floor', 'Browser SCTP retransmissions unavailable in getStats',
            'Bulk goodput includes the native echo return path', 'I-DATA interleaving unsupported'] };
   } catch (error) {
      const stats = [...(await pc.getStats()).values()].filter(value =>
         ['transport', 'candidate-pair', 'data-channel', 'sctp-transport', 'local-candidate',
            'remote-candidate'].includes(value.type));
      if (ws.readyState === WebSocket.OPEN) {
         nativeStats = null;
         send({ type: 'stats' });
         const deadline = performance.now() + 1000;
         while (!nativeStats && performance.now() < deadline) await new Promise(resolve => setTimeout(resolve, 10));
      }
      const diagnostics = JSON.parse(JSON.stringify({ nativeOffer, pendingPayload, browser: stats, native: nativeStats,
         connectionState: pc.connectionState, iceState: pc.iceConnectionState,
         channels: [...channels].map(([label, entry]) => ({ label, state: entry.channel.readyState,
            buffered: entry.channel.bufferedAmount, replies: entry.replies.length })) },
         (key, value) => /address|credential|username|password/i.test(key) || key === 'ip' ? '<redacted>' : value));
      throw new Error(`${error.message}\nDiagnostics: ${JSON.stringify(diagnostics)}`);
   } finally { pc.close(); ws.close(); }
}
async function main() {
   const [origo, script, chrome, output, mode = 'interop', policy = 'NO_MDNS'] = process.argv.slice(2);
   const reservation = net.createServer();
   await new Promise((resolve, reject) => {
      reservation.once('error', reject); reservation.listen(0, '127.0.0.1', resolve);
   });
   const port = reservation.address().port;
   await new Promise(resolve => reservation.close(resolve));
   const nativePolicy = policy === 'MDNS' ? '' : policy;
   const server = spawn(origo, [script, `port=${port}`, `policy=${nativePolicy}`, '--log-warning'],
      { stdio: ['ignore', 'pipe', 'pipe'] });
   let errors = '';
   server.stdout.resume();
   server.stderr.on('data', chunk => { errors = (errors + chunk).slice(-32000); });
   let browser;
   const cpuStart = process.cpuUsage();
   try {
      await ready(port, server);
      browser = await chromium.launch({ executablePath: chrome, headless: true,
         args: policy === 'NO_MDNS' ? args : ['--no-sandbox'] });
      const results = [];
      for (const nativeOffer of [false, true]) {
         const page = await browser.newPage();
         try { results.push(await page.evaluate(exercise, { port, nativeOffer })); }
         finally { await page.close(); }
      }
      // Drive the shipped HTML, including reconnection cleanup, with both offer directions.
      const demo = await browser.newPage();
      try {
         await demo.goto(pathToFileURL(path.join(path.dirname(script), 'index.html')).href);
         await demo.locator('#url').fill(`ws://127.0.0.1:${port}/`);
         for (const nativeOffer of [false, true]) {
            await demo.locator('#native').setChecked(nativeOffer);
            await demo.locator('#connect').click();
            await demo.waitForFunction(() => document.getElementById('channel').textContent === 'open');
            await demo.locator('#text').fill(`demo-${nativeOffer}`);
            await demo.locator('#send').click();
            await demo.waitForFunction(text =>
               document.getElementById('log').textContent.includes('Native: ' + text),
               `demo-${nativeOffer}`);
            await demo.locator('#disconnect').click();
            await demo.waitForFunction(() => document.getElementById('signal').textContent === 'disconnected');
         }
      } finally { await demo.close(); }
      const report = { candidate_policy: nativePolicy || 'mdns', demo_offers: ['browser', 'native'],
         browser: browser.version(),
         playwright: require('playwright/package.json').version,
         harness_cpu_us: process.cpuUsage(cpuStart), results,
         cpu_scope: 'Node harness only; native/browser process CPU is recorded separately by run_bench.py' };
      fs.mkdirSync(path.dirname(output), { recursive: true });
      fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
      console.log('Public channels: both offers, both opens/closes, exact text/binary/empty/near-limit bulk; ' +
         output);
   } catch (error) {
      fs.mkdirSync(path.dirname(output), { recursive: true });
      fs.writeFileSync(output + `.failure.${Date.now()}.json`, JSON.stringify({ error: error.message, nativeLog: errors,
         nativeExitCode: server.exitCode, nativeSignal: server.signalCode }, null, 2) + '\n');
      throw new Error(`${error.stack}\nNative log:\n${errors}`);
   }
   finally {
      if (browser) await browser.close();
      server.kill('SIGTERM');
      await new Promise(resolve => {
         if (server.exitCode !== null || server.signalCode !== null) resolve();
         else server.once('exit', resolve);
      });
   }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
