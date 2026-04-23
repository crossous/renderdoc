#include <windows.h>
#include <stdio.h>
#include "detours.h"

// Minimal test DLL for injection testing.
// No RenderDoc features, no graphics API hooks.
// Opens a console and uses Detours to inline-hook OutputDebugString
// so all game log output appears in the console.

#pragma comment(lib, "detours.lib")

// Original function pointers
static decltype(&OutputDebugStringA) Real_OutputDebugStringA = OutputDebugStringA;
static decltype(&OutputDebugStringW) Real_OutputDebugStringW = OutputDebugStringW;

static void WINAPI Hooked_OutputDebugStringA(LPCSTR lpOutputString)
{
    if(lpOutputString)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        fputs(lpOutputString, stdout);
        size_t len = strlen(lpOutputString);
        if(len > 0 && lpOutputString[len - 1] != '\n')
            fputc('\n', stdout);
        fflush(stdout);
    }
    Real_OutputDebugStringA(lpOutputString);
}

static void WINAPI Hooked_OutputDebugStringW(LPCWSTR lpOutputString)
{
    if(lpOutputString)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        fwprintf(stdout, L"%s", lpOutputString);
        size_t len = wcslen(lpOutputString);
        if(len > 0 && lpOutputString[len - 1] != L'\n')
            fputc('\n', stdout);
        fflush(stdout);
    }
    Real_OutputDebugStringW(lpOutputString);
}

static void OpenConsole()
{
    if(AllocConsole())
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        SetConsoleTitleA("Test Inject DLL - Console");
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if(GetConsoleMode(hStdIn, &mode))
        {
            mode |= ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hStdIn, mode);
        }
    }
}

static void WriteLog(const char *msg)
{
    char logPath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, logPath, MAX_PATH);

    char *lastSlash = strrchr(logPath, '\\');
    if(lastSlash)
        *(lastSlash + 1) = '\0';
    strcat_s(logPath, "test_inject_dll.log");

    HANDLE hFile = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if(ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        OpenConsole();

        char exePath[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        DWORD pid = GetCurrentProcessId();

        printf("[test_inject_dll] Injected successfully!\n");
        printf("[test_inject_dll] PID  = %lu\n", pid);
        printf("[test_inject_dll] EXE  = %s\n", exePath);
        printf("[test_inject_dll] Hooking OutputDebugString (Detours inline hook)...\n");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach((PVOID *)&Real_OutputDebugStringA, Hooked_OutputDebugStringA);
        DetourAttach((PVOID *)&Real_OutputDebugStringW, Hooked_OutputDebugStringW);
        LONG err = DetourTransactionCommit();

        if(err == NO_ERROR)
            printf("[test_inject_dll] Hook OK. Game output will appear below in white.\n\n");
        else
            printf("[test_inject_dll] Hook FAILED (error %ld)\n\n", err);

        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        char msg[512];
        wsprintfA(msg, "[test_inject_dll] Injected into PID=%lu, exe=%s", pid, exePath);
        WriteLog(msg);
    }
    else if(ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach((PVOID *)&Real_OutputDebugStringA, Hooked_OutputDebugStringA);
        DetourDetach((PVOID *)&Real_OutputDebugStringW, Hooked_OutputDebugStringW);
        DetourTransactionCommit();

        WriteLog("[test_inject_dll] DLL_PROCESS_DETACH");
        FreeConsole();
    }

    return TRUE;
}
