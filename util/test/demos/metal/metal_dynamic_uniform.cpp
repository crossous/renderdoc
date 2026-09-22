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

RD_TEST(Metal_Dynamic_Uniform, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws two viewports from one fragment uniform buffer using two aligned offsets.";

  struct UniformData
  {
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VSOut
{
  float4 position [[position]];
};

struct UniformData
{
  float4 colour;
};

vertex VSOut vs_main(uint vertexID [[vertex_id]])
{
  const float2 positions[] = {
      float2(-1.0, -1.0),
      float2(3.0, -1.0),
      float2(-1.0, 3.0),
  };

  VSOut output;
  output.position = float4(positions[vertexID], 0.0, 1.0);
  return output;
}

fragment float4 fs_main(constant UniformData &uniforms [[buffer(0)]])
{
  return uniforms.colour;
}
)EOSHADER";

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

    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    if(pipeline == NULL)
    {
      TEST_WARN("Failed to create Metal render pipeline: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    byte uniformBytes[512] = {};
    const UniformData left = {{1.0f, 0.125f, 0.0625f, 1.0f}};
    const UniformData right = {{0.0625f, 0.875f, 0.1875f, 1.0f}};
    memcpy(uniformBytes, &left, sizeof(left));
    memcpy(uniformBytes + 256, &right, sizeof(right));

    MTL::Buffer *uniformBuffer =
        device->newBuffer(uniformBytes, sizeof(uniformBytes), MTL::ResourceStorageModeShared);
    if(uniformBuffer == NULL)
    {
      TEST_WARN("Failed to create T04 uniform buffer");
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
      encoder->setFragmentBuffer(uniformBuffer, 0, 0);

      const double halfWidth = double(screenWidth) * 0.5;
      encoder->setViewport(MTL::Viewport{0.0, 0.0, halfWidth, double(screenHeight), 0.0, 1.0});
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

      encoder->setFragmentBufferOffset(256, 0);
      encoder->setViewport(
          MTL::Viewport{halfWidth, 0.0, halfWidth, double(screenHeight), 0.0, 1.0});
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    uniformBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
