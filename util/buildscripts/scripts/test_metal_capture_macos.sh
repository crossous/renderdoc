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
      "${CAPTURE_DIR}/t00.xml" "${CAPTURE_DIR}/t01.xml" "${CAPTURE_DIR}/t02.xml" \
      "${CAPTURE_DIR}/t03.xml" \
      "${CAPTURE_DIR}/t01_event_clear.ppm" "${CAPTURE_DIR}/t01_event_draw.ppm" \
      "${CAPTURE_DIR}/t01_event_rewind.ppm" "${CAPTURE_DIR}/t01_texture.dds" \
      "${CAPTURE_DIR}/t02_replay.ppm" "${CAPTURE_DIR}/t03_replay.ppm"

"${DEMO_BIN}" Metal_Empty_Frame --frames 5
"${DEMO_BIN}" Metal_Simple_Triangle --frames 5
"${DEMO_BIN}" Metal_Indexed_Cube --frames 5
"${DEMO_BIN}" Metal_Textured_Quad --frames 5

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

test -s "${CAPTURE_DIR}/t00_capture.rdc"
test -s "${CAPTURE_DIR}/t01_capture.rdc"
test -s "${CAPTURE_DIR}/t02_capture.rdc"
test -s "${CAPTURE_DIR}/t03_capture.rdc"

"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t00_capture.rdc" \
  -o "${CAPTURE_DIR}/t00.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t01_capture.rdc" \
  -o "${CAPTURE_DIR}/t01.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t02_capture.rdc" \
  -o "${CAPTURE_DIR}/t02.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t03_capture.rdc" \
  -o "${CAPTURE_DIR}/t03.xml" -c xml

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

"${LIFECYCLE_SMOKE}" "${CAPTURE_DIR}/t00_capture.rdc" \
  "${CAPTURE_DIR}/t01_capture.rdc" "${CAPTURE_DIR}/t02_capture.rdc" \
  "${CAPTURE_DIR}/t03_capture.rdc" 10

echo "Metal capture smoke test passed."
echo "T00: ${CAPTURE_DIR}/t00_capture.rdc"
echo "T01: ${CAPTURE_DIR}/t01_capture.rdc"
echo "T02: ${CAPTURE_DIR}/t02_capture.rdc"
echo "T03: ${CAPTURE_DIR}/t03_capture.rdc"
echo "T00 replay: ${CAPTURE_DIR}/t00_replay.ppm"
echo "T01 replay: ${CAPTURE_DIR}/t01_replay.ppm"
echo "T01 event replay: clear -> draw -> clear verified for 10 cycles"
echo "T01 shader entry/stage/MSL reflection verified"
echo "T01 pipeline/shaders/topology/vertex buffer/color target state verified"
echo "T01 texture readback/pixel picking/DDS save verified"
echo "T02 indexed state, clear/draw/rewind images, buffer data, generic VS input, and mesh preview verified"
echo "T03 texture upload/readback/pixel picking, sampler, bindings, and sampled output verified"
echo "T00/T01/T02/T03 replay lifecycle and unsupported-interface stability verified"
