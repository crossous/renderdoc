#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
RENDERDOC_BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO_BIN="${REPO_ROOT}/bin/demos_x64"
RENDERDOC_LIB="${RENDERDOC_BUILD_DIR}/lib/librenderdoc.dylib"
RENDERDOCCMD="${RENDERDOC_BUILD_DIR}/bin/renderdoccmd"
OUTPUT_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_output_smoke"
LIFECYCLE_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_lifecycle_smoke"
TARGET_ARCH="$(uname -m)"

cmake -S "${REPO_ROOT}/util/test/demos" -B "${DEMO_BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"

"${SCRIPT_DIR}/build_metal_dev_macos.sh"

mkdir -p "${CAPTURE_DIR}"
rm -f "${CAPTURE_DIR}/t00_capture.rdc" "${CAPTURE_DIR}/t01_capture.rdc" \
      "${CAPTURE_DIR}/t02_capture.rdc" "${CAPTURE_DIR}/t03_capture.rdc" \
      "${CAPTURE_DIR}/t04_capture.rdc" "${CAPTURE_DIR}/t05_capture.rdc" \
      "${CAPTURE_DIR}/t06_capture.rdc" "${CAPTURE_DIR}/t07_capture.rdc" \
      "${CAPTURE_DIR}/t08_capture.rdc" "${CAPTURE_DIR}/t09_capture.rdc" \
      "${CAPTURE_DIR}/t00.xml" "${CAPTURE_DIR}/t01.xml" "${CAPTURE_DIR}/t02.xml" \
      "${CAPTURE_DIR}/t03.xml" "${CAPTURE_DIR}/t04.xml" "${CAPTURE_DIR}/t05.xml" \
      "${CAPTURE_DIR}/t06.xml" "${CAPTURE_DIR}/t07.xml" "${CAPTURE_DIR}/t08.xml" \
      "${CAPTURE_DIR}/t09.xml" \
      "${CAPTURE_DIR}/t01_event_clear.ppm" "${CAPTURE_DIR}/t01_event_draw.ppm" \
      "${CAPTURE_DIR}/t01_event_rewind.ppm" "${CAPTURE_DIR}/t01_texture.dds" \
      "${CAPTURE_DIR}/t02_replay.ppm" "${CAPTURE_DIR}/t03_replay.ppm" \
      "${CAPTURE_DIR}/t04_replay.ppm" "${CAPTURE_DIR}/t05_replay.ppm" \
      "${CAPTURE_DIR}/t06_replay.ppm" "${CAPTURE_DIR}/t07_replay.ppm" \
      "${CAPTURE_DIR}/t08_replay.ppm" "${CAPTURE_DIR}/t09_replay.ppm" \
      "${CAPTURE_DIR}/t09_cube.dds"

"${DEMO_BIN}" Metal_Empty_Frame --frames 5
"${DEMO_BIN}" Metal_Simple_Triangle --frames 5
"${DEMO_BIN}" Metal_Indexed_Cube --frames 5
"${DEMO_BIN}" Metal_Textured_Quad --frames 5
"${DEMO_BIN}" Metal_Dynamic_Uniform --frames 5
"${DEMO_BIN}" Metal_Instanced_Mesh --frames 5
"${DEMO_BIN}" Metal_MRT_Blend --frames 5
"${DEMO_BIN}" Metal_Depth_Stencil --frames 5
"${DEMO_BIN}" Metal_MSAA_Resolve --frames 5
"${DEMO_BIN}" Metal_Texture_Subresources --frames 5

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t00" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Empty_Frame --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t01" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Simple_Triangle --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t02" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indexed_Cube --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t03" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Textured_Quad --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t04" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Dynamic_Uniform --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t05" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Instanced_Mesh --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t06" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_MRT_Blend --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t07" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Depth_Stencil --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t08" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_MSAA_Resolve --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t09" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Texture_Subresources --frames 8

test -s "${CAPTURE_DIR}/t00_capture.rdc"
test -s "${CAPTURE_DIR}/t01_capture.rdc"
test -s "${CAPTURE_DIR}/t02_capture.rdc"
test -s "${CAPTURE_DIR}/t03_capture.rdc"
test -s "${CAPTURE_DIR}/t04_capture.rdc"
test -s "${CAPTURE_DIR}/t05_capture.rdc"
test -s "${CAPTURE_DIR}/t06_capture.rdc"
test -s "${CAPTURE_DIR}/t07_capture.rdc"
test -s "${CAPTURE_DIR}/t08_capture.rdc"
test -s "${CAPTURE_DIR}/t09_capture.rdc"

"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t00_capture.rdc" \
  -o "${CAPTURE_DIR}/t00.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t01_capture.rdc" \
  -o "${CAPTURE_DIR}/t01.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t02_capture.rdc" \
  -o "${CAPTURE_DIR}/t02.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t03_capture.rdc" \
  -o "${CAPTURE_DIR}/t03.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t04_capture.rdc" \
  -o "${CAPTURE_DIR}/t04.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t05_capture.rdc" \
  -o "${CAPTURE_DIR}/t05.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t06_capture.rdc" \
  -o "${CAPTURE_DIR}/t06.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t07_capture.rdc" \
  -o "${CAPTURE_DIR}/t07.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t08_capture.rdc" \
  -o "${CAPTURE_DIR}/t08.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t09_capture.rdc" \
  -o "${CAPTURE_DIR}/t09.xml" -c xml

rg -q 'driver id="11">Metal<' "${CAPTURE_DIR}/t00.xml"
rg -q 'name="MTLCommandBuffer::presentDrawable"' "${CAPTURE_DIR}/t00.xml"
rg -q 'name="MTLDevice::newLibraryWithSource"' "${CAPTURE_DIR}/t01.xml"
rg -q 'name="FunctionName" typename="NSString" important="true">vs_main<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="FunctionName" typename="NSString" important="true">fs_main<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="96"' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t01.xml"
rg -q 'name="vertexCount" typename="uint64_t" width="8" important="true">3<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="stride" typename="uint64_t" width="8">28<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLVertexFormatFloat3">30<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLVertexFormatFloat4">31<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float">252<' \
  "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthCompareFunction".*string="MTLCompareFunctionLess">1<' \
  "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthWriteEnabled" typename="bool">true<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLWindingCounterClockwise">1<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLCullModeBack">2<' "${CAPTURE_DIR}/t02.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setScissorRect"' "${CAPTURE_DIR}/t02.xml")" = "2"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawIndexedPrimitives"' "${CAPTURE_DIR}/t02.xml")" = "2"
test "$(rg -c 'name="indexCount" typename="uint64_t" width="8" important="true">36<' \
  "${CAPTURE_DIR}/t02.xml")" = "2"
rg -q 'name="indexType".*string="MTLIndexTypeUInt16">0<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="indexType".*string="MTLIndexTypeUInt32">1<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="224"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="72"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="144"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="MTLDevice::newTextureWithDescriptor"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatRGBA8Unorm">70<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="width" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="height" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLTexture::replaceRegion"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="contents" typename="Byte Buffer" important="true" byteLength="64"' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLDevice::newSamplerStateWithDescriptor"' "${CAPTURE_DIR}/t03.xml"
test "$(rg -c 'string="MTLSamplerMinMagFilterNearest">0<' "${CAPTURE_DIR}/t03.xml")" = "2"
test "$(rg -c 'string="MTLSamplerAddressModeClampToEdge">0<' "${CAPTURE_DIR}/t03.xml")" = "3"
rg -q 'name="MTLRenderCommandEncoder::setFragmentTexture"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="texture" typename="MTLTexture" width="8" important="true">17<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentSamplerState"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="sampler" typename="MTLSamplerState" width="8" important="true">18<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'string="MTLPrimitiveTypeTriangleStrip">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="vertexCount" typename="uint64_t" width="8" important="true">4<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="512"' \
  "${CAPTURE_DIR}/t04.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBuffer"' "${CAPTURE_DIR}/t04.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBufferOffset"' "${CAPTURE_DIR}/t04.xml"
rg -q 'name="offset" typename="uint64_t" width="8">256<' "${CAPTURE_DIR}/t04.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t04.xml")" = "2"
rg -q 'name="stride" typename="uint64_t" width="8">8<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="stride" typename="uint64_t" width="8">24<' "${CAPTURE_DIR}/t05.xml"
rg -q 'string="MTLVertexStepFunctionPerInstance">2<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="24"' \
  "${CAPTURE_DIR}/t05.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="96"' \
  "${CAPTURE_DIR}/t05.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setVertexBuffer"' "${CAPTURE_DIR}/t05.xml")" = "2"
rg -q 'name="instanceCount" typename="uint64_t" width="8">3<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="baseInstance" typename="uint64_t" width="8">1<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatBGRA8Unorm">80<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatRGBA8Unorm">70<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="sourceRGBBlendFactor".*string="MTLBlendFactorSourceAlpha">4<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="destinationRGBBlendFactor".*string="MTLBlendFactorOneMinusSourceAlpha">5<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="writeMask".*string="MTLColorWriteMaskAll">15<' "${CAPTURE_DIR}/t06.xml"
rg -q 'name="writeMask".*MTLColorWriteMaskBlue.*MTLColorWriteMaskGreen.*MTLColorWriteMaskRed">14<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="240"' \
  "${CAPTURE_DIR}/t06.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t06.xml")" = "2"
rg -q 'name="depthAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float_Stencil8">260<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="stencilAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float_Stencil8">260<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="stencilFailureOperation".*string="MTLStencilOperationZero">1<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="depthFailureOperation".*string="MTLStencilOperationIncrementClamp">3<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="depthStencilPassOperation".*string="MTLStencilOperationReplace">2<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="readMask" typename="uint32_t" width="4">63<' "${CAPTURE_DIR}/t07.xml"
rg -q 'name="writeMask" typename="uint32_t" width="4">255<' "${CAPTURE_DIR}/t07.xml"
rg -q 'name="MTLRenderCommandEncoder::setStencilFrontReferenceValue"' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="frontReferenceValue" typename="uint32_t" width="4" important="true">9<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="backReferenceValue" typename="uint32_t" width="4" important="true">5<' \
  "${CAPTURE_DIR}/t07.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setStencilReferenceValue"' \
  "${CAPTURE_DIR}/t07.xml")" = "5"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t07.xml")" = "5"
rg -q 'name="textureType".*string="MTLTextureType2DMultisample">4<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="sampleCount" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t08.xml"
rg -q 'name="rasterSampleCount" typename="uint64_t" width="8">4<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="resolveTexture" typename="MTLTexture" width="8">[1-9][0-9]*<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="storeAction".*string="MTLStoreActionMultisampleResolve">2<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="216"' \
  "${CAPTURE_DIR}/t08.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t08.xml")" = "3"
rg -q 'name="textureType".*string="MTLTextureType2D">2<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="textureType".*string="MTLTextureType2DArray">3<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="textureType".*string="MTLTextureTypeCube">5<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="mipmapLevelCount" typename="uint64_t" width="8">3<' \
  "${CAPTURE_DIR}/t09.xml"
rg -q 'name="arrayLength" typename="uint64_t" width="8">3<' "${CAPTURE_DIR}/t09.xml"
test "$(rg -c 'name="MTLTexture::replaceRegion"' "${CAPTURE_DIR}/t09.xml")" = "12"
test "$(rg -c 'name="slice" typename="uint64_t" width="8" important="true"' \
  "${CAPTURE_DIR}/t09.xml")" = "9"
test "$(rg -c 'name="MTLRenderCommandEncoder::setFragmentTexture"' \
  "${CAPTURE_DIR}/t09.xml")" = "3"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t09.xml")" = "1"

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${OUTPUT_SMOKE}"

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_lifecycle_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${LIFECYCLE_SMOKE}"

"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t00_capture.rdc" "${CAPTURE_DIR}/t00_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t01_capture.rdc" "${CAPTURE_DIR}/t01_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t01_capture.rdc" \
  "${CAPTURE_DIR}/t01_event_clear.ppm" \
  "${CAPTURE_DIR}/t01_event_draw.ppm" \
  "${CAPTURE_DIR}/t01_event_rewind.ppm" \
  "${CAPTURE_DIR}/t01_texture.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t02_capture.rdc" "${CAPTURE_DIR}/t02_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t03_capture.rdc" "${CAPTURE_DIR}/t03_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t04_capture.rdc" "${CAPTURE_DIR}/t04_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t05_capture.rdc" "${CAPTURE_DIR}/t05_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t06_capture.rdc" "${CAPTURE_DIR}/t06_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t07_capture.rdc" "${CAPTURE_DIR}/t07_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t08_capture.rdc" "${CAPTURE_DIR}/t08_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t09_capture.rdc" "${CAPTURE_DIR}/t09_replay.ppm" \
  "${CAPTURE_DIR}/t09_cube.dds"

read_rgb()
{
  local image="$1"
  local x="$2"
  local y="$3"
  local offset=$((15 + (y * 640 + x) * 3))
  dd if="${image}" bs=1 skip="${offset}" count=3 2>/dev/null | xxd -p
}

test "$(read_rgb "${CAPTURE_DIR}/t00_replay.ppm" 320 240)" = "14335c"
test "$(read_rgb "${CAPTURE_DIR}/t01_replay.ppm" 10 10)" = "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_replay.ppm" 320 240)" != "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_clear.ppm" 320 240)" = "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_draw.ppm" 320 240)" != "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_rewind.ppm" 320 240)" = "14141a"
test -s "${CAPTURE_DIR}/t01_texture.dds"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 10 10)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 160 240)" != "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 480 240)" != "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 10 10)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 200 150)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 440 150)" = "10e030"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 200 330)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 440 330)" = "f0d020"
test "$(read_rgb "${CAPTURE_DIR}/t04_replay.ppm" 160 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t04_replay.ppm" 480 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 320 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 496 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t06_replay.ppm" 32 32)" = "8c292e"
test "$(read_rgb "${CAPTURE_DIR}/t06_replay.ppm" 320 240)" = "6d572e"
test "$(read_rgb "${CAPTURE_DIR}/t07_replay.ppm" 160 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t07_replay.ppm" 480 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 16 16)" = "080a0f"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 160 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 320 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 480 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 26 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 80 240)" = "10e030"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 133 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 186 240)" = "f0d020"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 240 240)" = "e030c0"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 293 240)" = "20d0e0"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 346 240)" = "ff8020"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 400 240)" = "8020ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 453 240)" = "20ff80"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 506 240)" = "ff4080"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 560 240)" = "80ff20"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 613 240)" = "2080ff"
test -s "${CAPTURE_DIR}/t09_cube.dds"

"${LIFECYCLE_SMOKE}" "${CAPTURE_DIR}/t00_capture.rdc" \
  "${CAPTURE_DIR}/t01_capture.rdc" "${CAPTURE_DIR}/t02_capture.rdc" \
  "${CAPTURE_DIR}/t03_capture.rdc" "${CAPTURE_DIR}/t04_capture.rdc" \
  "${CAPTURE_DIR}/t05_capture.rdc" "${CAPTURE_DIR}/t06_capture.rdc" \
  "${CAPTURE_DIR}/t07_capture.rdc" "${CAPTURE_DIR}/t08_capture.rdc" \
  "${CAPTURE_DIR}/t09_capture.rdc" 10

echo "Metal capture smoke test passed."
echo "T00: ${CAPTURE_DIR}/t00_capture.rdc"
echo "T01: ${CAPTURE_DIR}/t01_capture.rdc"
echo "T02: ${CAPTURE_DIR}/t02_capture.rdc"
echo "T03: ${CAPTURE_DIR}/t03_capture.rdc"
echo "T04: ${CAPTURE_DIR}/t04_capture.rdc"
echo "T05: ${CAPTURE_DIR}/t05_capture.rdc"
echo "T06: ${CAPTURE_DIR}/t06_capture.rdc"
echo "T07: ${CAPTURE_DIR}/t07_capture.rdc"
echo "T08: ${CAPTURE_DIR}/t08_capture.rdc"
echo "T09: ${CAPTURE_DIR}/t09_capture.rdc"
echo "T00 replay: ${CAPTURE_DIR}/t00_replay.ppm"
echo "T01 replay: ${CAPTURE_DIR}/t01_replay.ppm"
echo "T01 event replay: clear -> draw -> clear verified for 10 cycles"
echo "T01 shader entry/stage/MSL reflection verified"
echo "T01 pipeline/shaders/topology/vertex buffer/color target state verified"
echo "T01 texture readback/pixel picking/DDS save verified"
echo "T02 indexed state, clear/draw/rewind images, buffer data, generic VS input, and mesh preview verified"
echo "T03 texture upload/readback/pixel picking, sampler, bindings, and sampled output verified"
echo "T04 dynamic uniform bytes, fragment buffer offset/reflection, event replay, and output verified"
echo "T05 multi-buffer instancing, base instance, generic VS input, mesh preview, and output verified"
echo "T06 MRT targets, per-attachment blending/write masks, event replay, and output verified"
echo "T07 combined depth/stencil, front/back state, dynamic references, event replay, and output verified"
echo "T08 4x MSAA attachment, explicit resolve, sample state, event replay, and output verified"
echo "T09 mip/array/cube upload, descriptors, readback, display, picking, save, and output verified"
echo "T00/T01/T02/T03/T04/T05/T06/T07/T08/T09 replay lifecycle and unsupported-interface stability verified"
