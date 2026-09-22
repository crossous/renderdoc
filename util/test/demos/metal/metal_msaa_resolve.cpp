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

RD_TEST(Metal_MSAA_Resolve, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws three triangles into a 4x multisample attachment and explicitly resolves it.";

  struct Vertex
  {
    float position[2];
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    static const NS::UInteger SampleCount = 4;
    if(!device->supportsTextureSampleCount(SampleCount))
    {
      TEST_WARN("T08 requires 4x MSAA support");
      return 4;
    }

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float2 position [[attribute(0)]];
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
  output.position = float4(input.position, 0.0, 1.0);
  output.colour = input.colour;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    const float red[4] = {1.0f, 0.125f, 0.0625f, 1.0f};
    const float blue[4] = {0.09375f, 0.25f, 1.0f, 1.0f};
    const float green[4] = {0.0625f, 0.875f, 0.1875f, 1.0f};
#define VERTEX(X, Y, C) {{X, Y}, {C[0], C[1], C[2], C[3]}}
    const Vertex vertices[] = {
        VERTEX(-0.95f, -0.75f, red), VERTEX(-0.05f, -0.75f, red),
        VERTEX(-0.50f, 0.75f, red), VERTEX(0.05f, -0.75f, blue),
        VERTEX(0.95f, -0.75f, blue), VERTEX(0.50f, 0.75f, blue),
        VERTEX(-0.28f, -0.38f, green), VERTEX(0.28f, -0.38f, green),
        VERTEX(0.0f, 0.48f, green),
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
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
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
    pipelineDesc->setSampleCount(SampleCount);
    pipelineDesc->setRasterSampleCount(SampleCount);
    pipelineDesc->setAlphaToCoverageEnabled(true);
    pipelineDesc->setAlphaToOneEnabled(false);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    vertexDesc->release();
    if(pipeline == NULL)
    {
      TEST_WARN("Failed to create T08 Metal pipeline: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::alloc()->init();
    textureDesc->setTextureType(MTL::TextureType2DMultisample);
    textureDesc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    textureDesc->setWidth(screenWidth);
    textureDesc->setHeight(screenHeight);
    textureDesc->setDepth(1);
    textureDesc->setMipmapLevelCount(1);
    textureDesc->setArrayLength(1);
    textureDesc->setSampleCount(SampleCount);
    textureDesc->setStorageMode(MTL::StorageModePrivate);
    textureDesc->setUsage(MTL::TextureUsageRenderTarget);
    MTL::Texture *multisampleTarget = device->newTexture(textureDesc);
    textureDesc->release();
    if(vertexBuffer == NULL || multisampleTarget == NULL)
    {
      TEST_WARN("Failed to create T08 Metal resources");
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
      colour->setTexture(multisampleTarget);
      colour->setResolveTexture(drawable->texture());
      colour->setLoadAction(MTL::LoadActionClear);
      colour->setStoreAction(MTL::StoreActionMultisampleResolve);
      colour->setClearColor(MTL::ClearColor::Make(0.03, 0.04, 0.06, 1.0));

      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(3), NS::UInteger(3));
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(6), NS::UInteger(3));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    multisampleTarget->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
