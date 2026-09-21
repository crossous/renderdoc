# Agent 交接规范

## 新 agent 的最短接手路径

1. 阅读本目录的 `README.md`、`STATUS.md`、`PLAN.md` 和当前阶段文件 `PHASE4.md`。
2. 执行 `git status --short --branch`，不得覆盖未知的用户或其他 agent 修改。
3. 查看 `STATUS.md` 的“当前任务”和“下一步”，只领取一个有明确边界的任务 ID。
4. 开始修改前，在 `STATUS.md` 记录任务状态、负责人标识和开始时间。
5. 先运行该任务最近一次记录的验证命令，确认基线可复现。

当前最短基线验证命令：

```sh
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

该脚本会构建 qrenderdoc/renderdoccmd 和 Metal demos，原生运行 T00/T01/T02/T03，分别截帧并把 `.rdc`
转成 XML 后检查关键字段。生成物位于 `captures/metal-smoke/`，已被 `.gitignore` 排除。

基线还应追加执行：

```sh
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t00_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t01_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t02_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t03_capture.rdc
```

四条命令当前均通过。qrenderdoc 也能加载并在 Texture Viewer 显示 T01/T03，且不再出现 degraded
弹窗。`test_metal_capture_macos.sh` 还会生成并检查 `t00_replay.ppm`、`t01_replay.ppm` 以及
T01 的 `clear -> draw -> clear` event-range replay PPM。

`PHASE4.md` 的 P4.4 第三切片已经关闭：标准 IA/VS/RS/FS/OM、empty-slot、
`RDTreeWidget`/`RDHeaderView` 资源表、通用资源操作/预览、五阶段 HTML export，以及基于 Metal
argument reflection 的 used/unused 过滤均已完成。下一接手任务是 `PHASE5.md` P5.1：建立 T04
动态 uniform 的确定性 native fixture，随后扩展 fragment buffer binding。不要回退重做 T03
texture upload、sampler、fragment binding 与通用 descriptor 闭环，也不要回退重做 T02
vertex/index/depth/Mesh 闭环、output window、event-range replay、RGBA8/BGRA8 readback、
shader source/entry reflection、T01 Pipeline State 或 T00/T01 lifecycle；这些已有自动数据回归和
qrenderdoc 实机验证。

最新 T02 qrenderdoc 验证直接打开 `captures/metal-smoke/t02_capture.rdc`。Event Browser 显示 EID 2/3 两次
`drawIndexedPrimitives(36)`；EID 3 API Inspector 标明 `MTLIndexTypeUInt32, Buffer 20`，Texture Viewer
列出 `FB0 Texture 27` 与 `DS Texture 17` 并正确显示双立方体，状态栏无错误。Pipeline State 进一步
验证 EID 2 的左 viewport/scissor 与 UInt16 Buffer 19/72、EID 3 的右 viewport/scissor 与 UInt32
Buffer 20/144，并显示 Float3/Float4、stride 28、less/write、back/CCW。P3.4 随后已接通通用
`GetVertexInputs()` 与最小 Metal `RenderMesh()`：EID 2 Mesh Viewer 显示 UInt16 展开的 `attr0/attr1`
和立方体线框；Buffer 18 自动解析 Float3/Float4，Buffer 19/20 分别以 `ushort`/`uint index` 显示
72/144-byte 范围。T03 最新 qrenderdoc 验证已选择 EID 2：Pipeline 显示 `Triangle Strip`、
Float2/Float2、Fragment Texture 17（RGBA8）和 Sampler 18（Point/Point/None、ClampEdge x3），四象限
图像正确。双击 Texture 17 进入标准 Texture Viewer，双击 Sampler 18 进入 Resource Inspector；
所有表已经切换到标准 RDTree，并接入通用 context/usage 与 thumbnail/preview 分发。Export 控件已在
T03 EID 2 实际写出 `/tmp/metal-pipeline-t03.html`，核对包含五阶段、Triangle Strip、Texture 17 和
Sampler 18。状态栏无错误；当前进程保持运行在 T03 EID 2 的 FS 页。

生命周期回归位于 `util/test/metal/metal_replay_lifecycle_smoke.mm`，由一键脚本自动编译执行。它会在
同一进程中打开/关闭 T00/T01/T02/T03 各 10 次，并验证 unsupported 接口与 resident growth。2026-09-21
完成 P4.4 第三个 UI 切片后的最新完整回归值为 540,672 字节；失败阈值为 64 MiB。新增资源 wrapper
必须沿用 D017 的所有权规则。

当前 texture data 回归位于 `util/test/metal/metal_replay_output_smoke.mm`：它在 T01 clear/draw EID
读取 400x300 BGRA backbuffer，校验背景字节和拾取浮点值，再保存
`captures/metal-smoke/t01_texture.dds`。readback helper 为
`MetalReplay::ReadTextureSubresource()`，当前范围由 D014 固定；扩展格式时必须增加对应 fixture 和
数据断言。

2026-09-22 的最新 Pipeline UI 验证：最终构建已将 Metal 页面重排为 Controls + 标准
`PipelineFlowChart` + IA/VS/RS/FS/OM 阶段页，并将表格切换到标准 RDTree。T03 EID 2 的 FS 页显示
Function 14、Texture 17、Sampler 18，texture 双击进入 Texture Viewer；Export 实际生成并核对五阶段
HTML。`Show Empty Items` 在 IA/OM 显示红色空 index/depth 槽。T02 EID 2 的 IA 显示 Buffer 19/UInt16，
RS 显示左 viewport/scissor、Back/CCW，OM 显示 Texture 27、Depth Texture 17 和 Less/Write。状态栏
均为 `No problems detected`。第三切片还从 Metal pipeline argument reflection 枚举 T03 的
`colourTexture`/`colourSampler`；同一资源绑定到未声明 slot 1 后，默认表只显示 slot 0，启用
`Show Unused Items` 后 texture/sampler 表均显示真实 slot 1。当前 qrenderdoc 保持打开 T03 EID 2 的
FS 页并启用该过滤开关。

最近一次 qrenderdoc 手工验证：打开 `captures/metal-smoke/t01_capture.rdc`，选择 EID 2，在纹理
坐标 `(202, 128)` 右键拾取得到 `(0.61176, 0.34118, 0.32157, 1.00)`；保存按钮成功写出
`captures/metal-smoke/t01_texture_ui.dds`，状态栏保持 `No problems detected`。

2026-09-21 的 Shader Viewer 验证：新构建 qrenderdoc 加载 T01 后，Resource Inspector 的
Function 13/14 均出现 “View Contents”；分别打开后都显示 `captured.metal` 的真实 MSL，入口为
`vs_main` 与 `fs_main`，状态栏保持 `No problems detected`。若本机布局保存过 D3D11 Pipeline State
子页面，当前代码会在加载 Metal capture 时清空该不匹配页面，避免旧 backend viewer 崩溃。

2026-09-21 的 Pipeline State 验证：完整 smoke 会断言 T01 draw 的真实 pipeline/shader/topology/
vertex buffer/color target。随后已关闭旧 qrenderdoc、启动本次新构建并打开最新 T01；EID 2 的
Metal Pipeline State 页面显示 `Pipeline State 15`、`Triangle List`、Vertex/Pixel 的
`Function 13/14` 与 `vs_main/fs_main`、`Buffer 16`（offset 0、size 96）及 `Texture 23`
（mip/slice 0），状态栏保持 `No problems detected`。

2026-09-21 的阶段关闭 UI 验证：关闭旧进程并启动最新 qrenderdoc，同一进程依次完成
`T01 -> Close -> T00 -> Close -> T01`。重开 T01 后 EID 2 仍显示彩色三角形和上述 Pipeline State；
T00/T01 状态栏均为 `No problems detected`。Texture Viewer 的 History/Debug 按钮分别显示
`Pixel History not supported on this API` 与 `Shader Debugging not supported on this API` 并保持禁用。

## 每次工作必须更新的内容

- `STATUS.md`：当前阶段、已完成、实际验证命令/结果、阻塞项、下一步。
- `TEST_MATRIX.md`：只要 API 覆盖或样例状态变化，就同步修改对应行。
- `DECISIONS.md`：出现影响架构、capture 格式、兼容范围或用户可见行为的选择时新增记录。
- `PLAN.md`：阶段范围发生变化时更新；禁止只在聊天中改变计划。

## 阶段关闭规则

一个阶段只有在以下事项全部完成后才能标为完成：

1. 阶段验收条件全部通过，或未通过项得到用户明确接受并记录。
2. 验证命令、产物路径和结果已写入 `STATUS.md`。
3. 新增/变更能力已反映到 `TEST_MATRIX.md`。
4. 已知限制和遗留问题有明确任务 ID。
5. 下一阶段已拆成文件级或接口级任务，并在 `STATUS.md` 中指定第一项。

## 修改与验证约定

- 实现优先放在 RenderDoc 原有架构位置；不要另建绕开 replay API 的独立查看器。
- 构建产物统一放在 `build-*` 目录，不提交二进制和本机绝对配置。
- 第三方测试样例固定 commit 和许可证；未经核验不复制源码。
- 每个 Metal chunk 的支持应包含 capture/序列化、replay、状态更新和测试四方面检查。
- 暂不支持的接口应稳定返回错误/unsupported，并写清楚日志，不留下 silent success。
- 遇到新 SDK 兼容补丁时，将纯兼容修改与 replay 功能修改尽量分开。

## 建议的任务记录格式

在 `STATUS.md` 工作日志中追加：

```text
| YYYY-MM-DD HH:mm | Mx.y | owner | 做了什么 | 验证命令与结果 | 下一步/阻塞 |
```

若任务中断，必须留下：已修改文件、最后成功命令、当前失败命令、关键日志摘要和安全的下一操作。
