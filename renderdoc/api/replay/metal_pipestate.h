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

#pragma once

#include "common_pipestate.h"

// NOTE: Python sees namespaces flattened to a prefix, so MetalPipe::State is MetalState.
namespace MetalPipe
{
DOCUMENT(R"(
MetalShader()
MetalShader(other: MetalShader)

Describes a shader function bound to a Metal pipeline stage.
)");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the Metal function.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  const ShaderReflection *reflection = NULL;

  DOCUMENT(R"(The source entry point name.

:type: str
)");
  rdcstr entryPoint;

  DOCUMENT(R"(The pipeline stage this function is bound to.

:type: ShaderStage
)");
  ShaderStage stage = ShaderStage::Vertex;
};

DOCUMENT(R"(
MetalVertexBuffer()
MetalVertexBuffer(other: MetalVertexBuffer)

Describes a Metal vertex buffer binding.
)");
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the bound buffer.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The byte offset from the start of the buffer.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes available from :data:`byteOffset`.

:type: int
)");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The vertex stride when it is available from a vertex descriptor.

:type: int
)");
  uint32_t byteStride = 0;

  DOCUMENT(R"(Whether this binding advances per instance instead of per vertex.

:type: bool
)");
  bool perInstance = false;

  DOCUMENT(R"(The Metal vertex layout step rate.

:type: int
)");
  uint32_t stepRate = 1;
};

DOCUMENT(R"(
MetalVertexAttribute()
MetalVertexAttribute(other: MetalVertexAttribute)

Describes an attribute in a Metal vertex descriptor.
)");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;
  VertexAttribute &operator=(const VertexAttribute &) = default;

  DOCUMENT(R"(The Metal attribute index.

:type: int
)");
  uint32_t attributeIndex = 0;

  DOCUMENT(R"(The vertex buffer slot used by the attribute.

:type: int
)");
  uint32_t bufferIndex = 0;

  DOCUMENT(R"(The byte offset within each vertex element.

:type: int
)");
  uint32_t byteOffset = 0;

  DOCUMENT(R"(The format of the attribute.

:type: ResourceFormat
)");
  ResourceFormat format;
};

DOCUMENT(R"(
MetalRasterizer()
MetalRasterizer(other: MetalRasterizer)

Describes Metal dynamic rasterizer state used by the current draw.
)");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer()
  {
    viewport.enabled = false;
    scissor.enabled = false;
  }
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  Viewport viewport;
  Scissor scissor;
  CullMode cullMode = CullMode::NoCull;
  bool frontCCW = false;
};

DOCUMENT(R"(
MetalDepthStencil()
MetalDepthStencil(other: MetalDepthStencil)

Describes the Metal depth state used by the current draw.
)");
struct DepthStencil
{
  DOCUMENT("");
  DepthStencil() = default;
  DepthStencil(const DepthStencil &) = default;
  DepthStencil &operator=(const DepthStencil &) = default;

  ResourceId resourceId;
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  bool depthWrites = false;
};

DOCUMENT(R"(
The current Metal render pipeline state.
)");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT(R"(The currently bound render pipeline state object.

:type: ResourceId
)");
  ResourceId pipelineResourceId;

  DOCUMENT(R"(The bound vertex function.

:type: MetalShader
)");
  Shader vertexShader;

  DOCUMENT(R"(The bound fragment function.

:type: MetalShader
)");
  Shader fragmentShader;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;

  DOCUMENT(R"(The current vertex buffer bindings, indexed by Metal buffer slot.

:type: List[MetalVertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The attributes from the currently bound Metal vertex descriptor.

:type: List[MetalVertexAttribute]
)");
  rdcarray<VertexAttribute> vertexAttributes;

  DOCUMENT(R"(The fragment-stage texture bindings, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> fragmentTextures;

  DOCUMENT(R"(The fragment-stage sampler bindings, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> fragmentSamplers;

  DOCUMENT(R"(The index buffer used by the current indexed draw.

:type: MetalVertexBuffer
)");
  VertexBuffer indexBuffer;

  DOCUMENT(R"(The dynamic rasterizer state used by the current draw.

:type: MetalRasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The bound depth-stencil state.

:type: MetalDepthStencil
)");
  DepthStencil depthStencil;

  DOCUMENT(R"(The current color render targets.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> colorTargets;

  DOCUMENT(R"(The current depth render target.

:type: Descriptor
)");
  Descriptor depthTarget;
};
};    // namespace MetalPipe

DECLARE_REFLECTION_STRUCT(MetalPipe::Shader);
DECLARE_REFLECTION_STRUCT(MetalPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(MetalPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(MetalPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(MetalPipe::DepthStencil);
DECLARE_REFLECTION_STRUCT(MetalPipe::State);
