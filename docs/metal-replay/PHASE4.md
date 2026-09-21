# 阶段 4 细化计划：T03 纹理采样、资源绑定与 Pipeline UI 收敛

T02 已关闭 indexed/depth/VS Input 纵向链路。下一条 feature 固定为 T03 纹理四边形：用已知 texel、
固定 sampler 和单次 draw 同时扩展 texture 初始内容、fragment texture/sampler binding、资源跳转，
并开始把 Metal Pipeline State 从开发期纵向列表收敛到 RenderDoc 标准信息架构。

当前状态：P4.1-P4.3 已于 2026-09-21 完成；P4.4 已于 2026-09-22 完成标准阶段骨架、
`RDTreeWidget`/资源操作/HTML export 和 shader reflection/used-unused 三个收敛切片；P4.5 的 T03
自动化和实机验证已完成。T03 阶段闭环已关闭；更完整的 Metal 状态字段、绑定类型和 UI 细节继续按
`PLAN.md` 的 M4.3f/M4.7 推进。

## P4.1：建立确定性 T03 fixture（已完成）

- 新增无外部文件依赖的 RGBA8 checker/色块纹理，CPU 端生成固定 texel。
- 使用 Float2 position/Float2 UV 的显式 vertex descriptor 和 triangle strip/list 绘制四边形。
- 创建固定 nearest/clamp sampler，fragment shader 只做 texture sample，避免颜色空间歧义。
- 保存原生参考输出，并在 structured XML 中断言 texture descriptor、初始字节、sampler 和 binding。

验收：T03 未注入输出稳定；一键脚本生成非空 capture；texel、descriptor、entry point 和 draw 参数
均有自动断言。

结果：新增 `Metal_Textured_Quad`，以 4x4 RGBA8 四色块、Float2 position/Float2 UV、triangle strip
和 nearest/clamp sampler 形成无外部资产的确定性输入；native、capture 与 structured XML 均通过。

## P4.2：补齐 sampled texture/sampler capture 与 replay（已完成）

- 包装并序列化 T03 所需的 texture upload/replace、sampler 创建和 fragment binding。
- replay 恢复 texture 初始内容、sampler state 与 fragment slot 后执行 draw。
- 明确 storage mode、row pitch、mip/slice 的当前支持边界，不用 unified memory 偶然行为代替语义。

验收：CLI replay 与原生参考图一致；capture texture 原始 texel 可逐像素读取。

结果：已实现 2D `replaceRegion`、sampler 一级资源和 fragment texture/sampler bind 的 capture/replay；
GPU 输出四象限、64-byte texture data 和四个 `PickPixel()` 值均精确验证。当前边界固定为单层、
非压缩 RGBA8 2D upload，slice/bytesPerImage/mip/其他格式继续明确 unsupported。

## P4.3：PipeState bindings 与标准资源交互（已完成）

- 在 `MetalPipe::State` 保存 vertex/fragment buffer、texture、sampler binding。
- 优先通过通用 descriptor/resource 查询暴露数据，Texture/Buffer/Shader Viewer 不新增 Metal 分叉。
- Pipeline State 的资源激活遵循其他后端：texture 进入 Texture Viewer/Resource Inspector，buffer 进入
  Buffer Viewer，shader 进入 Shader Viewer。

验收：选定 T03 draw 时 Pipeline、Texture、Shader 与 Buffer 页面引用同一组 resource ID 和 slot。

结果：fragment texture/sampler 已经由通用 `DescriptorAccess`、`GetDescriptors()` 和
`GetSamplerDescriptors()` 暴露；Pipeline 双击 texture 进入标准 Texture Viewer，sampler 进入
Resource Inspector，未增加 Metal 专用资源查看器。

## P4.4：Pipeline State 布局向 RenderDoc 标准收敛（当前 T03 范围已完成）

- 按 Input Assembly、Vertex Shader、Rasterizer、Fragment Shader、Output Merger/Targets 分组，而不是
  继续堆叠 Metal 专用长列表。
- 复用其他后端已有的 used/empty 语义、资源样式、双击/右键行为和自动格式；不复制 viewer 逻辑。
- 当前尚无证据的数据保持空槽或明确 unsupported，不为追求视觉完整伪造状态。
- 允许分步迁移，但每一步的数据源都必须来自 `MetalPipe::State`/通用 `PipeState`。

验收：常用操作路径、字段层级和资源跳转可与 D3D/Vulkan 页面逐项对照；剩余视觉差异被记录为
明确任务，而不是长期保留的 Metal 特例。

已完成的切片：

- 复用标准 `PipelineFlowChart` 和隐藏 stage tabs，形成 IA -> VS -> RS -> FS -> OM 导航。
- 将 vertex input/index、shader、raster、fragment resources 和 color/depth output 分别归入对应阶段。
- 增加标准 `Show Empty Items` 行为和红色空槽提示；无静态 shader binding reflection 时
  `Show Unused Items` 明确禁用并给出原因，不伪造 used 状态。
- shader 行直接进入通用 Shader Viewer；既有 Mesh/Buffer/Texture/Resource Inspector 跳转保持不变。
- 所有状态表迁移到标准 `RDTreeWidget`/`RDHeaderView`，Metal ResourceId 接入父级
  `SetupResourceView()`、thumbnail 和 preview 分发，复用通用复制、Resource Inspector 和 usage 菜单。
- 工具栏加入与其他后端一致的 Export 控件，导出 IA/VS/RS/FS/OM 五阶段 HTML；T03 EID 2 实际导出
  已核对 topology、shader、Texture 17、Sampler 18 和 output 表。
- replay 创建 render pipeline 时请求 `MTLPipelineOptionArgumentInfo`，将 fragment argument reflection
  映射为 `colourTexture`/`colourSampler`、slot 0、类型、数组长度和 active 状态；没有反射的 pipeline
  仍保持保守行为。
- T03 将同一 texture/sampler 同时绑定到 shader 声明的 slot 0 与未声明的 slot 1。通用 descriptor
  查询把 slot 0 映射到真实 reflection index，把 slot 1 标记为 `NoShaderBinding + staticallyUnused`；
  UI 默认只显示 slot 0，勾选 `Show Unused Items` 后以真实 Metal 物理槽号显示 slot 1。

后续范围：继续核对紧凑布局与键盘操作，并用新 fixture 扩展 fragment buffer、vertex
texture/sampler、argument buffer 和更多 pipeline 字段；这些不阻塞当前 T03 阶段关闭。

## P4.5：回归与阶段关闭（已完成）

- 将 T03 native/capture/XML/replay/pixel/binding/UI/lifecycle 并入一键脚本。
- T00/T01/T02 不退化；qrenderdoc 重启后完成 T03 Pipeline/Texture/Shader 交叉核对。
- 同步更新 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 和 `HANDOFF.md`。
