# 阶段 1 细化计划：Metal 样例与最小 Capture

阶段 0 已完成。阶段 1 的目标不是先造一个只能“接受文件”的空 replay driver，而是建立可靠的
Metal 输入链路：样例原生运行、RenderDoc 注入/加载、触发截帧、写出可检查的 `.rdc`。

## M1.1：建立最小测试程序

第一批只做两个 target：

- T00：创建窗口和 `CAMetalLayer`，每帧 clear 为固定颜色后 present，不使用 shader。
- T01：MSL 源码创建 library/function/pipeline，上传 vertex buffer，绘制彩色三角形并 present。

优先复用 RenderDoc `util/test/demos/apple` 的窗口基础设施和 Apple Learn Metal with C++ 的调用
顺序。第三方代码只在许可证和固定 commit 记录完成后引入；能自行写清楚的小型 fixture 不复制
大型示例。

完成条件：两个 target 在未加载 RenderDoc 时持续运行，输出与固定参考图一致。

## M1.2：确定受控样例的加载方式

按以下优先级验证：

1. qrenderdoc Launch Application 路径。
2. `DYLD_INSERT_LIBRARIES` 加载当前 build 的 `librenderdoc.dylib`，仅用于自有、未 hardened 的测试程序。
3. 测试程序显式链接/加载 RenderDoc，并通过 in-app API 触发 capture。

先保证开发样例可截帧，不在本阶段解决 SIP、Hardened Runtime、签名应用和任意第三方程序注入。

完成条件：日志确认 Metal hook 初始化且 `MTLCreateSystemDefaultDevice` 返回 wrapped device。

## M1.3：打通帧边界和写盘

主要文件：

- `renderdoc/driver/metal/metal_hook.cpp`
- `renderdoc/driver/metal/metal_hook_bridge.mm`
- `renderdoc/driver/metal/metal_core.cpp`
- `renderdoc/driver/metal/metal_device.cpp`
- `renderdoc/os/posix/apple/apple_process.cpp`

工作内容：

- 验证 `CAMetalLayer` 注册、`nextDrawable` 跟踪和 present 路径。
- 验证 UI/快捷键/in-app API 中至少一种 capture trigger。
- 确认 frame scope、thumbnail（可暂缺）和 capture section 正确结束。
- 把 `.rdc` 写入确定的测试输出目录并记录日志。

完成条件：T00 连续三次截帧都产生可被 `CaptureFile` 识别的非空 Metal `.rdc`。

## M1.4：补齐 T00/T01 的 Capture API

只实现两个样例实际调用到的路径：

- Device、queue、command buffer、drawable。
- Buffer 创建与初始内容。
- `newLibraryWithSource`、function 和 render pipeline。
- Render pass descriptor、render encoder、viewport（如使用）、vertex buffer。
- `drawPrimitives`、end encoding、present、commit/wait。

每个 chunk 必须记录对象 ID、关键参数和依赖资源，不能只为了让调用不报错而写空 chunk。

完成条件：structured inspection 能按顺序看到 T00/T01 的完整调用，资源初始内容和 MSL 源码存在。

## M1.5：Capture 文件检查工具

在 replay 尚未完成前，也要能验证 capture 不是“空壳文件”。优先复用 RenderDoc 的 structured
file 读取能力；若 Metal provider 尚未注册导致工具无法读取，则增加一个仅检查 container/section/
chunk 的开发测试入口，但不得另造 capture 格式。

至少检查：

- driver 为 Metal，section version 正确。
- frame capture section 存在且非空。
- device、resource、pipeline、encoder、draw、present chunks 数量符合预期。
- buffer 初始字节与 T01 顶点数组一致。
- library chunk 保存 T01 的 MSL 和入口名称。

## M1.6：自动化与交接

增加脚本完成：构建 fixtures、原生 smoke、执行一次 capture、打印 `.rdc` 路径、运行结构检查。
随后更新 `STATUS.md` 和 `TEST_MATRIX.md`，再开始阶段 2 的 replay provider。

## 为什么不先截完所有样例

生成 `.rdc` 只证明写出了容器，不证明资源生命周期、初始内容或命令参数足以 replay。若先扩大到
几十个 feature，再统一实现 replay，capture 格式错误会在很晚才暴露，返工范围很大。因此采用
逐 feature 纵向闭环：T00/T01 capture + replay 完成后，再扩展纹理、索引、MSAA、compute 等场景。
