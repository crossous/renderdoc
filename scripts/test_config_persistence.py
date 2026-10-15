"""Exercise config persistence across fresh replay processes, restoring the user's config."""
import argparse
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    runtime = Path(args.runtime).resolve()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    # Match the Known Folder path used by the Windows runtime, rather than a
    # potentially overridden APPDATA environment variable.
    import ctypes
    from ctypes import wintypes
    class GUID(ctypes.Structure):
        _fields_ = [('Data1', wintypes.DWORD), ('Data2', wintypes.WORD),
                    ('Data3', wintypes.WORD), ('Data4', ctypes.c_ubyte * 8)]
    roaming = GUID(0x3eb685db, 0x65f9, 0x4cf6,
                   (ctypes.c_ubyte * 8)(0xa0, 0x3a, 0xe3, 0xef, 0x65, 0x72, 0x9f, 0x3d))
    folder = ctypes.c_wchar_p()
    shell = ctypes.WinDLL('shell32')
    shell.SHGetKnownFolderPath.argtypes = [ctypes.POINTER(GUID), wintypes.DWORD,
                                         wintypes.HANDLE, ctypes.POINTER(ctypes.c_wchar_p)]
    if shell.SHGetKnownFolderPath(ctypes.byref(roaming), 0, None, ctypes.byref(folder)):
        raise RuntimeError('Could not find the roaming configuration folder')
    config = Path(folder.value) / 'renderdoc' / 'renderdoc.conf'
    ctypes.WinDLL('ole32').CoTaskMemFree(ctypes.cast(folder, ctypes.c_void_p))
    original = config.read_bytes() if config.exists() else None
    if original is not None:
        (output / 'renderdoc.conf.before').write_bytes(original)
    host = Path(__file__).with_name('doom_rt_python.py')

    def run(name, code):
        script = output / (name + '.py')
        script.write_text(code, encoding='utf-8')
        with (output / (name + '.log')).open('w', encoding='utf-8') as log:
            subprocess.run([sys.executable, str(host), '--runtime', str(runtime), str(script)],
                           stdout=log, stderr=subprocess.STDOUT, timeout=45, check=True)

    try:
        legacy = '''<config version="1">
<Hook><DLL><ws2><!-- legacy socket hook --><32 type="UnsignedInteger">0</32></ws2></DLL></Hook>
<Inject><!-- injection path --><CustomDLLPath type="String">C:/config test/capture.dll</CustomDLLPath></Inject>
<NV><!-- NVAPI --><BlockNVAPI type="Boolean">true</BlockNVAPI></NV>
<Vulkan><Hack><!-- AS replay --><RestoreASInCreationOrder type="Boolean">true</RestoreASInCreationOrder></Hack></Vulkan>
</config>'''
        config.write_text(legacy, encoding='utf-8')
        run('legacy-read', '''import renderdoc as rd
assert rd.GetConfigSetting('Hook_DLL_ws2_32').data.basic.u == 0
assert rd.GetConfigSetting('Inject_CustomDLLPath').data.string == 'C:/config test/capture.dll'
assert rd.GetConfigSetting('NV_BlockNVAPI').data.basic.b
assert rd.GetConfigSetting('Vulkan_Hack_RestoreASInCreationOrder').data.basic.b
print('Legacy settings, including all settings after the invalid node, loaded')
''')
        ET.parse(config)
        assert b'<32' not in config.read_bytes()
        run('save', '''import renderdoc as rd
rd.SetConfigSetting('Hook_DLL_ws2_32').data.basic.u = 2
rd.SetConfigSetting('Vulkan_Hack_RestoreASInCreationOrder').data.basic.b = True
rd.SetConfigSetting('Inject_CustomDLLPath').data.string = 'C:/saved config/capture.dll'
rd.SaveConfigSettings()
''')
        ET.parse(config)
        run('fresh-read', '''import renderdoc as rd
assert rd.GetConfigSetting('Hook_DLL_ws2_32').data.basic.u == 2
assert rd.GetConfigSetting('Inject_CustomDLLPath').data.string == 'C:/saved config/capture.dll'
assert rd.GetConfigSetting('NV_BlockNVAPI').data.basic.b
assert rd.GetConfigSetting('Vulkan_Hack_RestoreASInCreationOrder').data.basic.b
print('Saved settings persisted into a fresh process')
''')
        broken = (legacy + '<broken').encode('utf-8')
        config.write_bytes(broken)
        run('malformed-preserved', '''import renderdoc as rd
rd.SetConfigSetting('Vulkan_Hack_RestoreASInCreationOrder').data.basic.b = True
rd.SaveConfigSettings()
print('Attempted save after malformed config load')
''')
        assert config.read_bytes() == broken, 'Malformed config was overwritten'
        config.write_text('<unrelated></unrelated>', encoding='utf-8')
        run('wrong-root-preserved', 'import renderdoc as rd\nrd.SaveConfigSettings()\n')
        assert config.read_text(encoding='utf-8') == '<unrelated></unrelated>'
        config.unlink()
        run('first-run', 'import renderdoc as rd\nrd.SaveConfigSettings()\n')
        ET.parse(config)
        print('PASS: legacy migration, fresh-process persistence, malformed file protection, first run')
    finally:
        if original is None:
            if config.exists():
                config.unlink()
        else:
            config.write_bytes(original)


if __name__ == '__main__':
    main()
