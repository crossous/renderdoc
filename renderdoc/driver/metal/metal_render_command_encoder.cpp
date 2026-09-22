/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "metal_render_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_depth_stencil_state.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"
#include "metal_replay.h"
#include "metal_sampler_state.h"
#include "metal_texture.h"

WrappedMTLRenderCommandEncoder::WrappedMTLRenderCommandEncoder(
    MTL::RenderCommandEncoder *realMTLRenderCommandEncoder, ResourceId objId,
    WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLRenderCommandEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realMTLRenderCommandEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(pipelineState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setRenderPipelineState(Unwrap(pipelineState));
    m_Device->GetReplay()->BindRenderPipeline(GetResID(pipelineState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setRenderPipelineState(WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setRenderPipelineState(Unwrap(pipelineState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState);
      Serialise_setRenderPipelineState(ser, pipelineState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(pipelineState), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBuffer(SerialiserType &ser,
                                                               WrappedMTLBuffer *buffer,
                                                               NS::UInteger offset,
                                                               NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setVertexBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindVertexBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                                     NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffer);
      Serialise_setVertexBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBuffer(SerialiserType &ser,
                                                                 WrappedMTLBuffer *buffer,
                                                                 NS::UInteger offset,
                                                                 NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindFragmentBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBufferOffset(SerialiserType &ser,
                                                                       NS::UInteger offset,
                                                                       NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentBufferOffset(offset, index);
    m_Device->GetReplay()->SetFragmentBufferOffset((uint32_t)index, (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentBufferOffset(NS::UInteger offset,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBufferOffset(offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBufferOffset);
      Serialise_setFragmentBufferOffset(ser, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
}

void WrappedMTLRenderCommandEncoder::setFragmentBuffer(WrappedMTLBuffer *buffer,
                                                       NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer);
      Serialise_setFragmentBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentTexture(SerialiserType &ser,
                                                                  WrappedMTLTexture *texture,
                                                                  NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentTexture(Unwrap(texture), index);
    m_Device->GetReplay()->BindFragmentTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentTexture(Unwrap(texture), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentTexture);
      Serialise_setFragmentTexture(ser, texture, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerState(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindFragmentSampler((uint32_t)index, GetResID(sampler));
  }

  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerState(WrappedMTLSamplerState *sampler,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerState(Unwrap(sampler), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState);
      Serialise_setFragmentSamplerState(ser, sampler, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setViewport(SerialiserType &ser,
                                                           MTL::Viewport &viewport)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(viewport).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setViewport(viewport);
    m_Device->GetReplay()->SetViewport(viewport);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setViewport(MTL::Viewport &viewport)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setViewport(viewport));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setViewport);
      Serialise_setViewport(ser, viewport);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setScissorRect(SerialiserType &ser,
                                                              MTL::ScissorRect &rect)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(rect).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setScissorRect(rect);
    m_Device->GetReplay()->SetScissor(rect);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setScissorRect(MTL::ScissorRect &rect)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setScissorRect(rect));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setScissorRect);
      Serialise_setScissorRect(ser, rect);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFrontFacingWinding(SerialiserType &ser,
                                                                    MTL::Winding winding)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(winding).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFrontFacingWinding(winding);
    m_Device->GetReplay()->SetFrontFacingWinding(winding);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFrontFacingWinding(MTL::Winding winding)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFrontFacingWinding(winding));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFrontFacingWinding);
      Serialise_setFrontFacingWinding(ser, winding);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setCullMode(SerialiserType &ser,
                                                           MTL::CullMode cullMode)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(cullMode).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setCullMode(cullMode);
    m_Device->GetReplay()->SetCullMode(cullMode);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setCullMode(MTL::CullMode cullMode)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setCullMode(cullMode));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setCullMode);
      Serialise_setCullMode(ser, cullMode);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthStencilState(
    SerialiserType &ser, WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(depthStencilState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setDepthStencilState(Unwrap(depthStencilState));
    m_Device->GetReplay()->BindDepthStencilState(GetResID(depthStencilState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthStencilState(
    WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthStencilState(Unwrap(depthStencilState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthStencilState);
      Serialise_setDepthStencilState(ser, depthStencilState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(depthStencilState), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValue(
    SerialiserType &ser, uint32_t referenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(referenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setStencilReferenceValue(referenceValue);
    m_Device->GetReplay()->SetStencilReferenceValue(referenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValue(uint32_t referenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValue(referenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilReferenceValue);
      Serialise_setStencilReferenceValue(ser, referenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValues(
    SerialiserType &ser, uint32_t frontReferenceValue, uint32_t backReferenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(frontReferenceValue).Important();
  SERIALISE_ELEMENT(backReferenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)
        ->setStencilReferenceValues(frontReferenceValue, backReferenceValue);
    m_Device->GetReplay()->SetStencilReferenceValues(frontReferenceValue, backReferenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValues(uint32_t frontReferenceValue,
                                                                uint32_t backReferenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValues(frontReferenceValue,
                                                               backReferenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilFrontReferenceValue);
      Serialise_setStencilReferenceValues(ser, frontReferenceValue, backReferenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
    NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(vertexStart);
  SERIALISE_ELEMENT(vertexCount).Important();
  SERIALISE_ELEMENT(instanceCount);
  SERIALISE_ELEMENT(baseInstance);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    Unwrap(RenderCommandEncoder)
        ->drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawPrimitives(%llu)", (uint64_t)vertexCount);
      action.flags = ActionFlags::Drawcall;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)vertexCount;
      action.numInstances = (uint32_t)instanceCount;
      action.vertexOffset = (uint32_t)vertexStart;
      action.instanceOffset = (uint32_t)baseInstance;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount,
                                                    NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitiveType, vertexStart, vertexCount,
                                                   instanceCount, baseInstance));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced);
      Serialise_drawPrimitives(ser, primitiveType, vertexStart, vertexCount, instanceCount,
                               baseInstance);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    m_Device->GetReplay()->BindIndexBuffer(GetResID(indexBuffer), indexBufferOffset, indexType);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexCount, indexType, Unwrap(indexBuffer),
                                indexBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPrimitives(%llu)", (uint64_t)indexCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      action.numIndices = (uint32_t)indexCount;
      action.numInstances = 1;
      action.indexOffset = (uint32_t)(indexBufferOffset /
                                      (indexType == MTL::IndexTypeUInt16 ? 2 : 4));
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives);
      Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                      indexBufferOffset);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, 1, 0);
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, 0);
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->endEncoding();
    m_Device->SetReplayRenderCommandEncoder(NULL);

    ActionDescription action;
    if(IsLoading(m_State))
    {
      action.customName = "End Metal Render Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      m_Device->GetReplay()->SetActionOutputs(action);
    }

    m_Device->GetReplay()->EndRenderPass();

    if(IsLoading(m_State))
    {
      AddEvent();
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_endEncoding);
      Serialise_endEncoding(ser);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setRenderPipelineState,
                                WrappedMTLRenderPipelineState *pipelineState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBufferOffset,
                                NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setViewport,
                                MTL::Viewport &viewport);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setScissorRect,
                                MTL::ScissorRect &rect);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFrontFacingWinding,
                                MTL::Winding winding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setCullMode,
                                MTL::CullMode cullMode);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthStencilState,
                                WrappedMTLDepthStencilState *depthStencilState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValue,
                                uint32_t referenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValues,
                                uint32_t frontReferenceValue, uint32_t backReferenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
                                NS::UInteger vertexCount, NS::UInteger instanceCount,
                                NS::UInteger baseInstance);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                                MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                                NS::UInteger indexBufferOffset);
