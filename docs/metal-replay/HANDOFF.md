# Agent 交接规范

## 默认工作周期

一个 agent 默认负责一个完整的 `PHASEx.md` / Txx 纵向切片，而不是只领取其中一个 Pxx.y 后就停下。
例如下一 agent 应从 `PHASE11.md` 的 P11.1 连续推进到 P11.4；无需等待用户反复发送“继续”。只有以下
情况可以在阶段关闭前停下：

1. 需要用户选择会显著改变范围、兼容策略或用户可见行为。
2. 外部条件不可用，且安全的替代检查已穷尽。
3. 上下文已明显膨胀，需要先写恢复检查点再 compact。

每个阶段内部固定按以下顺序推进：

1. **Fixture/native**：建立最小、确定性输入，先证明未注入运行正确。
2. **Capture/data**：补齐 capture/chunk/initial contents，并用 XML 或结构化检查锁定参数。
3. **Replay/state**：完成 GPU replay、event seek、readback 与 pipeline/descriptor 状态。
4. **Standard UI**：复用通用 Viewer、资源跳转和 export；Metal 页面布局与操作继续向 D3D/Vulkan
   等标准页面收敛，不新建旁路查看器。
5. **阶段收口**：完整自动回归、CLI replay/lifecycle、最新 qrenderdoc 实机验收、文档同步。

## 新 agent 的最短接手路径

1. 阅读本目录的 `README.md`、`STATUS.md`、`PLAN.md`、`HANDOFF.md` 和 `STATUS.md` 指向的当前
   `PHASEx.md`。
2. 执行 `git status --short --branch`，把工作区视为可能包含前任未提交的有效修改，不得清理、覆盖或
   回退未知改动。
3. 读取 `STATUS.md` 的“当前任务”“下一步”和“恢复检查点”；从第一项未完成工作继续，不重做已有
   明确验证证据的步骤。
4. 用增量构建或当前 fixture 的最短定向测试确认环境可用；新 agent 接手时默认**不**先跑全部历史
   capture 的完整回归。
5. 连续完成当前阶段。开始时只需在 `STATUS.md` 留一条简短状态；实现过程中的每个微小修复不要求
   逐次改文档，架构决策、风险或中断检查点除外。

## 分层验证与额度控制

验证按风险从低到高执行，避免每个修改都重复读取长日志：

- L0：增量编译受影响 target、`git diff --check`、必要的静态检查。
- L1：只运行当前 Txx fixture 的 native/capture/XML/replay/readback/state 检查。
- L2：运行与本次公共路径直接相关的旧 fixture；例如 T09 子资源优先回归 T03 texture 路径。
- L3：阶段功能已经齐备后，运行一次当前全部 T00-Txx 的一键回归、lifecycle 和 CLI replay。
- L4：L3 通过后的最新构建只做一次 qrenderdoc 实机验收，覆盖 Event、目标 Viewer、Pipeline、资源
  跳转/保存/export 和 `No problems detected`。

命令输出应优先重定向到日志文件；成功时只读取摘要，失败时只读取相关错误和末尾日志。阶段收口后
若只修复 fixture 局部问题，可重跑 L1/L2；若修改公共 replay、序列化、资源所有权、事件语义或通用
UI 路径，必须重跑 L3/L4。不要为了节省额度省略最终 qrenderdoc 验收。

当前完整回归命令（只在阶段收口或公共路径高风险修复后运行）：

```sh
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

该脚本会构建 qrenderdoc/renderdoccmd 和 Metal demos，原生运行 T00-T09，分别截帧并把 `.rdc`
转成 XML 后检查关键字段。生成物位于 `captures/metal-smoke/`，已被 `.gitignore` 排除。

阶段收口还应追加执行：

```sh
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t00_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t01_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t02_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t03_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t04_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t05_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t06_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t07_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t08_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 1 captures/metal-smoke/t09_capture.rdc
```

十条命令当前均通过。qrenderdoc 也能加载并在 Texture Viewer 显示 T01/T03/T04/T05/T06/T07/T08/T09，且不再出现 degraded
弹窗。`test_metal_capture_macos.sh` 还会生成并检查 `t00_replay.ppm`、`t01_replay.ppm` 以及
T01 的 `clear -> draw -> clear` event-range replay PPM。

`PHASE10.md` 的 T09 已关闭：2D mip/array/cube 子资源、slice-aware upload/readback/pick/display、
标准 Pipeline/Texture Viewer/DDS 与完整 T00-T09 回归均已完成。下一接手任务是 `PHASE11.md`
P11.1：建立 T10 buffer/texture blit 的确定性 native fixture。不要回退重做 T09，也不要回退重做 T08
MSAA resolve，也不要回退重做 T07
depth/stencil、T06 MRT/blending、T05 instancing、T04 动态 uniform，也不要回退重做 T03
texture upload、sampler、fragment binding 与通用 descriptor 闭环，也不要回退重做 T02
vertex/index/depth/Mesh 闭环、output window、event-range replay、RGBA8/BGRA8 readback、
shader source/entry reflection、T01 Pipeline State 或 T00/T01 lifecycle；这些已有自动数据回归和
qrenderdoc 实机验证。

## Compact、继续与新任务边界

### 继续当前任务

只要当前阶段尚未完成、上下文仍清晰且没有用户决策阻塞，agent 应自行继续下一项 Pxx.y，不让用户
通过重复发送“继续”来驱动。一次失败或一次修复循环不是切换任务的理由。

### 建议 compact

出现自动上下文压缩提示、关键输出反复截断、已经历多轮大范围排查，或 agent 难以可靠保留早期实现
细节时，应在安全检查点建议 compact。建议前必须先在 `STATUS.md` 的“恢复检查点”写明：

- 当前阶段和第一项未完成任务；
- 已修改文件及不可回退的已有改动；
- 最后成功命令与结果；
- 当前失败命令、最短关键日志和已排除原因；
- 下一条安全操作；
- 是否尚未执行 L3 完整回归或 L4 qrenderdoc。

compact 只是压缩当前任务的聊天历史，不改变阶段目标。compact 后先读上述文档和 `git diff`，直接从
检查点继续；不重新做已经通过的阶段收口测试，也不假定 dirty worktree 可以清理。

### 建议新建任务

完整阶段关闭后默认建议新建任务，而不是继续在同一长对话中进入下一 Txx。只有 L3、L4、文档同步
和下一阶段拆分全部完成，才可对用户说“本阶段完成，可以新开任务”。若阶段中途因上下文原因必须
换新任务，也必须先留下同等完整的恢复检查点，并明确这不是阶段完成。

agent 的最终回复必须给出二选一的明确动作：

- `阶段已完成，建议新建任务`，并附上可直接复制的下一任务提示；或
- `阶段未完成，建议 compact 后继续当前任务`，并说明恢复检查点位置与 compact 后的第一步。

下一任务通用提示模板：

```text
继续 RenderDoc Metal replay。工作区可能包含未提交的有效改动，禁止清理或回退。请先阅读
docs/metal-replay/README.md、STATUS.md、PLAN.md、HANDOFF.md 和 STATUS 指向的当前 PHASEx.md，
从 STATUS 的第一项未完成任务继续。按 HANDOFF 的省额度稳定模式连续完成整个 Txx 阶段：开发中
只做 L0-L2 定向验证，阶段末执行一次 L3 完整回归和一次 L4 最新 qrenderdoc 实机验收；同步更新
PLAN/STATUS/TEST_MATRIX/DECISIONS/HANDOFF。阶段完成后给我下一任务的可复制提示；若上下文先变长，
先写 STATUS 恢复检查点，再明确告诉我 compact 后如何继续。
```

当前 T10 的可直接复制版本：

```text
继续 RenderDoc Metal replay 的 PHASE11/T10 buffer/texture blit 纵向切片。工作区包含前序未提交的
有效改动，禁止清理或回退。先阅读 docs/metal-replay/README.md、STATUS.md、PLAN.md、HANDOFF.md
和 PHASE11.md，从 P11.1 第一项未完成工作继续，并连续推进到 P11.4。开发中只跑 T10 及相关 T03/T09
texture 路径的定向验证；阶段末再运行一次完整 T00-T10 回归和一次最新 qrenderdoc 实机验收，确认
Event Browser、Buffer/Texture Viewer 的 blit 结果、Pipeline 绑定、保存路径和状态栏。同步所有阶段文档；完成后告诉我
应新开哪个任务并给出下一段可复制提示，若上下文先变长则先写 STATUS 恢复检查点并建议 compact。
```

2026-09-23 最新 T09：一键脚本生成 `captures/metal-smoke/t09_capture.rdc`，完整 T00-T09 回归和
十份 capture 各 10 次 lifecycle 通过。qrenderdoc EID 2 FS 页显示 Texture 17/18/19 分别为
2D/2D Array/Cube，Sampler 20 为 Point/Clamp；Texture Viewer 的三层 mip、三层 array slice、
六个 cube face 可选择，X-/Z- 等固定色已实机核对。macOS 26 的 Qt 5 combo popup 崩溃，子资源框
在 macOS 上改为点击逐项循环、键盘方向键/Home/End 选择。标准 Save Texture 对话框导出全部 cube
faces 到 `captures/metal-smoke/t09_cube_ui.dds`（512 字节），状态栏为 `No problems detected`。
本次 L4 对最新 `.rdc` 使用 `/tmp/t09-ui-capture.rdc` 字节拷贝，因为直接从 macOS `Documents`
路径启动 qrenderdoc 偶发卡在文件 open；CLI replay 对原路径正常。T10 不需重做 T09 验收。

较早的 T02 qrenderdoc 验证直接打开 `captures/metal-smoke/t02_capture.rdc`。Event Browser 显示 EID 2/3 两次
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
Sampler 18；该次验证状态栏无错误。

生命周期回归位于 `util/test/metal/metal_replay_lifecycle_smoke.mm`，由一键脚本自动编译执行。它会在
同一进程中打开/关闭 T00-T09 各 10 次，并验证 unsupported 接口与 resident growth。完成 T09 后的
最新完整回归值为 475,136 字节；失败阈值为 64 MiB。新增资源 wrapper
必须沿用 D017 的所有权规则。

当前 texture data 回归位于 `util/test/metal/metal_replay_output_smoke.mm`：它在 T01 clear/draw EID
读取 400x300 BGRA backbuffer，校验背景字节和拾取浮点值，再保存
`captures/metal-smoke/t01_texture.dds`。readback helper 为
`MetalReplay::ReadTextureSubresource()`，基础路径由 D014 固定，mip/array/cube 扩展由 D031 记录；
扩展格式时必须增加对应 fixture 和数据断言。

2026-09-22 的最新 Pipeline UI 验证：最终构建已将 Metal 页面重排为 Controls + 标准
`PipelineFlowChart` + IA/VS/RS/FS/OM 阶段页，并将表格切换到标准 RDTree。T03 EID 2 的 FS 页显示
Function 14、Texture 17、Sampler 18，texture 双击进入 Texture Viewer；Export 实际生成并核对五阶段
HTML。`Show Empty Items` 在 IA/OM 显示红色空 index/depth 槽。T02 EID 2 的 IA 显示 Buffer 19/UInt16，
RS 显示左 viewport/scissor、Back/CCW，OM 显示 Texture 27、Depth Texture 17 和 Less/Write。状态栏
均为 `No problems detected`。第三切片还从 Metal pipeline argument reflection 枚举 T03 的
`colourTexture`/`colourSampler`；同一资源绑定到未声明 slot 1 后，默认表只显示 slot 0，启用
`Show Unused Items` 后 texture/sampler 表均显示真实 slot 1；该次 T03 验证已完成。

2026-09-22 的 T04 qrenderdoc 验证：EID 2/3 的 Texture Viewer 分别显示左红/右背景与左红/右绿；
FS Constant Buffers 分别显示 `Buffer 16 / offset 0 / size 512 / needed 16` 和
`Buffer 16 / offset 256 / size 256 / needed 16`。双击 EID 3 binding 后，标准 Buffer Viewer 的 byte
range 为 256/256，开头四个值为 `.0625/.875/.1875/1.0` 对应的原始 float 字节。状态栏为
`No problems detected`；当前最终构建保持运行在 T04 EID 3 FS 页。

2026-09-22 的 T05 qrenderdoc 验证：EID 2 Texture Viewer 显示红、绿、蓝三个实例；IA 显示
`Buffer 16 / 24 / stride 8 / Vertex / 1` 和 `Buffer 17 / 96 / stride 24 / Instance / 1`。Mesh Viewer
instance 0/1 按 base instance 读取 record 1/2，offset 为 `-0.55/0.00`，instance 1 colour 为
`.0625/.875/.1875/1.0`。两个 buffer 均进入标准自动格式 Buffer Viewer，状态栏为
`No problems detected`；最终进程保持运行在 T05 capture。

2026-09-22 的 T06 qrenderdoc 验证：EID 3 Texture Viewer Outputs 列出 FB0 Texture 24 与 FB1
Texture 17，前者显示 alpha blending 后的红褐背景/重叠三角形，后者显示 RGB write-mask 结果的蓝底
黄三角。OM 页显示两个 Color Targets；Blend State slot 0 为
`True / Src Alpha / 1 - Src Alpha / Add / One / Zero / Add / RGBA`，slot 1 为 disabled 且 `RGB_`。
实际导出的 `captures/metal-smoke/t06_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T06 EID 3 OM 页。

2026-09-22 的 T07 qrenderdoc 验证：EID 6 Texture Viewer 显示左绿右蓝三角形；OM 页显示 color
Texture 27、combined depth/stencil Texture 20、`Less / Enabled` depth state，以及 Front/Back
reference 5、compare mask `000000FF`、write mask `00000000`、Equal、`Inc Sat/Dec Sat`。共享
PipelineFlowChart 的 Home/End 键导航已实机从 IA 往返 OM。UI 实际导出的
`captures/metal-smoke/t07_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T07 EID 6 OM 页。

2026-09-22 的 T08 qrenderdoc 验证：EID 4 OM 页显示 Multisample State 为
`4 / Enabled / Disabled`，Color Targets 为 `Texture 17 / Texture 2D MS / 4 samples`，Resolve Targets
为 `Texture 24 / Texture 2D / 1 sample`。双击 resolve 行进入标准 Texture Viewer 后显示左红、中绿、
右蓝三角形，中心拾取为 `(0.06275, 0.87451, 0.18824, 1.00)`。实际导出的
`captures/metal-smoke/t08_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T08 EID 4。

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

更新节奏默认是“阶段开始一次、架构决策/中断时一次、阶段收口一次”。不要仅为了记录每个小修复而
反复重写长文档；测试覆盖或用户可见能力实际变化时，仍必须在收口前完整同步。

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
