# Metal Replay 决策记录

## D001：固定在 RenderDoc v1.46 上开发

- 日期：2026-09-20
- 状态：已采用
- 决策：以官方 `v1.46` 标签、提交 `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200` 为基线，
  在 `metal-replay-v1.46` 分支开发。
- 原因：用户明确要求 1.46；固定提交可以避免上游 Metal 骨架持续变化导致计划和 capture 格式漂移。

## D002：首版只承诺离线 replay 产品能力

- 日期：2026-09-20
- 状态：已采用
- 决策：首版不承诺任意应用注入和完整 capture 支持，但允许补齐测试程序生成 `.rdc` 所需的最小
  capture/序列化路径。
- 原因：没有可靠 Metal capture 输入无法验证 replay；限定输入生成范围可保持目标聚焦。

## D003：优先使用 RenderDoc 通用 UI 框架

- 日期：2026-09-20
- 状态：已采用
- 决策：通过 `IReplayDriver`、`PipeState`、资源描述和现有 qrenderdoc viewer 框架接入 Metal；
  Pipeline State 的后端专用内容页由 D016 进一步细化。
- 原因：用户需要的是 RenderDoc 能力；复用通用 UI 能让 Buffer、Texture、Pipeline、Shader、Mesh
  Viewer 的行为与其他后端一致。

## D004：以小型确定性测试为主、外部样例为辅

- 日期：2026-09-20
- 状态：已采用
- 决策：仓库内建立最小 Metal 用例覆盖单一特性；Apple 和 GitHub 样例用于组合场景回归。
- 原因：大型样例难以定位 replay 差异，而只测自建三角形又不足以证明常见接口覆盖。

## D005：MSL 展示遵循“只展示真实可获得信息”

- 日期：2026-09-20
- 状态：已采用
- 决策：对 source-created library 保存和展示 MSL；对预编译 `.metallib` 展示函数、入口和反射，
  若源码不可恢复则明确标记，不把反编译或 shader debugging 作为首版门槛。
- 原因：Metal binary 不保证包含可恢复的原始 MSL，伪造源码会误导分析。

## D006：SDK 26 新增协议方法通过 Objective-C forwarding 保持兼容

- 日期：2026-09-20
- 状态：已采用
- 决策：对 RenderDoc 尚未包装的新增 Metal 协议方法，保留现有 `forwardInvocation` 行为，并仅在
  `rdoc_metal` target 上关闭协议完整性和相关 property synthesis 的 error 诊断。
- 原因：这些方法本来就应转交真实 Metal 对象；为了通过编译而生成大量无 capture 语义的伪包装
  会造成错误的支持承诺。未来纳入 replay 范围时应逐个实现并移除对应缺口。

## D007：Capture 是 Replay 的前置条件，采用逐 feature 纵向闭环

- 日期：2026-09-20
- 状态：已采用
- 决策：先建立可原生运行的 Metal fixture，再补足其 capture 并生成 `.rdc`，随后立即实现同一
  feature 的 replay/UI 验证；不先批量截取所有高级样例。
- 原因：RenderDoc `.rdc` 依赖 API 专用序列化和资源初始状态，不存在可直接替代的通用 Metal
  trace。只验证文件生成无法发现资源和命令语义缺失，逐 feature 闭环能更早暴露格式设计问题。

## D008：CAMetalLayer 内部始终使用真实 MTLDevice

- 日期：2026-09-20
- 状态：已采用
- 决策：在 hook 的 `nextDrawable()` 调用期间临时把 layer device 切换为真实 device，让系统创建
  drawable/IOSurface/residency 资源；返回后恢复代理 device，并把 drawable 的 `texture` getter 映射
  为对应的 RenderDoc wrapper。
- 原因：macOS 26 的 Metal 驱动会把 drawable texture 加入内部 `MTLResidencySet`。若 layer 持有代理
  device，系统私有路径会混入 wrapper，最终在 `AGXFamilyResidencySet` 中崩溃。系统对象必须看到
  真实 Metal 对象，而应用和序列化层仍需看到 wrapper。

## D009：先注册 structured processor，再实现完整 replay provider

- 日期：2026-09-20
- 状态：已采用
- 决策：阶段 1 增加 Metal structured processor，使 `renderdoccmd convert -c xml` 能解码 capture，但
  不把它当作 replay 完成。
- 原因：它能自动验证 chunk 名称、字段、MSL 和 buffer 数据，且与未来 replay 共用 `ProcessChunk`
  序列化入口；真实 GPU replay 和 UI 仍由阶段 2 的 `IReplayDriver` provider 单独验收。

## D010：首版 frame stream 在加载时执行，显示闭环后再引入 seekable replay

- 日期：2026-09-20
- 状态：已完成并由 D013 取代
- 决策：M2.1-M2.3 先复用 `ReadLogInitialisation()` 顺序执行 capture command stream，以尽快验证
  对象重建、命令执行和最终 render target；M2.4 显示闭环后必须实现按 event 的 full、without-draw、
  only-draw replay。
- 原因：这能尽早暴露 Metal 序列化与真实 API 执行错误，但只执行一次无法满足事件跳转，不能作为
  最终 replay 架构。
- 结果：显示闭环后已按计划引入 frame reader 和 event offset；加载时执行仅用于建立结构化 action，
  交互式查看由 D013 的 seekable replay 负责。

## D011：Texture Viewer 未接通前保留 degraded 能力标记

- 日期：2026-09-20
- 状态：已完成
- 决策：在 output window 和 `RenderTexture()` 仍为占位实现期间，`GetAPIProperties()` 返回
  `degraded=true`；T00/T01 的 Texture Viewer 真实显示通过后再取消。
- 原因：qrenderdoc 的 degraded 弹窗文案偏向硬件回退，但对当前实现来说仍比宣称完整支持更诚实。
  文档和日志必须明确它不是 capture 加载失败。
- 结果：T00/T01 Texture Viewer、resize 和 output readback 验证通过后已设置 `degraded=false`；
  尚未支持的高级纹理类型和 replay 功能继续通过具体接口结果表达，而不再使用全局 degraded 标记。

## D012：output window 使用持久纹理，再复制绘制到每个 drawable

- 日期：2026-09-20
- 状态：已采用
- 决策：每个 output window 保存一张私有 BGRA8 render-target/shader-read texture；viewer 操作先写入
  该纹理，`FlipOutputWindow()` 再用内部 fullscreen pipeline 绘制到当前 drawable。
- 原因：`ReplayOutput` 在非 dirty 帧仍会请求新的 drawable 并调用 flip。直接只绘制当前 drawable 会在
  后续刷新丢失画面；持久纹理同时简化 output readback 和 resize 生命周期。

## D013：以 frame-relative event offset 驱动 Metal 两段式 replay

- 日期：2026-09-20
- 状态：已采用
- 决策：在 `CaptureScope` 后保存独立 `StreamReader`，加载 action 时为每个 event 记录 frame-relative
  file offset。`WithoutDraw` 从帧头执行到目标 offset 前并保留 encoder；`OnlyDraw` 从目标 offset
  执行到下一 event offset；`Full` 从帧头执行到目标 event，并在结束时补齐 encoder/command buffer。
- 原因：这与 `ReplayController::SetFrameEvent()` 的调用顺序一致，既能恢复目标 draw 的绑定状态，又能
  对 clear/pass boundary 等非 draw action 给出确定结果。以 offset 而非硬编码 EID/chunk 名切分，也能
  保持 action ID 调整后的 replay 稳定性。
- 当前边界：只验证了 T00/T01 的单 command buffer、单 render pass。多 command buffer、多 pass 和
  load-action initial contents 必须随对应测试样例扩展；`OnlyDraw` 仍按控制器约定依赖先行的
  `WithoutDraw`。

## D014：首个 capture texture readback 只承诺紧密 RGBA8/BGRA8 2D 数据

- 日期：2026-09-20
- 状态：已采用
- 决策：`GetTextureData()` 首版只支持 sample 0、slice 0 的单采样 2D
  `RGBA8Unorm(_sRGB)`/`BGRA8Unorm(_sRGB)`。使用 Metal blit 复制到 shared buffer，并按设备要求
  对齐 GPU row pitch，返回时逐行去除 padding。`PickPixel()` 复用同一数据路径并做真实通道重排。
- 原因：当前 T00/T01 backbuffer 正好覆盖这条确定性路径；先确保 qrenderdoc 读取、拾取和保存拿到
  真实数据，再按新 fixture 扩展 MSAA、array/cube/3D、depth/stencil、压缩、整数和浮点格式。
  未支持情况必须明确记录错误，不能用空成功或伪造像素掩盖能力缺口。

## D015：source-created library 按 function 复用真实 MSL reflection

- 日期：2026-09-21
- 状态：已采用
- 决策：replay 时按 library resource 保存 `newLibraryWithSource` 的原始 MSL；每个重建的
  `MTLFunction` 根据 `functionType` 映射 RenderDoc shader stage，并生成引用同一真实 source file 的
  `ShaderReflection`。新增 `ShaderEncoding::MSL`，让通用 Shader Viewer 识别并高亮 MSL。
- 原因：Metal 的 source 属于 library，而 RenderDoc Shader Viewer 从 function resource 查询 reflection；
  在两层间显式关联既能保持入口/stage 正确，也不会复制或改写源码。binary/default library 没有原始
  source 时只暴露真实可得信息，继续遵守 D005，不生成伪 MSL。

## D016：Metal 使用独立 pipeline state 类型和现有后端页面模式

- 日期：2026-09-21
- 状态：已采用
- 决策：新增 `MetalPipe::State`，通过 replay controller、proxy serialization 和通用 `PipeState`
  提供跨 API 查询；qrenderdoc 在现有 Pipeline State 容器内增加 Metal 页面。首个 snapshot 只保存
  T01 已有证据支持的 render pipeline、vertex/fragment shader、primitive topology、vertex buffer、
  color/depth target，不借用 D3D/GL/Vulkan state，也不填充尚未 capture 的字段。
- 原因：RenderDoc v1.46 的 Pipeline State 顶层是通用容器，但实际内容页按后端区分。给 Metal 独立
  数据模型和页面符合现有结构，并避免为了复用 UI 而伪装成其他图形 API；后续状态字段可随 fixture
  和验证一起扩展。该决定细化 D003：继续复用通用 viewer 框架和交互，而不是复用错误的后端数据。

## D017：Replay wrapper 显式记录 Metal 对象所有权

- 日期：2026-09-21
- 状态：已采用
- 决策：replay 资源 wrapper 统一持有一个真实 Metal 对象引用，并记录该引用是否由 wrapper 负责
  释放。`new*` API 的 retained 返回值直接转移所有权；`commandBuffer()`、render encoder 等
  autoreleased 返回值先 retain；同一 capture resource 在重复 replay 中替换 live 对象时先持有新值，
  再释放旧值。Objective-C embedded bridge 只用于 capture 跟踪，不参与 replay 析构。
- 原因：无差别释放会让 autorelease pool 二次释放并崩溃，完全不释放又会在线程内循环打开 capture
  时线性泄漏。显式所有权既保留 Cocoa/Metal 返回约定，又让 resource manager 可以确定性 shutdown。
- 验证：T00/T01 各 10 次同进程打开/关闭通过，完整回归 warm-up 后 resident growth 为 491,520 字节。

## D018：Unsupported 能力返回安全空对象或明确错误

- 日期：2026-09-21
- 状态：已采用
- 决策：能力标志继续声明 shader debugging/pixel history 不支持；直接调用时 histogram、pixel
  history、post-VS 返回空数据，custom/target shader build 返回空 ResourceId 与错误文本，四个
  `Debug*()` 返回 stage 正确、`debugger == NULL` 且可释放的空 `ShaderDebugTrace`，不返回空指针。
- 原因：`ReplayController` 会在 debug 调用后访问 trace；返回 `NULL` 会把“未支持”升级成崩溃。
  对 histogram 伪造 256 个零值同样会误导 UI，应使用真正的空结果表达 unsupported。
- 验证：lifecycle smoke 直接覆盖全部上述调用；qrenderdoc 的 History/Debug 控件显示明确的不支持
  提示并保持禁用。

## D019：T02 先固定直接 indexed draw，并显式追踪全部 render-pass attachment

- 日期：2026-09-21
- 状态：已采用
- 决策：首个索引绘制切片只包装直接的 `drawIndexedPrimitives(indexCount:indexType:indexBuffer:
  indexBufferOffset:)`，同时覆盖 UInt16/UInt32；instance/base/indirect 重载继续明确 unsupported，直到
  对应 fixture 到位。render encoder capture 对 color/depth/stencil attachment 及各自 resolve texture
  统一标记帧引用，不能只序列化 ResourceId。
- 原因：直接重载足以闭合 T02 且不会虚假承诺 base/instance 语义。仅把 depth texture ID 写入 render
  pass chunk 而不标帧引用，会使资源创建 chunk 缺失，structured XML 看似正确但 replay 得到空 depth
  attachment；统一 attachment 引用规则消除了这一隐蔽失配。
- 验证：T02 `.rdc` 包含独立 `Depth32Float` texture 创建 chunk；replay pipeline depth target 非空，
  UInt16/UInt32 index 字节、两个 indexed action 和最终双视口图像均由一键回归断言。

## D020：Pipeline snapshot 按 action event 保存 draw-time 状态

- 日期：2026-09-21
- 状态：已采用
- 决策：Metal 在初始加载建立 action 时保存一份对应 event 的 pipeline snapshot；控制器请求状态时
  返回目标 event 或其最近前序 action 的快照，而不是复制 replay 区间结束时的最后动态状态。
- 原因：RenderDoc 的 `OnlyDraw` 区间从目标 draw chunk 延伸到下一 event 前，因此会执行下一 draw 前
  的 viewport/scissor 等状态设置。图像仍只包含目标 draw，但若在区间末尾取状态，EID 2 会错误显示
  EID 3 的右半屏 viewport。draw-time snapshot 让 UI 状态与实际执行该 action 时完全一致。
- 验证：自动回归在 EID 2/3 分别断言左/右 viewport/scissor 与 UInt16/UInt32 index binding，并执行
  clear -> draw1 -> draw2 -> draw1 图像回退；qrenderdoc 实机切换两条 draw 的 Pipeline State 结果一致。

## D021：Metal Pipeline UI 以通用数据和标准交互为收敛边界

- 日期：2026-09-21
- 状态：已采用
- 决策：Metal 继续保留真实的 `MetalPipe::State`，但优先通过通用 `PipeState` 查询和 RenderDoc 既有
  Buffer/Texture/Shader/Mesh Viewer 消费数据；Pipeline State 中的属性、buffer、texture、shader
  激活行为与其他后端保持一致。当前代码生成的 Metal 页面允许作为开发期过渡布局，后续按其他后端
  的 IA/Shader/Raster/Output 分组、可见性过滤、空槽和资源操作逐步重构，不建立 Metal 专用查看器分叉。
- 原因：后端原生状态模型必须保持语义正确，但用户使用路径和信息层级应最终符合 RenderDoc 标准，
  否则每增加一个 Metal 字段都会扩大 UI 差异并重复维护 viewer 逻辑。
- 验证：T02 vertex descriptor 经 `GetVertexInputs()` 直接被标准 Mesh Viewer/自动 buffer formatter
  消费；Pipeline attribute、VB、IB 激活分别进入 Mesh Viewer 和带格式 Buffer Viewer。页面视觉排版
  尚未宣称完成，将在 T03 绑定扩展时继续收敛。

## D022：T03 texture upload 保留调用顺序，sampler 作为一级资源进入通用 descriptor 模型

- 日期：2026-09-21
- 状态：已采用
- 决策：当前支持的 `MTLTexture::replaceRegion` 作为 texture resource record 中的有序初始化 chunk
  保存，在创建 texture 后按原调用参数 replay；不提前伪装成覆盖所有子资源的通用 initial-state
  snapshot。sampler 使用独立 wrapped resource、ResourceId、descriptor 和生命周期，并与 fragment
  texture binding 一起通过通用 `DescriptorAccess`/`Descriptor`/`SamplerDescriptor` 暴露给 UI。
- 原因：T03 的 CPU upload 发生在 frame 前，保留 API 顺序可在最小范围内忠实恢复内容；若直接抽象成
  全量 initial state，会错误暗示 mip/slice/private/压缩路径已经支持。sampler 若只存进 pipeline 快照，
  Resource Inspector、资源身份和后续 argument buffer 扩展都会失去统一基础。
- 当前边界：只覆盖单层、非压缩 RGBA8 2D 的非 slice `replaceRegion`，紧密源数据按真实
  `bytesPerRow * height` 保存；slice/bytesPerImage、mip 链、其他格式/storage mode 继续 unsupported。
- 验证：T03 XML 保存 64-byte upload 与 sampler 参数；GPU replay 四象限像素、原始 texture data、
  `PickPixel()`、通用 fragment texture/sampler 查询和 qrenderdoc Texture/Resource 跳转全部一致。

## D023：Metal Pipeline 复用标准阶段导航，used 状态必须以静态反射为依据

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal Pipeline 页面复用 qrenderdoc 的 `PipelineFlowChart` 和隐藏 stage tabs，固定映射为
  IA/VS/RS/FS/OM；数据继续来自通用 `PipeState`/`MetalPipe::State`。`Show Empty Items` 只显示状态
  模型能证明存在且未绑定的槽；`Show Unused Items` 在 shader resource binding reflection 可用前保持
  禁用并向用户说明原因。
- 原因：阶段导航、信息层级和操作路径应与 D3D/Vulkan 一致，但“unused”是 shader 静态引用语义，
  不能从当前仅记录实际绑定的 descriptor access 反推。把按钮禁用比将所有绑定误标为 used 更准确。
- 验证：T03 IA/FS/OM 和 T02 IA/RS/OM 在最终 qrenderdoc 中逐页核对，empty index/depth 槽、shader
  直接跳转、texture/sampler/depth/raster 数据均正确，状态栏保持 `No problems detected`。

## D024：Metal 资源表复用 RDTree 公共操作，HTML export 按标准阶段输出

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal Pipeline 的状态表统一使用 `RDTreeWidget`/`RDHeaderView`，ResourceId 存入 item tag，
  并由 `PipelineStateViewer` 的 `SetupResourceView()`、thumbnail/preview 分发消费；不复制 Metal 专用
  右键菜单。HTML export 复用公共 writer/table helper，按 IA/VS/RS/FS/OM 输出现有真实状态。
- 原因：资源复制、usage、Resource Inspector、hover preview 与 HTML 样式属于 qrenderdoc 通用交互，
  独立实现会扩大和 D3D/Vulkan 的差异。按当前真实表导出还能避免为追求完整报告而伪造未捕获字段。
- 验证：最终 qrenderdoc 的 T03 FS 表保持标准选择和 Texture Viewer 双击跳转；Export 在 EID 2 写出
  `/tmp/metal-pipeline-t03.html`，核对标题为 Metal Pipeline export，且包含五阶段、Triangle Strip、
  Texture 17 与 Sampler 18。完整 T00-T03 回归通过，resident growth 为 524,288 字节。

## D025：used/unused 以 Metal pipeline argument reflection 为准，物理槽与反射索引分离

- 日期：2026-09-22
- 状态：已采用
- 决策：source-created render pipeline 在 replay 创建时请求 `MTLPipelineOptionArgumentInfo`，直接使用
  Metal 返回的 vertex/fragment argument reflection 建立 shader resource binding；不解析 MSL 源码，
  也不从“当前已绑定资源”反推 shader 声明。`DescriptorAccess.index` 保持通用 API 所需的 shader
  reflection 数组索引，Metal 物理 slot 继续保存在 descriptor store offset 中并由 UI 换算显示。
  已绑定但 shader 未声明的 slot 使用 `NoShaderBinding + staticallyUnused`；没有 argument reflection 的
  pipeline 保持原兼容映射并禁用过滤，不伪造 unused 证据。
- 原因：shader 接口索引和 API 物理 binding slot 并不保证相同；混用二者会让未声明 slot 在 UI 显示为
  `65535`，也会破坏通用 descriptor API。编译器 reflection 是静态使用语义的权威来源，并能为后续
  argument buffer 和更多 binding 类型保留正确模型。
- 验证：T03 的 slot 0 texture/sampler 分别映射到 `colourTexture`/`colourSampler` reflection index 0，
  同一资源额外绑定的 slot 1 被标记为 statically unused；自动回归验证 `onlyUsed=true` 只返回 slot 0。
  最终 qrenderdoc 默认仅显示 slot 0，勾选 `Show Unused Items` 后显示真实 slot 1，状态栏为
  `No problems detected`。完整 T00-T03 回归通过，resident growth 为 540,672 字节。
