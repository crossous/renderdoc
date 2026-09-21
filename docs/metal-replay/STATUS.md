# Metal Replay 当前状态

最后更新：2026-09-22（Asia/Shanghai）

## 当前阶段

- 阶段：T03 纹理采样、资源绑定与 Pipeline UI 收敛已关闭，准备进入下一条 P1 纵向切片
- 状态：T00-T03 既有纵向切片保持关闭；P4.1-P4.5 自动化与 qrenderdoc 实机验证均已完成
- 当前任务：P4.4 第三切片已完成；下一任务为 `PHASE5.md` P5.1 的 T04 确定性 native fixture
- 上一阶段：T03 Native/Capture/RDC inspect/Replay/Texture/Sampler/Binding/Pipeline/lifecycle 闭环
  已完成
- 下一验收点：T04 能以固定 buffer 内容、offset 和多 draw 参数完成 Native/Capture/RDC inspect 基线，
  并为 fragment buffer binding 与 Pipeline 资源布局提供可验证输入

## 已完成

- RenderDoc 官方仓库已检出到工作区。
- 已核验标签 `v1.46`，基线提交为 `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`。
- 已创建本地开发分支 `metal-replay-v1.46`。
- 已确认仓库原有 Metal 骨架位于 `renderdoc/driver/metal`。
- 已完成 Metal 代码和本机工具链的第一轮静态盘点。
- 已建立计划、测试矩阵、决策和交接文档。
- 已安装 Qt 5.15.19、autoconf 2.73、automake 1.19、PCRE 8.45 和 bison 3.8.2。
- 已完成 `ENABLE_METAL=OFF` 的 qrenderdoc Debug 构建并验证主窗口。
- 已为 Xcode 26 / SDK 26 增加最小 Metal bridge 编译兼容补丁。
- 已完成 `ENABLE_METAL=ON` 的 qrenderdoc Debug 构建并重新验证主窗口。
- 已增加可重复构建/启动脚本 `util/buildscripts/scripts/build_metal_dev_macos.sh`。
- 已写好阶段 1 文件级计划 `PHASE1.md`。
- 已将 T00 `Metal_Empty_Frame` 和 T01 `Metal_Simple_Triangle` 接入 `util/test/demos`。
- 两个样例未注入 RenderDoc 时均能稳定运行；T01 的运行时 MSL 编译和三角形绘制通过。
- 修复 macOS 26 `CAMetalLayer` 内部 residency set 与代理 `MTLDevice` 不兼容导致的崩溃。
- 建立 drawable texture 的真实对象/RenderDoc wrapper 映射，render pass 可继续使用受跟踪纹理。
- 修复 app-controlled capture 在 present 时不记录 backbuffer、导致 `EndFrameCapture` 失败的问题。
- T00/T01 均能通过 `DYLD_INSERT_LIBRARIES` 和 in-app API 生成 Metal `.rdc`。
- T00 连续三次 capture 均成功，验证了最小链路稳定性。
- 注册 Metal structured processor；`renderdoccmd convert -c xml` 可解析 capture chunks。
- T01 structured XML 已核验 MSL、入口名、96 字节 vertex buffer、pipeline/binding、draw 和 present。
- 新增一键回归脚本 `util/buildscripts/scripts/test_metal_capture_macos.sh`。
- 已写好阶段 2 文件级计划 `PHASE2.md`。
- `MetalReplay` 已实现 `IReplayDriver` 并注册 `RDCDriver::Metal` replay provider。
- replay 初始化会创建真实 `MTLDevice`，并沿现有 structured 路径读取 capture。
- 已重建 T00/T01 所需的 device、queue、drawable 替代 texture、command buffer、render encoder、
  MSL library/function、pipeline、vertex buffer 和基础 render 命令。
- 已执行 T00/T01 的 clear、pipeline/buffer binding、`drawPrimitives`、commit/wait。
- 已建立 render pass、clear、draw、present 和 capture end 的最小 action/event。
- 已提供基础资源、buffer、texture 描述和 shared buffer 原始字节读取。
- `renderdoccmd replay --loops 1` 已分别对 T00/T01 成功运行并正常退出。
- qrenderdoc 已成功加载 `t01_capture.rdc`。此前的 degraded 弹窗来自 Metal replay 主动声明，
  并非 capture 加载失败或实际回退到软件渲染；完成 Texture Viewer 后已移除该标记。
- 已实现 macOS `CAMetalLayer` output window、尺寸/resize 跟踪、持久 BGRA8 output texture、clear、
  present 和 output readback。
- 已实现最小 fullscreen Metal texture display pipeline，支持 T00/T01 所需的 2D 单采样 color texture、
  fit/scale/pan、mip、flip、channel mask 和 range 映射。
- T00 output readback 得到预期 clear 色；T01 output readback 得到预期黑色背景和彩色三角形。
- qrenderdoc Texture Viewer 已实机显示 T01 三角形；窗口最大化后 resize/fit 仍正确，状态栏为
  `No problems detected`。
- Texture Viewer 闭环通过后已将 `APIProperties.degraded` 改为 `false`，新构建不再弹 degraded
  support 警告。
- `test_metal_capture_macos.sh` 现会编译离屏 output smoke、输出 T00/T01 PPM，并断言关键像素。
- 已保留 `CaptureBegin` 后的 frame stream，并为每个 API event 记录 frame-relative file offset。
- 已实现 T00/T01 所需的 `ReplayLog(Full/WithoutDraw/OnlyDraw)` 区间执行和未闭合 Metal
  encoder/command buffer 的安全收尾。
- 自动化会在同一个 replay controller 上连续 10 轮执行 T01 `clear -> draw -> clear`，中心像素依次为
  `#14141a -> #83645f -> #14141a`，验证前进和回退均会真实重放。
- qrenderdoc 实机切换 EID 1（clear）、EID 2（draw）、EID 1（clear）时画面分别为纯背景、彩色
  三角形、纯背景，状态栏始终为 `No problems detected`。
- 已实现 capture texture 的 `GetTextureData()`：通过 Metal blit 将单采样 2D RGBA8/BGRA8 mip
  复制到 shared buffer，并移除 Metal 行对齐 padding 后返回紧密排列的原始字节。
- 已基于同一 readback 路径实现 `PickPixel()`，按 RGBA/BGRA 通道顺序返回归一化浮点值；超出当前
  范围的 remap、resolve、MSAA、array/cube/3D、depth/stencil、压缩及其他格式均明确报 unsupported。
- T01 自动验证了 clear EID 的 400x300 BGRA backbuffer、背景字节 `1a1414ff`、背景拾取值约
  `(0.08, 0.08, 0.10, 1.0)`，以及 draw EID 中心像素不再是背景色。
- `ReplayController::SaveTexture()` 已使用同一路径成功保存 T01 为 400x300 ARGB8888 DDS，产物
  `captures/metal-smoke/t01_texture.dds` 为 480128 字节。
- 已把 `ShaderEncoding::MSL` 接入通用 replay/API 字符串与 qrenderdoc 源码高亮；source-created
  library 的源码会随 replay library 保存，并为 Metal function 生成真实 entry point、stage 和
  `ShaderReflection`，不为没有源码的 binary library 伪造 MSL。
- T01 自动断言恰有 `vs_main`/Vertex 与 `fs_main`/Fragment 两个 shader resource，两者均返回
  `captured.metal`、MSL encoding 和同一份包含真实入口声明的源码。
- qrenderdoc 已从 Resource Inspector 分别打开 Function 13/14；两个 Shader Viewer 均显示
  `captured.metal` 的真实 `metal_stdlib`、vertex/fragment 源码，状态栏保持 `No problems detected`。
- 修复 qrenderdoc 恢复到旧 D3D11 Pipeline State 子页面后加载 Metal capture 会错误调用 D3D11
  controller 并崩溃的问题：非 D3D/GL/Vulkan API 现在清空旧后端子页面，Metal capture 可稳定加载。
- 已新增独立 `MetalPipe::State`，并通过 replay controller、proxy serialization 和通用 `PipeState`
  暴露 T01 所需的 render pipeline、vertex/fragment shader、topology、vertex buffer 和 color target；
  不把 Metal 状态伪装成 D3D/GL/Vulkan。
- replay 会在 render pass、pipeline/buffer bind 和 draw 时更新 Metal snapshot；T01 自动断言
  `Pipeline State 15`、`Function 13/14`、`vs_main/fs_main`、`TriangleList`、96-byte `Buffer 16` 和
  swapbuffer `Texture 23` 均来自真实 replay 资源。
- qrenderdoc 已接入最小 Metal Pipeline State 页面。实机重新启动本次构建、加载最新 T01 并选择
  EID 2 后，页面显示上述 pipeline/shader/topology/VB/color target，状态栏为
  `No problems detected`；各资源行可进入 Resource Inspector。
- replay resource manager 现会确定性释放 wrapper 与真实 Metal 对象；wrapper 区分 `new*` 转移来的
  retained 对象和需要主动 retain 的 autoreleased command buffer/render encoder，并在重复 replay
  替换 live 对象时释放旧引用。ObjC embedded bridge 仅保留在 capture 路径。
- 新增 `metal_replay_lifecycle_smoke.mm`，在同一进程中打开/关闭 T00/T01 各 10 次并覆盖 action、
  texture readback 与 unsupported 接口；完整回归在两轮 warm-up 后 resident growth 为 491,520 字节，
  额外 50 轮压力检查增长 1,441,792 字节。
- shader debug 不再返回会被控制器解引用的空指针；四种 debug 调用返回可释放的空 trace。histogram、
  pixel history、post-VS 和 custom/target shader build 也返回稳定空结果或明确错误。
- 最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`。重开后的 T01 EID 2 仍显示
  彩色三角形和完整最小 Pipeline State；T00/T01 状态栏均为 `No problems detected`，History/Debug
  按钮明确提示不支持并保持禁用。
- 已新增 `PHASE3.md`，将 T02 拆为 fixture、capture/resource、GPU replay/action、UI/data 和阶段关闭。
- 已新增确定性的 `Metal_Indexed_Cube`（T02）：一个 interleaved Float3 position/Float4 color vertex
  buffer、36 个 UInt16 index 和同内容的 UInt32 index、private `Depth32Float` attachment、less/write
  depth state，以及左右两个 viewport/scissor 的 indexed cube。
- 新增 `WrappedMTLDepthStencilState` 及 Objective-C bridge，序列化/重建
  `newDepthStencilStateWithDescriptor` 和 `setDepthStencilState`；descriptor 当前真实保存 label、depth
  compare 和 depth write，stencil face 明确保留为后续范围。
- 已接通 T02 所需的 `setScissorRect`、`setFrontFacingWinding`、`setCullMode` 和直接
  `drawIndexedPrimitives` capture/replay；indexed action 标记 `ActionFlags::Indexed` 并保存 count/offset。
- 修复 render pass 只引用 color attachment 的缺口：capture 现在统一追踪 color/depth/stencil 及
  resolve texture，因此 T02 private depth texture 的创建 chunk 会进入 `.rdc`，replay depth target 非空。
- 一键脚本现会原生运行并重新截取 T02，XML 断言 stride/attribute/depth/raster/scissor/index 参数，
  replay smoke 逐字节比较 72-byte UInt16 与 144-byte UInt32 index buffer，并检查左右半屏图像。
- P3.2 完整回归通过 T00/T01/T02 capture/replay 与三份 capture 各 10 次同进程开关；warm-up 后
  resident growth 为 1,196,032 字节，低于 64 MiB 阈值。
- 最新 qrenderdoc 已加载 T02，Event Browser 显示 EID 2/3 两个 `drawIndexedPrimitives(36)`；选中
  EID 3 后 API Inspector 标明 UInt32/Buffer 20，Texture Viewer 同时列出 color Texture 27 与 depth
  Texture 17，双立方体图像正确，状态栏为 `No problems detected`。
- `MetalPipe::State` 已扩展 vertex attributes/layout、index buffer、depth state 和 raster state，并通过
  通用 `PipeState` 暴露 index、viewport/scissor、depth/raster 查询；proxy serialization 同步覆盖。
- 修复 pipeline state 的 event 边界：加载 capture 时按 action event 保存 draw-time snapshot，避免
  `OnlyDraw` 区间继续执行到下一 event 前时把第一条 draw 的动态状态覆盖为第二条 draw 状态。
- T02 replay smoke 现逐 draw 断言 Float3/Float4、stride 28、UInt16/UInt32 binding、less/write、
  back/CCW、左右 viewport/scissor，并验证 clear -> draw1 -> draw2 -> draw1 回退的 BGRA 图像。
- 最新完整回归重新生成 T00/T01/T02 并全部通过；三份 capture 各 10 次 lifecycle 的 resident growth
  为 720,896 字节。最新 qrenderdoc 中 EID 2/3 Pipeline State 分别显示左/右 viewport 与
  `Buffer 19 / 72 / UInt16`、`Buffer 20 / 144 / UInt32`，其余 vertex/depth/raster/target 字段一致，
  状态栏为 `No problems detected`。
- `PipeState::GetVertexInputs()` 已映射 Metal attribute/layout，标准 Mesh Viewer 能按 T02 UInt16/UInt32
  index 展开 `attr0` Float3 与 `attr1` Float4；Metal output backend 可绘制当前 VS Input Float2/3/4
  点线/三角拓扑，自动 smoke 会检查 indexed cube 线框产生真实非背景像素。
- Pipeline State 的 vertex attribute 激活现进入 Mesh Viewer；vertex buffer 使用通用自动格式进入
  Buffer Viewer，index buffer 分别用 `ushort index`/`uint index` 查看精确子范围。Post-VS 输出表明确
  显示 Metal 不支持，不返回伪数据。
- P3.4/P3.5 最新完整脚本通过，三份 capture 各 10 次 lifecycle 的 resident growth 为 294,912 字节。
  最新 qrenderdoc 实机验证 EID 2 的 VS Input 表格/立方体线框、Buffer 18 interleaved 数据、Buffer 19
  UInt16 数据及 EID 3 Buffer 20 UInt32 数据，状态栏保持 `No problems detected`。
- 已新增确定性的 `Metal_Textured_Quad`（T03）：4x4 RGBA8 四色块纹理、Float2 position/UV、
  triangle strip 与 nearest/clamp sampler，原生运行、capture 和 structured XML 参数断言均通过。
- 已实现单层 2D `MTLTexture::replaceRegion` capture/replay，以及完整 wrapped sampler resource 的
  创建、序列化、重建和释放；`setFragmentTexture`/`setFragmentSamplerState` 会真实 replay 并更新状态。
- T03 replay smoke 精确验证 64-byte RGBA8 内容、四个 texel 的 `PickPixel()`、四象限 GPU 输出、
  `TriangleStrip`、Float2/Float2 input 和 fragment slot 0 的 texture/sampler。
- fragment texture/sampler 已经通过通用 descriptor API 暴露给 `PipeState`。Metal Pipeline 页面显示
  Fragment Textures/Samplers；texture 双击进入标准 Texture Viewer，sampler 进入 Resource Inspector。
- 最新 qrenderdoc 已加载 T03 EID 2，显示 Texture 17、Sampler 18（Point/Point/None、ClampEdge x3）和
  正确四色纹理；Texture Viewer/Resource Inspector 跳转均通过，状态栏保持 `No problems detected`。
- Metal Pipeline 页面已改用标准 Controls + `PipelineFlowChart` + 隐藏阶段页，形成 IA/VS/RS/FS/OM
  信息架构；`SelectPipelineStage()` 现在会切换对应页面，shader 行直接进入通用 Shader Viewer。
- `Show Empty Items` 会对已知空 binding/target/state 显示标准红色空槽；没有静态 shader resource
  reflection 的 pipeline 会禁用 `Show Unused Items` 并说明原因，避免将“已绑定”错误解释为“shader 使用”。
- 最终构建已分别用 T03/T02 实机核对新布局：T03 FS 显示 Texture 17/Sampler 18，IA/OM 空槽开关与
  shader 直接跳转正常；T02 IA、RS、OM 分别保持 UInt16 输入、左 viewport/scissor + Back/CCW、
  Texture 27 + Depth Texture 17 + Less/Write。两份 capture 状态栏均为 `No problems detected`。
- Metal Pipeline 的所有表已迁移到标准 `RDTreeWidget`/`RDHeaderView`，并通过父级
  `SetupResourceView()` 复用资源上下文菜单、usage、thumbnail/preview；Metal ResourceId 已补入三个
  通用分发点。最终 T03 FS 表双击 Texture 17 仍进入标准 Texture Viewer。
- 工具栏已加入标准 Export 控件；最终 qrenderdoc 在 T03 EID 2 实际写出
  `/tmp/metal-pipeline-t03.html`，文件包含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和 Sampler 18。
  随后完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 524,288 字节。
- replay 创建 Metal render pipeline 时会请求 argument reflection；T03 fragment shader 现枚举真实
  `colourTexture`/`colourSampler`、slot 0、类型、只读状态和 active 状态，并接入通用
  `ShaderReflection`/descriptor 查询。无反射证据的 pipeline 继续保守降级。
- T03 fixture 将同一 texture/sampler 额外绑定到 shader 未声明的 slot 1。自动回归验证 slot 0 映射
  reflection index 0 且为 used，slot 1 为 `NoShaderBinding + staticallyUnused`，`onlyUsed=true` 仅返回
  slot 0。最终 qrenderdoc 默认隐藏 slot 1；启用 `Show Unused Items` 后 texture/sampler 表均显示真实
  slot 1，状态栏为 `No problems detected`。最新完整 lifecycle resident growth 为 540,672 字节。

## 已验证环境

| 项目 | 当前值 | 结论 |
| --- | --- | --- |
| macOS | 26.1 / Build 25B5042k | 目标主机 |
| 架构 | arm64 | Apple Silicon |
| Xcode | 26.0.1 / Build 17A400 | 高于仓库最低要求 12.2 |
| macOS SDK | 26.0 | 需要留意 v1.46 与新 SDK 的兼容差异 |
| CMake | 4.4.3 | 高于 Apple 构建最低要求 3.23 |
| Ninja | 1.13.2 | 可用 |
| Qt 5 qmake | 5.15.19 | 已验证 |
| autoconf | 2.73 | 已验证 |
| automake | 1.19 | 已验证 |
| PCRE | 8.45 | 已验证；CMake 当前仍选择本地构建副本 |
| bison | 3.8.2 | 已验证 |

Qt 5 会警告它只测试到 macOS SDK 14，当前 SDK 26 属于 Qt 未验证组合，但实际编译和窗口启动已
通过。SWIG 配置期间 macOS 打印过缺少 Java Runtime 的提示，后续 SWIG build/bindings 生成仍成功。

## 代码基线发现

- `renderdoc/driver/metal` 顶层源文件总计约 15,395 行。
- `METAL_NOT_HOOKED()` 约 237 处。
- TODO/FIXME/未实现/未处理类标记约 402 处。
- `MetalReplay` 已继承 `IReplayDriver`，但大量进阶接口仍明确返回 unsupported/空结果。
- Metal replay provider 已注册，T00/T01 可进入真实 GPU replay 初始化。
- `WrappedMTLDevice::ProcessChunk()` 只覆盖一小部分 device/resource/render encoder chunk。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 已接入最小 frame record。
- qrenderdoc 已有 macOS `CAMetalLayer` 输出窗口适配，能作为后续 replay output 的基础。

## 当前阻塞与风险

1. Qt 5.15 对 SDK 26 给出未验证警告，后续 UI 回归需要持续关注。
2. SDK 26 新增的 Metal 协议方法目前由 Objective-C forwarding 转交真实对象，尚未被 capture。
3. Metal replay 不是局部补丁：接口注册、事件模型、资源读取、状态快照和输出都需要实现。
4. Texture Viewer 和 texture readback 当前只支持单采样 2D RGBA8/BGRA8 color texture；
   array/cube/3D、MSAA、depth/stencil、整数、浮点和压缩格式尚未实现。
5. 当前 drawable hook 只验证了本机 `CAMetalDrawable` 具体类；后续需要覆盖多屏/不同 GPU 可能出现
   的其他 drawable class。
6. 当前 event-range replay 已覆盖 T00/T01 与 T02 的单 command buffer、单 render pass，并包含 T02
   两次 indexed draw 前进/回退专项断言；多 command buffer、多 pass、嵌套 debug group 和
   load-action initial contents 仍需按后续样例扩展。
7. `OnlyDraw` 遵循 RenderDoc 控制器约定，依赖紧邻的 `WithoutDraw` 建好同一 encoder 的前置状态；
   当前不承诺把 `OnlyDraw` 当作独立入口调用。
8. T00/T01/T02/T03 的 replay wrapper/Metal object 释放已经完成并通过循环测试；后续 wrapper 必须继续
   遵守 D017 的 transferred/retained 所有权规则，避免重新引入双重释放或泄漏。
9. 当前 Metal Pipeline State 承诺 T01 基础字段、T02 的 Float3/Float4 vertex descriptor、UInt16/32
   index/depth/raster，以及 T03 fragment texture/sampler；其他 vertex format、blend、stencil、完整
   attachment 参数、fragment buffer 和 vertex texture/sampler bindings 仍归 M4 后续范围。
10. 当前只支持直接 indexed draw 重载；instanced/base vertex/base instance/indirect indexed draw 在
    对应 fixture 加入前继续明确 unsupported。
11. 当前 Metal mesh renderer 只承诺 VS Input 的 Float2/Float3/Float4 和 Metal 可直接绘制的常见
    point/line/triangle topology；post-VS、选点、高亮、solid/secondary/bbox 等仍待后续实现。
12. Metal Pipeline State 已接入标准 IA/VS/RS/FS/OM、empty-slot、RDTree 资源操作/预览、HTML export
    和有反射证据的 used/unused 过滤；剩余差异是更细的紧凑布局/键盘核对、更多状态字段和
    fragment buffer/vertex texture/sampler 等绑定类型。

## 下一步（按顺序）

1. 执行 `PHASE5.md` P5.1，建立 T04 动态 uniform 确定性 fixture，固定 buffer 内容、offset 与多 draw 输出。
2. 以 T04 扩展 fragment buffer capture/replay/reflection 与 Pipeline 展示，同时继续对照 D3D/Vulkan
   核对紧凑布局和键盘操作。

## 构建与启动

```sh
./util/buildscripts/scripts/build_metal_dev_macos.sh
./util/buildscripts/scripts/build_metal_dev_macos.sh --run
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

- Build 目录：`build-macos-debug`
- App：`build-macos-debug/bin/qrenderdoc.app`
- 核心库：`build-macos-debug/lib/librenderdoc.dylib`
- CLI：`build-macos-debug/bin/renderdoccmd`
- Metal demos：`bin/demos_x64`
- smoke capture/XML：`captures/metal-smoke/`（生成目录，不提交）
- 清理：删除 `build-macos-debug` 后重新运行脚本；该目录已被 `.gitignore` 忽略。

## 最近验证

```sh
cmake --build build-macos-debug --target renderdoc renderdoccmd build-qrenderdoc -j 12
./util/buildscripts/scripts/test_metal_capture_macos.sh
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t00_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t01_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t02_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t03_capture.rdc
git diff --check
```

2026-09-21 均通过。另在 qrenderdoc 中手工验证 T01 Texture Viewer 和窗口最大化 resize，画面正确，
状态栏显示 `No problems detected`，未再出现 degraded support 弹窗。事件回放自动化与 qrenderdoc
手工切换 EID 1 -> 2 -> 1 也通过，画面按 clear -> draw -> clear 正确变化。
`test_metal_capture_macos.sh` 还自动验证 capture texture 的紧密 BGRA 字节、clear/draw 像素拾取和
DDS 保存；`t01_texture.dds` 经 `file` 识别为 400x300、32-bit ARGB8888。
qrenderdoc 中也已手工验证 EID 2 的 Texture Viewer：右键拾取纹理坐标 `(202, 128)` 得到
`(0.61176, 0.34118, 0.32157, 1.00)`，状态栏为 `No problems detected`；“Save selected Texture”
成功写出 `t01_texture_ui.dds`，同样识别为 400x300、32-bit ARGB8888、480128 字节。
最终收口时将 `GetTextureData()`/`PickPixel()` 的资源类型校验改为 replay texture 描述表，避免因
live wrapper 不保留 capture record 而误拒绝合法纹理；修正后重新执行完整 capture smoke、T00/T01
单轮 CLI replay 和 `git diff --check`，均通过。
2026-09-21 的 Shader Viewer 收口中，自动回归验证了 `vs_main`/Vertex、`fs_main`/Fragment 和真实
MSL source。随后启动新构建 qrenderdoc，分别从 Function 13/14 打开 `captured.metal`，源码显示正确、
状态栏无错误。测试期间还复现并修复了持久化 D3D11 Pipeline State 子页面误用于 Metal capture 的
崩溃；修复后的同一路径已重新实机加载通过。
最终重新执行 `test_metal_capture_macos.sh`，生成新 T00/T01 capture，并通过 shader reflection、
texture data/pick/save 和 10 轮 event replay；随后 T00/T01 各自执行 `renderdoccmd replay --loops 3`
也均以状态 0 退出。
2026-09-21 的 Pipeline State 收口中，`renderdoc`、`renderdoccmd`、SWIG 与 qrenderdoc 均成功构建；
完整 smoke 重新生成 T00/T01，并自动验证 Metal pipeline/shader/topology/VB/color target 与原有
shader/texture/event 路径。随后关闭旧进程、启动新 qrenderdoc，在最新 T01 EID 2 的 Metal Pipeline
State 页面核对 `Pipeline State 15`、`Triangle List`、`Function 13/14`、`vs_main/fs_main`、
`Buffer 16`（0/96）和 `Texture 23`（mip/slice 0），状态栏为 `No problems detected`。
2026-09-21 的 M2.6 阶段关闭中，完整一键 smoke 再次通过并自动执行 T00/T01 各 10 次同进程
打开/关闭，warm-up 后 resident growth 为 491,520 字节；两份 capture 的 `renderdoccmd replay
--loops 3` 均以状态 0 退出。最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`，
重开 T01 EID 2 后图像和 Pipeline State 仍正确，状态栏无错误；History/Debug 明确禁用。工作区路径
通过 Recent Captures 打开时曾被 macOS 26 阻塞在文件 `open()`，使用 SHA-256 相同的 `/private/tmp`
副本完成验证，因此该现象记录为宿主文件访问问题，不计为 replay 失败。
随后额外运行 T00/T01 各 50 次 lifecycle 压力检查，resident growth 为 1,441,792 字节并通过。
2026-09-21 的 T02 P3.1/P3.2 回归中，一键脚本重新构建并原生运行 T00/T01/T02，生成三份 capture
与 XML；T02 的 vertex descriptor、private depth texture/state、CCW/back cull、两组 viewport/scissor、
36 个 UInt16/UInt32 index draw 全部通过 structured 断言。Replay smoke 验证两个 indexed action、
真实 depth target、已知 index 字节与左右半屏输出；三份 capture 各 10 次生命周期循环通过，resident
growth 为 1,196,032 字节。
同一份 T02 capture 随后由最新 qrenderdoc 实机打开；EID 3 的 UInt32 indexed action、color/depth
outputs 和双立方体图像均正确，状态栏无错误。因 macOS 26 对工作区 Documents 路径弹出文件访问
授权，仍按既有方式使用 SHA-256 相同的 `/private/tmp` 副本完成验证。
2026-09-21 的 T02 P3.3 收口中，Metal pipeline snapshot 增加 vertex descriptor、index buffer、depth
与 raster 字段，并改为按 action event 保存 draw-time 状态。完整脚本重新截取三份 capture，验证
T02 clear/draw1/draw2/回退图像和两条 draw 的精确状态，lifecycle resident growth 为 720,896 字节。
随后关闭旧 qrenderdoc、启动最新构建，加载 SHA-256 与工作区一致的 T02 副本：EID 2 显示左半屏、
UInt16 Buffer 19/72，EID 3 显示右半屏、UInt32 Buffer 20/144；Float3/Float4、stride 28、less/write、
back/CCW 与 color/depth target 均正确，状态栏为 `No problems detected`。
2026-09-21 的 T02 P3.4/P3.5 收口中，Metal vertex inputs 接入通用 `PipeState`，Pipeline 资源激活
复用标准 Mesh/Buffer Viewer，并新增最小 VS Input wireframe renderer。完整脚本重新截取三份 capture，
验证 T02 generic vertex input 与 mesh output，lifecycle resident growth 为 294,912 字节。最新 qrenderdoc
重启后，EID 2 的 Mesh Viewer 显示 UInt16 展开的 `attr0/attr1` 和正确立方体线框；Buffer 18 自动显示
Float3/Float4 interleaved 数据，Buffer 19 显示 72-byte `ushort index`；EID 3 的 Buffer 20 显示
144-byte `uint index`，状态栏始终为 `No problems detected`。收尾时 T00/T01/T02 又分别执行
`renderdoccmd replay --loops 3`，并通过 `git diff --check`。

2026-09-21 的 T03 P4.1-P4.3/P4.5 验证中，新增 4x4 RGBA8 纹理四边形、`replaceRegion` upload、
一级 sampler resource、fragment texture/sampler replay 与通用 descriptor 查询。完整脚本重新构建并
生成 T00-T03 capture，T03 的 64-byte texel、四点 `PickPixel()`、四象限输出和 binding 均通过；
四份 capture 各 10 次 lifecycle resident growth 为 1,245,184 字节，随后四份 capture 的
`renderdoccmd replay --loops 3` 均以状态 0 退出。最终构建的 qrenderdoc 在 T03 EID 2 显示
Texture 17、Sampler 18、Float2/Float2、Triangle Strip 和正确四象限；标准 Texture Viewer 与
Resource Inspector 跳转均通过，状态栏为 `No problems detected`。阶段 4 下一项保持为 P4.4 布局收敛。

2026-09-22 的 P4.4 首个收敛切片中，Metal Pipeline 页面复用标准 Controls、`PipelineFlowChart` 和
隐藏 stage tabs，按 IA/VS/RS/FS/OM 重排现有真实状态。`Show Empty Items` 使用红色空槽，静态
binding reflection 尚未实现时 `Show Unused Items` 明确禁用；shader 行可直接进入 Shader Viewer。
完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 294,912 字节。最终
qrenderdoc 先用 T03 验证 FS texture/sampler、empty index/depth 和 shader 跳转，再用 T02 验证 IA
UInt16 binding、RS viewport/scissor/raster 及 OM color/depth/depth-state，状态栏均为
`No problems detected`。该首个切片完成后，P4.4 转入资源树、上下文操作、预览/export 与 used
reflection 收敛。

2026-09-22 的 P4.4 第二个收敛切片将 Metal 表迁移到 `RDTreeWidget`/`RDHeaderView`，并把 Metal
ResourceId 接入通用 context/usage、thumbnail 和 preview 分发；资源双击路径保持不变。工具栏新增标准
Export 控件，T03 EID 2 实际导出的 HTML 含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和
Sampler 18。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle resident growth
为 524,288 字节；最终 qrenderdoc 保持运行且状态栏为 `No problems detected`。下一项转为 shader
resource binding reflection 与 used/unused 过滤。

2026-09-22 的 P4.4 第三个收敛切片让 replay 创建 pipeline 时请求 Metal argument reflection，并将
T03 的 `colourTexture`/`colourSampler` 映射到通用 shader reflection。fixture 将同一资源额外绑定到
未声明的 slot 1；自动测试验证 slot 0 为 used、slot 1 为 `NoShaderBinding + staticallyUnused`，且
used-only 查询只返回 slot 0。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle
resident growth 为 540,672 字节。最终构建的 qrenderdoc 在 T03 EID 2 FS 页默认只显示 slot 0，勾选
`Show Unused Items` 后 texture/sampler 表各显示 slot 1，状态栏为 `No problems detected`；进程保持运行。

## 工作日志

| 日期 | 任务 | 结果 |
| --- | --- | --- |
| 2026-09-20 | M0.1 | 从官方 GitHub 检出 `v1.46`，成功 |
| 2026-09-20 | M0.2 | 创建 `metal-replay-v1.46` 分支，成功 |
| 2026-09-20 | M0.3 | 完成源码/工具链预检，发现 Qt 5 等依赖缺失 |
| 2026-09-20 | 计划初始化 | 建立 `docs/metal-replay/` 文档集 |
| 2026-09-20 | M0.4 | 通过 Homebrew 安装并核验 macOS/Qt 构建依赖 |
| 2026-09-20 | M0.5 | `ENABLE_METAL=OFF` 构建成功，qrenderdoc 主窗口启动成功 |
| 2026-09-20 | M0.6 | 修复 SDK 26 bridge 编译问题；`ENABLE_METAL=ON` 构建和启动成功 |
| 2026-09-20 | M0.7-M0.8 | 增加一键脚本并记录产物、启动和清理方法 |
| 2026-09-20 | M1.1-M1.2 | 增加 T00/T01 Metal fixtures；原生构建与运行成功 |
| 2026-09-20 | M1.3-M1.4 | 修复 macOS 26 drawable/residency 边界和主动 capture backbuffer；成功写出 `.rdc` |
| 2026-09-20 | M1.5-M1.6 | T00/T01 chunks、MSL、96-byte VB、draw/present 经 XML structured export 验证 |
| 2026-09-20 | M1.7 | 增加并通过 `test_metal_capture_macos.sh` 一键回归；阶段 1 完成 |
| 2026-09-20 | M2 预备 | 注册 Metal structured processor；真实 replay provider 待实现 |
| 2026-09-20 | M2.1 | 注册真实 Metal replay provider；T00/T01 可由 `renderdoccmd replay` 加载 |
| 2026-09-20 | M2.2 | 重建 T00/T01 基础 Metal 对象、资源和命令并在加载期间真实执行 |
| 2026-09-20 | M2.3 | 增加 render pass、clear、draw、present、capture end 的最小 action/event |
| 2026-09-20 | M2.4 开始 | qrenderdoc 成功加载 T01；确认 degraded 弹窗是输出未实现的主动能力标记 |
| 2026-09-20 | M2.4 output | 实现 Metal output/`RenderTexture()`/readback；T00/T01 像素回归和 qrenderdoc UI 验证通过 |
| 2026-09-20 | M2.4/M2.8 event replay | 保存 frame stream/event offset，实现 Full/WithoutDraw/OnlyDraw；自动和 UI 的 clear -> draw -> clear 验证通过 |
| 2026-09-20 | M2.5 texture data | 实现 capture texture readback/`PickPixel()`，自动验证 clear/draw BGRA 数据并成功保存 DDS |
| 2026-09-20 | M2.5 texture data 收口 | 修正 live texture 类型校验；重跑完整 capture smoke、T00/T01 CLI replay 和 DDS 格式检查，全部通过 |
| 2026-09-21 | M2.5 Shader Viewer 开始 | 开始汇合 source library MSL、function 入口和 Metal function stage；完成后需自动回归并启动 qrenderdoc 实机验收 |
| 2026-09-21 | M2.5 Shader Viewer 收口 | `ShaderEncoding::MSL`、source/entry/stage reflection、自动测试和 Function 13/14 实机查看通过；下一项为最小 Pipeline State |
| 2026-09-21 | M2.5 Pipeline State 开始 | 建立 Metal 专用最小 pipeline state，目标为 T01 draw 的 pipeline、vertex/fragment shader、vertex buffer、topology 和 color target |
| 2026-09-21 | M2.5 Pipeline State 收口 | Metal snapshot、通用 `PipeState`、proxy serialization、qrenderdoc 专用页、自动状态断言与最新 T01 EID 2 实机验证全部通过；M2.5 完成，转入 M2.6 |
| 2026-09-21 | M2.6 生命周期 | 修复 replay Metal 对象所有权和确定性 shutdown；T00/T01 各 10 次循环通过，resident growth 491,520 字节 |
| 2026-09-21 | M2.6 unsupported | shader debug 返回安全空 trace；histogram/pixel history/post-VS/custom/target shader 稳定降级并纳入自动回归 |
| 2026-09-21 | M2.6 UI/文档收口 | 最新 qrenderdoc 完成 T01/T00 关闭重开和禁用能力验证；新增 `PHASE3.md`，下一项为 T02 P3.1 |
| 2026-09-21 | P3.1 T02 fixture | 新增确定性 indexed cube、显式 vertex descriptor、UInt16/UInt32 index、Depth32Float 与固定 raster state；原生运行通过 |
| 2026-09-21 | P3.2 capture/replay | 新增 depth-state wrapper，接通 scissor/front-face/cull/直接 indexed draw，并修复 depth attachment 帧引用；structured capture 与 GPU replay 通过 |
| 2026-09-21 | P3.1-P3.2 自动回归 | T00/T01/T02 全量脚本通过；T02 index 字节/depth target/双视口图像和三份 capture 各 10 次 lifecycle 通过 |
| 2026-09-21 | P3.2 T02 UI | 最新 qrenderdoc 的 Event/Texture Viewer 显示 EID 2/3、UInt32 Buffer 20、FB0/DS 与双立方体，状态栏无错误；完整 Pipeline/Mesh 留给 P3.3/P3.4 |
| 2026-09-21 | P3.3 T02 state/event | 增加 draw-time event snapshot、vertex/index/depth/raster 状态和 clear/draw1/draw2/回退断言；全量回归及 qrenderdoc Pipeline State 实机验证通过，转入 P3.4 Mesh/Buffer UI |
| 2026-09-21 | P3.4-P3.5 T02 Mesh/UI 收口 | 通用 VS input、标准 Mesh/Buffer 跳转、Float3 indexed wireframe、自动输出断言及 qrenderdoc UInt16/UInt32 实机验证通过；阶段 3 完成，转入 T03 |
| 2026-09-21 | P4.1-P4.3 T03 texture/sampler | 完成确定性纹理四边形、upload/sampler/binding capture/replay、通用 descriptor 与标准资源跳转 |
| 2026-09-21 | P4.5 T03 自动化/UI | T00-T03 完整回归、四份 capture 三轮 CLI replay、生命周期及 qrenderdoc T03 实机验证通过；转入 P4.4 标准布局收敛 |
| 2026-09-22 | P4.4 标准阶段布局 | Metal Pipeline 接入 Controls + PipelineFlowChart + IA/VS/RS/FS/OM，empty-slot 与 shader 直接跳转通过 T02/T03 实机验证；继续资源树/反射收敛 |
| 2026-09-22 | P4.4 标准资源表/export | Metal 表迁移到 RDTree/RDHeader，接入通用资源操作/预览与五阶段 HTML export；T03 实际导出、完整回归和最终 qrenderdoc 验证通过；继续 shader reflection |
| 2026-09-22 | P4.4 shader reflection/过滤 | Metal argument reflection、slot 0/1 used-unused 自动断言、完整回归及 qrenderdoc 过滤实机验证通过；T03 阶段关闭，下一项为 T04 动态 uniform |
