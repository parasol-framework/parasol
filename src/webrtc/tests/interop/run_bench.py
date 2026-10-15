#!/usr/bin/env python3
"""Record repeatable local CTest benchmarks and aggregate child CPU/RSS on Linux."""
import argparse
import json
from pathlib import Path
import resource
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('--build', default='build/agents')
parser.add_argument('--output', default='build/agents/webrtc_bench')
options = parser.parse_args()
output = Path(options.output)
output.mkdir(parents=True, exist_ok=True)
results = []
for name in ('webrtc_channels_bench', 'webrtc_public_channels_bench'):
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.monotonic()
    command = ['ctest', '--build-config', 'Debug', '--test-dir', options.build,
               '--output-on-failure', '-V', '-R', '^' + name + '$', '--no-tests=error']
    with (output / (name + '.log')).open('w') as log:
        run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    results.append({'test': name, 'command': command, 'exit_code': run.returncode,
                    'wall_seconds': time.monotonic() - start,
                    'child_user_seconds': after.ru_utime - before.ru_utime,
                    'child_system_seconds': after.ru_stime - before.ru_stime,
                    'cumulative_child_peak_rss_kib': after.ru_maxrss})
    print(name, run.returncode, flush=True)
    if run.returncode:
        break
(output / 'process_usage.json').write_text(json.dumps({
    'cpu_scope': 'CTest and reaped descendants, including native server and Chromium',
    'rss_scope': 'cumulative maximum child RSS, not simultaneous aggregate memory',
    'results': results}, indent=2) + '\n')
raise SystemExit(any(item['exit_code'] for item in results))
