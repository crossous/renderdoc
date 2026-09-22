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

#include "metal_texture.h"
#include "metal_device.h"

WrappedMTLTexture::WrappedMTLTexture(MTL::Texture *realMTLTexture, ResourceId objId,
                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLTexture, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLTexture && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_replaceRegion(SerialiserType &ser, MTL::Region &region,
                                                 NS::UInteger level, const void *pixelBytes,
                                                 NS::UInteger bytesPerRow)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();

  bytebuf contents;
  if(ser.IsWriting() && pixelBytes && bytesPerRow > 0 && region.size.height > 0)
  {
    const size_t dataSize = size_t(bytesPerRow) * size_t(region.size.height);
    contents.assign((const byte *)pixelBytes, dataSize);
  }
  SERIALISE_ELEMENT(contents).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(Texture)->replaceRegion(region, level, contents.data(), bytesPerRow);
  }

  return true;
}

void WrappedMTLTexture::replaceRegion(MTL::Region &region, NS::UInteger level,
                                      const void *pixelBytes, NS::UInteger bytesPerRow)
{
  SERIALISE_TIME_CALL(Unwrap(this)->replaceRegion(region, level, pixelBytes, bytesPerRow));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_replaceRegion);
      Serialise_replaceRegion(ser, region, level, pixelBytes, bytesPerRow);
      chunk = scope.Get();
    }

    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_PartialWrite);
    }
    else
    {
      GetRecord(this)->AddChunk(chunk);
    }
  }
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_replaceRegion(SerialiserType &ser, MTL::Region &region,
                                                 NS::UInteger level, NS::UInteger slice,
                                                 const void *pixelBytes, NS::UInteger bytesPerRow,
                                                 NS::UInteger bytesPerImage)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(slice).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();
  SERIALISE_ELEMENT(bytesPerImage).Important();

  bytebuf contents;
  if(ser.IsWriting() && pixelBytes && bytesPerRow > 0 && region.size.height > 0)
  {
    const size_t dataSize =
        bytesPerImage > 0 ? size_t(bytesPerImage) : size_t(bytesPerRow) * size_t(region.size.height);
    contents.assign((const byte *)pixelBytes, dataSize);
  }
  SERIALISE_ELEMENT(contents).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(Texture)->replaceRegion(region, level, slice, contents.data(), bytesPerRow,
                                   bytesPerImage);
  }

  return true;
}

void WrappedMTLTexture::replaceRegion(MTL::Region &region, NS::UInteger level, NS::UInteger slice,
                                      const void *pixelBytes, NS::UInteger bytesPerRow,
                                      NS::UInteger bytesPerImage)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->replaceRegion(region, level, slice, pixelBytes, bytesPerRow, bytesPerImage));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_replaceRegion_slice);
      Serialise_replaceRegion(ser, region, level, slice, pixelBytes, bytesPerRow, bytesPerImage);
      chunk = scope.Get();
    }

    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_PartialWrite);
    }
    else
    {
      GetRecord(this)->AddChunk(chunk);
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, replaceRegion, MTL::Region &,
                                NS::UInteger, const void *, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, replaceRegion, MTL::Region &,
                                NS::UInteger, NS::UInteger, const void *, NS::UInteger,
                                NS::UInteger);
