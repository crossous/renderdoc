"""Report recorded RT calls, replay a capture, and export its final presentation image."""
import argparse
import collections
import json
import os
import shutil
import time
import renderdoc as rd

parser = argparse.ArgumentParser()
parser.add_argument('capture')
parser.add_argument('output')
parser.add_argument('--structured-only', action='store_true')
parser.add_argument('--validate', action='store_true')
parser.add_argument('--single-submit', action='store_true')
parser.add_argument('--restore-as-creation-order', action='store_true',
                    help='Enable the optional AS backing-storage replay compatibility setting')
parser.add_argument('--verify-rt-events', action='store_true',
                    help='Replay each TraceRays event separately and require at least one')
args = parser.parse_args()
os.makedirs(args.output, exist_ok=True)
rd.SetDebugLogFile(os.path.abspath(os.path.join(args.output, 'renderdoc.log')))
single_submit_setting = rd.SetConfigSetting('Vulkan_Debug_SingleSubmitFlushing')
original_single_submit = single_submit_setting.data.basic.b
single_submit_setting.data.basic.b = args.single_submit
as_order_setting = rd.SetConfigSetting('Vulkan_Hack_RestoreASInCreationOrder')
original_as_order = as_order_setting.data.basic.b
as_order_setting.data.basic.b = args.restore_as_creation_order
report = {'capture': os.path.abspath(args.capture), 'complete': False,
          'single_submit': args.single_submit, 'api_validation': args.validate,
          'restore_as_creation_order': args.restore_as_creation_order,
          'original_as_creation_order': original_as_order}

def save_report():
    with open(os.path.join(args.output, 'replay-report.json'), 'w') as f:
        json.dump(report, f, indent=2)

def flatten(actions):
    for action in actions:
        yield action
        for child in flatten(action.children):
            yield child

cap = rd.OpenCaptureFile()
controller = None
try:
    status = cap.OpenFile(args.capture, 'rdc', None)
    report['open_file'] = str(status)
    if status != rd.ResultCode.Succeeded:
        raise RuntimeError('OpenFile failed: ' + str(status))
    start = time.monotonic()
    sd = cap.GetStructuredData()
    counts = collections.Counter(chunk.name for chunk in sd.chunks)
    report['structured_seconds'] = time.monotonic() - start
    report['chunk_counts'] = dict(counts)
    report['rt_chunk_counts'] = {
        name: count for name, count in counts.items()
        if any(part in name for part in ('AccelerationStructure', 'RayTracing', 'TraceRays'))
    }
    save_report()
    print(json.dumps(report['rt_chunk_counts']), flush=True)
    if args.structured_only:
        raise SystemExit()
    del sd
    print('Opening replay', flush=True)
    start = time.monotonic()
    options = rd.ReplayOptions()
    options.apiValidation = args.validate
    status, controller = cap.OpenCapture(options, None)
    report['open_replay'] = str(status)
    report['open_replay_seconds'] = time.monotonic() - start
    save_report()
    print('Replay result: ' + str(status), flush=True)
    if status != rd.ResultCode.Succeeded or controller is None:
        raise RuntimeError('Replay failed: ' + str(status))
    actions = list(flatten(controller.GetRootActions()))
    report['action_count'] = len(actions)
    report['resource_count'] = len(controller.GetResources())
    report['rt_actions'] = [
        {'event': action.eventId, 'dimensions': list(action.dispatchDimension)}
        for action in actions if action.flags & rd.ActionFlags.DispatchRay
    ]
    if args.verify_rt_events:
        if not report['rt_actions']:
            raise RuntimeError('Replay contains no ray tracing dispatch actions')
        for action in report['rt_actions']:
            controller.SetFrameEvent(action['event'], True)
            status = controller.GetFatalErrorStatus()
            if status != rd.ResultCode.Succeeded:
                raise RuntimeError('Ray tracing event failed: ' + str(status))
        report['rt_events_replayed'] = len(report['rt_actions'])
    present = next((a for a in reversed(actions) if a.flags & rd.ActionFlags.Present), None)
    if present is None:
        raise RuntimeError('Capture has no present action')
    report['present_event'] = present.eventId
    controller.SetFrameEvent(present.eventId, True)
    texture = rd.TextureSave()
    texture.resourceId = present.copyDestination
    texture.destType = rd.FileType.PNG
    filename = os.path.join(args.output, 'replayed-present.png')
    result = controller.SaveTexture(texture, filename)
    report['save_texture'] = str(result)
    report['present_resource'] = str(present.copyDestination)
    save_report()
    if result != rd.ResultCode.Succeeded:
        raise RuntimeError('Presentation export failed: ' + str(result))
    status = controller.GetFatalErrorStatus()
    if status != rd.ResultCode.Succeeded:
        raise RuntimeError('Presentation replay failed: ' + str(status))
    report['complete'] = True
    save_report()
    print(json.dumps({key: value for key, value in report.items() if key not in ('chunk_counts',)}), flush=True)
except Exception as error:
    report['error'] = str(error)
    save_report()
    raise
finally:
    single_submit_setting.data.basic.b = original_single_submit
    as_order_setting.data.basic.b = original_as_order
    log = os.path.join(args.output, 'renderdoc.log')
    if os.path.isfile(log):
        shutil.copyfile(log, log + '.retained')
    if controller is not None:
        controller.Shutdown()
    cap.Shutdown()
