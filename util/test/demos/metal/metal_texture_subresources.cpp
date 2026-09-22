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

RD_TEST(Metal_Texture_Subresources, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Samples deterministic colours from 2D mip levels, 2D-array slices, and cube faces.";

  struct Vertex
  {
    float position[2];
    float uv[2];
  };

  static void Fill(uint8_t *data, uint32_t width, uint32_t height, const uint8_t colour[4])
  {
    for(uint32_t i = 0; i < width * height; i++)
      memcpy(data + i * 4, colour, 4);
  }

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

fragment float4 fs_main(VSOut input [[stage_in]], texture2d<float> mipTexture [[texture(0)]],
                        texture2d_array<float> arrayTexture [[texture(1)]],
                        texturecube<float> cubeTexture [[texture(2)]],
                        sampler nearestSampler [[sampler(0)]])
{
  const uint band = min(uint(input.uv.x * 12.0), 11u);
  if(band < 3)
    return mipTexture.sample(nearestSampler, float2(0.5), level(float(band)));
  if(band < 6)
    return arrayTexture.sample(nearestSampler, float2(0.5), band - 3, level(0.0));

  const float3 directions[6] = {
      float3(1.0, 0.0, 0.0), float3(-1.0, 0.0, 0.0),
      float3(0.0, 1.0, 0.0), float3(0.0, -1.0, 0.0),
      float3(0.0, 0.0, 1.0), float3(0.0, 0.0, -1.0),
  };
  return cubeTexture.sample(nearestSampler, directions[band - 6], level(0.0));
}
)EOSHADER";

    const Vertex vertices[] = {
        {{-1.0f, -1.0f}, {0.0f, 1.0f}}, {{1.0f, -1.0f}, {1.0f, 1.0f}},
        {{-1.0f, 1.0f}, {0.0f, 0.0f}},  {{1.0f, 1.0f}, {1.0f, 0.0f}},
    };
    const uint8_t colours[12][4] = {
        {255, 32, 16, 255},  {16, 224, 48, 255},  {24, 64, 255, 255},
        {240, 208, 32, 255}, {224, 48, 192, 255}, {32, 208, 224, 255},
        {255, 128, 32, 255}, {128, 32, 255, 255},  {32, 255, 128, 255},
        {255, 64, 128, 255}, {128, 255, 32, 255},  {32, 128, 255, 255},
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T09 Metal shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
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

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::alloc()->init();
    textureDesc->setTextureType(MTL::TextureType2D);
    textureDesc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    textureDesc->setWidth(4);
    textureDesc->setHeight(4);
    textureDesc->setDepth(1);
    textureDesc->setMipmapLevelCount(3);
    textureDesc->setArrayLength(1);
    textureDesc->setSampleCount(1);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *mipTexture = device->newTexture(textureDesc);

    textureDesc->setTextureType(MTL::TextureType2DArray);
    textureDesc->setMipmapLevelCount(1);
    textureDesc->setArrayLength(3);
    MTL::Texture *arrayTexture = device->newTexture(textureDesc);

    textureDesc->setTextureType(MTL::TextureTypeCube);
    textureDesc->setArrayLength(1);
    MTL::Texture *cubeTexture = device->newTexture(textureDesc);
    textureDesc->release();

    uint8_t texels[4 * 4 * 4] = {};
    if(mipTexture)
    {
      for(uint32_t mip = 0; mip < 3; mip++)
      {
        const uint32_t size = 4U >> mip;
        Fill(texels, size, size, colours[mip]);
        mipTexture->replaceRegion(MTL::Region::Make2D(0, 0, size, size), mip, texels, size * 4);
      }
    }
    if(arrayTexture)
    {
      for(uint32_t slice = 0; slice < 3; slice++)
      {
        Fill(texels, 4, 4, colours[3 + slice]);
        arrayTexture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, slice, texels, 16, 64);
      }
    }
    if(cubeTexture)
    {
      for(uint32_t face = 0; face < 6; face++)
      {
        Fill(texels, 4, 4, colours[6 + face]);
        cubeTexture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, face, texels, 16, 64);
      }
    }

    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMipFilter(MTL::SamplerMipFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setRAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();

    if(vertexFunction == NULL || fragmentFunction == NULL || pipeline == NULL ||
       vertexBuffer == NULL || mipTexture == NULL || arrayTexture == NULL || cubeTexture == NULL ||
       sampler == NULL)
    {
      TEST_WARN("Failed to create T09 Metal resources");
      return 4;
    }

    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable == NULL)
      {
        pool->drain();
        continue;
      }
      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->setFragmentTexture(mipTexture, 0);
      encoder->setFragmentTexture(arrayTexture, 1);
      encoder->setFragmentTexture(cubeTexture, 2);
      encoder->setFragmentSamplerState(sampler, 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();
      pool->drain();
    }

    sampler->release();
    cubeTexture->release();
    arrayTexture->release();
    mipTexture->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
