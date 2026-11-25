"""Replay the standalone mapped-upload regression and compare GPU-produced integers."""
import json
import os
import shutil
import struct
import sys
import renderdoc as rd

out = os.path.abspath(sys.argv[2])
os.makedirs(out, exist_ok=True)
log = os.path.join(out, 'renderdoc.log')
rd.SetDebugLogFile(log)
rd.SetConfigSetting('D3D12_Replay_PreserveUploadHeaps').data.basic.b = '--preserve-upload' in sys.argv
rd.SetConfigSetting('D3D12_Replay_SkipDiscardPattern').data.basic.b = False
if '--sdk' in sys.argv:
    rd.SetConfigSetting('D3D12_D3D12CoreDirPath').data.str = os.path.abspath(sys.argv[sys.argv.index('--sdk') + 1])
cap = rd.OpenCaptureFile()
controller = None
try:
    assert cap.OpenFile(sys.argv[1], 'rdc', None) == rd.ResultCode.Succeeded
    options = rd.ReplayOptions()
    options.apiValidation = '--validate' in sys.argv
    status, controller = cap.OpenCapture(options, None)
    assert status == rd.ResultCode.Succeeded, str(status)
    resources = controller.GetResources()
    sd = controller.GetStructuredFile()
    write_stats = {}
    for chunk in sd.chunks:
        data = chunk.FindChild('MappedData')
        resource = chunk.FindChild('Resource')
        if data and resource:
            write_stats.setdefault(str(resource.AsInt()), []).append(data.type.byteSize)
    output = next(r.resourceId for r in resources if r.name == 'UploadRegressionOutput')
    def flatten(actions):
        for action in actions:
            yield action
            yield from flatten(action.children)
    actions = list(flatten(controller.GetRootActions()))
    final = max(a.eventId for a in actions)
    expected_path = sys.argv[1] + '.expected.bin'
    with open(expected_path, 'rb') as stream:
        raw = stream.read()
    if not raw or len(raw) % 4:
        raise RuntimeError('Expected native output must contain complete DWORDs')
    expected = list(struct.unpack('<%dI' % (len(raw) // 4), raw))
    discard_checks = []
    for resource in resources:
        if not resource.name.startswith('DiscardRegression_'):
            continue
        size = int(resource.name.split('_')[-1])
        event = next(a.eventId for a in actions if a.flags & rd.ActionFlags.Clear and
                     a.copyDestination == resource.resourceId)
        controller.SetFrameEvent(event, True)
        data = bytes(controller.GetBufferData(resource.resourceId, 0, size))
        pattern = struct.pack('<I', 0xD15CAD3D)
        wanted = (pattern * ((size + 3) // 4))[:size]
        discard_checks.append(dict(name=resource.name, event=event, size=size, passed=data == wanted))
    repeats = []
    for event in (final, actions[len(actions)//2].eventId, final):
        controller.SetFrameEvent(event, True)
        assert controller.GetFatalErrorStatus() == rd.ResultCode.Succeeded
        if event == final:
            values = list(struct.unpack('<%dI' % len(expected),
                          bytes(controller.GetBufferData(output, 0, len(expected) * 4))))
            repeats.append(values)
    report = dict(expected=expected, replayed=repeats,
                  passed=all(x == expected for x in repeats) and all(x['passed'] for x in discard_checks),
                  preserve_upload='--preserve-upload' in sys.argv)
    report['mapped_write_sizes'] = write_stats
    report['discard_checks'] = discard_checks
    report['validation'] = options.apiValidation
    messages = controller.GetDebugMessages()
    report['debug_messages'] = [dict(event=m.eventId, severity=str(m.severity),
                                     description=m.description) for m in messages]
    report['passed'] &= not any(m.severity == rd.MessageSeverity.High for m in messages)
    json.dump(report, open(os.path.join(out, 'result.json'), 'w'), indent=2)
    print(json.dumps(report), flush=True)
    if not report['passed']:
        raise RuntimeError('Captured GPU output does not match the native GPU output')
finally:
    if controller: controller.Shutdown()
    cap.Shutdown()
    if os.path.exists(log): shutil.copyfile(log, log + '.retained')
