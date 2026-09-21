# RenderDoc Metal Replay 项目入口

本目录是 RenderDoc v1.46 Metal replay 适配工作的唯一计划与交接入口。实现代码仍放在
RenderDoc 原有目录中，项目状态、阶段计划、测试覆盖和关键决策统一记录在这里。

## 项目目标

在 macOS 上为 RenderDoc v1.46 建立一套可用的 Metal 离线 replay 能力。首版完成后，
用户应当能够在 qrenderdoc 中打开受支持的 Metal `.rdc`，浏览事件，并查看：

- Buffer 列表、元数据和原始/格式化内容。
- Texture 列表、子资源、像素和常见格式。
- Render pipeline、render pass、资源绑定和固定功能状态。
- 顶点/索引输入以及 Mesh Viewer 中的网格。
- Metal shader 的入口点、阶段、可获得的 MSL 源码和反射信息。
- 选定 draw call 的 replay 输出。

这一阶段不以 shader debugging、pixel history、性能计数器、ray tracing、mesh shader、
MetalFX、任意第三方应用注入或完整 capture 产品化为目标。为了产生可重复测试输入，允许
实现最小范围的 capture/序列化补全和测试夹具，但它们只服务于 replay 验证。

## 固定基线

- 上游：`https://github.com/baldurk/renderdoc.git`
- 标签：`v1.46`
- 基线提交：`e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`
- 开发分支：`metal-replay-v1.46`
- 工作平台：Apple Silicon macOS

## 文档导航

- [PLAN.md](PLAN.md)：总体路线、阶段门槛和验收条件。
- [PHASE1.md](PHASE1.md)：已完成的 Metal 样例与 capture 阶段记录。
- [PHASE2.md](PHASE2.md)：已完成的 T00/T01 replay 后端纵向闭环。
- [PHASE3.md](PHASE3.md)：已完成的 T02 索引立方体、depth 与 Mesh Viewer 纵向闭环。
- [PHASE4.md](PHASE4.md)：已完成的 T03 纹理采样、资源绑定与 Pipeline UI 收敛计划。
- [PHASE5.md](PHASE5.md)：下一条 T04 动态 uniform 与 fragment buffer binding 纵向切片。
- [STATUS.md](STATUS.md)：当前状态、最近验证结果、阻塞项和下一步。
- [TEST_MATRIX.md](TEST_MATRIX.md)：Metal API/资源/UI 覆盖矩阵与测试样例来源。
- [HANDOFF.md](HANDOFF.md)：新 agent 的接手规则和文档更新约定。
- [DECISIONS.md](DECISIONS.md)：关键架构与范围决策。

## 当前状态

T00 空帧、T01 彩色三角形、T02 索引立方体与 T03 纹理四边形已经完成各自的 Native/Capture/RDC
inspect/seekable Replay/UI 纵向闭环。自动回归覆盖 texture readback、像素拾取、DDS 保存、事件往返、
vertex/index 数据、VS Input mesh preview、texture/sampler shader reflection、Pipeline used/unused 过滤
和 replay 生命周期；四份 capture 各 10 次同进程打开/关闭以及最新 qrenderdoc 实机核对均已通过。
当前下一项是 `PHASE5.md` 的 T04 动态 uniform 与 fragment buffer binding。准确进度和已知限制以
`STATUS.md`、`PLAN.md` 为准。

## 初始基线结论（2026-09-20）

RenderDoc v1.46 已有约 1.5 万行 Metal 驱动骨架，包含对象包装、部分 capture 序列化、
初始资源内容和 Objective-C bridge。然而它并不是可用的 replay 后端：

- `MetalReplay` 尚未实现 `IReplayDriver`。
- 尚未注册 Metal replay provider，qrenderdoc 不能把 Metal `.rdc` 作为可 replay capture 打开。
- `MetalReplay` 当前只有资源描述索引辅助函数。
- bridge 中约有 237 个 `METAL_NOT_HOOKED()`；Metal 目录中约有 402 个未实现/未处理标记。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 仍未实现，事件树尚未形成。
- `ProcessChunk()` 只处理少量资源和 draw 相关 chunk，大量常见状态仍直接报未处理。

因此首个工程里程碑不是扩充所有 Metal API，而是先建立可编译、可启动、可截取、可打开并可
replay 的最小纵向链路，再按测试矩阵逐项扩大支持面。每个新 feature 都先验证样例原生运行，
随后补 capture，再立即补同一 feature 的 replay 和 UI 检查，避免积累一批无法验证语义的 `.rdc`。
