# TimePrint Android BLE App 开发规划

> 目标：把 TimePrint 做成一个以 BLE 为默认入口的轻量 Android 控制 App。App 可以覆盖 Web 现有功能，但不成为设备大脑；ESP32 仍然是计时、打印、话语、网络配置的权威主体。

---

## 1. 产品定位

### 1.1 App 的角色

Android App 是 TimePrint 的近场遥控器和更精致的移动端界面。

它负责：

- 发现并连接 TimePrint 设备。
- 前台连接期间实时读取设备状态。
- 向设备发送控制命令。
- 以更好的交互呈现计时、话语、网络和设备管理。
- 在需要时通过 Wi-Fi 手动备用连接设备。

它不负责：

- 后台长期保持 BLE 连接。
- 自己跑倒计时。
- 自己决定计时完成、打印或随机话语结果。
- 保存业务数据的权威副本。
- 替代 ESP32 的状态机、配置存储或打印逻辑。

### 1.2 设计原则

- **ESP32 是权威主体**：所有状态以设备返回为准，App 不做乐观状态更新。
- **BLE 是默认入口**：用户优先通过 App 扫描 BLE 广播发现设备。
- **Web 是备用管理界面**：保留现有 Web/Wi-Fi 能力，用于无 App、调试、局域网控制。
- **App 只保存连接偏好**：最近设备、最近连接方式、少量 UI 偏好；话语、Wi-Fi、打印机状态都从设备读取。
- **前台实时，后台不承诺**：App 在前台时保持 BLE 连接和 notify；退后台或锁屏后不承诺持续连接。

---

## 2. 技术栈

### 2.1 Android

- 语言：Kotlin
- UI：Jetpack Compose + Material 3
- 并发：Coroutines + Flow
- BLE：Android 原生 Bluetooth LE API
- 最低系统：Android 12
- 网络备用：OkHttp HTTP + WebSocket
- 序列化：kotlinx.serialization 或 Moshi，二选一后全项目统一

Android 12+ 需要处理 Nearby devices 权限：

- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`

首版不支持 Android 12 以下版本，避免旧版定位权限和 BLE 权限模型带来的额外复杂度。

### 2.2 项目目录建议

建议新增：

```text
app-android/
  app/
    src/main/
```

核心模块按职责拆分：

```text
ble/          BLE 扫描、连接、GATT 队列、notify 订阅
wifi/         HTTP/WebSocket 备用连接
protocol/     设备 JSON 协议 DTO 与编码/解码
data/         TimePrintRepository，统一设备状态和命令入口
ui/           Compose 页面、导航、主题、组件
```

---

## 3. 连接模型

### 3.1 连接方式

App 支持两种手动选择的连接方式：

1. BLE
   - 默认入口。
   - 适合近场控制、读取状态、发送命令。
   - 前台保持连接，后台不承诺。

2. Wi-Fi
   - 手动备用。
   - 用户输入或选择设备 IP。
   - 使用与 Web 相同的 HTTP + WebSocket 语义。

不做自动魔法切换。连接页或首页连接胶囊允许用户明确知道当前使用 BLE 还是 Wi-Fi。

### 3.2 BLE 生命周期

- 打开 App 后请求 Bluetooth 权限。
- 扫描 TimePrint BLE 广播，扫描必须限时，建议 10 秒。
- 用户选择设备后连接。
- 连接成功后执行 service discovery。
- 订阅 `status` 和 `response` notify。
- App 前台期间保持连接。
- App 进入后台时允许主动断开，或者接受系统断开。
- 返回前台后可一键重连最近设备。

### 3.3 断连表现

断连时不要直接把用户踢回连接页。

界面应：

- 保留最后一次设备状态。
- 表盘变灰。
- 显示最后更新时间。
- 首页连接胶囊显示“断开”。
- 提供“重新连接”和“切到 Wi-Fi”入口。

---

## 4. 信息架构

底部四栏：

```text
计时 / 话语 / 网络 / 设备
```

### 4.1 计时页

核心是大表盘。

待机态：

- 拖动表盘设置分钟数。
- 1 分钟粒度。
- 点按表盘开始。
- 保留快捷时长：5 / 15 / 25 / 45 分钟。

运行态：

- 点按表盘暂停。
- 不允许修改时长。

暂停态：

- 点按表盘继续。

完成态：

- 后续产品方向取消 overtime 机制。
- 计时到点后设备进入完成态并触发打印。
- App 表盘变为安静的完成展示。
- 完成态保留到用户再次拖动表盘。
- 用户拖动表盘后进入下一轮设时。

破坏性操作：

- 停止、重置使用长按确认。

状态原则：

- 用户发出命令后，等待设备响应或状态 notify。
- 设备确认前不改变权威状态。

### 4.2 话语页

话语页管理设备话语池，不管理 App 本地话语池。

功能：

- 读取设备内置预设话语。
- 读取设备自定义话语。
- 新增自定义话语。
- 删除自定义话语。
- 预设话语只读。
- 支持固定选中和随机模式。

随机模式：

- 随机逻辑最终放在设备端。
- App 只配置模式或候选范围。
- App 不在线时，设备仍能在完成时生成话语并打印。

### 4.3 网络页

网络页按“设备网络能力”组织，而不是按 App 连接方式组织。

展示：

- Wi-Fi 状态：关闭 / AP / STA。
- 当前 IP。
- 已保存网络数量。
- 当前 App 连接方式：BLE 或 Wi-Fi。

操作：

- 临时开启 Wi-Fi。
- 扫描 Wi-Fi。
- 添加 Wi-Fi 凭据。
- 删除已保存网络。
- 重置 Wi-Fi。

注意：

- 保存 Wi-Fi、重置 Wi-Fi、切换无线模式可能导致设备重启或断连。
- 执行前明确提示，执行后进入等待重连状态。

首版安全策略：

- BLE 配网按样机策略开放传输。
- 页面需要用轻量提示标明：当前 BLE 配网未做加密，适合开发/内测，不适合量产。

### 4.4 设备页

设备页覆盖低频硬件管理。

展示：

- 设备名称。
- 设备 ID。
- 固件版本。
- 打印机类型。
- 打印机 RX/TX 引脚。
- 打印机波特率。
- 当前打印机状态。

操作：

- 打印测试页。
- 重启设备。
- 查看基础错误提示。

不做：

- 原始日志面板。
- 原始 JSON 命令面板。
- 串口调试替代品。

---

## 5. 视觉与交互风格

### 5.1 视觉母题

App 的母题是：

```text
纸条 + 表盘
```

计时页突出表盘，完成反馈和话语页突出纸条感。

### 5.2 语气

轻陪伴，不话痨。

示例：

- “开始啦，慢慢来。”
- “已暂停。”
- “完成了。”
- “纸条在路上。”
- “没连上，可以再试一次，或切到 Wi-Fi。”

避免：

- 大段教程。
- 过度拟人。
- 只显示底层错误码。

### 5.3 动效

使用轻动效：

- 表盘拖动平滑。
- 计时状态切换有轻微过渡。
- 完成态有轻微反馈。
- 打印提示可用小纸条滑出效果。

不要让动效影响命令响应、状态同步或低端 Android 设备流畅度。

---

## 6. 设备协议使用方式

App 不定义自己的业务协议。它使用固件提供的同一套设备语义：

- BLE：GATT command write + status/response notify。
- Wi-Fi：HTTP command + WebSocket status。

App 侧应把 BLE 和 Wi-Fi 包装成同一个接口：

```kotlin
interface TimePrintTransport {
    val connectionState: Flow<ConnectionState>
    val status: Flow<DeviceStatus>
    suspend fun send(command: DeviceCommand): CommandResult
    suspend fun readSnapshot(kind: SnapshotKind): SnapshotResult
}
```

Repository 面向 UI 暴露统一状态：

```kotlin
data class TimePrintUiState(
    val connection: ConnectionUiState,
    val timer: TimerUiState,
    val messages: MessagesUiState,
    val network: NetworkUiState,
    val device: DeviceInfoUiState
)
```

页面同步策略：

- 计时页依赖 status notify。
- 话语页进入时读取快照，新增/删除/切换后重新读取。
- 网络页进入时读取快照，扫描或配置后重新读取。
- 设备页进入时读取快照，测试打印后显示轻提示。

---

## 7. 开发里程碑

### A1. Android 工程骨架

- 新建 `app-android/`。
- 配置 Kotlin、Compose、Android 12+。
- 建立主题、导航和四个空页面。
- 完成权限请求流程。

验收：

- App 可启动。
- 底部四栏可切换。
- 未授权时能请求 Nearby devices 权限。

### A2. 协议和假设备

- 定义 `DeviceStatus`、`DeviceCommand`、snapshot DTO。
- 实现 fake transport。
- UI 先接 fake transport 开发。

验收：

- 无硬件时首页表盘可展示假状态。
- 命令按钮能通过 fake transport 改变状态。

### A3. BLE 连接

- BLE 扫描 TimePrint 设备。
- 连接 GATT。
- 发现服务和特征。
- 订阅 status/response notify。
- 实现串行 GATT 操作队列。

验收：

- 前台连接后可收到设备状态。
- 断连后 UI 保留最后状态并显示断连。

### A4. 计时页完整交互

- 表盘拖动设时。
- 点按开始/暂停/继续。
- 长按停止/重置。
- 完成态展示。

验收：

- 所有状态由设备确认后更新。
- 运行中不可改时长。

### A5. 话语、网络、设备页

- 话语池读取、新增、删除、模式配置。
- Wi-Fi 状态、扫描、添加、删除、重置。
- 打印机状态、自检打印、设备信息。

验收：

- 覆盖 Web 当前功能。
- 所有业务数据都从设备读取。

### A6. Wi-Fi 手动备用

- 支持输入 IP。
- HTTP command。
- WebSocket status。
- 与 BLE 使用同一个 repository。

验收：

- 用户可手动切换 BLE / Wi-Fi。
- 两种连接下 UI 行为一致。

---

## 8. 测试计划

单元测试：

- JSON encode/decode。
- command rid 匹配。
- status 解析。
- repository 状态合并。

假设备测试：

- 连接成功。
- 连接失败。
- notify 状态更新。
- 命令超时。
- 断连重连。

实机测试：

- Android 12+ 权限请求。
- BLE 扫描限时。
- BLE 连接和服务发现。
- 状态 notify。
- 计时命令。
- 话语池读取和修改。
- Wi-Fi 扫描和保存。
- 打印测试页。
- Wi-Fi 手动备用控制。

---

## 9. 已知风险

- BLE 与 Wi-Fi 共用 2.4GHz 射频，Wi-Fi 扫描期间 BLE 可能短暂不稳定。
- 首版 BLE 配网开放传输，不适合量产安全要求。
- App 不做后台常连，用户退后台后不能依赖 App 收到完成事件。
- 固件完成态、设备端随机话语、Wi-Fi 开关模式仍需配套实现。

