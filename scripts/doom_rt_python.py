"""Run a test script in the fork's bundled Python 3.6 without installing another runtime."""
import ctypes
import os
from pathlib import Path
import sys

runtime_dir = Path(__file__).resolve().parents[1] / 'x64' / 'Release'
script_args = sys.argv[1:]
if script_args[:1] == ['--runtime']:
    runtime_dir = Path(script_args[1])
    script_args = script_args[2:]
if not script_args:
    raise SystemExit('Usage: doom_rt_python.py [--runtime DIRECTORY] SCRIPT [ARGUMENTS]')
runtime_dir = runtime_dir.resolve()
python_dll = next(runtime_dir.glob('python3[0-9].dll'))
# Load the Python extension first: it exports RenderDoc's replay marker. Loading
# renderdoc.dll directly would initialise capture hooks in this host process.
runtime = ctypes.PyDLL(str(python_dll))
runtime.Py_SetPath.argtypes = [ctypes.c_wchar_p]
runtime.Py_SetPath(';'.join(str(runtime_dir / name) for name in
                         [python_dll.with_suffix('.zip').name, 'pymodules', '']))
runtime.Py_Initialize()
runtime.PyRun_SimpleString.argtypes = [ctypes.c_char_p]
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
kernel.GetModuleHandleW.restype = ctypes.c_void_p
kernel.GetModuleFileNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint]
original_directory = os.getcwd()
os.chdir(runtime_dir)
try:
    import_result = runtime.PyRun_SimpleString(b'import renderdoc as rd; rd.InitialiseReplay(rd.GlobalEnvironment(), [])')
finally:
    os.chdir(original_directory)
if import_result:
    raise RuntimeError('Bundled replay module could not initialise')
module_path = ctypes.create_unicode_buffer(32768)
kernel.GetModuleFileNameW(kernel.GetModuleHandleW('renderdoc.dll'), module_path, len(module_path))
print('Loaded replay DLL: ' + module_path.value, flush=True)
if Path(module_path.value).resolve() != (runtime_dir / 'renderdoc.dll').resolve():
    raise RuntimeError('Unexpected replay DLL loaded')
script = str(Path(script_args[0]).resolve())
code = "import sys; sys.argv=%r; exec(compile(open(%r).read(), %r, 'exec'))" % (
    script_args, script, script)
result = runtime.PyRun_SimpleString(code.encode('utf8'))
runtime.PyRun_SimpleString(b'import sys; sys.stdout.flush(); sys.stderr.flush()')
runtime.PyRun_SimpleString(b'rd.ShutdownReplay()')
raise SystemExit(0 if result == 0 else 1)
