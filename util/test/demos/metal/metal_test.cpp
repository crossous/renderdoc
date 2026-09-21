/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

#include "metal_test.h"

bool MetalGraphicsTest::Init()
{
  if(!GraphicsTest::Init() || !AppleWindow::Init())
    return false;

  mainWindow = new AppleWindow(screenWidth, screenHeight, screenTitle);
  metalLayer = (CA::MetalLayer *)mainWindow->layer;
  device = MTL::CreateSystemDefaultDevice();

  if(device == NULL)
  {
    TEST_WARN("MTLCreateSystemDefaultDevice returned NULL");
    return false;
  }

  metalLayer->setDevice(device);
  metalLayer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  metalLayer->setFramebufferOnly(false);
  metalLayer->setDrawableSize(CGSizeMake(screenWidth, screenHeight));

  queue = device->newCommandQueue();
  if(queue == NULL)
  {
    TEST_WARN("newCommandQueue returned NULL");
    return false;
  }

  const std::string capturePath = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH");
  if(rdoc && !capturePath.empty())
  {
    rdoc->SetCaptureFilePathTemplate(capturePath.c_str());
    TEST_LOG("Metal auto-capture enabled, output template: %s", capturePath.c_str());
  }

  return true;
}

void MetalGraphicsTest::Shutdown()
{
  if(queue)
    queue->release();
  if(device)
    device->release();
  delete mainWindow;

  queue = NULL;
  device = NULL;
  metalLayer = NULL;
  mainWindow = NULL;
}

bool MetalGraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}

bool MetalGraphicsTest::BeginCaptureFrame()
{
  if(captureAttempted || curFrame < 2 || rdoc == NULL ||
     GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty())
    return false;

  captureAttempted = true;
  rdoc->StartFrameCapture(NULL, NULL);
  captureActive = rdoc->IsFrameCapturing() != 0;

  if(captureActive)
    TEST_LOG("Started RenderDoc Metal capture on frame %d", curFrame);
  else
    TEST_WARN("RenderDoc was loaded but did not start a Metal capture");

  return captureActive;
}

bool MetalGraphicsTest::EndCaptureFrame()
{
  if(!captureActive)
    return false;

  captureActive = false;
  const bool success = rdoc->EndFrameCapture(NULL, NULL) != 0;
  if(success)
    TEST_LOG("Finished RenderDoc Metal capture");
  else
    TEST_WARN("RenderDoc failed to finish the Metal capture");

  return success;
}

CA::MetalDrawable *MetalGraphicsTest::AcquireDrawable()
{
  return metalLayer->nextDrawable();
}

MTL::RenderPassDescriptor *MetalGraphicsTest::MakeBackbufferRenderPass(
    CA::MetalDrawable *drawable, MTL::ClearColor clearColor)
{
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(drawable->texture());
  colour->setLoadAction(MTL::LoadActionClear);
  colour->setStoreAction(MTL::StoreActionStore);
  colour->setClearColor(clearColor);
  return pass;
}
