#!/usr/bin/env python3
"""Real Linux link/address events, confined to a disposable user/network namespace."""
import subprocess
import sys

def run(*args):
    subprocess.run(args, check=True, timeout=10)

if len(sys.argv) == 2:
    run("unshare", "-Urn", sys.executable, __file__, sys.argv[1], "inside")
else:
    run("ip", "link", "set", "lo", "up")
    probe = subprocess.Popen([sys.argv[1], "--watch"], stdout=subprocess.PIPE, text=True)
    try:
        assert probe.stdout.readline().strip() == "READY"
        run("ip", "link", "add", "rtc_fixture", "type", "dummy")
        run("ip", "addr", "add", "192.0.2.1/24", "dev", "rtc_fixture")
        run("ip", "link", "set", "rtc_fixture", "up")
        assert probe.stdout.readline().strip() == "CHANGED"
        run("ip", "link", "del", "rtc_fixture")
        assert probe.wait(timeout=10) == 0
        print("PASS real netlink link/address addition, burst debounce and removal in isolated namespace")
    finally:
        if probe.poll() is None:
            probe.kill()
        probe.wait()
