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

RD_TEST(Metal_MRT_Blend, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws two overlapping triangles to two color attachments with independent blend state.";

  struct Vertex
  {
    float position[2];
    float colour0[4];
    float colour1[4];
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
  float2 position [[attribute(0)]];
  float4 colour0 [[attribute(1)]];
  float4 colour1 [[attribute(2)]];
};

struct VSOut
{
  float4 position [[position]];
  float4 colour0;
  float4 colour1;
};

struct FSOut
{
  float4 colour0 [[color(0)]];
  float4 colour1 [[color(1)]];
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.colour0 = input.colour0;
  output.colour1 = input.colour1;
  return output;
}

fragment FSOut fs_main(VSOut input [[stage_in]])
{
  FSOut output;
  output.colour0 = input.colour0;
  output.colour1 = input.colour1;
  return output;
}
)EOSHADER";

    const float first0[4] = {1.0f, 0.125f, 0.0625f, 0.5f};
    const float first1[4] = {0.09375f, 0.25f, 1.0f, 0.75f};
    const float second0[4] = {0.0625f, 0.875f, 0.1875f, 0.25f};
    const float second1[4] = {0.9375f, 0.8125f, 0.125f, 0.25f};
    const Vertex vertices[] = {
        {{-1.0f, -1.0f}, {first0[0], first0[1], first0[2], first0[3]},
         {first1[0], first1[1], first1[2], first1[3]}},
        {{3.0f, -1.0f}, {first0[0], first0[1], first0[2], first0[3]},
         {first1[0], first1[1], first1[2], first1[3]}},
        {{-1.0f, 3.0f}, {first0[0], first0[1], first0[2], first0[3]},
         {first1[0], first1[1], first1[2], first1[3]}},
        {{-0.45f, -0.45f}, {second0[0], second0[1], second0[2], second0[3]},
         {second1[0], second1[1], second1[2], second1[3]}},
        {{0.45f, -0.45f}, {second0[0], second0[1], second0[2], second0[3]},
         {second1[0], second1[1], second1[2], second1[3]}},
        {{0.0f, 0.55f}, {second0[0], second0[1], second0[2], second0[3]},
         {second1[0], second1[1], second1[2], second1[3]}},
    };

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
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Vertex, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Vertex, colour0));
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->attributes()->object(2)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(2)->setOffset(offsetof(Vertex, colour1));
    vertexDesc->attributes()->object(2)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);

    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);

    MTL::RenderPipelineColorAttachmentDescriptor *attachment0 =
        pipelineDesc->colorAttachments()->object(0);
    attachment0->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment0->setBlendingEnabled(true);
    attachment0->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment0->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment0->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment0->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment0->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
    attachment0->setAlphaBlendOperation(MTL::BlendOperationAdd);
    attachment0->setWriteMask(MTL::ColorWriteMaskAll);

    MTL::RenderPipelineColorAttachmentDescriptor *attachment1 =
        pipelineDesc->colorAttachments()->object(1);
    attachment1->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    attachment1->setBlendingEnabled(false);
    attachment1->setWriteMask((MTL::ColorWriteMask)(MTL::ColorWriteMaskRed |
                                                    MTL::ColorWriteMaskGreen |
                                                    MTL::ColorWriteMaskBlue));

    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    vertexDesc->release();
    if(pipeline == NULL)
    {
      TEST_WARN("Failed to create Metal render pipeline: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, screenWidth, screenHeight, false);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage((MTL::TextureUsage)(MTL::TextureUsageRenderTarget |
                                              MTL::TextureUsageShaderRead));
    MTL::Texture *secondTarget = device->newTexture(textureDesc);
    if(vertexBuffer == NULL || secondTarget == NULL)
    {
      TEST_WARN("Failed to create T06 Metal resources");
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
      MTL::RenderPassColorAttachmentDescriptor *pass0 = pass->colorAttachments()->object(0);
      pass0->setTexture(drawable->texture());
      pass0->setLoadAction(MTL::LoadActionClear);
      pass0->setStoreAction(MTL::StoreActionStore);
      pass0->setClearColor(MTL::ClearColor::Make(0.10, 0.20, 0.30, 1.0));
      MTL::RenderPassColorAttachmentDescriptor *pass1 = pass->colorAttachments()->object(1);
      pass1->setTexture(secondTarget);
      pass1->setLoadAction(MTL::LoadActionClear);
      pass1->setStoreAction(MTL::StoreActionStore);
      pass1->setClearColor(MTL::ClearColor::Make(0.02, 0.04, 0.06, 1.0));

      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(3), NS::UInteger(3));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    secondTarget->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
