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

#include "Code/Interface/QRDInterface.h"

#include "Code/QRDUtils.h"
#include "Helpers.h"

ICaptureContext *BufferInterpreter::context = NULL;

ParsedBufferFormat BufferInterpreter::Parse(rdcstr format)
{
  ParsedBufferFormat ret;

  ParsedFormat tmp = BufferFormatter::ParseFormatString(format, 0, false);

  ret.fixedHeader = tmp.fixed;
  ret.structure = tmp.repeating;
  ret.packing = tmp.packing;

  for(auto err = tmp.errors.begin(); err != tmp.errors.end(); ++err)
    ret.errors.push_back({err.key(), rdcstr(err.value())});

  return ret;
}

rdcstr BufferInterpreter::Unparse(ShaderConstantType structType, PackingRules pack, ResourceId shader)
{
  return BufferFormatter::DeclareStruct(pack, shader, structType.name, structType.members,
                                        structType.arrayByteStride);
}

PackingRules BufferInterpreter::EstimatePackingRules(ShaderConstantType baseType, ResourceId shader)
{
  return BufferFormatter::EstimatePackingRules(shader, baseType.members);
}

ShaderConstantType BufferInterpreter::GetPointerValType(PointerVal val)
{
  return PointerTypeRegistry::GetTypeDescriptor(val);
}

ShaderConstantType BufferInterpreter::GetPointerType(uint32_t pointerTypeId, ResourceId shader)
{
  return PointerTypeRegistry::GetTypeDescriptor(shader, pointerTypeId);
}

rdcpair<ResourceId, uint64_t> BufferInterpreter::LookupPointer(uint64_t pointerAddress,
                                                               uint64_t minSize)
{
  if(context)
  {
    for(const BufferDescription &b : context->GetBuffers())
    {
      if(b.gpuAddress && b.gpuAddress <= pointerAddress &&
         b.gpuAddress + b.length > pointerAddress + minSize)
      {
        return {b.resourceId, pointerAddress - b.gpuAddress};
      }
    }
  }

  return {ResourceId(), 0};
}

uint32_t BufferInterpreter::GetVariableAdvance(PackingRules pack, const ShaderConstant &var)
{
  return BufferFormatter::GetVarAdvance(pack, var);
}

rdcarray<ShaderVariable> BufferInterpreter::GetShaderVariables(const ShaderConstant &elem,
                                                               const bytebuf &data,
                                                               int32_t maxVariables)
{
  rdcarray<ShaderVariable> ret;
  const byte *cur = data.begin();
  const byte *end = data.end();

  for(int i = 0; cur < end && (i < maxVariables || maxVariables == -1); i++)
  {
    ret.push_back(InterpretShaderVar(elem, cur, end));

    if(elem.type.arrayByteStride == 0)
      break;

    cur += elem.type.arrayByteStride;
  }

  return ret;
}
