#!/usr/bin/env python3
"""Replay copied regression seeds and run bounded fail-fast protocol campaigns."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--runs', type=int, default=20000)
parser.add_argument('--seed', type=int, default=8656)
args = parser.parse_args()
if args.runs <= 0:
    parser.error('--runs must be positive')
root = Path(__file__).resolve().parents[4]
build = args.build.resolve()
evidence = build / 'evidence'
evidence.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
results = []
for name, length in [('stun', 8192), ('sdp', 8192), ('sctp_codec', 8192), ('sctp_state', 512),
                     ('turn_codec', 8192), ('turn_state', 512), ('mdns', 8192)]:
    binary = build / ('src/network/fuzz_network_mdns' if name == 'mdns' else f'src/webrtc/fuzz_webrtc_{name}')
    source = root / ('src/network/mdns/fixtures' if name == 'mdns' else
                     'src/webrtc/tests/fixtures' if name == 'sdp' else f'src/webrtc/tests/fixtures/fuzz_{name}')
    corpus = evidence / f'corpus_{name}'
    corpus.mkdir(exist_ok=True)
    for fixture in source.iterdir():
        if fixture.is_file() and (name != 'sdp' or fixture.suffix == '.sdp'):
            shutil.copy(fixture, corpus / fixture.name)
    command = [str(binary), f'-runs={args.runs}', f'-seed={args.seed}', f'-max_len={length}', str(corpus)]
    start = time.monotonic()
    with (evidence / f'fuzz_{name}.log').open('w') as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
    results.append({'target': name, 'command': command, 'exit_code': result.returncode,
                    'wall_seconds': time.monotonic() - start})
    (evidence / 'campaigns.json').write_text(json.dumps(results, indent=2) + '\n')
    print(name, result.returncode, flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
