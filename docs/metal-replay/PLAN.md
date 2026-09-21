# Metal Replay 总体计划

## 原则

1. 先打通纵向链路，再增加 API 宽度：UI 启动、样例原生运行、截帧、加载事件、replay 一个三角形。
2. Replay 是产品目标，capture 是不可省略的输入前提；先支持受控测试程序，不把任意应用注入列入首版。
3. 每个功能必须有最小样例、`.rdc` 夹具或自动测试，以及 UI/数据验证方法。
4. 每个阶段结束前必须更新 `STATUS.md`，补齐本阶段证据，并写好下一阶段的细化任务。
5. 不用空实现伪装支持。未支持能力必须返回明确结果或在 UI 中禁用，而不是崩溃或给出错误数据。

## 首版完成定义

首版（MVP）同时满足以下条件才算完成：

- qrenderdoc 在目标 macOS/Apple Silicon 机器上稳定启动。
- qrenderdoc 能识别并打开由受控测试程序生成的 Metal `.rdc`。
- Event Browser 能显示 render pass、draw、dispatch/blit（若测试中存在）及 debug marker。
- 能正确 replay 基础非索引、索引和实例化绘制。
- Buffer Viewer、Texture Viewer、Pipeline State、Mesh Viewer 和 Shader Viewer 对首版矩阵中的
  用例给出正确内容。
- 对支持矩阵中的每个 P0/P1 项都有可重复测试；重开 capture 和切换 event 不产生明显资源泄漏或崩溃。
- 已知限制被写入用户文档，不声称支持未覆盖的高级 Metal 特性。

## 阶段 0：仓库与可重复构建基线

目标：建立不依赖个人 shell 状态的 macOS 构建和启动路径。

任务：

- [x] M0.1 检出 RenderDoc `v1.46` 并记录精确提交。
- [x] M0.2 从 detached tag 创建开发分支 `metal-replay-v1.46`。
- [x] M0.3 盘点本机 Xcode、SDK、CMake、Ninja、Qt 和构建依赖。
- [x] M0.4 安装/修复 Qt 5.15、autoconf、automake、pcre 和必要的 bison 工具。
- [x] M0.5 先以 `ENABLE_METAL=OFF` 完成最小 qrenderdoc Debug 构建并启动 UI。
- [x] M0.6 以 `ENABLE_METAL=ON` 完成构建并启动 UI，记录与非 Metal 基线的差异。
- [x] M0.7 增加仓库内的 macOS 配置/构建/启动脚本或 CMake preset，避免手工命令漂移。
- [x] M0.8 记录产物路径、启动命令、日志位置和清理方式。

建议的最小配置从关闭无关驱动开始，以缩短反馈时间：

```sh
cmake -S . -B build-macos-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DQMAKE_QT5_COMMAND=<qt5-qmake-absolute-path> \
  -DENABLE_METAL=ON \
  -DENABLE_GL=OFF \
  -DENABLE_GLES=OFF \
  -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF
cmake --build build-macos-debug --target qrenderdoc
```

准确 target 名称和 app 路径以首次成功配置结果为准，随后写入脚本和 `STATUS.md`。

阶段验收：

- 一条文档化命令能从干净 build 目录构建 qrenderdoc。
- RenderDoc UI 可启动、显示主窗口并正常退出。
- `ENABLE_METAL=ON` 构建无未解释的链接或 Objective-C runtime 错误。

## 阶段 1：Metal 样例与最小 capture 闭环

目标：先让一个确定性的 Metal 样例原生运行，再通过 RenderDoc 生成结构正确的 `.rdc`。

任务：

- [x] M1.1 固定第一批样例来源，先选 T00 清屏与 T01 彩色三角形。
- [x] M1.2 将最小 macOS Metal 测试程序接入仓库构建，验证未注入时稳定运行。
- [x] M1.3 验证并修复测试程序加载 RenderDoc、hook `MTLCreateSystemDefaultDevice` 和对象包装路径。
- [x] M1.4 打通 frame trigger、`CAMetalLayer/nextDrawable`、present 和 `.rdc` 写盘。
- [x] M1.5 补齐 T00/T01 所需的 buffer、library/function、pipeline、render pass、draw 的 capture 序列化。
- [x] M1.6 用 structured export/chunk inspection 检查 `.rdc` 包含预期资源、命令和初始内容。
- [x] M1.7 固化“一键构建样例、一键截帧、输出 capture 路径”的测试脚本。

阶段验收：T00/T01 原生输出正确；RenderDoc 能稳定生成非空 Metal `.rdc`；文件 header、driver、
frame section 和核心 chunks 可检查。此时尚不要求 qrenderdoc 成功 replay。

状态：2026-09-20 已完成。自动验证入口为
`./util/buildscripts/scripts/test_metal_capture_macos.sh`。

## 阶段 2：Replay 后端骨架与基础图形 replay

目标：让 qrenderdoc 打开阶段 1 生成的 `.rdc`，重放一个 render pass 中的普通三角形，并形成
正确事件树。

任务：

- [x] M2.1 让 `MetalReplay` 实现 `IReplayDriver` 并注册 Metal replay provider。
- [x] M2.2 打通 `RDCFile -> ReadLogInitialisation -> ProcessChunk -> replay device`。
- [x] M2.3 实现 `AddEvent()`、`AddAction()`、event/action ID 分配和基础 action。
- [x] M2.4 补齐 T00/T01 的 command queue、command buffer、render encoder replay 生命周期。
- [x] M2.5 支持 T00/T01 render pass attachment 的 load/store/clear 基础语义。
- [ ] M2.6 支持 viewport、scissor、cull、front face、fill、depth bias 等常用固定状态。（T02
  所需 viewport/scissor/cull/front face 已完成 capture 与 GPU replay；fill/depth bias 等仍待 fixture。）
- [x] M2.7 支持基础 `drawPrimitives`，并让输出窗口通过现有 `CAMetalLayer` 显示结果。
- [x] M2.8 实现选定 event 的 full/without-draw/only-draw replay 区间语义。
- [x] M2.9 为当前未支持接口实现稳定降级，避免 UI 调用空指针或得到伪数据。

阶段验收：T00/T01 capture 能打开；Event Browser 的 pass/draw 次序正确；选择 draw 后输出与
原程序基准图一致。该 T00/T01 纵向切片已于 2026-09-21 完成，并通过同进程循环打开/关闭和
qrenderdoc 重开验证。M2.6 的完整固定状态仍随 T02/T07 等后续 fixture 扩展；索引绘制在阶段 5
的 Mesh Viewer 纵向切片中完成。

## 后续 feature 推进方式

阶段 3 以后不采用“先把所有样例都截成 `.rdc`，最后统一做 replay”的瀑布方式。每个测试场景按
以下顺序闭环后，才进入下一个场景：

1. 样例未注入时原生运行正确并保存参考输出/参考数据。
2. RenderDoc 能截取该样例，且 structured chunks 与调用参数一致。
3. replay 能执行到该 feature，不出现未处理 chunk 或资源缺失。
4. 对应 Buffer/Texture/Pipeline/Shader/Mesh UI 数据正确。
5. 把样例和 capture/replay 检查加入回归测试。

## 阶段 3：Buffer、Texture 与初始内容

目标：资源能被正确创建、恢复、枚举和读取。

当前进度：T00/T01 所需的 shared vertex buffer 原始数据，以及单采样 2D
RGBA8/BGRA8 render target 的枚举、原始读取、像素拾取和 DDS 保存已经闭环；完整 storage mode、
texture 类型/格式和 blit 覆盖仍按本阶段后续任务推进。T02 已新增 shared vertex/index buffer 与
private `Depth32Float` attachment 的创建/恢复，并自动逐字节验证 16/32-bit index 数据。T03 已覆盖
shared 4x4 RGBA8 sampled texture 的 descriptor、`replaceRegion` 初始内容、GPU replay、逐字节读取与
四个已知 texel 的 `PickPixel()`；mip/array/cube/view 与其他 storage mode 仍未完成。

任务：

- [ ] M3.1 完成 buffer 创建路径、storage mode、初始内容和生命周期映射。（shared vertex/index
  buffer 已覆盖；private/managed buffer 和更新路径待完成。）
- [ ] M3.2 实现 `GetBuffers()`、`GetBufferData()`、名称和派生关系。（T01 所需枚举和 shared
  buffer 原始字节已完成；名称、派生关系和更多 storage mode 待完成。）
- [ ] M3.3 完成 texture descriptor、子资源、mip、array/cube、初始内容和 view 映射。（T03 的单层
  shared RGBA8 2D descriptor 与 `replaceRegion` 初始内容已完成；其余子资源与 view 待扩展。）
- [x] M3.4a 实现 T00/T01 单采样 2D RGBA8/BGRA8 的 `GetTextures()`、`GetTextureData()`、
  `PickPixel()` 和保存路径。
- [ ] M3.4b 扩展常见整数/浮点/depth/stencil/压缩格式及 array/cube/3D/MSAA 子资源展示。
- [ ] M3.5 支持 blit copy/fill/mipmap generation 中 P0/P1 用例。
- [ ] M3.6 验证 render target、depth/stencil、MSAA resolve 和 sampled texture。（T00/T01 render
  target、T02 depth target、T03 sampled texture 已验证；stencil/MSAA resolve 待 fixture。）
- [ ] M3.7 增加大资源、零长度读取、越界范围和已销毁资源的错误测试。

阶段验收：Buffer Viewer 和 Texture Viewer 能通过 `TEST_MATRIX.md` 中所有 P0 资源用例，且内容
与测试程序写入的已知模式逐字节/逐像素匹配。

## 阶段 4：Pipeline State、Shader 与绑定反射

目标：在 UI 中准确解释选定 draw 的 Metal 管线和资源绑定。

当前进度：source-created library 的 MSL、`vs_main`/`fs_main` 入口和 vertex/fragment stage 已经通过
通用 Shader Viewer 展示并自动验证；T01 还已建立最小 Metal pipeline state snapshot 和专用状态页，
能显示 render pipeline、shader、topology、vertex buffer 与 color target。完整 descriptor、其余 stage
bindings、编译选项和 function constants 尚未实现。T02 的 interleaved vertex descriptor、
`Depth32Float` pipeline format、less/write depth state 与 front-face/cull/scissor 已完成 capture/XML
往返、真实 GPU replay、Metal pipeline snapshot 和 qrenderdoc 状态页；T02 vertex/index 输入现已
通过通用 `PipeState` 接入标准 Buffer/Mesh Viewer。Metal 专用 Pipeline 页的视觉布局仍是过渡实现，
后续按 D021 逐步收敛到 RenderDoc 其他后端的分组和操作习惯。T03 已进一步完成 fragment texture/
sampler 的 capture/replay、通用 descriptor 查询、Pipeline 展示和标准 Texture Viewer/Resource
Inspector 跳转。P4.4 的首个 UI 切片已复用标准 `PipelineFlowChart`，按 IA/VS/RS/FS/OM 分页，并
实现真实空槽显示及 shader 直接跳转；第二切片已将资源表迁移到 `RDTreeWidget`/`RDHeaderView`，
接入通用资源菜单、缩略图/预览分发与五阶段 HTML export。第三切片现已在 replay 创建 pipeline 时
请求 Metal argument reflection，枚举 T03 的 `colourTexture`/`colourSampler`，并用同一资源额外绑定到
shader 未声明的 slot 1 验证真实 used/unused 过滤：默认只显示 slot 0，启用 `Show Unused Items` 后
显示 slot 1。下一步继续完整状态字段与紧凑布局/键盘操作收敛。

任务：

- [ ] M4.1 保存并还原 render pipeline descriptor 的常用字段、color attachments 和 vertex descriptor。
  （T02 Float3/Float4 attributes、layout/stride/step 与 depth format 已验证往返并进入 snapshot/UI；
  完整格式和字段待继续覆盖。）
- [ ] M4.2 保存 depth/stencil、blend、raster、sample count 和 attachment 状态。（T02 depth
  compare/write、front face、cull、viewport/scissor 已进入 capture/replay/snapshot/UI；stencil、blend、
  sample count 等待后续 fixture。）
- [x] M4.3a 建立 T01 所需的 Metal pipeline state snapshot，接入 RenderDoc 通用 `PipeState`，并在
  qrenderdoc 显示 pipeline、shader、topology、vertex buffer 和 color target。
- [ ] M4.3b 扩展完整 render/depth/raster/blend/attachment 状态及后续 fixture 所需字段。
- [x] M4.3c 将 Metal Pipeline 页面接入标准 Controls、`PipelineFlowChart` 和 IA/VS/RS/FS/OM 阶段页，
  支持 empty-slot 显示及 shader/mesh/buffer/texture/resource 标准跳转。
- [x] M4.3d 对齐标准 `RDTreeWidget`/`RDHeaderView` 资源样式，复用通用上下文菜单与预览，并补齐
  IA/VS/RS/FS/OM HTML export。
- [x] M4.3e 以 shader resource reflection 启用真实 used/unused 过滤；无反射证据时保持禁用且不伪造
  使用状态。
- [ ] M4.3f 继续核对紧凑布局、键盘操作和更多状态字段，保持向 RenderDoc 标准页面收敛。
- [x] M4.4a 枚举 source-created vertex/fragment function 的 entry point 和 stage。
- [x] M4.4b 枚举当前 source-created MSL 的直接 fragment texture/sampler bindings，并将反射数组索引、
  Metal 物理 slot 与静态 active 状态接入通用 descriptor 查询。（fragment buffer、vertex
  texture/sampler 与 argument buffer 仍归 M4.7。）
- [x] M4.5a 对 `newLibraryWithSource` 保存并在 Shader Viewer 显示真实 MSL 源码。
- [ ] M4.5b 保留 library 编译选项和 function constants 元数据。
- [ ] M4.6 对预编译 `.metallib` 显示可获得的函数/反射信息；没有源码时明确标记，不伪造源码。
- [ ] M4.7 支持 buffer/texture/sampler 的 vertex 和 fragment stage 绑定。（T01/T02 vertex buffer
  与 T03 fragment texture/sampler binding 已完成；fragment buffer 及 vertex texture/sampler 待 fixture。）

阶段验收：Pipeline State 页面与测试程序创建参数一致；点击 shader 能看到正确入口和可获得的
MSL；绑定资源可跳转到对应 Buffer/Texture。

## 阶段 5：Mesh Viewer 与常用绘制覆盖

目标：从 Metal vertex descriptor 和 draw 参数重建网格输入。

任务：

- [ ] M5.1 映射 Metal vertex format、buffer layout、step function、stride 和 attribute offset。（T02
  的 Float3/Float4、per-vertex、stride 28、offset 0/12 已进入 pipeline snapshot、通用
  `GetVertexInputs()`、Buffer Viewer 和 Mesh Viewer；完整格式仍待后续 fixture。）
- [ ] M5.2 支持 16/32 位 index、base vertex、base instance 和 instance step rate。（直接 16/32-bit
  indexed draw 已完成 capture/replay/action/字节及 Mesh Viewer 验证；base/instance 重载待后续 fixture。）
- [ ] M5.3 为非标准/缺失 vertex descriptor 提供手工格式查看能力和清楚限制。
- [ ] M5.4 验证多 vertex buffer、interleaved/deinterleaved、instancing 和 primitive 类型。
- [ ] M5.5 校验 Mesh Viewer 的 VS input 与 replay 图像一致。（T02 已完成表格、UInt16/UInt32 索引和
  线框输出自动/实机验证；后续多 buffer、instancing 场景仍需覆盖。）

阶段验收：测试矩阵中的 triangle、indexed cube、多 buffer 和 instanced mesh 均能正确显示顶点值、
索引和几何形状。

## 阶段 6：Compute、同步与常用扩展面

目标：覆盖常见图形工作负载中与渲染紧密相关的 compute/blit/synchronization。

任务：

- [ ] M6.1 支持 compute pipeline、dispatch threadgroups/threads 和资源绑定。
- [ ] M6.2 支持常用 blit encoder 操作以及 encoder/command buffer 间资源可见性。
- [ ] M6.3 支持 fence/event/managed-resource 同步中实际测试需要的子集。
- [ ] M6.4 支持 argument buffer 的只读查看与常见资源引用解析。
- [ ] M6.5 根据样例结果决定是否把 indirect command buffer/heaps 纳入首版扩展。

阶段验收：compute texture processing 样例可 replay，dispatch 前后资源值正确；不支持的高级能力
有明确诊断且不会破坏同帧其他事件。

## 阶段 7：稳定性、回归与交付

目标：把实验后端整理为可持续开发的 replay 版本。

任务：

- [ ] M7.1 建立一键运行的样例构建、capture 生成、replay smoke test 和结果比对。
- [ ] M7.2 对 capture 打开/关闭、事件切换、资源查看、窗口 resize 做循环稳定性测试。
- [ ] M7.3 检查 Objective-C retain/release、wrapped/live resource 映射和 GPU 等待点。
- [ ] M7.4 在 Debug/Release、当前 macOS/SDK 和至少一个额外受支持 macOS 环境验证。
- [ ] M7.5 整理已知限制、支持矩阵、构建说明和故障排查。
- [ ] M7.6 进行代码审查、格式化和与 RenderDoc 通用 replay 约定的最终核对。

阶段验收：P0/P1 自动测试通过；手工 UI 验收清单通过；文档能指导新环境从源码构建并重现结果。

当前进度：`test_metal_capture_macos.sh` 已覆盖 T00/T01/T02/T03 的构建、原生运行、capture、structured
inspection、CLI replay、output 像素、event seek、shader reflection、texture readback/pick/save；
T01 的最小 pipeline state 也会自动断言 pipeline/shader/topology/vertex buffer/color target；T02
会断言 draw-time vertex descriptor、depth/raster state、16/32-bit index binding、clear/draw1/draw2/
回退图像、index buffer 原始数据、通用 VS input 映射与 Metal mesh preview 输出；T03 会断言 4x4
RGBA8 upload、nearest/clamp sampler、fragment slots、通用 texture/sampler descriptor、四象限 GPU
输出和精确 texel/`PickPixel()`；M7.1
保持未完成，直到其余 P0/P1 场景进入同一回归入口。

T02 的 Event、Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 2 显示左半屏 viewport/
scissor 和 `Buffer 19 / 72 / UInt16`，EID 3 切换为右半屏和 `Buffer 20 / 144 / UInt32`；两者均显示
Float3/Float4 attributes、stride 28、less/write depth、back cull/CCW、color `Texture 27` 与 depth
`Texture 17`，双立方体图像正确且状态栏无错误。P3.4 进一步验证 VS Input 表格、立方体线框、
interleaved vertex Buffer Viewer，以及 UInt16/UInt32 index Buffer Viewer；Pipeline 资源激活行为已
复用标准 Mesh/Buffer Viewer，但 Pipeline 页面视觉排版仍按 D021 留作后续收敛。

T03 的 Event、Texture 与 Pipeline State 也已完成 qrenderdoc 实机验证：EID 2 显示四象限纹理四边形、
`Triangle Strip`、Float2/Float2 vertex input、fragment Texture 17 和 nearest/clamp Sampler 18；纹理资源
双击进入标准 Texture Viewer，sampler 进入 Resource Inspector，状态栏为 `No problems detected`。

同一回归入口还会在单进程中依次打开并关闭 T00/T01/T02/T03 各 10 次，检查资源、event、texture readback
和 unsupported 接口。2026-09-21 完整运行的基线后 resident growth 为 491,520 字节；shader debug、
pixel history、histogram、post-VS 和 custom/target shader build 均返回稳定的空结果或明确错误。
完成 P4.4 第三个 UI 切片后的最新完整运行覆盖四份 capture，resident growth 为 540,672 字节。这只关闭当前四份
fixture 的资源生命周期缺口，不代表阶段 7 对全部 P0/P1 场景的稳定性验收已经完成。

## 风险与应对

- Metal capture 输入不足：优先用小型自有测试程序和现有 capture 骨架生成确定性 `.rdc`；不把
  任意应用注入作为前置条件。
- 新 SDK 与 v1.46 兼容性：先固定已验证 Xcode/SDK，针对 SDK 26 的编译差异单独记录补丁。
- Qt 5 在新 macOS 上的兼容性：固定 Qt 5.15 路径；必要时把 UI 基线与 replay library 构建拆开。
- `IReplayDriver` 接口面很大：先使用 RenderDoc 的 dummy driver 组合并显式覆盖必须正确的接口，
  避免一次性复制其他后端的大量无关实现。
- Metal shader 源码并非总能从 `.metallib` 恢复：首版保证 source-created library 的 MSL 展示，
  对 binary library 只展示真实可用信息。
- Apple Silicon unified memory 会掩盖离散 GPU storage 问题：测试矩阵仍区分 shared/private/managed，
  无法在本机验证的模式标记为待外部机器验证。
