# 阶段 5 细化计划：T04 动态 uniform 与 fragment buffer binding

T03 已关闭纹理/sampler 与 Pipeline UI 的第一条完整绑定链路。下一条纵向切片固定为 T04 动态
uniform：用一个有明确对齐和已知字节的 buffer、两个 offset 和两次 draw，扩展 fragment buffer
capture/replay、事件级状态、shader reflection 与标准 Pipeline 资源交互。页面继续复用 RenderDoc
标准布局，不为 Metal 新建独立查看器。

当前状态：P5.1-P5.4 已于 2026-09-22 完成。T04 已进入完整 T00-T04 自动回归，并由最终构建的
qrenderdoc 完成事件、画面、Pipeline constant buffer 与 Buffer Viewer 实机验收。

## P5.1：建立确定性 T04 fixture（已完成）

- 新增 `Metal_Dynamic_Uniform`，使用无外部资产的简单几何和单个 shared uniform buffer。
- buffer 内放置两个按 256 字节边界对齐、字节值固定的记录；fragment shader 通过直接
  `[[buffer(0)]]` 参数读取颜色或等价的可见数据。
- 第一条 draw 绑定 offset 0，第二条 draw 通过 `setFragmentBufferOffset` 或等价 Metal API 切换到
  offset 256；左右区域输出不同固定颜色，使事件和 offset 可由像素独立验证。
- 固定入口、slot、draw count、viewport/scissor 与参考像素，不依赖窗口时序或随机数据。

验收：未注入运行稳定；左右参考像素与 uniform 原始字节一致；测试程序可由现有 demos 构建入口执行。

结果：新增 `Metal_Dynamic_Uniform`。512-byte buffer 在 offset 0/256 保存红/绿两个 16-byte 记录；
两条 fullscreen triangle draw 使用左右 viewport，native 输出稳定。

## P5.2：补齐 fragment buffer capture 与 GPU replay（已完成）

- 包装并序列化 T04 实际调用的 fragment buffer bind/offset 更新，不顺带声称支持批量或 argument
  buffer API。
- replay 恢复 buffer 内容、初始 binding 和第二条 draw 前的 offset 更新，并生成两条独立 action/event。
- 按 action event 保存 draw-time snapshot，确保 EID 往返不会把第一条 draw 的 offset 覆盖为第二条。

验收：structured XML 精确包含 ResourceId、slot、0/256 offset 和两条 draw；CLI replay 的整帧及
clear -> draw1 -> draw2 -> draw1 往返像素均与预期一致。

结果：`setFragmentBuffer` 现同步 replay binding 状态；`setFragmentBufferOffset` 已完成 bridge、chunk、
structured processing、GPU replay 和事件状态更新。XML 精确记录 offset 0/256 与两条 draw，event
seek 图像依次为背景、左红、左红右绿、回到左红。

## P5.3：descriptor、reflection 与 Pipeline State（已完成）

- 将 fragment buffer 纳入 Metal descriptor store、`DescriptorAccess`、`GetDescriptors()` 和
  `PipeState` 查询，保留物理 slot、byte offset、有效范围及资源身份。
- 从 Metal pipeline argument reflection 枚举 buffer 名称、slot、数组长度、访问类型和 active 状态；
  不从实际 binding 推断 shader 声明。
- Metal Pipeline 的 FS 页复用现有 `RDTreeWidget`、used/unused、资源上下文菜单与 Buffer Viewer
  自动格式路径，并保持 IA/VS/RS/OM 布局不退化。

验收：两条 draw 的 FS 表引用同一 Buffer ResourceId 但分别显示 0/256 offset；shader binding 与
fixture 声明一致；双击进入标准 Buffer Viewer 的精确子范围。

结果：Metal state 新增 fragment buffer binding；通用 constant-block descriptor/reflection 枚举
`uniforms`、slot 0、16 bytes、active。FS 页新增标准 Constant Buffers 表，EID 2/3 分别显示
`Buffer 16 / 0 / 512` 与 `Buffer 16 / 256 / 256`；双击 EID 3 行进入 Buffer Viewer 的 256-byte
子范围并显示 `.0625/.875/.1875/1.0` 的原始浮点字节。

## P5.4：自动回归与 qrenderdoc 实机验收（已完成）

- 将 T04 native/capture/XML/replay/readback/binding/lifecycle 并入
  `test_metal_capture_macos.sh`，T00-T03 不退化。
- 对两条 draw 的 event seek、buffer 原始字节、descriptor offset、shader reflection 和输出像素增加
  自动断言。
- 启动最终构建的 qrenderdoc，实机核对 Event Browser、FS buffer 行、两条 draw 的 offset 切换、
  Buffer Viewer 跳转、used/unused 行为和状态栏。
- 同步更新 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 与 `HANDOFF.md`。

验收：完整 T00-T04 回归通过；最终 qrenderdoc 状态栏为 `No problems detected`，并保持运行供复核。

结果：一键脚本完整通过，五份 capture 各 10 次 lifecycle resident growth 为 376,832 字节；T04
最终 PPM 左右关键像素为 `ff2010`/`10df30`。qrenderdoc 在 EID 2/3 显示正确半屏结果、0/256 offset
与 Buffer Viewer 子范围，状态栏为 `No problems detected`，当前保持运行在 EID 3 的 FS 页。

## 当前明确不在本切片内

- argument buffer、批量 buffer binding、vertex texture/sampler。
- indirect draw、base vertex/base instance 和 instancing。
- private/managed buffer 的 CPU 更新追踪、跨 command buffer 同步和 compute binding。
- 为填满页面而添加没有 capture/replay 证据的 pipeline 字段。
