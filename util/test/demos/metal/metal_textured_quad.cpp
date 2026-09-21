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

RD_TEST(Metal_Textured_Quad, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a deterministic RGBA8 texture with an explicit Metal sampler and vertex descriptor.";

  struct Vertex
  {
    float position[2];
    float uv[2];
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
  float2 uv [[attribute(1)]];
};

struct VSOut
{
  float4 position [[position]];
  float2 uv;
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]], texture2d<float> colourTexture [[texture(0)]],
                        sampler colourSampler [[sampler(0)]])
{
  return colourTexture.sample(colourSampler, input.uv);
}
)EOSHADER";

    const Vertex vertices[] = {
        {{-0.80f, -0.80f}, {0.0f, 1.0f}},
        {{0.80f, -0.80f}, {1.0f, 1.0f}},
        {{-0.80f, 0.80f}, {0.0f, 0.0f}},
        {{0.80f, 0.80f}, {1.0f, 0.0f}},
    };

    // Four 2x2 colour blocks make orientation and channel order visible while retaining exact
    // texels for GetTextureData()/PickPixel() assertions.
    const uint8_t textureData[] = {
        255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
        255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
        24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
        24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
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
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Vertex, uv));
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);

    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

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
        MTL::PixelFormatRGBA8Unorm, 4, 4, false);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *texture = device->newTexture(textureDesc);
    if(texture)
      texture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, textureData, 4 * 4);

    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();

    if(vertexBuffer == NULL || texture == NULL || sampler == NULL)
    {
      TEST_WARN("Failed to create T03 Metal resources");
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
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->setFragmentTexture(texture, 0);
      // Bind the same objects to an undeclared slot so replay can prove that shader reflection,
      // rather than the presence of a binding, drives the used/unused filter.
      encoder->setFragmentTexture(texture, 1);
      encoder->setFragmentSamplerState(sampler, 0);
      encoder->setFragmentSamplerState(sampler, 1);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    sampler->release();
    texture->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
