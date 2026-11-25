"""Count captured heap properties without GPU execution; do not copy large buffer blobs."""
import collections
import json
import sys
import renderdoc as rd

capture = rd.OpenCaptureFile()
try:
    status = capture.OpenFile(sys.argv[1], 'rdc', None)
    if status != rd.ResultCode.Succeeded:
        raise RuntimeError(str(status))
    data = capture.GetStructuredData()
    types = collections.Counter()
    custom = collections.Counter()
    for chunk in data.chunks:
        properties = chunk.FindChild('pHeapProperties')
        if not properties:
            desc = chunk.FindChild('pDesc')
            if desc and 'CreateHeap' in chunk.name:
                properties = desc.FindChild('Properties')
        if not properties:
            continue
        heap_type = properties.FindChild('Type')
        if heap_type:
            value = heap_type.AsInt()
            types[value] += 1
            if value == 4:
                page = properties.FindChild('CPUPageProperty').AsInt()
                pool = properties.FindChild('MemoryPoolPreference').AsInt()
                custom['page=%s pool=%s' % (page, pool)] += 1
    report = dict(capture=sys.argv[1], heap_types=dict(types), custom_heaps=dict(custom),
                  gpu_upload_count=types[5], custom_write_combine_l1_count=custom['page=2 pool=2'])
    with open(sys.argv[2], 'w') as output:
        json.dump(report, output, indent=2)
    print(json.dumps(report), flush=True)
finally:
    capture.Shutdown()
