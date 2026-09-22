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

RD_TEST(Metal_Depth_Stencil, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Writes front/back stencil values, tests them on later draws, and verifies a depth failure.";

  struct Vertex
  {
    float position[3];
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float3 position [[attribute(0)]];
  float4 colour [[attribute(1)]];
};

struct VSOut
{
  float4 position [[position]];
  float4 colour;
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 1.0);
  output.colour = input.colour;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    const float mask[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float green[4] = {0.0625f, 0.875f, 0.1875f, 1.0f};
    const float blue[4] = {0.09375f, 0.25f, 1.0f, 1.0f};
    const float red[4] = {1.0f, 0.125f, 0.0625f, 1.0f};
#define VERTEX(X, Y, Z, C) {{X, Y, Z}, {C[0], C[1], C[2], C[3]}}
    const Vertex vertices[] = {
        // Counter-clockwise left mask and clockwise right mask exercise separate stencil faces.
        VERTEX(-0.90f, -0.70f, 0.20f, mask), VERTEX(-0.10f, -0.70f, 0.20f, mask),
        VERTEX(-0.50f, 0.70f, 0.20f, mask), VERTEX(0.10f, -0.70f, 0.20f, mask),
        VERTEX(0.50f, 0.70f, 0.20f, mask), VERTEX(0.90f, -0.70f, 0.20f, mask),
        // Fullscreen triangles used with left/right scissors and different depth values.
        VERTEX(-1.0f, -1.0f, 0.40f, green), VERTEX(3.0f, -1.0f, 0.40f, green),
        VERTEX(-1.0f, 3.0f, 0.40f, green), VERTEX(-1.0f, -1.0f, 0.60f, blue),
        VERTEX(3.0f, -1.0f, 0.60f, blue), VERTEX(-1.0f, 3.0f, 0.60f, blue),
        VERTEX(-1.0f, -1.0f, 0.80f, red), VERTEX(3.0f, -1.0f, 0.80f, red),
        VERTEX(-1.0f, 3.0f, 0.80f, red),
    };
#undef VERTEX

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile Metal shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
    if(vertexFunction == NULL || fragmentFunction == NULL)
    {
      TEST_WARN("Failed to find Metal shader entry points");
      return 4;
    }

    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Vertex, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Vertex, colour));
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);

    auto makePipeline = [&](MTL::ColorWriteMask writeMask) {
      MTL::RenderPipelineDescriptor *desc = MTL::RenderPipelineDescriptor::alloc()->init();
      desc->setVertexFunction(vertexFunction);
      desc->setFragmentFunction(fragmentFunction);
      desc->setVertexDescriptor(vertexDesc);
      desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      desc->colorAttachments()->object(0)->setWriteMask(writeMask);
      desc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
      desc->setStencilAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
      MTL::RenderPipelineState *ret = device->newRenderPipelineState(desc, &error);
      desc->release();
      return ret;
    };

    MTL::RenderPipelineState *maskPipeline = makePipeline(MTL::ColorWriteMaskNone);
    MTL::RenderPipelineState *colourPipeline = makePipeline(MTL::ColorWriteMaskAll);
    vertexDesc->release();
    if(maskPipeline == NULL || colourPipeline == NULL)
    {
      TEST_WARN("Failed to create T07 Metal pipelines: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    auto configureFace = [](MTL::StencilDescriptor *face, MTL::CompareFunction compare,
                            MTL::StencilOperation fail, MTL::StencilOperation depthFail,
                            MTL::StencilOperation pass, uint32_t readMask, uint32_t writeMask) {
      face->setStencilCompareFunction(compare);
      face->setStencilFailureOperation(fail);
      face->setDepthFailureOperation(depthFail);
      face->setDepthStencilPassOperation(pass);
      face->setReadMask(readMask);
      face->setWriteMask(writeMask);
    };

    MTL::DepthStencilDescriptor *maskDesc = MTL::DepthStencilDescriptor::alloc()->init();
    maskDesc->setLabel(MTLSTR("T07 Stencil Write"));
    maskDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
    maskDesc->setDepthWriteEnabled(false);
    MTL::StencilDescriptor *maskFront = MTL::StencilDescriptor::alloc()->init();
    MTL::StencilDescriptor *maskBack = MTL::StencilDescriptor::alloc()->init();
    configureFace(maskFront, MTL::CompareFunctionAlways, MTL::StencilOperationZero,
                  MTL::StencilOperationIncrementClamp, MTL::StencilOperationReplace, 0x3f, 0xff);
    configureFace(maskBack, MTL::CompareFunctionAlways, MTL::StencilOperationInvert,
                  MTL::StencilOperationDecrementWrap, MTL::StencilOperationReplace, 0x7f, 0xff);
    maskDesc->setFrontFaceStencil(maskFront);
    maskDesc->setBackFaceStencil(maskBack);
    MTL::DepthStencilState *maskState = device->newDepthStencilState(maskDesc);
    maskFront->release();
    maskBack->release();
    maskDesc->release();

    MTL::DepthStencilDescriptor *testDesc = MTL::DepthStencilDescriptor::alloc()->init();
    testDesc->setLabel(MTLSTR("T07 Depth Stencil Test"));
    testDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
    testDesc->setDepthWriteEnabled(true);
    MTL::StencilDescriptor *testFront = MTL::StencilDescriptor::alloc()->init();
    MTL::StencilDescriptor *testBack = MTL::StencilDescriptor::alloc()->init();
    configureFace(testFront, MTL::CompareFunctionEqual, MTL::StencilOperationKeep,
                  MTL::StencilOperationIncrementClamp, MTL::StencilOperationKeep, 0xff, 0x00);
    configureFace(testBack, MTL::CompareFunctionEqual, MTL::StencilOperationKeep,
                  MTL::StencilOperationDecrementClamp, MTL::StencilOperationKeep, 0xff, 0x00);
    testDesc->setFrontFaceStencil(testFront);
    testDesc->setBackFaceStencil(testBack);
    MTL::DepthStencilState *testState = device->newDepthStencilState(testDesc);
    testFront->release();
    testBack->release();
    testDesc->release();

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *depthDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatDepth32Float_Stencil8, screenWidth, screenHeight, false);
    depthDesc->setStorageMode(MTL::StorageModePrivate);
    depthDesc->setUsage(MTL::TextureUsageRenderTarget);
    MTL::Texture *depthStencil = device->newTexture(depthDesc);
    if(maskState == NULL || testState == NULL || vertexBuffer == NULL || depthStencil == NULL)
    {
      TEST_WARN("Failed to create T07 depth/stencil resources");
      return 4;
    }

    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();

      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable == NULL)
      {
        TEST_WARN("CAMetalLayer returned no drawable");
        pool->drain();
        continue;
      }

      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
      MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
      colour->setTexture(drawable->texture());
      colour->setLoadAction(MTL::LoadActionClear);
      colour->setStoreAction(MTL::StoreActionStore);
      colour->setClearColor(MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      pass->depthAttachment()->setTexture(depthStencil);
      pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->depthAttachment()->setClearDepth(1.0);
      pass->stencilAttachment()->setTexture(depthStencil);
      pass->stencilAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->stencilAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->stencilAttachment()->setClearStencil(0);

      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->setRenderPipelineState(maskPipeline);
      encoder->setDepthStencilState(maskState);
      encoder->setStencilReferenceValues(9, 5);
      encoder->setStencilReferenceValue(5);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->setStencilReferenceValue(9);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(3), NS::UInteger(3));

      encoder->setRenderPipelineState(colourPipeline);
      encoder->setDepthStencilState(testState);
      encoder->setStencilReferenceValue(5);
      MTL::ScissorRect left = {0, 0, NS::UInteger(screenWidth / 2),
                               NS::UInteger(screenHeight)};
      encoder->setScissorRect(left);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 6, 3);

      encoder->setStencilReferenceValue(9);
      MTL::ScissorRect right = {NS::UInteger(screenWidth / 2), 0,
                                NS::UInteger(screenWidth / 2), NS::UInteger(screenHeight)};
      encoder->setScissorRect(right);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 9, 3);

      encoder->setStencilReferenceValue(5);
      encoder->setScissorRect(left);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 12, 3);
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    depthStencil->release();
    vertexBuffer->release();
    testState->release();
    maskState->release();
    colourPipeline->release();
    maskPipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
