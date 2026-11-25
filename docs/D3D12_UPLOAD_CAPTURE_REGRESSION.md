# D3D12 上传捕获与回放：当前实现和回归

2026-11-25。本文描述整理后的正式修改；历史诊断和旧补录不属于当前实现。
修改不识别游戏名、shader hash、资源 ID 或常量布局。独立 GPU 回归覆盖合法 API
用法和明确标记的可选兼容用法。游戏诊断文件及大截帧仅保存在本地 analysis 中。

## 保留的通用修复

### 合法嵌套 Map 的计数

D3D12 支持嵌套 Map，也允许 `ppData=NULL`。原 wrapper 只对返回指针的 Map
计数，却对每次 Unmap 减计数，导致混合 NULL/非 NULL 的合法嵌套提前停止追踪。
以下两种顺序已独立复现并修复：

1. Map 返回指针 → Map 不请求指针 → Unmap 一次 → 通过仍有效的指针更新并提交。
2. Map 不请求指针 → Map 返回指针 → Unmap 一次 → 通过仍有效的指针更新并提交。

现在所有成功 Map 都计数，第一次取得指针时注册提交追踪，最后一次 Unmap 才注销。
失败 Map 不改变计数；纯 NULL 映射不做空指针快照。原生 Map/Unmap 与 wrapper
计数由同一资源锁保护。RE9 检查过的两只常量 buffer 未使用这种混合嵌套，不能
将这个独立工具缺陷当作该游戏遗漏更新的原因。

### 保留 UPLOAD 映射的可选支持

`D3D12.Capture.RetainUploadMappings` 默认关闭。开启后，应用成功取得普通 UPLOAD
buffer 的 CPU 指针时，工具增加一层原生嵌套 Map；应用计数独立。应用最后一次
Unmap 后仍保留正常提交追踪，资源销毁时释放工具引用及 shadow。

它支持已定位的“应用 Unmap 后仍使用缓存指针写入”生命周期依赖，沿用正常
persistent-map flush，不额外扫描从未返回过映射指针的资源。适用范围仅限普通
UPLOAD buffer，未扩大到 GPU_UPLOAD、CUSTOM heap 或 texture。保留 CPU 地址
和映射追踪可能增加捕获耗时及内存用量。

### 64 KiB 分页差分与 Unmap 冗余省略

提交快照和最后一次 Unmap 复用同一差分函数：按 64 KiB 页找不同字节，合并
相邻变化页，省略未变化页；没有 shadow 时仍完整保存，最后不足一页也比较。
不依赖应用可能填写不准确的 `pWrittenRange`。

shadow 从 `Serialise_MapDataWrite` 实际序列化的字节更新，避免再次读取活动指针
而将未保存的写入误认为已保存。资源、偏移、顺序及 chunk 格式保持兼容。

严格大小对照中，2 MiB+17-byte buffer 两次相距很远的 DWORD 修改，原单区间
保存 2,031,602 bytes，分页后保存 2+2 bytes，GPU 输出一致。首次仍保存完整的
2,097,169 bytes。证据在 `analysis/d3d12-upload-general/map-fix-size-audit/`
和 `paged-replay/`。节省的是写入数据和对应回放上传缓存；仍扫描完整映射，
shadow 仍与映射等大，未建立 shadow 的短期 Map/Unmap 仍可能完整保存。

### Discard 图案的尾部和缓存

旧实现向上对齐分配，但只初始化原资源长度；较大的资源复用同一分配时可能
读到未初始化尾部。现在使用一个完整填充的 1 MiB source，分段复制，末段精确
截取。六种大小的逐字节 GPU 校验全部通过，旧版三项尾部复用失败。

测试中的旧缓存合计 10,487,808 bytes，新缓存为 1,048,576 bytes。资源本体大小
不变，大 buffer 会增加复制命令。UPLOAD 内存节省不能直接等同于显存节省。
可选 `SkipDiscardPattern` 在共同入口省略附加填充，覆盖 DiscardResource 和
enhanced barrier 的 UndefinedTransition；后者尚无本轮独立 GPU 开关对照。

### GPU_UPLOAD 初始状态和故障处理

初始状态恢复的三个 CPU 路径补上 GPU_UPLOAD 分类。该分类只描述 CPU 上传
访问，保留 GPU_UPLOAD 的正常状态转换，未套用普通 UPLOAD 固定 GENERIC_READ
的限制。没有修改游戏查询 feature 的结果。

本机 RTX 5060 Ti / Windows 10 19045 的 OPTIONS16 返回
`GPUUploadHeapSupported=false`。清理后的 fixture 能力检查退出码为 5，实际
创建 GPU_UPLOAD 的 COMMON/GENERIC_READ 两种状态均返回 E_INVALIDARG。
这属于无法进行硬件回归，不能宣称 GPU_UPLOAD 已测通。能力证据在
`analysis/re9/release-cleanup/gpu-upload-support.log`，此前系统/runtime 核查在
`analysis/d3d12-upload-general/gpu-upload-capability/`。

同事原帧和检查过的本机 RE9 帧均无 GPU_UPLOAD，它不解释这些样本的故障。
官方支持记录见 [RenderDoc #3724](https://github.com/baldurk/renderdoc/issues/3724)。

此外保留 DRED 空节点、队列名称/对象、context 边界及接口生命周期修复，以及
加载失败后直接返回 fatal error、避免创建不完整 dummy driver 引发二次崩溃的修复。

## 当前设置与手动截帧

| 设置 | 默认 | 用途 |
|---|---|---|
| `D3D12.Capture.RetainUploadMappings` | false | 保留普通 UPLOAD 映射并记录后续提交更新 |
| `D3D12.Replay.PreserveUploadHeaps` | false | 避免将大 committed UPLOAD buffer 提升到 DEFAULT，减少这部分显存占用，回放可能变慢 |
| `D3D12.Replay.SkipDiscardPattern` | false | 省略工具附加的 Discard 诊断填充，仍保留应用调用和 barrier |

手动验证映射保留时，在 `Tools → Settings → Core → Direct3D 12 Capture`
勾选 `Retain UPLOAD buffer mappings`，然后重新启动或注入应用，再截帧。
其余两项位于同一 Core 页的 `Direct3D 12 Replay` 分组，修改后重新加载截帧。
三项均可通过高级配置编辑器设置；关闭编辑器后复选框会刷新，不额外写入配置。
捕获端修改不能补回旧 RDC 已遗漏的字节。

## 通用回归工具与命令

- `scripts/d3d12_upload_regression.cpp`：无窗口原生 GPU 对照和两份连续捕获。
- `scripts/check_d3d12_upload_regression.py`：校验 GPU 整数、事件跳转、Discard 内容和验证消息。
- `scripts/build_d3d12_upload_regression.bat`：编译 fixture。
- `scripts/test_d3d12_upload_regression.ps1`：原生/注入对照，两帧 × 两种回放方式，结束或失败后恢复配置原字节。
- `scripts/inspect_d3d12_heap_types.py`：只读检查截帧中的堆类型。

构建前全文阅读 [CUSTOM_RELEASE_BUILD.md](CUSTOM_RELEASE_BUILD.md)。回放需要可用的
Python RenderDoc runtime；fixture 目录的 `sdk616/` 需有匹配的 `D3D12Core.dll`
和 `d3d12SDKLayers.dll`。可用 `-D3D12Sdk` 指定其他现有目录。每次 Output 必须
是尚不存在的新目录。
测试入口使用 PATH 中的 64 位 `python` 承载仓库的 bundled Python runtime；
可用 `-Python` 指定宿主解释器路径。堆类型检查仅导出结构，不运行 GPU，但大型
截帧的结构化读取仍可能占用较多内存。

```powershell
& scripts/build_d3d12_upload_regression.bat *> analysis/d3d12-upload-general/build-test.log

# 默认路径：合法 API 用例
& scripts/test_d3d12_upload_regression.ps1 `
  -Output D:\Project\CPP\renderdoc\analysis\d3d12-upload-general\my-default-run

# 映射保留：加入明确标记的 Unmap 后写入兼容用例
& scripts/test_d3d12_upload_regression.ps1 `
  -Output D:\Project\CPP\renderdoc\analysis\d3d12-upload-general\my-retained-run `
  -CompatibilityWrites -RetainUploadMappings
```

合法用例覆盖混合 Map 顺序、两指针嵌套、NULL-only Unmap/销毁、失败 Map、
空 written range、四线程 Map、远距离写入及不满页尾部、placed alias、两队列
fence、跨捕获映射、主/工作线程 streaming-store+SFENCE，以及重复事件回放。
兼容用例另外检查 committed/placed buffer 在 Unmap 后的缓存指针更新；这部分
明确依赖更宽映射生命周期，不能作为合法 API 用法的反例。

## 清理后验证和边界

x64 Release 的 D3D12 静态库、`renderdoc.dll`、`system_load.dll`、
`q_render_soco_doc.exe` 及 fixture 编译通过。未进行 x86/Android 构建、打包或发布。

| 清理后用例 | 结果 |
|---|---|
| 默认捕获，两帧 × 默认/保留 UPLOAD 回放 | 4/4 通过 |
| 映射保留和兼容写入，两帧 × 默认/保留 UPLOAD 回放 | 4/4 通过 |

本次提交前再次验证，结果分别在本地
`analysis/d3d12-change-review/final-default-regression/summary.json` 和
`analysis/d3d12-change-review/final-retained-regression/summary.json`。无 High 级验证消息，
只有每项 2–4 条 buffer InitialState 的 Medium 提示。构建日志也保存在
`analysis/d3d12-change-review/`；上述 analysis 证据不随源码提交。

新映射保留此前已使游戏菜单帧的故障常量正确保存，并执行到 Present。另一个
样本的大片画面缺失已独立定位到前向 fence 等待被回放忽略；**本次提交未修复
跨队列依赖调度**。只重排 queue 调用的调查工具不纳入正式源码：它未完整处理
捕获开始前 fence 值、CPU Signal、动态 descriptor 和上传快照的关联。

独立最小 DX12 复现只使用两个队列、同 heap 同偏移的 placed buffer 和 fence。
原生 GPU 输出 111、当前回放输出 222，按 fence 依赖安排后的验证副本输出 111；
原生及回放 API 验证均无错误。这是已知回放限制，不能将 Map 修复或
SkipDiscardPattern 设置描述为它的解决方案。完整游戏场景、cloth、3090、
GPU_UPLOAD 支持环境和所有 alias/并发组合仍待独立验证。

尚未实现短期 Map/Unmap 跨生命周期去重、按显存预算提升资源或 fence 回收
上传缓存。不能把当前用例通过扩大为全部 D3D12 捕获问题已解决。

临时 hook、常量诊断、提交日志、资源编号过滤和旧 unmapped-upload 全量补录
已经撤出正式代码。清理前源码、diff、实验工具及诊断二进制保存在本地
`analysis/re9/release-cleanup/`，不纳入本次提交。

## 提交前 review 收口

- 映射保留默认关闭，只覆盖普通 UPLOAD buffer，资源销毁时释放工具 Map 引用。
  不改变游戏 feature 查询，不依赖游戏名称和 shader。
- 64 KiB 差分节省序列化数据和相应回放上传缓存；仍保留等大 shadow，仍扫描完整
  mapping，不承诺捕获延迟或全部显存占用下降。
- Discard 使用固定 1 MiB 完整初始化的图案 source；大 buffer 增加 CopyBufferRegion
  调用。SkipDiscardPattern 会隐藏未定义数据图案，应按需开启并重新加载截帧。
- 保留 UPLOAD 堆的回放选项仅覆盖大 committed UPLOAD buffer 的提升路径；
  不改变 placed/reserved 资源和 GPU_UPLOAD 的类型与状态。
- 加载失败在获取 pipeline/resource 信息前直接返回；保留 device-lost 原因，
  不尝试从不完整驱动创建 dummy driver。
- 补齐 OOM 日志的格式参数，避免错误处理本身读取无效参数。
- 回归必须存在原生结果 sidecar，缺失、空内容、不完整 DWORD 或任一回放非零退出
  都立即失败。测试创建缺失的配置节点，且在异常时恢复用户配置原字节。
