/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Baldur Karlsson
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

// API Monitor resource export utilities: DDS and PNG writers.

#include "api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "os/os_specific.h"
#include "strings/string_utils.h"
#include "stb/stb_image_write.h"

#include <stdio.h>

//=============================================================================
// DDS file format constants and structures
//=============================================================================

#pragma pack(push, 1)

#define DDS_MAGIC 0x20534444    // "DDS "

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_BUMPDUDV 0x80000

#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000

struct DDS_PIXELFORMAT
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwFourCC;
  uint32_t dwRGBBitCount;
  uint32_t dwRBitMask;
  uint32_t dwGBitMask;
  uint32_t dwBBitMask;
  uint32_t dwABitMask;
};

struct DDS_HEADER
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwHeight;
  uint32_t dwWidth;
  uint32_t dwPitchOrLinearSize;
  uint32_t dwDepth;
  uint32_t dwMipMapCount;
  uint32_t dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

struct DDS_HEADER_DXT10
{
  uint32_t dxgiFormat;
  uint32_t resourceDimension;
  uint32_t miscFlag;
  uint32_t arraySize;
  uint32_t miscFlags2;
};

#pragma pack(pop)

namespace ApiMonitorExport
{

bool WriteDDS(const rdcstr &path, uint32_t width, uint32_t height, uint32_t depth,
              uint32_t arraySize, uint32_t mipLevels, uint32_t dxgiFormat, const byte *pixelData,
              uint32_t dataSize, uint32_t rowPitch)
{
  FILE *f = FileIO::fopen(path, FileIO::WriteBinary);
  if(!f)
  {
    RDCERR("API Monitor: Failed to open %s for writing", path.c_str());
    return false;
  }

  // Write DDS magic
  uint32_t magic = DDS_MAGIC;
  FileIO::fwrite(&magic, sizeof(magic), 1, f);

  // Write DDS header
  DDS_HEADER header = {};
  header.dwSize = sizeof(DDS_HEADER);
  header.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH;
  header.dwHeight = height;
  header.dwWidth = width;
  header.dwPitchOrLinearSize = rowPitch;
  header.dwDepth = depth > 1 ? depth : 0;
  header.dwMipMapCount = mipLevels;

  if(depth > 1)
    header.dwFlags |= DDSD_DEPTH;
  if(mipLevels > 1)
    header.dwFlags |= DDSD_MIPMAPCOUNT;

  // Use DX10 extended header for all formats
  header.ddspf.dwSize = sizeof(DDS_PIXELFORMAT);
  header.ddspf.dwFlags = DDPF_FOURCC;
  header.ddspf.dwFourCC = 0x30315844;    // "DX10"

  header.dwCaps = DDSCAPS_TEXTURE;
  if(mipLevels > 1)
    header.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

  FileIO::fwrite(&header, sizeof(header), 1, f);

  // Write DX10 extended header
  DDS_HEADER_DXT10 dx10Header = {};
  dx10Header.dxgiFormat = dxgiFormat;
  dx10Header.resourceDimension = 3;    // D3D12_RESOURCE_DIMENSION_TEXTURE2D
  if(depth > 1)
    dx10Header.resourceDimension = 4;    // TEXTURE3D
  dx10Header.arraySize = arraySize > 0 ? arraySize : 1;
  dx10Header.miscFlags2 = 0;

  FileIO::fwrite(&dx10Header, sizeof(dx10Header), 1, f);

  // Write pixel data
  FileIO::fwrite(pixelData, 1, dataSize, f);

  FileIO::fclose(f);

  RDCLOG("API Monitor: Wrote DDS %ux%u to %s (%u bytes)", width, height, path.c_str(), dataSize);
  return true;
}

bool WritePNG(const rdcstr &path, uint32_t width, uint32_t height, uint32_t numChannels,
              const byte *pixelData, uint32_t rowPitch)
{
  int ret = stbi_write_png(path.c_str(), (int)width, (int)height, (int)numChannels, pixelData,
                           (int)rowPitch);

  if(ret)
    RDCLOG("API Monitor: Wrote PNG %ux%u to %s", width, height, path.c_str());
  else
    RDCERR("API Monitor: Failed to write PNG to %s", path.c_str());

  return ret != 0;
}

bool WriteBMP(const rdcstr &path, uint32_t width, uint32_t height, uint32_t numChannels,
              const byte *pixelData, uint32_t rowPitch)
{
  int ret = stbi_write_bmp(path.c_str(), (int)width, (int)height, (int)numChannels, pixelData);

  if(ret)
    RDCLOG("API Monitor: Wrote BMP %ux%u to %s", width, height, path.c_str());
  else
    RDCERR("API Monitor: Failed to write BMP to %s", path.c_str());

  return ret != 0;
}

}    // namespace ApiMonitorExport

#endif    // RENDERDOC_ENABLE_API_MONITOR
