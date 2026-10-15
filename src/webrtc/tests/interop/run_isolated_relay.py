#!/usr/bin/env python3
"""Real blocked-direct firewall and impaired relay checks, confined to a new user/network namespace."""
import argparse
import errno
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile


def run(command, **options):
    return subprocess.run(command, check=True, timeout=1100, **options)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--outer-net', help=argparse.SUPPRESS)
    parser.add_argument('--outer-user', help=argparse.SUPPRESS)
    parser.add_argument('--mode', choices=['firewall', 'impaired'], required=True)
    parser.add_argument('--nft', type=Path, required=True)
    parser.add_argument('--tc', type=Path, required=True)
    parser.add_argument('--ip', type=Path, default=Path('ip'))
    parser.add_argument('--unshare', type=Path, default=Path('unshare'))
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--turnserver', type=Path, required=True)
    parser.add_argument('--uclient', type=Path, required=True)
    parser.add_argument('--origo', type=Path, required=True)
    parser.add_argument('--public-turn', type=Path, required=True)
    parser.add_argument('--benchmark', type=Path, required=True)
    parser.add_argument('--chrome', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    if not args.inside:
        namespaces = []
        for kind in ('net', 'user'):
            namespaces += ['--outer-' + kind, str(Path(f'/proc/self/ns/{kind}').readlink())]
        run([str(args.unshare), '-Urn', sys.executable, str(Path(__file__).resolve()), '--inside',
             *sys.argv[1:], *namespaces])
        return

    for kind in ('net', 'user'):
        outer = getattr(args, 'outer_' + kind)
        if not outer or str(Path(f'/proc/self/ns/{kind}').readlink()) == outer:
            raise RuntimeError(f'Refusing fixture mutations outside a new {kind} namespace')
    args.evidence.mkdir(parents=True, exist_ok=True)
    run([str(args.ip), 'link', 'set', 'lo', 'up'])
    run([str(args.ip), 'link', 'add', 'rtc_client', 'type', 'veth', 'peer', 'name', 'rtc_relay'])
    run([str(args.ip), 'addr', 'add', '10.203.0.1/24', 'dev', 'rtc_client'])
    run([str(args.ip), 'link', 'set', 'rtc_client', 'up'])
    run([str(args.ip), 'link', 'set', 'rtc_relay', 'up'])
    run([str(args.ip), 'route', 'add', 'default', 'dev', 'rtc_client'])
    # Permit the TURN control/relay legs and mDNS, then block every other UDP datagram, including direct ICE/data.
    rules = '''table inet rtc_fixture {
 chain output {
  type filter hook output priority 0; policy accept;
  udp dport { 34780, 34781, 49160-49179, 5353 } accept
  udp sport { 34780, 34781, 49160-49179, 5353 } accept
  meta l4proto udp counter drop comment "blocked-direct"
 }
}
'''
    run([str(args.nft), '-f', '-'], input=rules, text=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as source, \
         socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as target:
        source.bind(('127.0.0.1', 48000))
        target.bind(('127.0.0.1', 48001))
        target.settimeout(0.2)
        try:
            source.sendto(b'direct-negative-control', target.getsockname())
        except PermissionError as error:
            if error.errno != errno.EPERM:
                raise
        try:
            target.recv(1024)
            raise RuntimeError('Direct UDP passed the firewall negative control')
        except socket.timeout:
            pass
    print('PASS real nftables blocked-direct UDP negative control', flush=True)

    command = [sys.executable, str(args.fixture.resolve()), '--turnserver', str(args.turnserver.resolve()),
               '--uclient', str(args.uclient.resolve()), '--origo', str(args.origo.resolve()),
               '--relay-ip', '10.203.0.1', '--evidence-dir', str(args.evidence.resolve())]
    if args.mode == 'firewall':
        command += ['--public-turn', str(args.public_turn.resolve()), '--browser-turn', str(args.chrome.resolve())]
        run(command)
    else:
        # Scope netem to each actual framework benchmark. Coturn/uclient readiness remains unimpaired, since UDP
        # sanity packets are not retransmitted and are not an acceptance test for reliable framework transfers.
        with tempfile.TemporaryDirectory(prefix='rtc-netem-') as directory:
            launcher = Path(directory) / 'benchmark'
            impairment = [str(args.tc), 'qdisc', 'add', 'dev', 'lo', 'root', 'netem',
                          'delay', '5ms', 'loss', '1%', 'seed', '42']
            launcher.write_text('#!' + sys.executable + '\n' + f'''
import json, subprocess, sys
from pathlib import Path
subprocess.run({impairment!r}, check=True)
try:
    result = subprocess.run([{str(args.benchmark.resolve())!r}, *sys.argv[1:],
                            '--bytes=1048576', '--timeout=300'], timeout=330)
    counters = subprocess.check_output({[str(args.tc), '-j', '-s', 'qdisc', 'show', 'dev', 'lo']!r}, text=True)
    print('NETEM_COUNTERS: ' + counters, flush=True)
    records = json.loads(counters)
    if not any(record.get('kind') == 'netem' and record.get('drops', 0) > 0 for record in records):
        raise RuntimeError('No actual kernel packet loss was observed')
    sys.exit(result.returncode)
finally:
    subprocess.run({[str(args.tc), 'qdisc', 'del', 'dev', 'lo', 'root']!r}, check=True)
''')
            launcher.chmod(0o700)
            command += ['--channel-bench', str(launcher), '--benchmarks-only', '--skip-direct-benchmark',
                        '--benchmark-timeout', '350']
            run(command)
    counters = run([str(args.nft), '-j', 'list', 'table', 'inet', 'rtc_fixture'], capture_output=True, text=True)
    (args.evidence / 'firewall.json').write_text(counters.stdout)
    table = json.loads(counters.stdout)
    blocked = [entry['rule'] for entry in table['nftables'] if 'rule' in entry and
               entry['rule'].get('comment') == 'blocked-direct']
    if not blocked or not any(expression.get('counter', {}).get('packets', 0) > 0
                              for expression in blocked[0]['expr']):
        raise RuntimeError('The firewall did not record blocked direct datagrams')
    (args.evidence / 'topology.json').write_text(json.dumps({
        'scope': 'disposable user/network namespace; no host network changes',
        'relay_address': '10.203.0.1', 'direct_udp': 'blocked with negative control and kernel counter',
        'benchmark_payload_bytes': 1048576 if args.mode == 'impaired' else None,
        'impairment': {'device': 'lo', 'delay_ms': 5, 'loss_percent': 1, 'seed': 42}
        if args.mode == 'impaired' else None}, indent=2) + '\n')
    print(f'PASS isolated {args.mode} relay fixture', flush=True)


if __name__ == '__main__':
    main()
