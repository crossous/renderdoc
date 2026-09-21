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

RD_TEST(Metal_Indexed_Cube, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws fixed indexed cubes with 16-bit and 32-bit indices, depth testing, and an explicit "
      "Metal vertex descriptor.";

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

    // The back square is smaller and shifted up/right. With z in Metal's [0, 1] clip range this
    // gives a deterministic perspective-like cube without introducing a matrix dependency.
    const Vertex vertices[] = {
        {{-0.68f, -0.68f, 0.20f}, {1.00f, 0.12f, 0.10f, 1.0f}},
        {{0.48f, -0.68f, 0.20f}, {1.00f, 0.55f, 0.08f, 1.0f}},
        {{0.48f, 0.48f, 0.20f}, {0.95f, 0.18f, 0.72f, 1.0f}},
        {{-0.68f, 0.48f, 0.20f}, {0.18f, 0.85f, 0.20f, 1.0f}},
        {{-0.38f, -0.38f, 0.78f}, {0.12f, 0.35f, 1.00f, 1.0f}},
        {{0.68f, -0.38f, 0.78f}, {0.15f, 0.92f, 0.86f, 1.0f}},
        {{0.68f, 0.68f, 0.78f}, {0.75f, 0.28f, 1.00f, 1.0f}},
        {{-0.38f, 0.68f, 0.78f}, {0.25f, 0.72f, 1.00f, 1.0f}},
    };

    const uint16_t indices16[] = {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
    };
    uint32_t indices32[ARRAY_COUNT(indices16)] = {};
    for(size_t i = 0; i < ARRAY_COUNT(indices16); i++)
      indices32[i] = indices16[i];

    NS::Error *error = NULL;
    MTL::Library *library =
        device->newLibrary(NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
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

    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    pipelineDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    vertexDesc->release();
    if(pipeline == NULL)
    {
      TEST_WARN("Failed to create Metal render pipeline: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::DepthStencilDescriptor *depthDesc = MTL::DepthStencilDescriptor::alloc()->init();
    depthDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDesc->setDepthWriteEnabled(true);
    MTL::DepthStencilState *depthState = device->newDepthStencilState(depthDesc);
    depthDesc->release();

    MTL::TextureDescriptor *depthTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatDepth32Float, screenWidth, screenHeight, false);
    depthTextureDesc->setStorageMode(MTL::StorageModePrivate);
    depthTextureDesc->setUsage(MTL::TextureUsageRenderTarget);
    MTL::Texture *depthTexture = device->newTexture(depthTextureDesc);

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::Buffer *indexBuffer16 =
        device->newBuffer(indices16, sizeof(indices16), MTL::ResourceStorageModeShared);
    MTL::Buffer *indexBuffer32 =
        device->newBuffer(indices32, sizeof(indices32), MTL::ResourceStorageModeShared);
    if(depthState == NULL || depthTexture == NULL || vertexBuffer == NULL || indexBuffer16 == NULL ||
       indexBuffer32 == NULL)
    {
      TEST_WARN("Failed to create T02 Metal resources");
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
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
      depth->setTexture(depthTexture);
      depth->setLoadAction(MTL::LoadActionClear);
      depth->setStoreAction(MTL::StoreActionStore);
      depth->setClearDepth(1.0);

      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setDepthStencilState(depthState);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
      encoder->setCullMode(MTL::CullModeBack);

      const double halfWidth = double(screenWidth) * 0.5;
      MTL::Viewport leftViewport = {0.0, 0.0, halfWidth, double(screenHeight), 0.0, 1.0};
      MTL::ScissorRect leftScissor = {0, 0, NS::UInteger(screenWidth / 2), NS::UInteger(screenHeight)};
      encoder->setViewport(leftViewport);
      encoder->setScissorRect(leftScissor);
      encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, ARRAY_COUNT(indices16),
                                     MTL::IndexTypeUInt16, indexBuffer16, 0);

      MTL::Viewport rightViewport = {halfWidth, 0.0, halfWidth, double(screenHeight), 0.0, 1.0};
      MTL::ScissorRect rightScissor = {NS::UInteger(screenWidth / 2), 0,
                                       NS::UInteger(screenWidth - screenWidth / 2),
                                       NS::UInteger(screenHeight)};
      encoder->setViewport(rightViewport);
      encoder->setScissorRect(rightScissor);
      encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, ARRAY_COUNT(indices32),
                                     MTL::IndexTypeUInt32, indexBuffer32, 0);

      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    indexBuffer32->release();
    indexBuffer16->release();
    vertexBuffer->release();
    depthTexture->release();
    depthState->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
