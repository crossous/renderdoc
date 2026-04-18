/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <windows.h>

// Enable D3D12 Agility SDK - allows loading a newer D3D12Core.dll from the D3D12 subdirectory.
// This is required for replaying captures that use ID3D12Device10+ features on Windows 10
// where the system D3D12 runtime is too old.
extern "C" __declspec(dllexport) extern const UINT D3D12SDKVersion = 618;
extern "C" __declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                    _In_ int nShowCmd)
{
  size_t len = 512 + wcslen(lpCmdLine) + 64;

  wchar_t *paramsAlloc = new wchar_t[len + 1];

  ZeroMemory(paramsAlloc, sizeof(wchar_t) * (len + 1));

  // get the path to this file
  wchar_t curFile[512] = {0};
  GetModuleFileNameW(NULL, curFile, 511);

  // find the last / or \ and erase everything after that to get the folder
  wchar_t *w = curFile + wcslen(curFile);

  while(w > curFile && *w != '/' && *w != '\\')
    w--;

  if(w > curFile)
    *(w + 1) = 0;
  else
    curFile[0] = 0;

  // transform / to \ just in case
  w = curFile;
  while(*w)
  {
    if(*w == '/')
      *w = '\\';
    w++;
  }

  wcscat_s(curFile, 511, L"qrenderdoc.exe");

  wcscpy_s(paramsAlloc, len, curFile);

  wcscat_s(paramsAlloc, len, L" ");

  wcscat_s(paramsAlloc, len, lpCmdLine);

  PROCESS_INFORMATION pi;
  STARTUPINFOW si;
  ZeroMemory(&pi, sizeof(pi));
  ZeroMemory(&si, sizeof(si));

  CreateProcessW(curFile, paramsAlloc, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

  if(pi.dwProcessId != 0)
  {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  delete[] paramsAlloc;

  return 0;
}
