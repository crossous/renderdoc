# 阶段 9 细化计划：T08 MSAA resolve

T07 已关闭 combined depth/stencil attachment、front/back stencil state、dynamic reference、事件回放和
标准 Output Merger 展示。下一条 P1 纵向切片固定为 T08 MSAA resolve：以最小 multisample color
attachment 和显式 resolve 建立可观察结果，再扩展 sample count、resolve output 与纹理子资源语义。

当前状态：P9.1-P9.4 已完成，T08 纵向切片关闭。

## P9.1：建立确定性 T08 fixture

- 新增单 render pass 的 MSAA color fixture，固定 sample count、clear、draw 和 resolve texture。
- 输出图案必须能区分“只 clear”“已 draw”“已 resolve”，并保留单采样参考区域。
- 不混入 mip/cube/array、memoryless、compute 或自定义 sample positions。

验收：未注入运行稳定，resolve 后单采样纹理有确定性像素参考。

## P9.2：capture/replay 与事件状态

- 补齐 multisample texture descriptor、render-pass resolve attachment、store/resolve action 与 sample
  count 的 capture/replay。
- 在 clear、draw 和 end-pass/resolve 事件间往返，确认 resolve 发生时机与 action outputs 一致。

验收：structured XML、GPU replay、event seek 与 fixture 一致。

## P9.3：通用状态与标准 UI

- 将 sample count、MSAA color target 和 resolve target 接入 Metal snapshot、通用 PipeState 与标准
  Pipeline/Texture Viewer 路径。
- 对当前不支持的逐 sample 查看、MSAA texture readback 或 remap 明确显示限制，不伪造 resolved 数据。

验收：Pipeline State、Texture Viewer 与 HTML export 显示同一 attachment/sample/resolve 关系。

## P9.4：自动回归与 qrenderdoc 实机验收

- 将 T08 native/capture/XML/replay/state/event seek/lifecycle 并入一键脚本，T00-T07 不退化。
- 启动最终 qrenderdoc，检查 Event Browser、Texture Viewer、Pipeline OM、HTML export 与状态栏。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md` 与 `HANDOFF.md`。

验收：完整 T00-T08 回归通过；最终 qrenderdoc 状态栏为 `No problems detected` 并保持运行。

## 完成记录（2026-09-22）

- 新增 `Metal_MSAA_Resolve`：4x BGRA8 multisample attachment 显式 resolve 到 drawable，三条 draw
  形成左红、右蓝和中央绿色叠加三角形。
- capture 已保存并重建 `TextureType2DMultisample`、texture/pipeline sample count、resolve texture、
  `StoreActionMultisampleResolve` 与 alpha-to-coverage；draw/clear action 的可显示 output 指向真实
  单采样 resolve texture。
- Metal snapshot 新增 sample count、alpha-to-coverage/one 与 resolve target；OM 按标准语义拆为
  Multisample State、Color Targets 和 Resolve Targets，资源激活与 HTML export 复用公共路径。
- 自动回归验证 clear、三条 draw 与 rewind 的 resolve 像素、216-byte vertex buffer、attachment 类型
  和 action output。T00-T08 各 10 次 lifecycle resident growth 为 376,832 字节，九份 capture 的
  三轮 CLI replay 均通过。
- 最终 qrenderdoc 在 EID 4 显示 `Texture 17 / Texture 2D MS / 4 samples` 和
  `Texture 24 / Texture 2D / 1 sample`；Resolve Targets 可直接进入 Texture Viewer，中心拾取为
  `(0.06275, 0.87451, 0.18824, 1.00)`。导出的
  `captures/metal-smoke/t08_pipeline_state_standard.html` 与页面一致，状态栏为
  `No problems detected`，进程保持运行。

## 当前明确不在本切片内

- mip/cube/array/3D、memoryless attachment 和自定义 sample positions。
- 通用逐 sample 像素读取、sample-frequency shader、programmable sample position。
- compute、argument buffer、indirect command 和 tile/imageblock。
