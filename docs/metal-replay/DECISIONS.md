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
- 首版边界：T03 只覆盖单层、非压缩 RGBA8 2D 的非 slice `replaceRegion`，紧密源数据按真实
  `bytesPerRow * height` 保存。T09 已按 D031 扩展 slice/bytesPerImage 与 mip 链；其他格式和
  storage mode 仍待后续 fixture。
- 验证：T03 XML 保存 64-byte upload 与 sampler 参数；GPU replay 四象限像素、原始 texture data、
  `PickPixel()`、通用 fragment texture/sampler 查询和 qrenderdoc Texture/Resource 跳转全部一致。

## D023：Metal Pipeline 复用标准阶段导航，used 状态必须以静态反射为依据

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal Pipeline 页面复用 qrenderdoc 的 `PipelineFlowChart` 和隐藏 stage tabs，固定映射为
  IA/VS/RS/FS/OM；数据继续来自通用 `PipeState`/`MetalPipe::State`。`Show Empty Items` 只显示状态
  模型能证明存在且未绑定的槽；`Show Unused Items` 在 shader resource binding reflection 可用前保持
  禁用并向用户说明原因。共享 `PipelineFlowChart` 接受焦点，并以 Left/Right/Home/End 切换同一组
  stage，不为 Metal 建立私有导航逻辑。
- 原因：阶段导航、信息层级和操作路径应与 D3D/Vulkan 一致，但“unused”是 shader 静态引用语义，
  不能从当前仅记录实际绑定的 descriptor access 反推。把按钮禁用比将所有绑定误标为 used 更准确。
- 验证：T03 IA/FS/OM 和 T02 IA/RS/OM 在最终 qrenderdoc 中逐页核对，empty index/depth 槽、shader
  直接跳转、texture/sampler/depth/raster 数据均正确；T07 又实机用 Home/End 从 IA 往返 OM，状态栏
  保持 `No problems detected`。

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

## D026：直接 Metal buffer argument 映射为 constant block，动态 offset 保留事件语义

- 日期：2026-09-22
- 状态：已采用
- 决策：当前 source-created MSL 中以 `constant T& [[buffer(n)]]` 声明的直接 buffer argument 映射到
  `ShaderReflection::constantBlocks` 和 `DescriptorType::ConstantBuffer`，而不是伪装成 texture-style
  read-only resource。Metal fragment buffer 使用独立 descriptor offset 空间，物理 slot 与 shader
  reflection index 继续分离。`setFragmentBufferOffset` 作为独立 chunk replay，在保留已绑定 ResourceId
  的同时更新当前 offset/available size，并由 draw-time snapshot 固化到各 event。
- 原因：constant block 能被通用 `PipeState::GetConstantBlocks()`、标准 Pipeline UI 和后续常量查看路径
  正确消费；动态 offset 若只修改真实 encoder 而不更新 snapshot，会让 EID 2/3 显示同一个范围。独立
  descriptor 空间避免 buffer slot 0 与 texture/sampler slot 0 冲突。
- 当前边界：只承诺 T04 使用的单个直接 fragment constant buffer 与单-slot offset 更新；storage
  buffer、批量 binding、argument buffer、vertex-stage texture/sampler 和结构成员反射继续留待 fixture。
- 验证：T04 XML 保存 512-byte 初始数据、slot 0、offset 0/256 和两条 draw；自动回归断言 buffer 字节、
  `uniforms`/16-byte reflection、descriptor 和事件图像。最终 qrenderdoc 在 EID 2/3 分别显示
  `Buffer 16 / 0 / 512` 与 `Buffer 16 / 256 / 256`，Buffer Viewer 精确打开第二个范围；完整 T00-T04
  回归通过，五份 capture 各 10 次 resident growth 为 376,832 字节。

## D027：实例化输入继续复用通用 VS Input，raw preview 不伪造 shader 输出

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal vertex descriptor 的 per-instance layout 直接映射到通用
  `VertexInputAttribute::perInstance/instanceRate`，action 的 base instance 由标准 Buffer Viewer 数据窗口
  应用；Pipeline IA 与 Mesh Viewer 不增加 Metal 专用实例解释。VS Input preview 只绘制被选作 position
  的原始 attribute，不尝试猜测其他 instance attribute 如何参与 vertex shader 变换；变换后的实例
  geometry 必须来自未来真实 post-VS 数据。
- 原因：任意 MSL vertex shader 都可能以非平移方式使用 per-instance 数据，UI 无法仅凭 attr 名称或
  buffer layout 推导输出位置。复用标准路径能正确展示实例记录、base instance 和 raw input，同时避免
  把 T05 特定的 offset 语义硬编码进 replay backend。
- 验证：T05 action 保存 `instanceCount=3/baseInstance=1`；自动回归逐字节检查 24/96-byte buffer、
  per-vertex/per-instance 映射、raw mesh preview 与三色 GPU 输出。qrenderdoc Mesh Viewer instance 0/1
  分别显示 record 1/2 的 offset/colour，Pipeline IA 和两个标准 Buffer Viewer 使用相同 ResourceId、
  offset、size 与 stride，状态栏为 `No problems detected`。

## D028：MRT action 与 OM 状态以完整 attachment 数组为准

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal render pass 的 clear/draw/end-pass action 从当前 pipeline snapshot 填充全部 color
  outputs，不再把 slot 0 当作唯一 framebuffer。render pipeline descriptor 中每个有效 color
  attachment 的 blend enable、RGB/alpha factor、operation 和 write mask 转成通用 `ColorBlend` 数组，
  由 `PipeState::GetColorBlends()`、标准 OM RDTree 和 HTML export 共同消费。Texture Viewer 继续使用
  action outputs 自动建立 FB0/FB1 列表，不建立 Metal 专用 MRT 查看器。
- 原因：多附件是 RenderDoc action、Texture Viewer 和 Pipeline State 共用的数据关系；若各 UI 从
  Metal 私有 descriptor 单独推导，会造成事件输出、缩略图与 OM 表不一致。通用 `ColorBlend` 也能让
  字段顺序和命名自然收敛到 GL/Vulkan/D3D 页面。
- 当前边界：只承诺 T06 覆盖的两个单采样 2D RGBA8/BGRA8 attachment、Add operation、
  SourceAlpha/OneMinusSourceAlpha 和 disabled blend/RGB mask；logic op、更多 factor/operation、MSAA、
  memoryless 与 programmable blending 仍需独立 fixture。
- 验证：T06 action slot 0/1、两行 `ColorBlend`、240-byte vertex 数据和两张 attachment 的 clear/draw/
  回退像素均由 smoke 自动断言。七份 capture 各 10 次 lifecycle 与三轮 CLI replay 通过；qrenderdoc
  EID 3 的 Outputs 可切换两张图，OM 表与实际 HTML export 显示相同 blend/write-mask 状态，状态栏为
  `No problems detected`。

## D029：Depth/stencil action、通用状态与标准 OM 必须共享 draw-time snapshot

- 日期：2026-09-22
- 状态：已采用
- 决策：combined depth/stencil attachment 继续使用同一真实 texture ResourceId；render-pass action 的
  `depthOut`、Metal depth-stencil state、通用 `DepthTestState`/`StencilFace` 与 qrenderdoc OM 表都从
  draw-time snapshot 读取。front/back compare、三类 stencil operation、read/write masks 与 dynamic
  reference 保留为独立字段；绑定新的 depth-stencil state 不覆盖 encoder 上已有的 dynamic reference。
  当前不支持的 depth/stencil texel readback 不从 color 结果反推，也不返回伪数据。
- 原因：Metal 把 descriptor state 与 encoder dynamic reference 分开，且 combined attachment 同时承担
  depth/stencil；若在 bind state 时重置 reference，或由 UI 重新解释 descriptor，会让 event seek、GPU
  结果与 Pipeline State 相互矛盾。复用通用 OM 数据结构也能保持与 D3D/Vulkan 的字段顺序和操作路径
  收敛。
- 当前边界：只承诺 T07 的单采样 `Depth32Float_Stencil8`、单 render pass、front/back descriptor、
  single/dual reference 与直接 draw；depth/stencil texture readback、MSAA、depth bounds 和 memoryless
  attachment 留待后续 fixture。
- 验证：T07 structured XML、五个 action/depth output、事件 state 与左绿右蓝 GPU 结果由 smoke 自动
  断言；八份 capture 各 10 次 lifecycle 与三轮 CLI replay 通过。qrenderdoc EID 6 的 OM 表和 UI
  导出的 `t07_pipeline_state_standard.html` 均显示 Texture 20、Less/Write Enabled、Front/Back
  reference 5、`000000FF/00000000` masks、Equal 与 `Inc Sat/Dec Sat`，状态栏无错误。

## D030：MSAA attachment 与 resolve output 必须保持为两个真实资源

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal pipeline snapshot 分别保存 multisample color attachment 与 resolve target，并保留各自
  texture type/sample count。Pipeline OM 使用独立的 Multisample State、Color Targets 和 Resolve
  Targets 标准资源表；clear/draw action 的可显示 output 在存在显式 resolve 时指向单采样 resolve
  resource。Texture Viewer、像素拾取和保存继续读取该真实 resolve texture，不把 multisample attachment
  降格成普通 2D texture，也不声称支持逐 sample readback。
- 原因：Metal 的 render attachment 是实际写入对象，resolve texture 才是 pass store 后可采样/展示的
  结果；若用一个 ResourceId 同时代表两者，会丢失 4x/1x 关系，并让 Pipeline State、action output 与
  Texture Viewer 产生矛盾。分离模型也与 Vulkan/D3D 的 attachment/resolve 语义一致，便于 UI 最终
  收敛到 RenderDoc 标准布局。
- 当前边界：只承诺 T08 的单 render pass、4x BGRA8 2D multisample attachment、单采样 2D resolve、
  `MultisampleResolve` store action 与 alpha-to-coverage。逐 sample 查看、MSAA array、custom sample
  positions、memoryless 和 depth/stencil resolve 留待后续 fixture。
- 验证：T08 structured XML、三条 draw/action output、sample/resolve snapshot 与 clear/draw/rewind 像素
  由 smoke 自动断言；九份 capture 各 10 次 lifecycle resident growth 为 376,832 字节，三轮 CLI
  replay 均通过。qrenderdoc EID 4 显示 Texture 17 为 4x Texture 2D MS、Texture 24 为 1x Texture 2D，
  resolve 行可进入正确红绿蓝图像，HTML export 与页面一致，状态栏无错误。

## D031：T09 子资源索引统一采用 mip 与线性 slice

- 日期：2026-09-23
- 状态：已采用
- 决策：保留真实 Metal texture type 与 `mipmapLevelCount`；RenderDoc 对外的 array size 对
  `Texture2DArray` 等于层数，对 cube 等于六面，对 cube array 等于六倍 cube 数。上传 chunk 明确记录
  mip/slice/bytesPerRow/bytesPerImage；`GetTextureData()`、`PickPixel()`、标准 output renderer 和
  DDS 保存使用相同的线性 slice 语义，越界及当前不支持的组合明确拒绝。
- 原因：只保存 mip 而丢失 slice 会让 array/cube 的不同内容被错误合并；把 cube 误报为单层也会使
  Texture Viewer face 选择、Pipeline 类型和导出结果互相矛盾。
- 当前边界：T09 验证单采样 RGBA8 的 2D mip、2D array 与 cube；cube array descriptor 可枚举，
  但 display/readback 与更多格式、3D、texture view 仍按后续 fixture 扩展。
- 验证：structured XML 覆盖全部 12 个 `replaceRegion`；smoke 检查 12 份原始字节/display、cube
  face pick、越界拒绝与 512-byte DDS；qrenderdoc EID 2 的 FS/Texture Viewer/保存路径一致。

## D032：macOS 26 的 Qt 5 子资源组合框避开 Cocoa popup

- 日期：2026-09-23
- 状态：已采用
- 决策：仅在 macOS 的 Texture Viewer mip 与 slice/face 两个 `QComboBox` 上拦截左键
  press/double-click/release，按当前模型顺序循环选项；原有方向键、Home/End 和通用 index-change
  更新保持不变。其他平台的组合框不变；控件 tooltip 说明点击和键盘用法。
- 原因：本机 Qt 5.15.19/macOS 26.1 的 `QComboBox::showPopup()` 会在 `libqcocoa.dylib` 中访问
  无效地址 0x28 并使 qrenderdoc 崩溃；仅改为非原生 `QListView` 仍会崩溃。问题在 UI popup 路径，
  与 Metal replay 数据无关。限制修复范围比改动全局 Qt style 更稳妥。
- 验证：最新 qrenderdoc 通过连续点击切换 mip0/1/2、array slice0/1/2、cube X+/X-，键盘 End
  选到 Z-；各颜色正确，标准 DDS 保存和 `No problems detected` 均通过。
