# 阶段 10 细化计划：T09 mip/cube/array 子资源

T08 已关闭 multisample attachment、显式 resolve、sample state、事件回放和标准 OM/Texture Viewer
路径。下一条 P1 纵向切片固定为 T09 mip/cube/array：先建立少量、可精确识别的纹理子资源，再扩展
枚举、上传、采样、读取和标准 Texture Viewer 的 mip/slice/face 选择。

当前状态：P10.1-P10.4 已完成。12 个可区分子资源、slice-aware upload、readback/pick/display、
通用 Pipeline 绑定与标准 Texture Viewer 均已验证；T00-T09 完整回归通过。

## P10.1：建立确定性 T09 fixture

- 新增包含 mipmapped 2D、2D array 和 cube texture 的最小 fixture，各 mip/slice/face 使用不同固定色块。
- 用独立屏幕区域采样三类资源，保证最终输出能定位到具体子资源。
- 不混入 compressed、3D、texture view、blit mip generation、compute 或 writable texture。

验收：未注入运行稳定，所有采样区域与预期子资源颜色一致。

## P10.2：capture/replay 与子资源数据

- 补齐 array/cube descriptor、各 mip/slice/face 的 `replaceRegion` 内容与 fragment binding replay。
- 扩展受支持格式的 `GetTextureData()`/`PickPixel()` 子资源索引，严格校验越界和未支持组合。

验收：structured XML、GPU replay、原始 texel 和逐子资源像素均与 fixture 一致。

## P10.3：通用状态与标准 UI

- 将 texture type、mip count、array size、cube face 与绑定类型接入通用 descriptor/PipeState。
- 使用标准 Texture Viewer 的 mip/slice/face 控件和保存路径，不增加 Metal 专用子资源查看器。

验收：Pipeline State、Texture Viewer、Resource Inspector 与保存结果使用同一子资源语义。

## P10.4：自动回归与 qrenderdoc 实机验收

- 将 T09 native/capture/XML/replay/readback/pick/state/lifecycle 并入一键脚本，T00-T08 不退化。
- 启动最终 qrenderdoc，检查子资源切换、最终图像、Pipeline binding、保存与状态栏。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 与 `HANDOFF.md`。

验收：完整 T00-T09 回归通过；最终 qrenderdoc 状态栏为 `No problems detected` 并保持运行。

## 收口记录（2026-09-23）

- P10.1：`Metal_Texture_Subresources` 原生运行稳定；3-mip 2D、3-slice 2D array、六面 cube
  的 12 个固定色子资源形成 12 条独立屏幕色带。
- P10.2：slice-aware `replaceRegion` chunk、三类 descriptor、fragment texture binding 和 GPU
  replay 已接通。structured XML、逐子资源原始字节、六面 cube pick、越界拒绝和输出色带通过。
- P10.3：通用 FS descriptor 显示 Texture 17/18/19 与 Sampler 20；标准 Texture Viewer 显示
  mip0/1/2、slice0/1/2、cube face，并从标准 Save Texture 对话框导出全部 face 的 DDS。
  macOS 26 上 Qt 5 的 popup 崩溃由 D032 的点击循环/键盘选择兼容处理避开。
- P10.4：`test_metal_capture_macos.sh` 最新完整 T00-T09 回归通过（日志
  `/tmp/t09-final-regression-v3.log`）；十份 capture 各 10 次生命周期增长 475,136 字节；
  T00-T09 CLI replay 均通过。最新 qrenderdoc 的 EID 2 子资源切换、Pipeline 绑定、512-byte
  `captures/metal-smoke/t09_cube_ui.dds` 保存与 `No problems detected` 均已实机核对。
- 下一阶段入口：`PHASE11.md` P11.1 / T10 buffer/texture blit。

## 当前明确不在本切片内

- compressed/depth/stencil/integer/float texture readback 与 texture view。
- 3D texture、memoryless、sparse texture、custom sample position。
- blit mipmap generation、compute、argument buffer 和 indirect command。
