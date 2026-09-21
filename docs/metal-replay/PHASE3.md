# 阶段 3 细化计划：T02 索引立方体、Depth 与 Mesh Viewer

T00/T01 已建立 capture、seekable replay、Texture/Shader/Pipeline UI 和资源生命周期闭环。下一条
纵向 feature 固定为 T02 索引立方体：用一个确定性的 3D 场景同时补齐 index buffer、vertex
descriptor、depth attachment/state、常用 raster state 与 Mesh Viewer 输入数据。此阶段不扩展
纹理采样、MSAA、MRT、instancing 或 compute。

## P3.1：建立确定性 T02 fixture

状态：2026-09-21 已完成。T02 原生运行、structured capture、vertex descriptor、224-byte vertex
buffer、72/144-byte index buffer、depth attachment/state、两组 viewport/scissor 和两次 36-index
draw 均已进入一键回归。

主要文件：

- `util/test/demos/metal/metal_indexed_cube.cpp`（新增）
- `util/test/demos/CMakeLists.txt`
- `util/buildscripts/scripts/test_metal_capture_macos.sh`
- `util/test/metal/metal_replay_output_smoke.mm`

工作内容：

- 生成固定相机与固定变换的彩色立方体，不使用逐帧动画或外部资源。
- 使用显式 `MTLVertexDescriptor` 描述 interleaved position/color 顶点。
- 建立 16-bit 和 32-bit 两种 index fixture 变体，首个可视输出以 36 个 triangle-list index 为基准。
- 创建 `Depth32Float` texture、render pass depth attachment 和 less/写入开启的 depth state。
- 显式设置 viewport、scissor、front face 与 back-face culling，使固定状态有可验证证据。
- 原生运行时保存参考像素/图像；capture 后在 XML 中断言 vertex descriptor、index 数据、depth
  attachment/state 和 indexed draw 参数。

验收：T02 未注入运行输出稳定；一键脚本生成非空 `.rdc`；structured export 中所有输入字节和
draw 参数与 fixture 常量一致。

## P3.2：补齐 capture 与资源包装

状态：2026-09-21 已完成。新增 depth-stencil state wrapper/bridge，直接 indexed draw 与 T02 所需
固定状态均已序列化并 replay；render pass 现在会把 color/depth/stencil 及 resolve attachment 全部
标记为帧引用，确保 private depth texture 的创建记录进入 `.rdc`。新增 wrapper 已通过三份 capture
各 10 次的同进程 lifecycle smoke。

主要文件：

- `renderdoc/driver/metal/metal_device.{h,cpp}`
- `renderdoc/driver/metal/metal_device_bridge.mm`
- `renderdoc/driver/metal/metal_render_command_encoder.{h,cpp}`
- `renderdoc/driver/metal/metal_render_command_encoder_bridge.mm`
- `renderdoc/driver/metal/metal_types.{h,cpp}`
- `renderdoc/driver/metal/metal_serialise.cpp`
- `renderdoc/driver/metal/metal_core.cpp`
- 新增 depth-stencil state wrapper/bridge 文件，并同步 Metal CMake/source lists

工作内容：

- 包装并序列化 `newDepthStencilStateWithDescriptor` 与 `setDepthStencilState`。
- 接通 `setViewport`、`setScissorRect`、`setFrontFacingWinding`、`setCullMode`。
- 序列化直接 indexed draw 的 index count/type/buffer/offset；base/instance 重载先保留明确边界。
- 确认现有 pipeline descriptor 中的 vertex descriptor、depth format 字段完整往返。
- 为新增 wrapper 沿用 D017 的 replay ownership 规则，并加入 lifecycle smoke。

验收：T02 capture 无未处理目标 chunk，资源派生关系包含 vertex/index/depth/pipeline/state。

## P3.3：实现 T02 GPU replay 与 action/state

状态：2026-09-21 已完成。GPU replay、depth texture/state、固定状态、直接 indexed draw、indexed
action 和最终双视口图像均已通过。`MetalPipe::State` 已包含 index binding、vertex descriptor、depth
state 与 raster state；加载阶段按 action event 保存 draw-time snapshot，避免下一 draw 前的动态状态
覆盖当前事件。自动回归已验证 clear -> UInt16 draw -> UInt32 draw -> 回退到 UInt16 draw 的状态和图像。

主要文件：

- `renderdoc/driver/metal/metal_device.cpp`
- `renderdoc/driver/metal/metal_render_command_encoder.cpp`
- `renderdoc/driver/metal/metal_replay.{h,cpp}`
- `renderdoc/api/replay/metal_pipestate.h`

工作内容：

- replay 创建 depth texture/state，并把 depth attachment 接入 render pass。
- 恢复 viewport/scissor/front-face/cull 和 vertex descriptor 后执行 `drawIndexedPrimitives`。
- action 正确标记 indexed draw，记录 index count、index offset、base vertex/base instance。
- Metal pipeline snapshot 增加 index buffer、vertex attributes/layouts、depth target/state 和 raster
  state；不填充 capture 中不存在的字段。
- 扩展 event-range replay，验证 indexed draw 前后和回退仍可重复。

验收：T02 CLI replay 输出与原生参考一致；选择 clear/draw/end 后状态和图像可重复恢复。

## P3.4：Buffer/Pipeline/Mesh 数据与 UI

状态：2026-09-21 已完成。Metal vertex descriptor 已通过通用 `PipeState::GetVertexInputs()` 进入
RenderDoc 既有 Mesh Viewer/Buffer Viewer 数据路径；T02 的 VS Input 表格可按 UInt16/UInt32 index
展开 Float3/Float4 数据，预览可绘制 indexed cube 线框。Pipeline State 的 vertex attribute 激活会
打开 Mesh Viewer，vertex/index buffer 激活会使用自动格式打开标准 Buffer Viewer。Post-VS 当前在
输出表明确显示 unsupported，不伪造数据。

主要文件：

- `renderdoc/api/replay/metal_pipestate.h`
- `renderdoc/api/replay/pipestate.{h,inl}`
- `qrenderdoc/Windows/PipelineState/MetalPipelineStateViewer.{h,cpp}`
- `renderdoc/driver/metal/metal_replay.{h,cpp}`
- 必要时扩展 `util/test/metal/metal_replay_output_smoke.mm`

工作内容：

- Buffer Viewer 可读取 vertex/index buffer 的已知字节与子范围。
- Pipeline State 显示 vertex attributes/layouts、index buffer/type/offset、depth target/state、viewport、
  scissor、front face 和 cull mode，并提供资源跳转。
- `GetPostVSBuffers()`/Mesh Viewer 首先提供 VS input；post-VS 若尚未实现必须明确为空且 UI 稳定。
- 16-bit 与 32-bit index 都进行数据断言；Mesh Viewer 的顶点和索引顺序与 CPU fixture 一致。

验收：qrenderdoc 的 Event、Buffer、Pipeline、Mesh 和 Texture 页面共同解释同一个 T02 draw，资源
ID、字节、格式、状态和最终图像一致。

验收结果：EID 2 的 Mesh Viewer 显示 `attr0/attr1`、UInt16 index 顺序和立方体线框；Buffer Viewer
显示 224-byte interleaved vertex 数据与 72-byte `ushort index`。EID 3 的 144-byte index buffer 以
`uint index` 展示同一索引序列。自动 output smoke 还会实际执行 Metal `RenderMesh()` 并检查输出中
存在足够的非背景像素。两条事件状态栏均为 `No problems detected`。

## P3.5：回归与阶段关闭

状态：2026-09-21 已完成。

- 将 T02 native/capture/XML/replay/output/data/state/lifecycle 全部并入
  `test_metal_capture_macos.sh`。
- T00/T01 不退化；三份 capture 各执行 CLI 多轮 replay。
- qrenderdoc 实机完成 T02 draw、Pipeline State、Buffer Viewer、Mesh Viewer 和关闭/重开检查。
- 更新 `PLAN.md` 中由 T02 实际完成的 M2.6、M3、M4、M5 子项；未覆盖项继续保持未完成。
- 更新 `STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 和 `HANDOFF.md`。

阶段验收：T02 的 Native/Capture/RDC inspect/Replay/UI 五列全部通过，且 T00/T01/T02 生命周期回归
无崩溃、无超过阈值的常驻内存增长。
