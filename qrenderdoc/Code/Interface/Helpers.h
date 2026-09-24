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

// NOTE: If any of these rules or the standard packings change, make sure to update
// BufferFormatter::EstimatePackingRules
DOCUMENT(R"(
PackingRules()
PackingRules(other: PackingRules)

A description of individual rules for how data is packed in GPU-side structures and
buffers.

Each individual member rule is such that ``False`` is more restrictive on packing, and ``True``
is less restrictive.

Several helpers are available for the common formats, see :meth:`PackingRules.STD140`, 
:meth:`PackingRules.STD430`, :meth:`PackingRules.D3DCB`, :meth:`PackingRules.C` which provide
a quick way to fetch a known set of packing rules.
)");
struct PackingRules
{
  DOCUMENT("");
  PackingRules() = default;
  PackingRules(bool vectorAlignComponent, bool vectorStraddle16b, bool tightArrays,
               bool trailingOverlap, bool tightBitfieldPacking)
      : vectorAlignComponent(vectorAlignComponent),
        vectorStraddle16b(vectorStraddle16b),
        tightArrays(tightArrays),
        trailingOverlap(trailingOverlap),
        tightBitfieldPacking(tightBitfieldPacking)
  {
  }

  bool operator==(PackingRules o) const
  {
    return vectorAlignComponent == o.vectorAlignComponent &&
           vectorStraddle16b == o.vectorStraddle16b && tightArrays == o.tightArrays &&
           trailingOverlap == o.trailingOverlap && tightBitfieldPacking == o.tightBitfieldPacking;
  }
  bool operator!=(PackingRules o) const { return !(*this == o); }

  //  property  | vector_align_component | vector_straddle_16b | tight_arrays | trailing_overlap
  //            |         false          |        false        |     false    |      false
  DOCUMENT(R"(
:return: The GLSL std140 rules, used on OpenGL and Vulkan.
:rtype: PackingRules
)");
  inline static const PackingRules STD140()
  {
    return PackingRules(false, false, false, false, false);
  }

  //            |         false          |        false        |      true    |      false
  DOCUMENT(R"(
:return: The GLSL std430 rules, used on OpenGL and Vulkan.
:rtype: PackingRules
)");
  inline static const PackingRules STD430()
  {
    return PackingRules(false, false, true, false, false);
  }

  //            |          true          |        false        |     false    |       true
  DOCUMENT(R"(
:return: The D3D11 and D3D12 Constant Buffer rules.
:rtype: PackingRules
)");
  inline static const PackingRules D3DCB() { return PackingRules(true, false, false, true, false); }

  //            |          true          |         true        |      true    |      false
  DOCUMENT(R"(
.. note::
  This is the standard C ABI with no special packing directives such as ``#pragma pack()``.

:return: The standard C packing rules.
:rtype: PackingRules
)");
  inline static const PackingRules C() { return PackingRules(true, true, true, false, false); }

  //            |          true          |         true        |      true    |       true
  DOCUMENT(R"(
:return: Vulkan scalar block layout rules, which are almost the same as C but in some cases
  allow more tight packing than C by default.
:rtype: PackingRules
)");
  inline static const PackingRules Scalar() { return PackingRules(true, true, true, true, false); }

  // D3D UAVs are assumed to be the same as C packing
  DOCUMENT(R"(
:return: The D3D Structured UAV buffer rules. This is considered to be the same as standard
  C packing rules.
:rtype: PackingRules
)");
  inline static const PackingRules D3DUAV() { return C(); }

  DOCUMENT(R"(Flag indicating if a vector's alignment is only equal to its component alignment.

If ``True`` this means vectors have no special alignment, and a 3-component float vector is
aligned to 4-byte the same as a scalar float.

If ``False``, 2-wide vectors are aligned to their size, and 3-wide and 4-wide vectors are aligned
to a 4-wide vector's size. E.g. ``float2`` has 8 byte alignment, ``float3`` and ``float4`` have
16-byte alignment.

:type: bool
)");
  bool vectorAlignComponent = false;

  DOCUMENT(R"(Flag indicating if vectors can straddle a 16-byte boundary with their components.

If ``True`` this means vectors have no special restrictions and can be at any aligned byte offset.

If ``False``, vectors must be at an offset such that the whole vector sits within the same
16-byte aligned region.

:type: bool
)");
  bool vectorStraddle16b = false;

  DOCUMENT(R"(Flag indicating if arrays are tightly packed with the stride being their natural
alignment.

If ``True`` this means each array element is at a suitably aligned offset after the previous.

If ``False``, each array element is at a 16-byte aligned offset after the previous regardless
of the element size and alignment.

:type: bool
)");
  bool tightArrays = false;

  DOCUMENT(R"(Flag indicating if the trailing alignment padding space after a struct member can
be used by the next member.

If ``True`` then a struct that contains e.g. 2 bytes of padding at its end to align it to a 4-byte
alignment can be followed by a 2-byte member which can have an offset inside that padding region.

If ``False`` then a struct consumes its padding space and the next member starts after the struct
even if it would otherwise fit and be aligned.

.. warning::
  This is not supported by standard C layouts, but is supported by some GPU layouts.

:type: bool
)");
  bool trailingOverlap = false;

  // whether bitfields will allow themselves to straddle their base type, or be aligned to stay
  // within it. Equivalent to #pragma pack(1) in C++
  DOCUMENT(R"(tight_bitfield_packing desc

:type: bool
)");
  DOCUMENT(R"(Flag indicating if bitfields allow bit-packed regions to straddle the base type.

If ``True`` this means bit regions are tightly packed even if one region would cross two
base elements. E.g. if the base type is uint then three 20-bit regions would only
consume two uints as that is 60 bits total.

If ``False`` this means each bit region ensures it only exists in one base element with
padding/unused bits in elements as necessary. E.g. if the base type is uint then three 20-bit
regions would consume three uints as each region would be padded into its own element.

.. note::
  Bitfield packing is compiler defined on CPU but this mostly matches what happens with
  ``#pragma pack(1)``.

:type: bool
)");
  bool tightBitfieldPacking = false;
};

DECLARE_REFLECTION_STRUCT(PackingRules);

DOCUMENT(R"(
ParseError()
ParseError(other: ParseError)

An error that occurred while parsing a buffer format string.
)");
struct ParseError
{
  DOCUMENT("");
  ParseError() = default;

  bool operator==(const ParseError &o) const { return line == o.line && error == o.error; }
  bool operator<(const ParseError &o) const
  {
    if(line != o.line)
      return line < o.line;
    return error < o.error;
  }

  DOCUMENT(R"(The 0-based line number in the input string where the error occurred.

:type: int
)");
  int line;

  DOCUMENT(R"(The text of the format parsing error.

:type: str
)");
  rdcstr error;
};

DECLARE_REFLECTION_STRUCT(ParseError);

DOCUMENT(R"(
ParsedBufferFormat()
ParsedBufferFormat(other: ParsedBufferFormat)

The result of parsing a buffer format string.
)");
struct ParsedBufferFormat
{
  DOCUMENT("");
  ParsedBufferFormat() = default;

  DOCUMENT(R"(The fixed SoA variables before any repeating structure, if present.

On some APIs it is possible to declare a fixed size amount of SoA data before then
an unbounded/runtime array of AoS data. If the format string is interpreted this
way then the initial fixed data structure will be returned in this member

:type: renderdoc.ShaderConstant
)");
  ShaderConstant fixedHeader;

  DOCUMENT(R"(The main structure of data described by the input buffer string.

:type: renderdoc.ShaderConstant
)");
  ShaderConstant structure;

  DOCUMENT(R"(The packing rules specified in the buffer format string. If not explicitly
stated then the estimated rules will be given here, as the most conservative packing that
would work for the current API.

:type: PackingRules
)");
  PackingRules packing;

  DOCUMENT(R"(The list of errors encountered while processing the format string.

:type: List[ParseError]
)");
  rdcarray<ParseError> errors;
};

DECLARE_REFLECTION_STRUCT(ParsedBufferFormat);

DOCUMENT(R"(
BufferInterpreter()
BufferInterpreter(other: BufferInterpreter)

A helper class with static methods to assist with interpreting buffer data
according to a given format or parsing format strings into format structures.
)");
struct BufferInterpreter
{
public:
  BufferInterpreter() = default;
  ~BufferInterpreter() = default;

#ifndef SWIG
  // for static lookups
  static ICaptureContext *context;
#endif

  DOCUMENT(R"(Parse a buffer format string and return the format in a structure that can be
inspected or used to interpret data.

For more information see :ref:`how_buffer_format`.

.. warning::
  For format strings that specify pointers, care should be taken that
  :data:`~renderdoc.ShaderConstantType.pointerTypeID` is an opaque identifier as there is no
  shader reflection to look up. You should use :meth:`GetPointerType` to obtain the type
  description of pointer types.

:param str format: The format string to interpret
:return: The result of parsing the format string.
:rtype: ParsedBufferFormat
)");
  static ParsedBufferFormat Parse(rdcstr format);

  DOCUMENT(R"(Generate a buffer format string from a known structure type.

This may not generate exactly, depending on the exact structure type. It may also not roundtrip
exactly when used with :meth:`Parse`.

:param renderdoc.ShaderConstantType structType: The structure type to unparse.
:param PackingRules pack: The packing rules to use when generating the string.
:param renderdoc.ResourceId shader=ResourceId(): **Optional parameter**. The shader this struct
  came from, used for determining the types of any pointer variables. Can be omitted if the struct
  type did not come from a shader.
:return: The generated format string, or an empty string if an error happened.
:rtype: str
)");
  static rdcstr Unparse(ShaderConstantType structType, PackingRules pack,
                        ResourceId shader = ResourceId());

  DOCUMENT(R"(Estimate the packing rules that apply to a given structure type.

This is not exact, as less strict packing rules will apply equally to structures that use more strict
packing rules. This function returns only the most-strict set of rules that would be valid.

:param renderdoc.ShaderConstantType baseType: The structure type to analyse.
:param renderdoc.ResourceId shader=ResourceId(): **Optional parameter**. The shader this struct
  came from, used for determining the types of any pointer variables. Can be omitted if the struct
  type did not come from a shader.
:return: The most conservative packing rules that satisfy the given type
:rtype: PackingRules
)");
  static PackingRules EstimatePackingRules(ShaderConstantType baseType,
                                           ResourceId shader = ResourceId());

  DOCUMENT(R"(Return the type description of a given pointer.

Returns an empty type if the value is invalid.

:param renderdoc.PointerVal val: The pointer value to inspect.
:return: The type description.
:rtype: renderdoc.ShaderConstantType
)");
  static ShaderConstantType GetPointerValType(PointerVal val);

  DOCUMENT(R"(Return the type description of a pointer ID. If this is from shader reflection
the ID is an index into :data:`~renderdoc.ShaderReflection.pointerTypes`. If the pointer type
was generated by parsing a buffer string it will be an arbitrary index.

Returns an empty type if the type ID is invalid or if a shader is not provided for a shader-based
type ID.

:param int pointerTypeId: The ID of the pointer type.
:param renderdoc.ResourceId shader=ResourceId(): **Optional parameter**. The shader this struct
  came from, used for determining the types of any pointer variables. Can be omitted if the struct
  type did not come from a shader.
:return: The type description.
:rtype: renderdoc.ShaderConstantType
)");
  static ShaderConstantType GetPointerType(uint32_t pointerTypeId, ResourceId shader = ResourceId());

  DOCUMENT(R"(Look up a pointer address and return the :data:`~renderdoc.ResourceId` and offset of
the containing buffer.

Due to buffer aliasing, it is possible a different overlapping buffer will be returned since there is
no way to differentiate based on purely an address. The :paramref:`LookupPointer.minSize` parameter can
be used to ensure that a range sits in the same buffer.

If no matching buffer is found then the result is default initialised.

This looks through the buffers available in :meth:`~CaptureContext.GetBuffers` and compares against
:data:`~renderdoc.BufferDescription.gpuAddress` and :data:`~renderdoc.BufferDescription.length`.

:param int pointerAddress: The address of the pointer to look up.
:param int minSize=0: **Optional parameter**. The minimum number of bytes that must be available
  after the address in the same buffer.
:return: The buffer ID and offset within that buffer.
:rtype: Tuple[renderdoc.ResourceId,int]
)");
  static rdcpair<ResourceId, uint64_t> LookupPointer(uint64_t pointerAddress, uint64_t minSize = 0);

  DOCUMENT(R"(Return the number of bytes between the start and end of a variable type.

.. note::
  Depending on the packing rules specified, this may not include the necessary padding or alignment
  needed for the given type and may calculate only the offset to the end of the variable. If you
  are calculating the offsets of elements of an array you should use
  :data:`~renderdoc.ShaderConstantType.arrayByteStride`.

:param PackingRules pack: The packing rules to use when calculating the advance.
:param renderdoc.ShaderConstant var: The variable to calculate.
:rtype: int
)");
  static uint32_t GetVariableAdvance(PackingRules pack, const ShaderConstant &var);

  DOCUMENT(R"(Return a list of shader variables by interpreting a ``bytes`` with a given type description.

The number of variables can be limited for example to 1 if needed without needing to calculate how to
truncate the byte data.

This will always return a whole number of variables even if the byte data is insufficient, with standard
out-of-bound reads returning 0 data.

:param renderdoc.ShaderConstant var: The description of the variable to interpret
:param bytes data: The byte data to read the variable from.
:param int maxVariables=-1: **Optional parameter**. The maximum number of variables to read, or -1 to
  read as many as can fit in the given byte data.
:rtype: List[renderdoc.ShaderVariable]
)");
  static rdcarray<ShaderVariable> GetShaderVariables(const ShaderConstant &var, const bytebuf &data,
                                                     int32_t maxVariables = -1);
};

DECLARE_REFLECTION_STRUCT(BufferInterpreter);
