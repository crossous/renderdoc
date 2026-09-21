# Metal Replay 测试矩阵

## 优先级

- P0：首条可用 replay 链路必须覆盖。
- P1：首版“常见图形接口”承诺的一部分。
- P2：首版后扩展；不能阻塞 P0/P1 交付。
- Out：当前阶段明确不做。

## 功能覆盖矩阵

| 类别 | P0 | P1 | P2 / Out |
| --- | --- | --- | --- |
| Device/Queue | default device、command queue、command buffer、commit/wait | 多 command buffer、debug label/group | 多 GPU、任意应用注入 |
| Buffer | length/bytes 创建、shared/private、初始内容、vertex/index/uniform | offset 更新、blit copy/fill、多 buffer | sparse/IO buffer |
| Texture | 2D、mip、sampled、render target、RGBA/BGRA/depth | array、cube、compressed、MSAA resolve、views | sparse texture（P2） |
| Render pass | clear/load/store、单 color、depth | MRT、stencil、resolve | tile/imageblock 高级路径（P2） |
| Pipeline | vertex/fragment、vertex descriptor、blend/depth/raster | function constants、多个 attachment | dynamic library/function stitching（P2） |
| Binding | vertex/fragment buffer、texture、sampler | 批量绑定、argument buffer 只读解析 | function table/ray resources（Out） |
| Draw | primitives、indexed | instanced、base vertex/base instance、常见 primitive | indirect/ICB（P2） |
| Compute | 不阻塞第一条三角形链路 | pipeline、buffer/texture binding、dispatch | advanced synchronization（P2） |
| Blit | texture/buffer copy | fill、mipmap、常见同步 | sparse mapping（Out） |
| UI | Event、Texture、Buffer、Pipeline、Shader、Mesh | resource links、markers、错误提示 | shader debug、pixel history（Out） |

## 必备测试场景

每个场景应当有源程序、固定输入、capture 生成说明、预期事件树和验证数据。测试程序优先保持小而
独立，不依赖大型引擎。

每个场景分别跟踪五个状态：Native、Capture、RDC inspect、Replay、UI。禁止仅因为生成了文件就把
场景标记为完成。

| ID | 场景 | 主要覆盖 | 优先级 | 状态 |
| --- | --- | --- | --- | --- |
| T00 | 空帧 + present | 文件加载、device/queue、输出窗口 | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/lifecycle/UI 重开验证通过 |
| T01 | 彩色三角形 | render pass、pipeline、MSL、非索引 draw | P0 | Native/Capture/RDC inspect/CLI Replay/Texture UI/resize/event seek/capture texture readback/像素拾取/DDS 保存/Shader UI/最小 Pipeline State UI/lifecycle/UI 重开验证通过 |
| T02 | 索引立方体 | vertex/index buffer、depth、Mesh Viewer | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/index/vertex 数据/lifecycle/Event + Texture + Pipeline + Buffer + Mesh UI 全部通过 |
| T03 | 纹理四边形 | texture upload、sampler、fragment binding | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/texture data+pick/generic binding/shader reflection/used-unused 过滤/lifecycle/Pipeline+Texture+Resource UI 验证通过 |
| T04 | 动态 uniform | buffer offset/更新、多 draw | P1 | 未开始 |
| T05 | 实例化网格 | instance layout、base instance | P1 | 未开始 |
| T06 | MRT + blending | 多 attachment、blend state | P1 | 未开始 |
| T07 | depth/stencil | depth/stencil state 和 attachment | P1 | 未开始 |
| T08 | MSAA resolve | multisample texture、resolve | P1 | 未开始 |
| T09 | mip/cube/array | 子资源枚举和查看 | P1 | 未开始 |
| T10 | buffer/texture blit | copy/fill/mipmap | P1 | 未开始 |
| T11 | compute texture filter | compute pipeline、dispatch、读写纹理 | P1 | 未开始 |
| T12 | argument buffer | 资源引用解析 | P2 | 未开始 |
| T13 | indirect draw/ICB | 间接命令 | P2 | 未开始 |

## 外部样例候选

外部样例用于补充验证，不直接替代仓库内的最小确定性测试。引入前必须记录固定 commit、许可证、
所需 SDK 和实际使用的 target。

1. Apple 官方 Metal Sample Code：
   `https://developer.apple.com/metal/sample-code/`
   - 优先场景：Using a Render Pipeline to Render Primitives、Creating and Sampling Textures、
     Customizing Render Pass Setup、Processing a Texture in a Compute Function、MSAA、argument buffer。
   - `LearnMetalCPP.zip` 适合验证 metal-cpp、基础绘制、buffer、texture 和 animation。
2. `metal-by-example/learn-metal-cpp-ios`
   - 地址：`https://github.com/metal-by-example/learn-metal-cpp-ios`
   - 初查 commit：`d966e516631f42bac8febdb44d955a545fac4661`
   - 许可证：Apache-2.0。
   - 主要用于 API 调用序列参考；它以 iOS 为目标，需筛选可移植到 macOS 的核心 renderer。
3. `LeeTeng2001/metal-cpp-cmake`
   - 地址：`https://github.com/LeeTeng2001/metal-cpp-cmake`
   - 初查 commit：`e22adb2d0e5fd8d34a96c9c7c6cb3ef6ec943285`
   - 许可证：Apache-2.0。
   - CMake 结构适合快速构建多个 metal-cpp 基础用例。
4. `dehesa/sample-metal`
   - 地址：`https://github.com/dehesa/sample-metal`
   - 初查 commit：`0003824a52516052f2d28503f576907e03425dd3`
   - 许可证：MIT。
   - 覆盖 Swift/macOS/iOS 的更广 Metal 示例，用于第二轮兼容验证。

外部样例应放在独立的 `tests/metal/external` 或工作区外缓存中，不能无说明地复制进 RenderDoc
源码。Apple 下载样例的再分发条款需要在 vendor 前单独确认。

## 验证方式

- 数据验证：已知 buffer 字节模式、已知纹理颜色/梯度、CPU 参考结果。
- 状态验证：测试程序写出 descriptor 摘要，与 qrenderdoc Pipeline State 对比。
- 图像验证：保存参考输出，允许明确的颜色空间/浮点误差阈值。
- 事件验证：对 action 名称、层级、draw 参数和 event ID 做结构化断言。
- 稳定性验证：重复打开 capture、切换事件和资源、关闭 capture，检查崩溃与资源增长。

当前自动回归还会把 Metal replay output 读回为 640x480 RGB PPM：T00 校验中心 clear 色，T01
校验背景色和中心非背景像素，以防 output window 或 texture display 退化为“只是不崩溃”。T01
还在同一 controller 中连续 10 轮依次选择 clear、draw、clear action，断言中心像素
`#14141a -> 非背景 -> #14141a`，覆盖前进与回退的 event-range replay。

T01 同时直接读取 capture backbuffer 的 400x300 BGRA 原始字节，在 clear EID 校验中心
`1a1414ff`，在 draw EID 校验中心发生变化，并通过 `PickPixel()` 校验背景 RGBA 约为
`(0.08, 0.08, 0.10, 1.0)`。最后经 `SaveTexture()` 写出 400x300 ARGB8888 DDS，覆盖 qrenderdoc
纹理保存所依赖的数据路径。qrenderdoc 实机在 EID 2 对 `(202, 128)` 的右键拾取返回
`(0.61176, 0.34118, 0.32157, 1.00)`，并成功从保存对话框写出同规格 DDS。

T01 的 replay smoke 还断言 shader 资源恰好包含 `vs_main`/Vertex 与 `fs_main`/Fragment，reflection
encoding 为 MSL，`captured.metal` 同时包含真实 vertex/fragment 入口。qrenderdoc 实机从 Resource
Inspector 的 Function 13/14 分别进入 Shader Viewer，两者均显示该 MSL，状态栏为
`No problems detected`。

T01 的 Pipeline State 自动回归会分别选择 clear 与 draw EID，确认 Metal capture 类型和 color
target；在 draw EID 进一步断言 render pipeline 为真实 `PipelineState` resource、vertex/fragment
shader 入口及 reflection、`TriangleList`、slot 0 的 96-byte vertex buffer 和 swapbuffer color target。
qrenderdoc 实机在 EID 2 的 Metal Pipeline State 页面显示 `Pipeline State 15`、`Function 13/14`、
`vs_main/fs_main`、`Triangle List`、`Buffer 16`（offset 0、size 96）和 `Texture 23`（mip/slice 0），
状态栏为 `No problems detected`。

T02 自动回归会核对 stride 28 的 interleaved Float3/Float4 vertex descriptor、`Depth32Float` attachment、
less/write depth state、CCW/back-face raster state、两组 viewport/scissor，以及 36 个 UInt16 和 36 个
UInt32 index draw。Replay smoke 逐字节比较 72/144-byte index buffer，确认两个 action 都带
`Drawcall|Indexed`，逐 draw 断言 vertex attributes/layout、index binding、depth/raster 与左右
viewport/scissor，并检查 clear -> UInt16 draw -> UInt32 draw -> UInt16 draw 回退的像素结果。
同一 smoke 会断言通用 `GetVertexInputs()` 返回 `attr0` Float3/offset 0 与 `attr1` Float4/offset 12，
创建 Mesh replay output，实际执行 Float3 VS Input 的 indexed wireframe 绘制并检查非背景像素。
qrenderdoc 实机选择 EID 3 后，API Inspector 显示
`drawIndexedPrimitives(36, MTLIndexTypeUInt32, Buffer 20)`，Texture Viewer 的 Outputs 同时列出
`FB0 Texture 27` 和 `DS Texture 17`，左右两个彩色立方体正确。Pipeline State 在 EID 2/3 分别显示
`Buffer 19 / 72 / UInt16` 与 `Buffer 20 / 144 / UInt32`、左/右 viewport/scissor，并共同显示
Float3/Float4、stride 28、less/write 和 back/CCW；状态栏为 `No problems detected`。
从 vertex attribute 行激活可进入标准 Mesh Viewer：EID 2 的 VS Input 表格按 UInt16 index 展开
`attr0/attr1`，预览显示立方体线框，VS Output 表明确显示 Metal post-VS unsupported。从 Pipeline
State 激活 Buffer 18 会打开 224-byte、stride 28 的自动格式 Buffer Viewer；Buffer 19/20 分别以
`ushort index`/`uint index` 打开 72/144-byte 子范围并显示相同索引序列。

T03 自动回归会核对 4x4 RGBA8 descriptor、64-byte `replaceRegion` 内容、nearest/clamp sampler、
fragment texture/sampler slot 0、Float2 position/Float2 UV 与 `TriangleStrip` draw。Replay output 在
四个象限分别断言 `ff2010`、`10e030`、`1840ff`、`f0d020`，并通过 `GetTextureData()` 与
`PickPixel()` 精确读取同一组 texel；通用 `PipeState::GetReadOnlyResources(Fragment)` 和
`GetSamplers(Fragment)` 同时核对 Texture 17 与 Sampler 18。qrenderdoc 实机在 EID 2 显示正确四象限，
Pipeline 双击纹理进入标准 Texture Viewer，双击 sampler 进入 Resource Inspector，状态栏无错误。

P4.4 的首个 UI 收敛切片复用标准 `PipelineFlowChart`，把 Metal 状态分到 IA/VS/RS/FS/OM 五个阶段页。
T03 实机验证 IA 的 Float2/Float2 与空 index 提示、FS 的 Function 14/Texture 17/Sampler 18、shader
直接跳转和 OM 空 depth 提示；T02 实机验证 IA 的 UInt16 Buffer 19、RS 的左 viewport/scissor 与
Back/CCW、OM 的 Texture 27/Depth Texture 17/Less/Write。`Show Empty Items` 使用标准红色空槽；
`Show Unused Items` 在 shader binding reflection 到位前保持禁用并显示原因。

P4.4 第二个 UI 收敛切片将所有表切换为标准 `RDTreeWidget`/`RDHeaderView`，并将 Metal ResourceId
接入通用资源上下文、usage、thumbnail/preview 分发。T03 EID 2 的 Texture 17 双击仍打开标准
Texture Viewer；标准 Export 控件实际写出 `/tmp/metal-pipeline-t03.html`，内容核对 IA/VS/RS/FS/OM、
Triangle Strip、Texture 17 和 Sampler 18。macOS CUA 的 secondary-click 对 Event Browser 和资源表
均不产生 Qt context-menu 事件，因此右键菜单本轮以公共接线、构建和资源激活回归为证据，不把自动化
限制误记为功能失败。

P4.4 第三个 UI 收敛切片在创建 render pipeline 时请求 Metal argument reflection。T03 自动回归断言
fragment shader 的 `colourTexture`/`colourSampler` 均为 slot 0、直接 Image/Sampler binding、只读且
active；fixture 同时把 Texture 17/Sampler 18 绑定到 shader 未声明的 slot 1，断言通用 descriptor
查询将 slot 0 映射到 reflection index 0，将 slot 1 标记为 `NoShaderBinding + staticallyUnused`，且
`onlyUsed=true` 只返回 slot 0。最终 qrenderdoc 在 T03 EID 2 的 FS 页默认仅显示 slot 0；勾选
`Show Unused Items` 后 texture/sampler 表各增加真实物理 slot 1，状态栏保持 `No problems detected`。

生命周期 smoke 会在同一进程中分别打开/关闭 T00、T01、T02 和 T03 各 10 次，检查 action、swapbuffer、draw、
texture readback，并首次调用 histogram、pixel history、post-VS、四种 shader debug 和 target/custom
shader build 的 unsupported 路径。完成 P4.4 第三个 UI 切片后的 2026-09-22 完整回归在两轮 warm-up 后
resident growth 为 540,672 字节；加入 T02 前的额外 50 轮压力检查增长 1,441,792 字节。qrenderdoc 还实机完成
`T01 -> Close -> T00 -> Close -> T01`，重开后 T01 的 EID 2 图像和 Pipeline State 正确；
History/Debug 按钮明确显示不支持且保持禁用。

## 单场景执行顺序

```text
Native reference -> Capture -> RDC/chunk inspection -> Replay -> UI/data comparison -> Regression
```

先对 T00/T01 完成整条链路，再依次推进 texture、indexed mesh、MRT、MSAA、compute 等 feature。
