# 阶段 2 细化计划：T00/T01 Replay 与 qrenderdoc 加载

阶段 1 已完成，已有可重复生成且可 structured export 的 T00/T01 Metal `.rdc`。阶段 2 只围绕这
两份 capture 建立第一条真实 replay/UI 链路，不提前扩展纹理采样、索引绘制或 compute。

## M2.1：建立 Metal replay provider

状态：已完成。

主要文件：

- `renderdoc/driver/metal/metal_replay.h`
- `renderdoc/driver/metal/metal_replay.cpp`
- `renderdoc/driver/metal/metal_device.h`
- `renderdoc/driver/metal/metal_core.cpp`

工作内容：

- 让 `MetalReplay` 通过 `DummyDriver` 或等价的最小实现满足 `IReplayDriver`。
- 注册 `RDCDriver::Metal` replay provider，区分真实 replay 与 structured-only 读取。
- 创建真实 `MTLDevice`，读取 `MetalInitParams`，建立 capture ID 到 live object 的映射。
- 对无法支持的接口返回明确的 `APIUnsupported`/空能力，不允许空指针或假数据。

验收：`renderdoccmd replay T00.rdc --loops 1` 不因 provider 缺失而失败，qrenderdoc 能进入加载流程。

## M2.2：资源和命令对象重建

状态：T00/T01 所需对象与命令已完成；更广接口随对应纵向 feature 扩展。

按 capture 中的依赖顺序实现：device、queue、library、function、pipeline、buffer、drawable 替代
texture、command buffer、render encoder。

关键约束：

- capture 中的 drawable 是外部 IOSurface 资源；replay 时必须映射到受控输出 texture，不能复用
  capture 时的 IOSurface。
- source-created library 必须从保存的 MSL 重编译，并把编译错误转成可读的 replay failure。
- shared buffer 初始字节必须在 draw 前恢复，T01 当前基准为 96 字节。

验收：初始化阶段重建 T00/T01 所有引用资源，无 unresolved ResourceId。

## M2.3：事件与 action 模型

状态：T00/T01 最小 action/event 已完成；层级、usage 和 marker 后续增强。

- 增加当前 event/action ID、根 action 列表和 action stack。
- 为 render pass begin/end、draw、present 建立稳定命名与 flags。
- `drawPrimitives` 记录 topology、vertex count、instance count、base vertex/base instance。
- 暂无 marker 的 T00/T01 仍应形成清楚的 pass/draw/present 顺序。

验收：qrenderdoc Event Browser 可显示 T00 的 clear/present，以及 T01 的 render pass、draw、present。

## M2.4：执行 replay

状态：T00/T01 范围内已完成。capture command stream 会在加载时建立 action/event，并被保留用于
后续按 event 重放；CLI replay、macOS output window、Texture Viewer 和 output readback 均已通过。

- 在读取 frame chunks 时重建 command buffer 和 render encoder。
- 应用 render pass clear/store、pipeline、vertex buffer 和 `drawPrimitives`。
- 实现 full replay、without-draw、only-draw 所需的最小 event range 逻辑。
- 将最后呈现纹理复制/绘制到 qrenderdoc 已有的 macOS `CAMetalLayer` output window。

当前实现顺序：

1. [完成] 为 `WindowingData.macOS.layer` 建立 output window 状态和尺寸跟踪。
2. [完成] 使用持久的 BGRA8 output texture 保存 viewer 内容，避免 qrenderdoc 非 dirty 刷新时呈现
   空 drawable。
3. [完成] 以最小 fullscreen Metal pipeline 显示 2D RGBA/BGRA color texture，并支持
   fit/scale/pan、mip、flip 和基础 channel/range 映射。
4. [完成] T00/T01 UI 验证通过后将 `APIProperties.degraded` 改为 `false`。
5. [完成] 保存 `CaptureBegin` 后的 frame stream，并在 action event 上记录流内 file offset。
6. [完成] `WithoutDraw` 从帧头执行到目标 action 之前，保留 command buffer/render encoder 状态。
7. [完成] `OnlyDraw` 从目标 action 的 offset 执行一个事件，并提交前一步保留的命令状态。
8. [完成] `Full` 从帧头执行到目标事件，并自动结束尚未闭合的 encoder/command buffer。

验证产物：

- `captures/metal-smoke/t00_replay.ppm`：预期 clear 色 `#14335c`。
- `captures/metal-smoke/t01_replay.ppm`：黑色背景和彩色三角形。
- `captures/metal-smoke/t01_event_clear.ppm`：EID 1 后中心为背景色 `#14141a`。
- `captures/metal-smoke/t01_event_draw.ppm`：EID 2 后中心为三角形颜色，本次为 `#83645f`。
- `captures/metal-smoke/t01_event_rewind.ppm`：从 EID 2 回到 EID 1 后中心恢复 `#14141a`；同一
  controller 连续执行 10 轮。
- qrenderdoc T01：Texture Viewer 实机显示通过，窗口 resize 后仍正确。
- qrenderdoc T01：实机切换 EID 1 -> 2 -> 1，画面按 clear -> draw -> clear 变化，状态栏无错误。

验收：T00 输出颜色与 thumbnail 一致；T01 三角形与原生参考图一致；T01 event seek 自动化和 UI
手工验证通过。

## M2.5：最小 UI 数据面

状态：已完成。texture readback/像素拾取/保存、Shader Viewer 和 T01 最小 Pipeline State 均已通过
自动回归与 qrenderdoc 实机验证。

- 返回 frame record、root actions、资源列表与基础 API properties。
- T01 至少能列出 device、queue、library、functions、pipeline、vertex buffer 和 backbuffer texture。
- shader 页面先显示 source library 的 MSL 与入口名；buffer 页面先支持原始字节。
- Pipeline State 和 Mesh Viewer 的完整语义仍分别归入阶段 4/5，但本阶段不得点击即崩溃。

建议执行顺序：

1. [完成] 实现 capture texture 的 `GetTextureData()`，覆盖单采样 2D RGBA8/BGRA8 render target；
   用 GPU blit 读到 shared buffer，并返回无行 padding 的紧密数据。
2. [完成] 基于同一 readback 路径实现 `PickPixel()`，自动断言 T01 clear 背景和 draw 中心颜色；
   同时通过 `ReplayController::SaveTexture()` 自动保存并验证 400x300 DDS。qrenderdoc 实机右键
   拾取和 “Save selected Texture” 也已通过，状态栏无错误。
3. [完成] 保存 source-created library 的 MSL，并按 Metal function type 暴露 `vs_main`/`fs_main`
   的 entry point 和 stage；自动 reflection 检查与 qrenderdoc Shader Viewer 实机验证均通过。
4. [完成] 为当前 draw 提供 Metal 专用最小 Pipeline State snapshot 和 qrenderdoc 页面，列出真实
   render pipeline、vertex/fragment shader 与入口、primitive topology、vertex buffer 和 color target；
   行可跳转到 Resource Inspector。T01 EID 2 实机显示 `Pipeline State 15`、`Triangle List`、
   `Function 13/14`、`vs_main/fs_main`、`Buffer 16`（offset 0、size 96）和 `Texture 23`（mip/slice 0），
   状态栏为 `No problems detected`。

当前 readback 边界：仅 sample 0、slice 0 的单采样 2D `RGBA8Unorm(_sRGB)` 和
`BGRA8Unorm(_sRGB)`；remap/resolve 及其他 texture 类型/格式会记录明确错误，不返回伪数据。

## M2.6：自动化与阶段关闭

状态：已完成。T00/T01 的 capture/replay/data/UI/lifecycle 自动化已形成单一入口；T02 的下一阶段
文件级计划见 `PHASE3.md`。

- [完成] 在现有 capture smoke 基础上增加 replay output、event seek、data/state 与 lifecycle smoke。
- [完成] 保存 T00/T01 replay 输出并按确定像素与原生参考语义比较。
- [完成] 单进程循环打开/关闭 T00/T01 各 10 次；完整回归在两轮 warm-up 后 resident growth 为
  491,520 字节，低于 64 MiB 失败阈值；额外 50 轮压力检查增长 1,441,792 字节。
- [完成] replay wrapper 区分 transferred 与 retained Metal 对象；重复 replay 替换 command
  buffer/render encoder 时释放旧引用，capture-only ObjC bridge 不再参与 replay 析构。
- [完成] histogram、pixel history、post-VS、shader debug 与 custom/target shader build 返回稳定的
  空结果或明确错误；`Debug*()` 返回可安全释放的空 trace，不再给控制器返回空指针。
- [完成] 最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`；T00/T01 状态栏均为
  `No problems detected`，重开后的 T01 EID 2 仍显示三角形和完整最小 Pipeline State。Texture
  Viewer 的 History/Debug 按钮明确显示不支持并保持禁用。
- [完成] 更新 `STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md`，并新增 T02 计划 `PHASE3.md`。

阶段结论：T00/T01 replay 纵向切片关闭。总体 `PLAN.md` 中 M2.6 的完整固定状态、阶段 3/4/5 的
广覆盖及阶段 7 的全矩阵稳定性仍未完成，不因本小节编号同名而提前标记。
