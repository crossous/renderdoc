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

RD_TEST(Metal_Simple_Triangle, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a coloured triangle from an MSL source library and a shared vertex buffer.";

  struct Vertex
  {
    float position[4];
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct Vertex
{
  float4 position;
  float4 colour;
};

struct VSOut
{
  float4 position [[position]];
  float4 colour;
};

vertex VSOut vs_main(const device Vertex *vertices [[buffer(0)]], uint vertexID [[vertex_id]])
{
  VSOut output;
  output.position = vertices[vertexID].position;
  output.colour = vertices[vertexID].colour;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    const Vertex vertices[] = {
        {{0.0f, 0.72f, 0.0f, 1.0f}, {1.0f, 0.15f, 0.10f, 1.0f}},
        {{-0.72f, -0.62f, 0.0f, 1.0f}, {0.10f, 0.85f, 0.20f, 1.0f}},
        {{0.72f, -0.62f, 0.0f, 1.0f}, {0.10f, 0.35f, 1.0f, 1.0f}},
    };

    NS::Error *error = NULL;
    NS::String *source = NS::String::string(shaderSource, NS::UTF8StringEncoding);
    MTL::Library *library = device->newLibrary(source, NULL, &error);
    if(library == NULL)
    {
      const char *message = error ? error->localizedDescription()->utf8String() : "unknown error";
      TEST_WARN("Failed to compile Metal shader: %s", message);
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
      const char *message = error ? error->localizedDescription()->utf8String() : "unknown error";
      TEST_WARN("Failed to create Metal render pipeline: %s", message);
      return 4;
    }

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    if(vertexBuffer == NULL)
    {
      TEST_WARN("Failed to create Metal vertex buffer");
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
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.08, 0.08, 0.10, 1.0));
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
