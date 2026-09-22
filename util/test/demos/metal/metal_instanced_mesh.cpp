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

RD_TEST(Metal_Instanced_Mesh, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws three coloured instances from separate per-vertex and per-instance buffers.";

  struct Position
  {
    float position[2];
  };

  struct Instance
  {
    float offset[2];
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
  float2 position [[attribute(0)]];
  float2 instanceOffset [[attribute(1)]];
  float4 instanceColour [[attribute(2)]];
};

struct VSOut
{
  float4 position [[position]];
  float4 colour;
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position + input.instanceOffset, 0.0, 1.0);
  output.colour = input.instanceColour;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    const Position positions[] = {
        {{-0.18f, -0.18f}},
        {{0.18f, -0.18f}},
        {{0.00f, 0.22f}},
    };

    // Element zero is deliberately skipped by baseInstance=1. It makes an incorrect base-instance
    // replay immediately visible as a magenta triangle near the top edge.
    const Instance instances[] = {
        {{0.00f, 1.40f}, {1.00f, 0.00f, 1.00f, 1.00f}},
        {{-0.55f, 0.00f}, {1.00f, 0.125f, 0.0625f, 1.00f}},
        {{0.00f, 0.00f}, {0.0625f, 0.875f, 0.1875f, 1.00f}},
        {{0.55f, 0.00f}, {0.09375f, 0.25f, 1.00f, 1.00f}},
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
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Position, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Instance, offset));
    vertexDesc->attributes()->object(1)->setBufferIndex(1);
    vertexDesc->attributes()->object(2)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(2)->setOffset(offsetof(Instance, colour));
    vertexDesc->attributes()->object(2)->setBufferIndex(1);

    vertexDesc->layouts()->object(0)->setStride(sizeof(Position));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);
    vertexDesc->layouts()->object(1)->setStride(sizeof(Instance));
    vertexDesc->layouts()->object(1)->setStepFunction(MTL::VertexStepFunctionPerInstance);
    vertexDesc->layouts()->object(1)->setStepRate(1);

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

    MTL::Buffer *positionBuffer =
        device->newBuffer(positions, sizeof(positions), MTL::ResourceStorageModeShared);
    MTL::Buffer *instanceBuffer =
        device->newBuffer(instances, sizeof(instances), MTL::ResourceStorageModeShared);
    if(positionBuffer == NULL || instanceBuffer == NULL)
    {
      TEST_WARN("Failed to create T05 vertex buffers");
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
      encoder->setVertexBuffer(positionBuffer, 0, 0);
      encoder->setVertexBuffer(instanceBuffer, 0, 1);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3),
                              NS::UInteger(3), NS::UInteger(1));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();

      EndCaptureFrame();
      pool->drain();
    }

    instanceBuffer->release();
    positionBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
