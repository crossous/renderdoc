#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <windows.h>

#include "core/core.h"

typedef std::map<std::string, DWORD> RVATable;
static std::map<std::wstring, RVATable> g_GlobalPETable;
 
std::vector<BYTE> ReadFileToMemory(const wchar_t* filePath)
{
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if(!file.is_open())
  {
    throw std::runtime_error("Failed to open file");
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<BYTE> buffer(static_cast<unsigned int>(size));
  if(!file.read((char *)buffer.data(), size))
  {
    throw std::runtime_error("Function not found");
  }

  return buffer;
}

// Convert RVA to file offset
DWORD RvaToFileOffset(const std::vector<BYTE> &fileData, DWORD rva)
{
  PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)fileData.data();
  PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(fileData.data() + pDosHeader->e_lfanew);

  // Get section headers
  PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
  WORD numberOfSections = pNtHeaders->FileHeader.NumberOfSections;

  // Iterate through sections to find the one containing the RVA
  for(WORD i = 0; i < numberOfSections; i++)
  {
    if(rva >= pSectionHeader[i].VirtualAddress &&
       rva < pSectionHeader[i].VirtualAddress + pSectionHeader[i].Misc.VirtualSize)
    {
      return rva - pSectionHeader[i].VirtualAddress + pSectionHeader[i].PointerToRawData;
    }
  }

  throw std::runtime_error("RVA not found in any section");
}

// Get function RVA
static void ParsePERva(const wchar_t* fileName)
{
  std::vector<BYTE> fileData = ReadFileToMemory(fileName);

  PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)fileData.data();
  if(pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    throw std::runtime_error("Invalid DOS header");
  }

  PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(fileData.data() + pDosHeader->e_lfanew);
  if(pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
  {
    throw std::runtime_error("Invalid NT header");
  }

  // Check if the file is 32-bit or 64-bit
  bool is64Bit = false;
  if(pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
  {
    is64Bit = true;
  }
  else if(pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
  {
    is64Bit = false;
  }
  else
  {
    throw std::runtime_error("Unknown PE format");
  }

  // Get the export directory RVA
  DWORD exportDirRVA = is64Bit ? ((PIMAGE_NT_HEADERS64)pNtHeaders)
                                     ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
                                     .VirtualAddress
                               : ((PIMAGE_NT_HEADERS32)pNtHeaders)
                                     ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
                                     .VirtualAddress;

  if(exportDirRVA == 0)
  {
    throw std::runtime_error("No export directory found");
  }

  // Convert export directory RVA to file offset
  DWORD exportDirOffset = RvaToFileOffset(fileData, exportDirRVA);
  PIMAGE_EXPORT_DIRECTORY pExportDirectory =
      (PIMAGE_EXPORT_DIRECTORY)(fileData.data() + exportDirOffset);

  // Get function addresses, names, and ordinals from the export directory
  DWORD *pFunctions =
      (DWORD *)(fileData.data() + RvaToFileOffset(fileData, pExportDirectory->AddressOfFunctions));
  DWORD *pNames =
      (DWORD *)(fileData.data() + RvaToFileOffset(fileData, pExportDirectory->AddressOfNames));
  WORD *pNameOrdinals =
      (WORD *)(fileData.data() + RvaToFileOffset(fileData, pExportDirectory->AddressOfNameOrdinals));

  auto insertRes = g_GlobalPETable.insert(std::make_pair(std::wstring(fileName), RVATable()));

  if (!insertRes.second)
  {
    std::runtime_error("Failed to insert table");
  }

  auto& rvaTable = insertRes.first->second;

  // Iterate through the export directory to find the target function
  for(DWORD i = 0; i < pExportDirectory->NumberOfNames; i++)
  {
    char *pFunctionName = (char *)(fileData.data() + RvaToFileOffset(fileData, pNames[i]));
    DWORD rva = pFunctions[pNameOrdinals[i]];

    rvaTable.insert(std::make_pair(pFunctionName, rva));
  }
}

DWORD GetFunctionRVA(const wchar_t* fileName, const char* functionName)
{
  try
  {
    auto ite = g_GlobalPETable.find(fileName);
    if (ite == g_GlobalPETable.end())
    {
      ParsePERva(fileName);
      ite = g_GlobalPETable.find(fileName);
    }

    auto &rvaTable = ite->second;
    auto rvaIte = rvaTable.find(functionName);
    if(rvaIte == rvaTable.end())
      throw std::runtime_error("Function not found");

    return rvaIte->second;
  }
  catch (const std::exception &e)
  {
    RDCERR("Parse PE error: %s. dll: %s, function: %s", e.what(), fileName, functionName);
    return NULL;
  }
}