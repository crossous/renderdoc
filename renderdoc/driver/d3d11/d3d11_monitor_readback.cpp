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

// D3D11 GPU resource readback for the API Monitor.
// Copies a tracked texture/buffer to a staging resource and writes it to disk.

#include "core/api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include <d3d11.h>
#include "maths/formatpacking.h"
#include "d3d11_device.h"
#include "d3d11_context.h"
#include "d3d11_resources.h"

// Forward declaration for export functions
namespace ApiMonitorExport
{
bool WriteDDS(const rdcstr &path, uint32_t width, uint32_t height, uint32_t depth,
              uint32_t arraySize, uint32_t mipLevels, uint32_t dxgiFormat, const byte *pixelData,
              uint32_t dataSize, uint32_t rowPitch);
bool WritePNG(const rdcstr &path, uint32_t width, uint32_t height, uint32_t numChannels,
              const byte *pixelData, uint32_t rowPitch);
bool WriteBMP(const rdcstr &path, uint32_t width, uint32_t height, uint32_t numChannels,
              const byte *pixelData, uint32_t rowPitch);
}

namespace D3D11MonitorReadback
{

// Process pending dump requests for a given D3D11 device.
// Call this from the Present() wrapper (between frames) for minimal GPU disruption.
void ProcessDumpRequests(WrappedID3D11Device *wrappedDevice)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  rdcarray<ApiMonitor::DumpRequest> requests = ApiMonitor::Inst().DrainDumpRequests();
  if(requests.empty())
    return;

  ID3D11Device *device = wrappedDevice->GetReal();
  ID3D11DeviceContext *ctx = NULL;
  device->GetImmediateContext(&ctx);
  if(!ctx)
    return;

  for(const ApiMonitor::DumpRequest &req : requests)
  {
    // TODO: Full implementation requires integration with the resource wrapping system
    ApiMonitor::Inst().Log(
        StringFormat::Fmt("Dump: Resource %s -> %s (readback requires device integration, "
                          "placeholder for now)",
                          ToStr(req.resourceId).c_str(), req.outputPath.c_str()));

    // TODO: Full implementation requires integration with the resource wrapping system
    // to look up live ID3D11Texture2D* from ResourceId. This will be done when we have
    // access to the device's internal resource map.
    //
    // The pattern would be:
    // 1. Get the WrappedID3D11Texture2D1 from ResourceId
    // 2. Get the real ID3D11Texture2D from it
    // 3. Create a staging texture with same desc but USAGE_STAGING | CPU_ACCESS_READ
    // 4. CopyResource from source to staging
    // 5. Map staging, read pixel data
    // 6. Write to DDS/PNG using ApiMonitorExport functions
    // 7. Unmap and release staging
  }

  ctx->Release();
}

// Direct readback from a known ID3D11Texture2D pointer.
// This can be called from the hook when we have the actual resource pointer.
bool DumpTexture2D(ID3D11Device *device, ID3D11DeviceContext *ctx, ID3D11Texture2D *srcTexture,
                   const rdcstr &outputPath, const rdcstr &format)
{
  if(!srcTexture || !device || !ctx)
    return false;

  D3D11_TEXTURE2D_DESC desc;
  srcTexture->GetDesc(&desc);

  // Create staging texture
  D3D11_TEXTURE2D_DESC stagingDesc = desc;
  stagingDesc.Usage = D3D11_USAGE_STAGING;
  stagingDesc.BindFlags = 0;
  stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  stagingDesc.MiscFlags = 0;
  stagingDesc.MipLevels = 1;
  stagingDesc.ArraySize = 1;

  ID3D11Texture2D *staging = NULL;
  HRESULT hr = device->CreateTexture2D(&stagingDesc, NULL, &staging);
  if(FAILED(hr) || !staging)
  {
    ApiMonitor::Inst().Log(
        StringFormat::Fmt("Dump failed: couldn't create staging texture (0x%08x)", hr));
    return false;
  }

  // Copy first mip, first array slice
  ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, srcTexture, 0, NULL);

  // Map the staging texture
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
  if(FAILED(hr))
  {
    ApiMonitor::Inst().Log(
        StringFormat::Fmt("Dump failed: couldn't map staging texture (0x%08x)", hr));
    staging->Release();
    return false;
  }

  bool success = false;

  if(format == "dds")
  {
    success = ApiMonitorExport::WriteDDS(outputPath, desc.Width, desc.Height, 1, desc.ArraySize,
                                         desc.MipLevels, (uint32_t)desc.Format,
                                         (const byte *)mapped.pData,
                                         mapped.RowPitch * desc.Height, mapped.RowPitch);
  }
  else if(format == "png" || format == "bmp")
  {
    // For PNG/BMP, we need RGBA8 data. If the format is already RGBA8, use directly.
    // Otherwise, this is a simplified path that works for common formats.
    uint32_t bpp = GetByteSize(1, 1, 1, desc.Format, 0);
    uint32_t channels = 4;
    if(bpp <= 1)
      channels = 1;
    else if(bpp <= 2)
      channels = 2;
    else if(bpp <= 3)
      channels = 3;

    if(format == "png")
      success = ApiMonitorExport::WritePNG(outputPath, desc.Width, desc.Height, channels,
                                           (const byte *)mapped.pData, mapped.RowPitch);
    else
      success = ApiMonitorExport::WriteBMP(outputPath, desc.Width, desc.Height, channels,
                                           (const byte *)mapped.pData, mapped.RowPitch);
  }

  ctx->Unmap(staging, 0);
  staging->Release();

  if(success)
  {
    ApiMonitor::Inst().Log(
        StringFormat::Fmt("Dumped texture %ux%u fmt=%u to %s", desc.Width, desc.Height,
                          (uint32_t)desc.Format, outputPath.c_str()));
  }

  return success;
}

}    // namespace D3D11MonitorReadback

#endif    // RENDERDOC_ENABLE_API_MONITOR
