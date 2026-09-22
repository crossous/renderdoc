# 阶段 8 细化计划：T07 depth/stencil

T06 已关闭 MRT、多输出 action、逐 attachment blend state 和标准 OM 表。下一条 P1 纵向切片固定为
T07 depth/stencil：在 T02 已有 Depth32Float/less/write 基础上加入可观察的 stencil attachment、前后
面状态与事件结果，并继续复用标准 Output Merger 和 Texture Viewer，不建立 Metal 专用旁路。

当前状态：P8.1-P8.4 已完成。T07 已进入与 T00-T06 相同的 Native -> Capture -> RDC inspect ->
seekable Replay -> 标准 UI/export -> lifecycle 闭环；下一纵向切片转入 T08 MSAA resolve。

## P8.1：建立确定性 T07 fixture

- 新增 `Metal_Depth_Stencil`，使用固定尺寸 `Depth32Float_Stencil8` attachment 和单一 color target。
- 用 clear、stencil write 和受 stencil/depth test 约束的后续 draw 形成可区分区域；固定 compare、read/
  write mask、reference、front/back operations 与 depth write 行为。
- 保持单采样、单 render pass 和直接 draw，不混入 MSAA、resolve、texture sampling 或 compute。

验收：已通过。fixture 使用 `Depth32Float_Stencil8`，五条 draw 分别建立左右 stencil mask、通过
depth/stencil 的绿/蓝输出和一次明确的 depth fail；最终左/右采样像素为 `10df30`/`1840ff`。

## P8.2：capture/replay 与事件状态

- 补齐 T07 使用的 combined depth/stencil texture format、render-pass attachment 和 stencil descriptor
  序列化/重建。
- replay depth/stencil clear、reference value、front/back compare 与 fail/depth-fail/pass operations。
- 在 clear、stencil write、受测 draw 之间往返，确保 attachment 与 color 结果恢复到对应 event。

验收：已通过。structured XML 核对 combined format、front/back operations/masks、一次双 reference
与五次单 reference；五个 draw action 的 `depthOut`、事件图像、回退重放和动态 reference snapshot
均由 output smoke 自动断言。

## P8.3：通用状态与标准 UI

- 扩展 `MetalPipe::State`、通用 `DepthTestState`/stencil 查询，保留 front/back、mask/reference 与
  operations 的真实值。
- OM 页按 D3D/Vulkan 的 Depth Target、Depth State、Stencil State 信息组织补齐列与空槽行为；资源
  跳转、Texture Viewer 和 HTML export 继续走公共路径。
- 对无法由当前 capture 证明或当前格式读取不了的值明确标记限制，不从 color 结果反推伪状态。

验收：已通过。通用 `DepthTestState`/`StencilFace`、Metal snapshot/proxy serialization 与标准 OM
`Depth Target`、`Depth State`、`Stencil State` 共用同一数据；最终 HTML export 包含 Texture 20、
Less/Write Enabled、Front/Back reference 5、`000000FF/00000000` masks 与 `Inc Sat/Dec Sat`。

## P8.4：自动回归与 qrenderdoc 实机验收

- 将 T07 native/capture/XML/replay/state/event seek/lifecycle 并入一键脚本，T00-T06 不退化。
- 启动最终 qrenderdoc，核对 Event Browser、Texture Viewer、OM depth/stencil 表、资源跳转、HTML export
  与状态栏。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 与 `HANDOFF.md`。

验收：已通过。完整 T00-T07 一键回归、八份 capture 各 10 次 lifecycle、八份三轮 CLI replay 与
`git diff --check` 通过；lifecycle resident growth 为 524,288 字节。最终 qrenderdoc 在 T07 EID 6
显示左绿右蓝输出和正确 depth/stencil OM 表，UI 实际导出
`captures/metal-smoke/t07_pipeline_state_standard.html`，状态栏为 `No problems detected` 并保持运行。

## 阶段关闭记录

- 新增 `Metal_Depth_Stencil`，覆盖 combined attachment、clear、front/back descriptor、单/双 dynamic
  stencil reference 与 depth-fail 事件语义。
- render-pass clear action 会标记 `ClearDepthStencil`，draw action 保存真实 depth output；只有 stencil
  target 时也能建立 render-pass depth/stencil 输出关系。
- Metal Pipeline OM 继续沿用标准分组与 RDTree；共享 `PipelineFlowChart` 新增焦点及
  Left/Right/Home/End 导航，鼠标与键盘都切换同一隐藏阶段页。
- depth/stencil texture 的通用像素读取仍不在本切片内；颜色结果、状态、action output 和 GPU replay
  已提供足够且互相独立的证据，不伪造 depth/stencil texel。

## 当前明确不在本切片内

- MSAA/resolve、mip/cube/array、memoryless attachment。
- depth/stencil texture 的通用浮点/整数可视化全部模式；只承诺 T07 自动测试与标准 UI 所需子集。
- depth bounds、raster order group、programmable blending、tile/imageblock。
- compute、blit、argument buffer 和 indirect command。
