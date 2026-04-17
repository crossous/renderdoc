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

// D3D12 GPU resource readback for the API Monitor.
// Uses a readback buffer + command list + fence to copy GPU texture data to CPU.

#include "core/api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "driver/dx/official/d3d12.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

namespace ApiMonitorExport
{
bool WriteDDS(const rdcstr &path, uint32_t width, uint32_t height, uint32_t depth,
              uint32_t arraySize, uint32_t mipLevels, uint32_t dxgiFormat, const byte *pixelData,
              uint32_t dataSize, uint32_t rowPitch);
}

namespace D3D12MonitorReadback
{

// Process pending dump requests for a given D3D12 device.
// Call this from the Present() wrapper (between frames).
void ProcessDumpRequests(WrappedID3D12Device *wrappedDevice)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  rdcarray<ApiMonitor::DumpRequest> requests = ApiMonitor::Inst().DrainDumpRequests();
  if(requests.empty())
    return;

  for(const ApiMonitor::DumpRequest &req : requests)
  {
    ApiMonitor::Inst().Log(
        StringFormat::Fmt("D3D12 Dump: Resource %s -> %s (readback requires device integration, "
                          "placeholder for now)",
                          ToStr(req.resourceId).c_str(), req.outputPath.c_str()));

    // TODO: Full D3D12 readback implementation:
    // 1. Look up WrappedID3D12Resource from ResourceId
    // 2. Get resource desc (Dimension, Width, Height, Format)
    // 3. Calculate readback buffer size from footprint
    // 4. Create readback buffer (D3D12_HEAP_TYPE_READBACK)
    // 5. Create command allocator + command list
    // 6. Record: ResourceBarrier (source -> COPY_SOURCE), CopyTextureRegion, barrier back
    // 7. Close and execute command list
    // 8. Create fence, signal, wait for completion
    // 9. Map readback buffer, write to file
    // 10. Cleanup all temporaries
  }
}

}    // namespace D3D12MonitorReadback

#endif    // RENDERDOC_ENABLE_API_MONITOR
