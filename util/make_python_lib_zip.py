from pathlib import Path
import sys
import os
import tempfile
import zipfile
import py_compile

#################################################
# Script to generate a compatible pythonXY.zip for a python version as
# RenderDoc expects to distribute the standard library.
# 
# Normally the 'embeddable package' includes this, but not every version
# of python has that - some versions are provided as source only if they
# are security-only releases.
# 
# These are not generated bitwise identically to the python.org zips for
# unknown reasons, more than just a timestamp/checksum.
#
# If you have an installed version of Python in PythonXY/ then
# running this like:
#
# PythonXY/python make_python_lib_zip.py
#
# Will set up everything RenderDoc expects for building against that
# version of python, expecting to write a pythonXY.zip next to that
# executable (this will likely not work on linux, but this script is
# generally only relevant on windows as linux distributions ship the
# plain python scripts standard library)

def usage():
    print(f"Usage: {sys.argv[0]} [PythonXY/Lib/] [pythonXY.zip]")
    print("       If zip is omitted, will write next to python binary.")
    print("       If Lib/ is omitted, will use first sys.path folder.")
    sys.exit(1)

if len(sys.argv) > 3:
    usage()

if len(sys.argv) >= 3:
    outpath = os.path.abspath(sys.argv[2])
else:
    ver = sys.version_info
    outpath = os.path.join(os.path.dirname(sys.executable), f"python{ver.major}{ver.minor}.zip")

if len(sys.argv) >= 2:
    inpath = os.path.abspath(sys.argv[1])
else:
    dirpaths = [x for x in sys.path if os.path.isdir(x) and os.path.exists(os.path.join(x, 'os.py'))]

    if len(dirpaths) == 0:
        print(f"Couldn't determine standard library location from {sys.path}")
        usage()

    inpath = dirpaths[0]

if not os.path.exists(os.path.join(inpath, 'abc.py')):
    print(f"Input path '{inpath}' must be a python Lib/ folder with standard .py files like os.py")
    usage()

if not outpath.endswith('.zip'):
    print(f"Output path '{outpath}' must be a zip")
    usage()

print(f"Creating {outpath} from compiling library in {inpath}")

# ensure we can write output zip
os.makedirs(os.path.dirname(outpath), exist_ok=True)

def include_path(path):
    # exclude based on directories (that may have false positives)
    if any([x in path.split(os.sep) for x in ['test', 'tests', 'distutils', 'site-packages', '__pycache__']]):
        return False
    # exclude general patterns
    if any([x in path for x in ['turtle', 'venv', 'tkinter', 'idlelib', 'lib2to3', 'ensurepip']]):
        return False

    # don't include any .pyc files that aren't in __pycache__
    if path.endswith('.pyc'):
        return False

    return True

with zipfile.ZipFile(outpath, mode='w', compression=zipfile.ZIP_DEFLATED) as z:
    os.chdir(inpath)

    for path in Path('.').rglob('*'):
        relpath = str(path)

        if not include_path(relpath):
            continue

        if str(path).endswith('.py'):
            fd, tmpname = tempfile.mkstemp(suffix=".pyc")
            os.close(fd)

            py_compile.compile(relpath, cfile=tmpname, dfile=relpath, optimize=2)
            z.write(tmpname, arcname=relpath + "c") # .py => .pyc
            os.unlink(tmpname)
        else:
            z.write(relpath)
