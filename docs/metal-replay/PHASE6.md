# 阶段 6 细化计划：T05 多 vertex buffer 与实例化网格

T04 已关闭 fragment constant buffer、动态 offset 和多 draw 事件状态。下一条 P1 纵向切片固定为
T05 实例化网格：以分离的 per-vertex/per-instance buffer、显式 step function/rate 和可见的多个实例，
扩展 draw metadata、Pipeline IA 与 Mesh Viewer。实现继续复用通用 `PipeState` 和标准 viewer。

当前状态：P6.1-P6.4 已完成。下一阶段转入 `PHASE7.md` 的 T06 MRT + blending。

## P6.1：建立确定性 T05 fixture（已完成）

- 新增 `Metal_Instanced_Mesh`，使用独立 position buffer 和 per-instance transform/color buffer。
- vertex descriptor 显式设置两个 buffer layout、stride、attribute offset、
  `MTLVertexStepFunctionPerInstance` 与固定 step rate。
- 使用带 instance count/base instance 的直接 draw 重载，在不同位置输出至少三个颜色固定的实例。
- 不依赖纹理、随机数或外部资产，以固定像素和 buffer 字节定义 native 参考。

验收：未注入输出稳定；三个实例的关键像素和两份 buffer 原始数据可确定性断言。

结果：新增 `Metal_Instanced_Mesh`。slot 0 为 24-byte Float2 position，slot 1 为 96-byte
Float2 offset + Float4 colour；第 0 个实例为未使用 sentinel，draw 使用
`instanceCount=3/baseInstance=1` 输出红、绿、蓝三个实例。

## P6.2：capture/replay 与事件状态（已完成）

- 序列化并 replay T05 实际使用的 instance count/base instance 参数及所需 buffer binding。
- action metadata 正确保存 `numInstances`、`baseInstance`、vertex count 和 topology。
- draw-time snapshot 同时保存两个 vertex buffer 的 offset/size/stride/step function/rate。

验收：structured XML、action 和 GPU replay 与 fixture 参数一致；event seek 图像稳定。

结果：现有 5 参数直接 draw 路径经 T05 首次完整验证。XML、action 与 draw-time snapshot 均保留
3 个 instance、base instance 1、两个 vertex slot 及 per-instance step；clear/draw 往返图像正确。

## P6.3：通用 vertex input 与 Mesh Viewer（已完成）

- 扩展 Metal `GetVertexInputs()` 映射，使 per-instance attribute 使用正确的 buffer slot、rate 与 offset。
- 标准 Buffer Viewer 分别打开 position 与 instance 数据；不新增 Metal 专用查看器。
- 扩展当前最小 Metal `RenderMesh()`，在 VS Input 中正确展开至少一个指定实例；不能支持的实例选择
  行为必须明确显示限制。

验收：Pipeline IA、Buffer Viewer 与 Mesh Viewer 使用相同 ResourceId/offset/stride，表格和预览与
GPU 输出一致。

结果：通用 `GetVertexInputs()` 把 attr0 映射到 per-vertex slot 0，把 attr1/attr2 映射到
per-instance slot 1。Mesh Viewer instance 0/1/2 会在应用 base instance 后读取记录 1/2/3；VS Input
预览按 RenderDoc 标准语义显示原始 position attribute，shader 变换后的实例位置属于当前明确不支持的
post-VS 数据。两个物理 buffer 均通过 Pipeline IA 进入标准自动格式 Buffer Viewer。

## P6.4：自动回归与 qrenderdoc 实机验收（已完成）

- 将 T05 native/capture/XML/replay/action/buffer/mesh/lifecycle 并入一键脚本，T00-T04 不退化。
- 启动最终 qrenderdoc，核对实例化 action、IA 两个 buffer、per-instance attribute、Buffer/Mesh Viewer
  跳转和最终图像。
- 同步所有进度、测试和决策文档。

验收：完整 T00-T05 回归通过；最终 qrenderdoc 状态栏为 `No problems detected` 并保持运行。

结果：一键脚本完整通过，六份 capture 各 10 次 lifecycle resident growth 为 573,440 bytes；六份
capture 的三轮 CLI replay 和 `git diff --check` 通过。最终 qrenderdoc 在 EID 2 显示三色实例；IA
显示 `Buffer 16 / 24 / stride 8 / Vertex / 1` 与 `Buffer 17 / 96 / stride 24 / Instance / 1`；
Mesh Viewer 切换实例后 offset/colour 正确，两个 Buffer Viewer 范围和值正确，状态栏为
`No problems detected`。

## 当前明确不在本切片内

- indirect draw/ICB、argument buffer 和 compute。
- tessellation、mesh/object shader 或 post-VS 捕获。
- 覆盖所有 Metal vertex format；只新增 T05 使用且有自动数据证据的格式。
