// Optional, exact-version DOOM startup adapter. This is not part of RenderDoc's core.
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vulkan.h>

namespace
{
HMODULE selfModule = NULL;
HMODULE captureModule = NULL;
HANDLE readyEvent = NULL;
HANDLE logFile = INVALID_HANDLE_VALUE;
SRWLOCK logLock = SRWLOCK_INIT;
PFN_vkCreateInstance nextCreateInstance = NULL;
uintptr_t gameBase = 0;
size_t gameSize = 0;
bool enablePathTracing = false;
bool useIATHooks = true;
volatile LONG adapterActive = 0;

class ScopedEnvironment
{
public:
  ScopedEnvironment(const wchar_t *variable, const wchar_t *value) : name(variable)
  {
    DWORD length = GetEnvironmentVariableW(name, NULL, 0);
    if(length)
    {
      previous.resize(length);
      DWORD written = GetEnvironmentVariableW(name, &previous[0], length);
      if(written >= length)
        return;
      previous.resize(written);
    }
    else if(GetLastError() != ERROR_ENVVAR_NOT_FOUND)
    {
      return;
    }
    changed = SetEnvironmentVariableW(name, value) != FALSE;
  }

  ~ScopedEnvironment()
  {
    if(changed && !SetEnvironmentVariableW(name, previous.empty() ? NULL : previous.c_str()))
      OutputDebugStringA("doom_capture: could not restore process environment.\n");
  }

  bool Active() const { return changed; }

private:
  const wchar_t *name;
  std::wstring previous;
  bool changed = false;
};

constexpr char expectedHash[] =
    "b7aece5fe2842035efd9036a1049edd53ee5efa25564811e20c39ca18362c018";

void Log(const char *message)
{
  OutputDebugStringA(message);
  if(logFile == INVALID_HANDLE_VALUE)
    return;
  AcquireSRWLockExclusive(&logLock);
  DWORD written = 0;
  WriteFile(logFile, message, (DWORD)strlen(message), &written, NULL);
  WriteFile(logFile, "\r\n", 2, &written, NULL);
  ReleaseSRWLockExclusive(&logLock);
}

bool Readable(const void *address, size_t length, bool writable = false)
{
  MEMORY_BASIC_INFORMATION info = {};
  if(!address || !VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
     (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
    return false;
  uintptr_t start = (uintptr_t)address;
  uintptr_t end = (uintptr_t)info.BaseAddress + info.RegionSize;
  if(start > end || length > end - start)
    return false;
  DWORD protect = info.Protect & 0xff;
  if(writable)
    return protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
  return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
         protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE ||
         protect == PAGE_EXECUTE_WRITECOPY;
}

bool ImageRange(uintptr_t offset, size_t length)
{
  return offset < gameSize && length <= gameSize - offset &&
         Readable((const void *)(gameBase + offset), length);
}

bool ImageString(uintptr_t offset, const char *expected, bool ignoreCase = false)
{
  size_t length = strlen(expected) + 1;
  return ImageRange(offset, length) &&
         (ignoreCase ? _stricmp((const char *)(gameBase + offset), expected) == 0
                     : strcmp((const char *)(gameBase + offset), expected) == 0);
}

bool FileMatches(const wchar_t *path)
{
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if(file == INVALID_HANDLE_VALUE)
    return false;
  BCRYPT_ALG_HANDLE algorithm = NULL;
  BCRYPT_HASH_HANDLE hash = NULL;
  BYTE digest[32] = {};
  bool valid = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) >= 0;
  if(valid)
    valid = BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) >= 0;
  BYTE buffer[65536];
  DWORD count = 0;
  while(valid)
  {
    if(!ReadFile(file, buffer, sizeof(buffer), &count, NULL))
    {
      valid = false;
      break;
    }
    if(count == 0)
      break;
    valid = BCryptHashData(hash, buffer, count, 0) >= 0;
  }
  if(valid)
    valid = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
  if(hash)
    BCryptDestroyHash(hash);
  if(algorithm)
    BCryptCloseAlgorithmProvider(algorithm, 0);
  CloseHandle(file);
  char hex[65] = {};
  for(size_t i = 0; i < sizeof(digest); i++)
    sprintf_s(hex + i * 2, 3, "%02x", digest[i]);
  return valid && strcmp(hex, expectedHash) == 0;
}

LONG *CVarValue(uintptr_t offset)
{
  uintptr_t *slot = (uintptr_t *)(gameBase + offset);
  if(!ImageRange(offset, sizeof(*slot)))
    return NULL;
  uintptr_t object = *slot;
  if(object < gameBase || object > gameBase + gameSize - 12 || (object & 3))
    return NULL;
  LONG *value = (LONG *)(object + 8);
  return Readable(value, sizeof(*value), true) ? value : NULL;
}

bool ApplyCVars()
{
  LONG *older = CVarValue(0x989c7e0);
  LONG *omms = CVarValue(0x97face0);
  LONG *pt = CVarValue(0x9831e10);
  if(!older || !omms || (enablePathTracing && !pt))
  {
    Log("CVAR validation failed; no values changed.");
    return false;
  }
  LONG oldOlder = InterlockedExchange(older, 1);
  LONG oldOmms = InterlockedExchange(omms, 0);
  LONG oldPT = enablePathTracing ? InterlockedExchange(pt, 1) : (pt ? *pt : -1);
  char message[240];
  sprintf_s(message, "Before vkCreateInstance: older %ld->1, omms %ld->0, PT %ld (%s).",
            oldOlder, oldOmms, oldPT, enablePathTracing ? "requested 1" : "preserved");
  Log(message);
  return true;
}

VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo *info,
                                              const VkAllocationCallbacks *allocator,
                                              VkInstance *instance)
{
  if(InterlockedCompareExchange(&adapterActive, 0, 0))
    ApplyCVars();
  return nextCreateInstance(info, allocator, instance);
}

bool InstallAdapter()
{
  BYTE *base = (BYTE *)gameBase;
  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
  if(!Readable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
    return false;
  if((size_t)dos->e_lfanew > 0x100000)
    return false;
  IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
  if(!Readable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE ||
     nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
     nt->OptionalHeader.SizeOfImage != 0xb74f000)
    return false;
  gameSize = nt->OptionalHeader.SizeOfImage;
  const IMAGE_DATA_DIRECTORY &imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if(imports.VirtualAddress == 0 || imports.VirtualAddress >= gameSize ||
     imports.Size > gameSize - imports.VirtualAddress)
    return false;
  IMAGE_IMPORT_DESCRIPTOR *desc = (IMAGE_IMPORT_DESCRIPTOR *)(base + imports.VirtualAddress);
  for(size_t n = 0; n < imports.Size / sizeof(*desc); n++)
  {
    if(!ImageRange(imports.VirtualAddress + n * sizeof(*desc), sizeof(*desc)))
      return false;
    if(!desc[n].Name)
      break;
    if(!ImageString(desc[n].Name, "vulkan-1.dll", true))
      continue;
    if(!ImageRange(desc[n].OriginalFirstThunk, sizeof(IMAGE_THUNK_DATA64)) ||
       !ImageRange(desc[n].FirstThunk, sizeof(IMAGE_THUNK_DATA64)))
      return false;
    IMAGE_THUNK_DATA64 *names = (IMAGE_THUNK_DATA64 *)(base + desc[n].OriginalFirstThunk);
    IMAGE_THUNK_DATA64 *slots = (IMAGE_THUNK_DATA64 *)(base + desc[n].FirstThunk);
    size_t limit = (gameSize - desc[n].OriginalFirstThunk) / sizeof(*names);
    size_t slotLimit = (gameSize - desc[n].FirstThunk) / sizeof(*slots);
    if(slotLimit < limit)
      limit = slotLimit;
    for(size_t i = 0; i < limit; i++)
    {
      if(!Readable(&names[i], sizeof(names[i])) || !Readable(&slots[i], sizeof(slots[i])))
        return false;
      if(!names[i].u1.AddressOfData)
        break;
      if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))
        continue;
      uintptr_t nameOffset = (uintptr_t)names[i].u1.AddressOfData;
      if(nameOffset >= gameSize || !ImageString(nameOffset + 2, "vkCreateInstance"))
        continue;
      if(!Readable(&slots[i], sizeof(slots[i])) || !slots[i].u1.Function)
        return false;
      // Chain the current import target. Vulkan calls still go through the normal capture layer.
      nextCreateInstance = (PFN_vkCreateInstance)slots[i].u1.Function;
      DWORD oldProtect = 0;
      if(!VirtualProtect(&slots[i], sizeof(slots[i]), PAGE_READWRITE, &oldProtect))
        return false;
      InterlockedExchange(&adapterActive, 1);
      InterlockedExchangePointer((void *volatile *)&slots[i].u1.Function, (void *)&CreateInstance);
      DWORD ignored = 0;
      if(!VirtualProtect(&slots[i], sizeof(slots[i]), oldProtect, &ignored))
        Log("Warning: could not restore import-page protection.");
      Log("Exact-version vkCreateInstance import adapter installed; EXE file unchanged.");
      return true;
    }
  }
  return false;
}

DWORD WINAPI Initialise(void *)
{
  wchar_t directory[32768] = {};
  GetModuleFileNameW(selfModule, directory, (DWORD)_countof(directory));
  wchar_t *slash = wcsrchr(directory, L'\\');
  if(!slash)
  {
    SetEvent(readyEvent);
    return 0;
  }
  slash[1] = 0;
  wchar_t path[32768] = {};
  swprintf_s(path, L"%sdoom_capture-%lu.log", directory, GetCurrentProcessId());
  logFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  swprintf_s(path, L"%sdoom_capture.ini", directory);
  enablePathTracing = GetPrivateProfileIntW(L"DOOM", L"EnablePathTracing", 0, path) != 0;
  useIATHooks = GetPrivateProfileIntW(L"DOOM", L"UseIATHooks", 1, path) != 0;
  wchar_t executable[32768] = {};
  GetModuleFileNameW(NULL, executable, (DWORD)_countof(executable));
  const wchar_t *name = wcsrchr(executable, L'\\');
  name = name ? name + 1 : executable;
  bool isDOOM = _wcsicmp(name, L"DOOMTheDarkAges.exe") == 0;
  bool verified = isDOOM && FileMatches(executable);
  if(isDOOM && !verified)
    Log("DOOM executable hash differs; refusing game patch.");
  if(!isDOOM)
    Log("Other executable: no game modification; loading standard capture runtime.");
  gameBase = (uintptr_t)GetModuleHandleW(NULL);
  if(verified && !InstallAdapter())
    Log("Import validation failed; refusing game patch.");

  // Retain this module for as long as its import wrapper can be called.
  HMODULE pinned = NULL;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                    (LPCWSTR)&Initialise, &pinned);
  swprintf_s(path, L"%ssystem_load.dll", directory);
  if(verified && useIATHooks)
  {
    // The runtime reads these during synchronous hook registration in LoadLibrary.
    // Restore them afterwards so future child processes do not inherit the overrides.
    ScopedEnvironment d3d12(L"RENDERDOC_HOOK_MODE_D3D12", L"1");
    ScopedEnvironment dxgi(L"RENDERDOC_HOOK_MODE_DXGI", L"1");
    if(d3d12.Active() && dxgi.Active())
      Log("Process-only D3D12/DXGI IAT hook override enabled.");
    else
      Log("Warning: could not set process hook overrides.");
    captureModule = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  }
  else
  {
    captureModule = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  }
  if(!captureModule)
  {
    char message[120];
    sprintf_s(message, "Could not load sibling system_load.dll: Windows error %lu.", GetLastError());
    Log(message);
    InterlockedExchange(&adapterActive, 0);
  }
  else
    Log("Standard system_load.dll loaded; bootstrap ready.");
  SetEvent(readyEvent);
  return 0;
}

FARPROC CaptureExport(const char *name)
{
  if(!readyEvent || WaitForSingleObject(readyEvent, 30000) != WAIT_OBJECT_0 || !captureModule)
    return NULL;
  return GetProcAddress(captureModule, name);
}
}    // namespace

// These are real forwarding functions, not PE forwarded exports: remote injection uses their RVAs.
#define FORWARD_VOID(name, type)                                      \
  extern "C" __declspec(dllexport) void __cdecl name(type argument)   \
  {                                                                  \
    typedef void(__cdecl *Function)(type);                            \
    Function function = (Function)CaptureExport(#name);               \
    if(function)                                                     \
      function(argument);                                            \
  }

FORWARD_VOID(INTERNAL_SetCaptureOptions, void *)
FORWARD_VOID(INTERNAL_SetCaptureFile, const char *)
FORWARD_VOID(INTERNAL_SetDebugLogFile, const char *)
FORWARD_VOID(INTERNAL_EnvModName, const char *)
FORWARD_VOID(INTERNAL_EnvModValue, const char *)
FORWARD_VOID(INTERNAL_EnvSep, void *)
FORWARD_VOID(INTERNAL_EnvMod, void *)
FORWARD_VOID(INTERNAL_ApplyEnvMods, void *)

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetLogFile(const char *path)
{
  INTERNAL_SetCaptureFile(path);
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_GetTargetControlIdent(uint32_t *ident)
{
  if(ident)
    *ident = 0;
  typedef void(__cdecl *Function)(uint32_t *);
  Function function = (Function)CaptureExport("INTERNAL_GetTargetControlIdent");
  if(function)
    function(ident);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, void *)
{
  if(reason == DLL_PROCESS_ATTACH)
  {
    selfModule = module;
    readyEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if(!readyEvent)
      return FALSE;
    HANDLE thread = CreateThread(NULL, 0, Initialise, NULL, 0, NULL);
    if(!thread)
    {
      CloseHandle(readyEvent);
      readyEvent = NULL;
      return FALSE;
    }
    CloseHandle(thread);
  }
  return TRUE;
}
