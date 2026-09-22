# 阶段 7 细化计划：T06 MRT 与 blending

T05 已关闭多 vertex buffer、base instance 和标准 Mesh/Buffer Viewer 链路。下一条 P1 纵向切片固定为
T06 MRT + blending：用两个 color attachment 和逐 attachment blend descriptor 扩展 render-pass
输出、pipeline snapshot、通用 OM 查询与标准 Pipeline 页面。布局继续以 RenderDoc 现有
IA/VS/RS/FS/OM 信息架构为边界，不增加 Metal 专用查看器。

当前状态：2026-09-22 已完成。T06 已进入一键回归，并完成最新 qrenderdoc 的双输出、OM Blend
State、Texture Viewer 与 HTML export 实机验收。下一条纵向切片见 `PHASE8.md`。

## P7.1：建立确定性 T06 fixture

- 新增 `Metal_MRT_Blend`，创建两张固定尺寸、可读回的单采样 color render target。
- fragment shader 通过两个 color output 写出不同固定颜色；至少一个 attachment 启用显式 alpha
  blending，另一个保持 blending disabled，形成可区分的参考结果。
- 使用两条 draw 产生确定性的覆盖区与非覆盖区，固定 attachment format、load/store/clear 和 blend
  factors/operations/write mask。
- 不引入 MSAA、depth/stencil、纹理采样或外部资产，避免把 T07/T08 范围混入本切片。

验收：已通过。fixture 使用 BGRA8 drawable 与 shared RGBA8 第二附件、240-byte Float2/Float4/Float4
顶点流和两条 draw；未注入运行稳定，clear、第一条 draw 和重叠区像素均有固定 CPU 参考值。

## P7.2：capture/replay 与多输出事件

- 补齐 T06 使用的 color attachment descriptor、blend enable、RGB/alpha factor/operation 和 write mask
  序列化/重建。
- render pass 与 draw action 保存所有真实 color outputs，不再只暴露 slot 0。
- event-range replay 在 clear、draw 1、draw 2 之间往返时，两张 attachment 都恢复到对应事件内容。

验收：已通过。修复 color attachment capture 误把 `sourceAlphaBlendFactor` 当作
`sourceRGBBlendFactor` 的错误；structured XML 记录 `SourceAlpha/OneMinusSourceAlpha`、两个 write
mask 和 240-byte 顶点数据。clear/draw action 均暴露 slot 0/1，event seek 对两张 texture 的关键
RGBA 像素逐项通过。

## P7.3：通用 OM state 与标准 Pipeline UI

- 扩展 `MetalPipe::State` 和通用 `PipeState` 查询，表达每个 color attachment 的 format、resource、
  blend enable、source/destination factor、operation 与 write mask。
- Metal Pipeline 的 OM 页使用标准 RDTree/列组织显示多 attachment 与 blend state，并保持资源上下文、
  Texture Viewer、thumbnail 和 HTML export 路径与其他后端一致。
- 对照 D3D11/Vulkan 页面检查字段顺序、空槽、紧凑布局与键盘选择；无法从 capture 证明的字段不显示
  伪值。

验收：已通过。`MetalPipe::State` 与 `PipeState::GetColorBlends()` 暴露两个 attachment 的独立状态；
OM 页显示 slot 0 `Src Alpha / 1 - Src Alpha / Add / RGBA` 与 slot 1 `False / RGB_`。Texture Viewer
Outputs 可在 FB0/FB1 间切换，导出的 `captures/metal-smoke/t06_pipeline_state_standard.html` 包含同一状态。

## P7.4：自动回归与 qrenderdoc 实机验收

- 将 T06 native/capture/XML/replay/readback/state/lifecycle 并入一键脚本，T00-T05 不退化。
- 对两张 attachment 的 event seek、raw bytes、blend 结果、descriptor 和 output list 增加自动断言。
- 启动最终 qrenderdoc，核对 Event Browser、Texture Viewer outputs、OM attachment/blend 表、资源跳转、
  HTML export 与状态栏。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 与 `HANDOFF.md`。

验收：已通过。完整 T00-T06 一键回归通过；7 份 capture 各 10 次 lifecycle 的 resident growth 为
1,015,808 字节，七份 capture 的 `renderdoccmd replay --loops 3` 全部成功。最终 qrenderdoc 保持运行
在 T06 EID 3 OM 页，状态栏为 `No problems detected`。

## 当前明确不在本切片内

- depth/stencil、MSAA/resolve、mip/cube/array。
- independent blend 的所有极端 factor/operation 组合；只承诺 fixture 覆盖且有像素证据的子集。
- logic op、tile/imageblock、memoryless attachment 和 programmable blending。
- compute、blit、argument buffer 和 indirect command。
