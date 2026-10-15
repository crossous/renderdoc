"""Bounded RenderDoc target-control capture using the fork's bundled Python module."""
import argparse
import json
import os
import time
import renderdoc as rd

parser = argparse.ArgumentParser()
parser.add_argument('ident', type=int)
parser.add_argument('--seconds', type=int, default=45)
parser.add_argument('--capture', action='store_true')
args = parser.parse_args()

control = rd.CreateTargetControl('', args.ident, 'Codex DOOM RT test', True)
if control is None:
    raise RuntimeError('Target control connection failed')
try:
    print(json.dumps({'target': control.GetTarget(), 'pid': control.GetPID(),
                      'api': control.GetAPI(), 'connected': control.Connected()}), flush=True)
    # A connection first delivers captures that already exist in the target. Drain
    # those before triggering, or an earlier file can look like the new result.
    existing = set()
    initial_deadline = time.monotonic() + 2
    while time.monotonic() < initial_deadline and control.Connected():
        message = control.ReceiveMessage(None)
        if message.type == rd.TargetControlMessageType.NewCapture:
            existing.add(message.newCapture.path)
    if args.capture:
        control.TriggerCapture(1)
        print('Capture requested', flush=True)
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline and control.Connected():
        message = control.ReceiveMessage(None)
        if message.type == rd.TargetControlMessageType.Noop:
            continue
        print('MESSAGE ' + str(message.type), flush=True)
        if message.type == rd.TargetControlMessageType.NewCapture:
            if message.newCapture.path in existing:
                continue
            print(json.dumps({'capture': message.newCapture.path,
                              'frame': message.newCapture.frameNumber,
                              'api': str(message.newCapture.api)}), flush=True)
            break
finally:
    control.Shutdown()
