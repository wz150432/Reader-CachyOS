#!/usr/bin/env python3
"""Measure control-process startup up to IPC delivery; never controls a real Reader."""
import argparse
import json
import os
from pathlib import Path
import socket
import statistics
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('--via-niri', action='store_true', help='include Niri spawn dispatch')
parser.add_argument('--runs', type=int, default=10)
args = parser.parse_args()
if args.runs < 1:
    parser.error('--runs must be positive')
binary = str(args.binary.resolve())

with tempfile.TemporaryDirectory(prefix='reader-shortcut-benchmark-') as runtime:
    environment = os.environ.copy()
    # Keep access to the real compositor when the old command initializes Qt GUI.
    if environment.get('WAYLAND_DISPLAY'):
        environment['WAYLAND_DISPLAY'] = os.path.join(
            environment.get('XDG_RUNTIME_DIR', ''), environment['WAYLAND_DISPLAY'])
    environment['XDG_RUNTIME_DIR'] = runtime
    samples = []
    with socket.socket(socket.AF_UNIX) as server:
        server.bind(str(Path(runtime) / 'reader-control.sock'))
        server.listen()
        server.settimeout(5)
        for _ in range(args.runs):
            command = [binary, '--toggle-hide']
            if args.via_niri:
                # env is invoked directly, without shell interpolation.
                command = ['niri', 'msg', 'action', 'spawn', '--', 'env',
                           f'XDG_RUNTIME_DIR={runtime}',
                           f'WAYLAND_DISPLAY={environment.get("WAYLAND_DISPLAY", "")}',
                           *command]
            started = time.perf_counter()
            process = subprocess.Popen(command, env=environment,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                with server.accept()[0] as connection:
                    connection.settimeout(2)
                    received = b''
                    while True:
                        chunk = connection.recv(100)
                        if not chunk:
                            break
                        received += chunk
                    samples.append((time.perf_counter() - started) * 1000)
                    if received != b'toggle-hide':
                        raise RuntimeError(f'unexpected command: {received!r}')
                if process.wait(timeout=3) != 0:
                    raise RuntimeError('control command failed')
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
    print(json.dumps({'binary': binary, 'via_niri': args.via_niri,
                      'runs': len(samples), 'median_ms': round(statistics.median(samples), 2),
                      'samples_ms': [round(value, 2) for value in samples]}))
