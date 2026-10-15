"""Read capability flags for the locally installed DOOM executable; never patch the process."""
import argparse
import ctypes
import ctypes.wintypes as w
import hashlib
import json
import struct

parser = argparse.ArgumentParser()
parser.add_argument('pid', type=int)
parser.add_argument('base', type=lambda value: int(value, 0))
parser.add_argument('--exe', required=True)
args = parser.parse_args()

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel.CloseHandle.argtypes = [w.HANDLE]
handle = kernel.OpenProcess(0x1010, False, args.pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, length):
    buffer = ctypes.create_string_buffer(length)
    actual = ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, address, buffer, length, ctypes.byref(actual)):
        raise ctypes.WinError(ctypes.get_last_error())
    return buffer.raw[:actual.value]

try:
    # These offsets were verified by disassembling this executable's capability checks.
    # Require the exact executable hash before using them with another installation.
    with open(args.exe, 'rb') as source:
        executable_hash = hashlib.file_digest(source, 'sha256').hexdigest()
    if executable_hash != 'b7aece5fe2842035efd9036a1049edd53ee5efa25564811e20c39ca18362c018':
        raise RuntimeError('Executable changed; re-verify capability offsets before probing')
    result = {'pid': args.pid, 'base': hex(args.base), 'exe_sha256': executable_hash}
    result['flags'] = {
        name: read(args.base + offset, 1)[0]
        for name, offset in [('raytracing_support', 0x494be97),
                             ('pathtracing_support', 0x494be99),
                             ('nv_mesh_shader', 0x494bedb),
                             ('nv_invocation_reorder', 0x494bedd),
                             ('opacity_micromap', 0x494be9f)]
    }
    result['cvars'] = {}
    for name, offset in [('r_allowPathtracing', 0x989c860),
                         ('r_pathtracingEnabled', 0x9831e10),
                         ('r_allowRayTracingOnOlderGPUs', 0x989c7e0),
                         ('r_omms', 0x97face0),
                         ('r_irradianceVolume', 0x97eb9c0),
                         ('r_enableRenderDocCapture', 0x98a3270),
                         ('comForceCommandLineCVars', 0xa8d9cd0)]:
        pointer = struct.unpack('<Q', read(args.base + offset, 8))[0]
        result['cvars'][name] = struct.unpack('<i', read(pointer + 8, 4))[0]
    print(json.dumps(result, indent=2))
finally:
    kernel.CloseHandle(handle)
