# API Monitor — 实时图形 API 监控功能文档

## 概述

API Monitor 是 RenderDoc 自定义 Fork 中新增的功能模块，允许用户通过 **Python 脚本** 实时监控注入进程中的 D3D11/D3D12 图形 API 调用。支持条件过滤、参数日志、资源追踪和纹理导出。

**核心价值**：无需每次编写 C++ 注入器，通过 Python 脚本即可灵活定义监控逻辑，热加载无需重启游戏。

## 依赖

- `python36.dll` 必须存在于 `system_load.dll`（或 `renderdoc.dll`）同目录下
- Python 通过 `LoadLibrary` 动态加载，不增加 DLL 体积
- 若 `python36.dll` 不存在，功能自动禁用，不影响正常截帧

## 使用方式

### 方式一：Launch Application 界面（推荐）

1. 在 Launch Application 界面的 **Monitor Script** 字段填入 `.py` 脚本路径
   - 如果 exe 同目录下存在 `rdcmonitor_autoload.py`，首次打开时会自动填入
   - 留空则不加载任何脚本
2. 点击 Launch 或 Enable Global Hook
3. 脚本自动加载到目标进程，日志输出到 `system_load.dll` 同目录下的 `rdcmonitor_log.txt`

### 方式二：API Monitor 窗口（实时控制）

通过 **Window → API Monitor** 打开独立窗口：

- **脚本编辑器**（上半部分）：Scintilla 编辑器，支持 Python 语法高亮，可直接编写/修改脚本
- **日志输出**（下半部分）：实时显示 `monitor.log()` 输出，错误以红色显示
- **工具栏**：
  - **Open** / **Save**：打开/保存 `.py` 文件
  - **Apply**：将编辑器中的脚本发送到目标进程并加载（覆盖旧脚本）
  - **Unload**：卸载当前脚本，游戏恢复无监控状态
  - **Clear Log**：清空日志面板
- **状态栏**：显示连接状态、活跃 hook 数量或错误信息
- 中途打开窗口时，会自动回放之前的历史日志

### 方式三：Global Hook

1. 在 Launch Application 填入 Monitor Script 路径
2. 启用 Global Hook
3. UI 将脚本写入 `%TEMP%\rdcmonitor_pending.py`
4. 目标进程启动后 DLL 自动检测、加载并删除该临时文件
5. 日志输出到 DLL 同目录的 `rdcmonitor_log.txt`

### 脚本加载机制

| 启动模式 | 脚本传递通道 | 说明 |
|----------|-------------|------|
| Launch | Target Control 协议 | LiveCapture 连接后通过 `SendMonitorScript()` 发送 |
| Inject | Target Control 协议 | 同上 |
| Global Hook | 临时文件 `%TEMP%\rdcmonitor_pending.py` | DLL 在第 10 帧后检测并加载，之后立即删除 |
| API Monitor 窗口 Apply | Target Control 协议 | 随时可重新发送脚本，支持热重载 |

**注意**：
- **没有隐式自动加载** — DLL 目录下放 `rdcmonitor_autoload.py` 不会自动生效，必须在 UI 中显式填入路径
- UI 启动时会自动清理 `%TEMP%\rdcmonitor_pending.py` 残留，防止上次崩溃/异常退出时的脚本意外加载
- `python36.dll` 必须存在于 `system_load.dll` 同目录下

## Python 脚本 API

脚本中可通过 `import _rdcmonitor as monitor` 或直接使用全局变量 `monitor`。

### `monitor.on(api_name, callback)`
注册一个 API 调用 hook。当目标 API 被调用时，`callback(call)` 被触发。

**参数**：
- `api_name` (str): API 全名，格式为 `"接口名::方法名"`
- `callback` (callable): 回调函数，接收一个 dict 参数

**call dict 结构**：
```python
{
    "apiName": "ID3D12Device::CreateResource",  # API 名称
    "args": { ... },                              # 参数字典（见下文）
    "result": 12345                               # 创建的资源 ID（仅资源创建 API 有）
}
```

### `monitor.off(api_name)`
取消注册指定 API 的 hook。

### `monitor.log(message)`
输出日志消息。日志自动添加 `[F:帧号]` 前缀（帧号与 RenderDoc overlay 一致），同时写入：
- `rdcmonitor_log.txt`（文件，位于 DLL 同目录）
- RenderDoc 调试日志
- Target Control 日志缓冲（传送给 API Monitor 窗口）

```python
monitor.log("hello")  # 输出: [F:142] hello
```

### `monitor.track(resource_id)`
标记一个资源 ID 进行追踪。后续可通过 `is_tracked()` 检查。

### `monitor.untrack(resource_id)`
取消追踪。

### `monitor.is_tracked(resource_id)`
返回 `True` / `False`。

### `monitor.dump(resource_id, output_path, format="dds")`
请求将 GPU 资源导出到磁盘。

**注意**：当前 D3D12 GPU 回读尚未完整实现（需要 staging buffer + fence 等待），dump 功能为占位符。

### `monitor.format_name(format_int)`
将 DXGI_FORMAT 整数值转换为可读字符串。
```python
monitor.format_name(28)  # 返回 "R8G8B8A8_TYPELESS"
monitor.format_name(77)  # 返回 "BC3_TYPELESS"
```

### `monitor.trigger_capture(num_frames=1)`
触发 RenderDoc 在下一帧开始截帧。
```python
monitor.trigger_capture()   # 截 1 帧
monitor.trigger_capture(3)  # 截 3 帧
```

## 可 Hook 的 API 列表

### D3D12（当前游戏主要使用）

| API 名称 | args 字段 |
|----------|----------|
| `ID3D12Device::CreateResource` | `pResourceDesc` (dict), `pHeapProperties` (dict), `HeapFlags` |
| `ID3D12Device::CreateTexture2D` | 同上（Dimension==TEXTURE2D 时自动触发） |
| `ID3D12Device::CreateBuffer` | 同上（Dimension==BUFFER 时自动触发） |
| `ID3D12Device::CreateTexture3D` | 同上（Dimension==TEXTURE3D 时自动触发） |
| `ID3D12GraphicsCommandList::DrawInstanced` | `VertexCountPerInstance`, `InstanceCount`, `StartVertexLocation`, `StartInstanceLocation` |
| `ID3D12GraphicsCommandList::DrawIndexedInstanced` | `IndexCountPerInstance`, `InstanceCount`, `StartIndexLocation`, `BaseVertexLocation`, `StartInstanceLocation` |
| `ID3D12GraphicsCommandList::Dispatch` | `ThreadGroupCountX/Y/Z` |
| `ID3D12GraphicsCommandList::CopyTextureRegion` | `pDst`, `DstSubresource`, `pSrc`, `SrcSubresource` |
| `ID3D12GraphicsCommandList::CopyBufferRegion` | `pDst`, `DstOffset`, `pSrc`, `SrcOffset`, `NumBytes` |
| `ID3D12GraphicsCommandList::CopyResource` | `pDst`, `pSrc` |
| `ID3D12GraphicsCommandList::ResourceBarrier` | `NumBarriers`, `resources` (list) |

### D3D11

| API 名称 | args 字段 |
|----------|----------|
| `ID3D11Device::CreateTexture2D` | `pDesc` (dict: Width, Height, MipLevels, ArraySize, Format, ...) |
| `ID3D11Device::CreateTexture3D` | `pDesc` (dict) |
| `ID3D11Device::CreateBuffer` | `pDesc` (dict: ByteWidth, Usage, BindFlags, ...) |
| `ID3D11Device::CreateShaderResourceView` | `pResource`, `Format`, `ViewDimension` |
| `ID3D11Device::CreateRenderTargetView` | 同上 |
| `ID3D11Device::CreateUnorderedAccessView` | 同上 |
| `ID3D11DeviceContext::Draw` | `VertexCount`, `StartVertexLocation` |
| `ID3D11DeviceContext::DrawIndexed` | `IndexCount`, `StartIndexLocation`, `BaseVertexLocation` |
| `ID3D11DeviceContext::DrawInstanced` | 各参数 |
| `ID3D11DeviceContext::DrawIndexedInstanced` | 各参数 |
| `ID3D11DeviceContext::Dispatch` | `ThreadGroupCountX/Y/Z` |
| `ID3D11DeviceContext::UpdateSubresource` | `pDstResource`, `DstSubresource`, `pDstBox`, `SrcRowPitch`, `SrcDepthPitch` |
| `ID3D11DeviceContext::Map` | `pResource`, `Subresource`, `MapType`, `MapFlags` |
| `ID3D11DeviceContext::CopyResource` | `pDstResource`, `pSrcResource` |
| `ID3D11DeviceContext::CopySubresourceRegion` | 各参数 |
| `ID3D11DeviceContext::PSSetShaderResources` | `StartSlot`, `NumViews`, `views` (list) |
| `ID3D11DeviceContext::VSSetShaderResources` | 同上 |
| `ID3D11DeviceContext::CSSetShaderResources` | 同上 |

### `pResourceDesc` 字段详情（D3D12）

```python
{
    "Dimension": "TEXTURE2D",    # str: BUFFER/TEXTURE1D/TEXTURE2D/TEXTURE3D
    "Alignment": 0,
    "Width": 1024,
    "Height": 1024,
    "DepthOrArraySize": 6,       # Texture2DArray 的 Slice 数 / Texture3D 的深度
    "MipLevels": 1,
    "Format": 77,                # DXGI_FORMAT 整数值，用 monitor.format_name() 转换
    "SampleCount": 1,
    "SampleQuality": 0,
    "Layout": 0,
    "Flags": 0
}
```

## 示例脚本

### 监控所有 Texture2DArray 创建
```python
import _rdcmonitor as monitor

def on_create(call):
    desc = call['args']['pResourceDesc']
    if desc['Dimension'] == 'TEXTURE2D' and desc['DepthOrArraySize'] > 1:
        monitor.log("Tex2DArray: %dx%d, Slices=%d, Fmt=%s (id=%s)" % (
            desc['Width'], desc['Height'], desc['DepthOrArraySize'],
            monitor.format_name(desc['Format']), str(call.get('result', '?'))))

monitor.on("ID3D12Device::CreateResource", on_create)
```

### 监控大纹理（包括单张）并追踪后续拷贝
```python
import _rdcmonitor as monitor

def on_create(call):
    desc = call['args']['pResourceDesc']
    if desc['Dimension'] == 'TEXTURE2D' and desc['Width'] >= 1024:
        fmt = monitor.format_name(desc['Format'])
        res_id = call.get('result', 0)
        monitor.log("Large Tex: %dx%d arr=%d fmt=%s id=%s" % (
            desc['Width'], desc['Height'], desc['DepthOrArraySize'], fmt, res_id))
        if res_id:
            monitor.track(res_id)

def on_copy(call):
    dst = call['args'].get('pDst', 0)
    src = call['args'].get('pSrc', 0)
    if monitor.is_tracked(dst) or monitor.is_tracked(src):
        monitor.log("Copy involving tracked resource: src=%s -> dst=%s" % (src, dst))

monitor.on("ID3D12Device::CreateResource", on_create)
monitor.on("ID3D12GraphicsCommandList::CopyResource", on_copy)
monitor.on("ID3D12GraphicsCommandList::CopyTextureRegion", on_copy)
```

### 监控 DrawCall 统计
```python
import _rdcmonitor as monitor

draw_count = [0]

def on_draw(call):
    draw_count[0] += 1
    if draw_count[0] % 1000 == 0:
        monitor.log("Total draws so far: %d" % draw_count[0])

monitor.on("ID3D12GraphicsCommandList::DrawIndexedInstanced", on_draw)
monitor.on("ID3D12GraphicsCommandList::DrawInstanced", on_draw)
```

## 性能特性

| 状态 | 每次 API 调用开销 |
|------|-----------------|
| 功能禁用（无 python36.dll） | 0（编译时条件排除） |
| 功能启用但无 hook 注册 | ~1ns（一个 `volatile bool` 检查） |
| D3D12 hook 匹配（事件队列模式） | ~100ns（lock + memcpy，不碰 Python） |
| D3D11 hook 匹配并执行 | ~5-50μs（GIL + Python 调用 + dict 构建） |

**D3D12 多线程安全**：D3D12 命令列表 hook 不获取 Python GIL，而是将事件推入 C++ 队列（`MonitorEvent`），由 `Tick()` 在 Present 线程一次性取出并执行 Python 回调，避免多线程 GIL 争用导致的卡死。

## 架构文件清单

### 核心框架
| 文件 | 说明 |
|------|------|
| `renderdoc/core/python_loader.h/.cpp` | 动态加载 python36.dll |
| `renderdoc/core/api_monitor.h/.cpp` | ApiMonitor 单例，管理 hook/script/log |
| `renderdoc/core/api_monitor_pymodule.cpp` | `_rdcmonitor` Python C 扩展模块 |
| `renderdoc/core/api_monitor_export.cpp` | DDS/PNG/BMP 导出工具 |
| `renderdoc/common/globalconfig.h` | `RENDERDOC_ENABLE_API_MONITOR` 编译开关 |

### D3D11 集成
| 文件 | 说明 |
|------|------|
| `renderdoc/driver/d3d11/d3d11_api_monitor.h/.cpp` | D3D11 参数构建器 + hook 调用 |
| `renderdoc/driver/d3d11/d3d11_monitor_readback.cpp` | D3D11 GPU 纹理回读（框架） |
| `renderdoc/driver/d3d11/d3d11_device_wrap.cpp` | 修改：CreateTexture2D/3D/Buffer 中插入 hook |
| `renderdoc/driver/d3d11/d3d11_context_wrap.cpp` | 修改：Draw/UpdateSubresource/CopyResource 中插入 hook |

### D3D12 集成
| 文件 | 说明 |
|------|------|
| `renderdoc/driver/d3d12/d3d12_api_monitor.h/.cpp` | D3D12 参数构建器 + hook 调用 |
| `renderdoc/driver/d3d12/d3d12_monitor_readback.cpp` | D3D12 GPU 纹理回读（框架） |
| `renderdoc/driver/d3d12/d3d12_device_rescreate_wrap.cpp` | 修改：统一 CreateResource 中插入 hook |

### Target Control 协议
| 文件 | 说明 |
|------|------|
| `renderdoc/core/target_control.cpp` | 修改：协议 v10，新增 4 种包类型 |
| `renderdoc/api/replay/renderdoc_replay.h` | 修改：ITargetControl 新增 SendMonitor* 方法 |
| `renderdoc/api/replay/replay_enums.h` | 修改：新增 MonitorLog/MonitorStatus 消息类型 |
| `renderdoc/api/replay/control_types.h` | 修改：TargetControlMessage 新增 monitorLogs/monitorStatus 字段 |

### 集成入口
| 文件 | 说明 |
|------|------|
| `renderdoc/core/core.cpp` | 修改：Initialise/Tick/Destructor 中集成 ApiMonitor |

### UI 集成
| 文件 | 说明 |
|------|------|
| `qrenderdoc/Windows/ApiMonitorWindow.h/.cpp/.ui` | 新增：API Monitor 独立窗口（脚本编辑器 + 日志输出） |
| `qrenderdoc/Code/CaptureContext.h/.cpp` | 修改：注册 ApiMonitorWindow 窗口生命周期 |
| `qrenderdoc/Windows/MainWindow.h/.cpp/.ui` | 修改：添加 Window → API Monitor 菜单项 |
| `qrenderdoc/Windows/Dialogs/CaptureDialog.h/.cpp/.ui` | 修改：添加 Monitor Script 路径字段 |
| `qrenderdoc/Windows/Dialogs/LiveCapture.h/.cpp` | 修改：MonitorLog/MonitorStatus 消息处理、脚本发送、日志缓存 |
| `qrenderdoc/Code/Interface/QRDInterface.h/.cpp` | 修改：CaptureSettings 添加 monitorScriptPath 字段 |

## 关键约束

1. **两个 vcxproj 同步**：所有 `renderdoc/` 下的新文件必须同时存在于 `renderdoc.vcxproj` 和 `renderdoc_no_export.vcxproj`
2. **UI 进程排除**：`ApiMonitor::Initialise()` 只在 `!IsReplayApp()` 时调用，避免和 UI 的 Python 冲突
3. **GIL 安全**：所有 Python C API 调用必须在 `ApiMonitor::ScopedGIL` 保护下进行
4. **D3D12 多线程安全**：D3D12 命令列表 hook 使用事件队列（`PushEvent`），不获取 GIL
5. **无隐式加载**：DLL 不会自动搜索 `rdcmonitor_autoload.py`，必须通过 UI 显式配置

## 待完成功能

- **GPU 资源回读**：D3D12 readback buffer + fence 完整实现（`monitor.dump()` 功能）
- **Vulkan 支持**：在 Vulkan wrapper 函数中添加 hook 点
