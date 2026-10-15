# DOOM: The Dark Ages — Vulkan 光追截帧验证

测试日期：2026-10-15。分支：`codex/doom-vulkan-raytracing`。

## 结论与范围

本机已经捕获包含真实 `vkCmdTraceRaysKHR` 的帧，并在修复加速结构恢复顺序后回放成功，能够导出最终画面。捕获来自标题场景；没有完成实际关卡、长时间运行、其他显卡或其他驱动的验证。

**发行包的普通全局注入启动未通过。** 后续实测确认，不进行下面的独立游戏 RAM 调整时，游戏启动会调用未初始化的 KHR AS 接口而崩溃。`RestoreASInCreationOrder` 只影响回放，不能解决启动问题。此前出包验证覆盖了已有 DOOM 捕获的回放及 Vulkan/D3D11 demo，但漏测撤销实验调整后的 DOOM 全局注入启动；不能将那些通过结果视为 DOOM 开箱截帧通过。

这不是通过一个 RenderDoc 设置就能开箱使用的方案。游戏的能力门槛与 RenderDoc 的回放正确性是两个问题：前者这次通过独立、限时、仅修改进程 RAM 的实验验证，后者有代码修复。进程内实验不写 EXE，不作为默认产品行为提交。

## 分支维护

- `3f95aeedc`：保存原有 18 个文件的未提交改动。
- `74379b741`：撤销其中 Vulkan 7 文件的 RT 绕过，恢复原 capture/replay 前提、AS 跟踪和内存处理。
- `1db315dba`：修复 AS 初始状态排序。
- 后续提交：保存自动回归工具及本记录。

原快照包含 D3D12 日志、路径改名、崩溃服务、shader cache 等临时改动；主干合并审核已撤销它们，详见下面的审核结论。

## 为什么注入后光追变灰

本机 NVIDIA GeForce RTX 5060 Ti、616.56 驱动具备 KHR AS、ray query、RT pipeline 及必要的 AS / shader handle capture-replay 能力。未注入时读取到 RT/PT capability 均为 1；原修改版注入后均为 0。

RenderDoc 隐藏尚不支持捕获的 `VK_NV_mesh_shader` 和 `VK_EXT_opacity_micromap`。本版本游戏在 NVIDIA 分支要求 NV mesh，或 `r_allowRayTracingOnOlderGPUs != 0`，才会允许 RT；PT 还有其他能力与开关条件。这个门槛使 KHR RT 可用的机器在注入后仍被游戏判为不可用。

独立进程内实验设置：`r_allowRayTracingOnOlderGPUs=1`、`r_omms=0`、`r_pathtracingEnabled=1`。得到 RT/PT support=1，游戏启用 KHR RT。普通 `+set` 测试中，`r_allowRayTracingOnOlderGPUs` 和 `r_omms` 未保持目标值；但 `r_pathtracingEnabled` 可以生效，不能笼统说命令行 CVAR 均无效。现有证据未区分前两个值被忽略还是被后续初始化覆盖，也未验证单靠 Steam 启动参数能完成这组启动调整。

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
  -Output build-doom-rt-test\B-normal-rt\my-retest `
  -RestoreASCreationOrder
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

## 双击打开失败的复查

2026-10-15 后续复查：本机 `.rdc` 文件关联为 `RenderDoc.RDCCapture.1`，启动 `C:\Program Files\RenderDoc\qrenderdoc.exe`。它没有上述 AS 排序修复，因此同一文件仍可能停在约 15% 并报 replay 失败。

再次运行自动回归成功，三个 RT 事件通过。启动 `x64/Release/q_render_soco_doc.exe` 并确认它加载的 `renderdoc.dll` SHA-256 与上述修复版一致；GUI 约 42 秒后成功加载，状态栏显示 `doom-forced_frame2026.rdc loaded`，事件树可见 `vkCmdTraceRaysKHR`。GUI 显示 68 个 capture issues，不能视为无警告捕获。

同目录创建 `build-doom-rt-test/B-normal-rt/用修复版打开_frame2026.lnk`，显式启动该修复版并传入捕获路径。未替换官方安装文件或改动 `.rdc` 全局关联。此次为用户查看保留了已加载的修复版窗口。

## 主干合并审核（2026-10-15）

- 恢复 Vulkan 多实例桥接默认开启，保持模拟器等应用的原有功能。
- 撤销全部 VKPROBE/EXITTRACE 临时日志及裸指针探测。
- 撤销配置目录、文件关联查找、临时目录、符号缓存和 DLL 版本资源的无关改名。原有配置继续从 RenderDoc 目录读取。
- 撤销关闭崩溃服务的临时代码。服务启动使用 fork 的 `render_soco_doccmd.exe`；官方及自定义命令工具均避免递归创建服务，ready event 保持与服务端一致。
- 撤销 shader cache 空文件句柄补丁：StreamReader 构造函数已经处理 NULL，不能用该补丁的注释推断原代码必然空指针崩溃。
- 保留一致的 AS 类型排序：AS 在普通资源之前，BLAS 在 TLAS/generic/无 metadata 之前。
- 同类 AS 的创建顺序仅通过 `Vulkan_Hack_RestoreASInCreationOrder` 启用，默认 false，仅作用于 replay，捕获序列化不使用该启发式。设置后重新打开捕获；本记录中的 DOOM 光追文件需要开启此项。
- Settings → Core → Config Editor → Vulkan → Hack → RestoreASInCreationOrder 可编辑该兼容项。无需游戏名称或版本判断；不暴露未支持的 NV mesh/OMM。
- 游戏精确版本探测只保留在独立测试脚本，未加入注入运行时。
- 修正自定义回放器的同目录/x86 上级目录查找。注册表查找仍保留原有 RenderDoc 文件关联。

Windows 出包继续使用 `scripts/build_win.bat`，委托 PowerShell 执行。`-Rebuild` 使用两架构完整 Release 重建和已验证的 LTCG 回退；`-PackageOnly` 用于构建成功后的独立出包。旧产物移至本次日志目录，不随意删除。包按运行依赖清单生成，含 Python 回放模块、两套 Agility SDK、x86 注入依赖和已有 Android APK；APK 不在此 Windows 构建中重编。包检查两架构 CLI 的完整源码 commit，并逐文件解压和比较 SHA-256。

本次审核不承诺全部游戏兼容，也不表示 NV mesh/OMM 或光追 shader 单步调试已实现。光追回归以真实 TraceRays 事件和最终导出图像为依据。

## 发行包全局注入启动崩溃复查（2026-10-15）

用户安装 `7b6b75e169d6a8391db5a8e642781e8f051d05a1` 发行包后启动崩溃。再次比较安装文件与包 manifest，38 个运行文件的哈希均一致；游戏加载的是 `C:\RenderSocoDoc\system_load.dll`，没有重复加载 `renderdoc.dll`。此次没有修改或重新构建运行时，也没有推送远端。

两份用户 BugSplat 转储及一次不修改内存的启动复现均为执行空地址的 `0xc0000005`，返回地址 `DOOMTheDarkAges+0xc89688`。对应调用是游戏 RVA `0xc89682` 间接调用 RVA `0x494bf88` 中的 `vkGetAccelerationStructureBuildSizesKHR` 指针，用于分配 TLAS；不是加速结构初始状态回放阶段的设备丢失。

只读观察显示，约 6.46 秒 RT/PT 标志变为 0，约 6.84 秒 Vulkan device 非空，但 create/build/size AS 指针始终为 0，约 11.34 秒游戏退出。反汇编确认 RVA `0xc1ab31` 检查 RT 标志，非 1 时跳过整组 RT 入口获取。因此不能把此次空指针归因于 `vkGetDeviceProcAddr` 返回 NULL：游戏没有执行对应查询。结合前面的 NVIDIA 能力门槛分析，NV mesh 扩展被过滤会让本版本游戏关闭 RT 初始化，后续却继续进入 TLAS 分配。

使用同一安装 DLL、同一全局注入配置的独立 RAM 对照重新设置 `r_allowRayTracingOnOlderGPUs=1`、`r_omms=0`、`r_pathtracingEnabled=1`，RT/PT 保持 1，AS 入口指向 RenderDoc 自身的 `hooked_vkCreateAccelerationStructureKHR` / `hooked_vkGetAccelerationStructureBuildSizesKHR`，观察 40 秒未复现该启动崩溃，随后可连接 target control。此次没有新捕获，不作为新一轮完整 capture/replay 或关卡稳定性通过；日志中仍有已有的 map range assertions。

同组对照未分别隔离三个 CVAR，不能据此宣称其中某一个单独足以解决全部启动问题。RAM 实验仍不属于产品功能，不写 EXE。测试已停止自建游戏与调试器进程，并按哈希恢复本地游戏配置和 profile；原有 GUI 与全局注入 helper 保留。

后续产品适配应提供真实支持的能力路径：实现所需扩展的捕获/回放，或找到游戏公开支持的 KHR 回退设置。单独增加“返回非 NULL”或无条件暴露未支持的 NV mesh/OMM 不能完成适配，也不符合主干兼容性要求。

证据目录：`build-doom-startup-review/20261015-154211/`。包含用户转储、`cdb.log` / `previous-cdb.log` / `reproduced-cdb.log`、`proc-xrefs.txt`、`rt-api-condition.txt`、`observed-startup.jsonl`、`ram-control.jsonl`、`wrapper-symbols.log` 与 `startup-and-control.log`。该目录及进程实验脚本均忽略，不进入发行包。

## 命令行参数的范围复查（2026-10-15）

用户指出 DeepSeek 曾通过 Steam 的 `cmd /c "%command% +set r_pathtracingEnabled 0 +set r_pathtracingDenoiserMode 0"` 获得有效结果。关闭路径追踪参数的效果不能代替开启 RT 所需能力路径的验证；也不能据此前测试将所有 CVAR 判为不支持命令行。

使用当前安装包，直接通过 `cmd /d /s /c` 启动相同 EXE，传入 `+set r_allowRayTracingOnOlderGPUs 1 +set r_omms 0 +set r_pathtracingEnabled 1`。此轮不经过 Steam UI，没有修改 Steam launch options 或游戏内存。只读观察约 3.18 秒 `r_pathtracingEnabled=1`，但 older=0、omms=1；约 8.32 秒 device 已创建、RT/PT=0、AS 入口仍为空，约 13.76 秒退出。转储仍为 `DOOMTheDarkAges+0xc89688` 返回的空指针执行异常。

结论：`+set` 确实能够设置部分 CVAR，当前测试未能通过命令行完成另外两个启动调整。`cmd /c` 用于包装启动，不会自行改变游戏对 CVAR 的处理。本轮未单独测试 DeepSeek 的 denoiser 参数，也不否定旧修改版下该组关闭 PT 参数有效的观察。测试已恢复并校验游戏配置和 profile；运行时与 Steam 设置未修改。证据位于 `build-doom-startup-review/20261015-commandline/`。
