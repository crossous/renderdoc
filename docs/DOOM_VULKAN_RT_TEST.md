# DOOM: The Dark Ages — Vulkan 光追截帧验证

测试日期：2026-10-15。分支：`codex/doom-vulkan-raytracing`。

## 结论与范围

本机已经捕获包含真实 `vkCmdTraceRaysKHR` 的帧，并在修复加速结构恢复顺序后回放成功，能够导出最终画面。捕获来自标题场景；没有完成实际关卡、长时间运行、其他显卡或其他驱动的验证。

这不是通过一个 RenderDoc 设置就能开箱使用的方案。游戏的能力门槛与 RenderDoc 的回放正确性是两个问题：前者这次通过独立、限时、仅修改进程 RAM 的实验验证，后者有代码修复。进程内实验不写 EXE，不作为默认产品行为提交。

## 分支维护

- `3f95aeedc`：保存原有 18 个文件的未提交改动。
- `74379b741`：撤销其中 Vulkan 7 文件的 RT 绕过，恢复原 capture/replay 前提、AS 跟踪和内存处理。
- `1db315dba`：修复 AS 初始状态排序。
- 后续提交：保存自动回归工具及本记录。

原快照中的 D3D12 日志、路径改名、崩溃服务、shader cache 等改动仍保留，未由此次 Vulkan 测试证明其通用正确性。尤其 `if(true) return` 关闭崩溃服务属于诊断能力变化，不应称为普通通用修复。

## 为什么注入后光追变灰

本机 NVIDIA GeForce RTX 5060 Ti、616.56 驱动具备 KHR AS、ray query、RT pipeline 及必要的 AS / shader handle capture-replay 能力。未注入时读取到 RT/PT capability 均为 1；原修改版注入后均为 0。

RenderDoc 隐藏尚不支持捕获的 `VK_NV_mesh_shader` 和 `VK_EXT_opacity_micromap`。本版本游戏在 NVIDIA 分支要求 NV mesh，或 `r_allowRayTracingOnOlderGPUs != 0`，才会允许 RT；PT 还有其他能力与开关条件。这个门槛使 KHR RT 可用的机器在注入后仍被游戏判为不可用。

独立进程内实验设置：`r_allowRayTracingOnOlderGPUs=1`、`r_omms=0`、`r_pathtracingEnabled=1`。得到 RT/PT support=1，游戏启用 KHR RT。普通命令行和本地配置对该零售版受限制 CVAR 没有效果；不能把普通 `+set` 描述为已经验证可用的办法。

用于只读检查的脚本要求精确 EXE SHA-256：
`b7aece5fe2842035efd9036a1049edd53ee5efa25564811e20c39ca18362c018`。

## 真实捕获与失败定位

文件：`build-doom-rt-test/B-normal-rt/doom-forced_frame2026.rdc`，4,814,311,009 字节。
SHA-256：`6d6f1bdce538654731221ba36e448e69d29d210869c675751b658c5ef5878658`。

| 捕获中的调用 | 数量 |
|---|---:|
| vkCreateAccelerationStructureKHR | 1,170 |
| vkCreateRayTracingPipelinesKHR | 1 |
| vkCmdBuildAccelerationStructuresKHR | 4 |
| vkCmdCopyAccelerationStructureKHR | 6 |
| vkCmdTraceRaysKHR | 3 |

修复前，匹配捕获代码的 replay DLL 在初始 AS 恢复阶段报 `VK_ERROR_DEVICE_LOST`。逐结构提交审计定位到 TLAS `ResourceId::152775`：21 个实例、1,536 字节输入。官方安装版 1.46 也无法回放该文件。

实例 0 引用 `0xe3a5200000`。同一缓冲区 `ResourceId::152771` 的偏移 0 被多个 BLAS 复用：`188883`、`189089`、`189283`、`189497`，后续还创建了 `190388`。捕获初始状态中前四个有数据；恢复日志显示 `189497` 先恢复，`188883` 后恢复。这会让旧 BLAS 覆盖当前内容。

`VulkanResourceManager::InitialContentResources()` 原比较函数只保证 BLAS 在 TLAS 前，同类 AS 没有稳定顺序。修复保留 BLAS/TLAS 分组，同时按 ResourceId 升序恢复同类 AS，让较新创建的结构最后覆盖复用存储；generic 或无 metadata 统一排入 TLAS 组，保证比较关系一致。

该修复解决了本捕获的设备丢失。它不是对所有 AS 别名生命周期的完整解决方案：如果应用以后重新构建较旧的句柄，创建顺序未必代表最终写入顺序，需要跟踪实际 GPU 写入时间。

## 已执行验证

- x64 Release `renderdoc.vcxproj` 与 `renderdoc_no_export.vcxproj` 构建成功；按构建文档关闭跨模块 LTCG。
- 审计版回放成功，约 9.9 秒，导出最终画面。
- 去除临时 `RTTEST` 与环境审计代码后，正常回放成功。
- 带 Vulkan validation 的回放成功；存在屏障 access/stage 与 `ResourceId::4840` 图像布局错误，未宣称 validation 无错误。未观察到 AS/TraceRays 相关 VUID。
- 分别回放 RT 事件：1252（256×1×1）、11347（768×432×1）、11362（3840×2160×1），再回放 present 13004 并导出 PNG。
- 自动工具要求进程退出码 0、报告 `complete=true`、至少一个 RT 事件；失败时保存日志，超时只停止本次创建的 host，拒绝复用旧报告。
- 最终正常批量提交复测：`single_submit=false`、`api_validation=false`，三个 RT 事件与最终 PNG 均成功，回放初始化约 16.9 秒，host 退出码 0。

最终测试产物 SHA-256（提交前构建，未发布）：

- `x64/Release/renderdoc.dll`：`ed098e61dbd93337d08bbf7e2de5bc9dfaafc69cb532e446d747f3683bf0d342`
- `x64/Release/system_load.dll`：`c7b1dbf0746e6a8354ea59489d5a3dd9102aa691909917eb508e4a98bc0d20fb`

原生 VK_Ray_Query demo 短跑成功，但未完成该 demo 的注入截帧对照，因此不列为 capture/replay 通过。

## 自动复测

在仓库根目录运行，使用新输出目录：

```powershell
& scripts\test_doom_rt.ps1 `
  -Capture build-doom-rt-test\B-normal-rt\doom-forced_frame2026.rdc `
  -Output build-doom-rt-test\B-normal-rt\my-retest
```

加 `-Validate` 启用 Vulkan validation；可用 `-Python`、`-Runtime`、`-TimeoutSeconds` 指定运行时与时限。该命令只回放已有文件，不启动游戏或改动全局注入。报告记录 single-submit 选项；默认明确关闭，退出前恢复原值。

`doom_rt_python.py` 用 x64 Python 3.14 托管构建产物附带的 Python 3.6。先导入带 replay marker 的 `renderdoc.pyd`，随后验证实际加载的 DLL，避免误加载仓库根目录旧 DLL。系统 `rdc doctor` 没有匹配 Python 模块，因此本次使用该托管方式。

主要本地证据：

- `build-doom-rt-test/native-capabilities.json`
- `build-doom-rt-test/A-existing/capabilities.json`
- `build-doom-rt-test/B-normal-rt/capabilities-final.json`
- `build-doom-rt-test/B-normal-rt/metadata-audit/renderdoc.log.retained`
- `build-doom-rt-test/B-normal-rt/as-refs.json`
- `build-doom-rt-test/B-normal-rt/final-confirmed/replay-report.json`
- `build-doom-rt-test/B-normal-rt/regression-final/replay-report.json`
- `build-doom-rt-test/B-normal-rt/regression-batched/replay-report.json`

大捕获文件、进程内实验和生成结果位于 ignored 目录，不提交到 Git。

## 对 DeepSeek 理由的评审

已读取用户提供的飞书文档 revision 3 与粘贴文本。

1. 本机 capture-replay feature 不满足的推断已被实测否定；不应无条件暴露原本被过滤的扩展。
2. 关闭 `m_AccelerationStructures` 及相关内存处理会破坏真实 AS 捕获前提，不能作为完整 RT 支持方案。此次成功捕获使用恢复后的正常路径。
3. `vkGetDeviceAccelerationStructureCompatibilityKHR` 询问序列化 AS blob 的兼容性，不是一般 RT 能力。RenderDoc 返回 INCOMPATIBLE 让应用重建；转发 COMPATIBLE 可能诱使应用走 RenderDoc 不支持的反序列化路径。
4. 返回驱动裸入口和“回填指针”不能自动让调用进入捕获路径；设备尚未启用 RT 时 NULL 与游戏能力门槛必须分开诊断。这次正常 KHR RT 捕获未依赖那组额外 GIPA/GDPA 绕过。
5. RenderDoc 的 RT 支持是记录、回放黑盒工作；不等于光追 shader 单步调试或完整 RT pipeline 检查。仓库文档关于 NVIDIA 驱动缺少功能的提示不能代替本机 feature 查询。

## 本机恢复

测试结束恢复并按 SHA-256 校验 installed `system_load.dll`（`eadebb7512805d25564834195b119a855214c27dd5553743b68307496e90f300`）、manifest、SystemLoad 与 RenderDoc 配置、游戏 local config 和 profile settings；两套 LoadAppInit_DLLs 恢复备份值 1，AppInit_DLLs 字符串保持原样。保留用户原有 GUI 与全局注入 helper；没有残留本次游戏、demo 或 replay host。
