# TimePrint BLE 固件配套开发规划

> 目标：在现有 PlatformIO + Arduino 固件中加入 BLE 控制入口，让 Android App 能通过 BLE 覆盖 Web 当前能力，同时保持 ESP32 是权威设备主体。

---

## 1. 架构原则

### 1.1 ESP32 是权威主体

固件负责：

- 计时状态机。
- 完成态判断。
- 打印触发。
- 话语池存储。
- 随机话语选择。
- Wi-Fi 凭据和无线模式保存。
- 打印机状态和自检。

App 和 Web 只负责：

- 展示状态。
- 发送命令。
- 读取快照。
- 提供交互界面。

### 1.2 BLE 是 transport，不是新业务层

BLE 不应该复制一套独立业务逻辑。

所有控制输入都应汇入同一套设备命令语义：

```text
Serial / HTTP / WebSocket / BLE
        ↓
CommandAdapter 或统一 DeviceCommandHandler
        ↓
TimerCore / WiFiManager / MessageStore / PrinterBridge
```

所有状态输出都走 listener 或 snapshot 边界：

```text
TimerCore / device stores
        ↓
StatusBuilder / SnapshotBuilder
        ↓
WebSocket / BLE notify / HTTP response
```

### 1.3 不破坏现有边界

- `TimerCore` 继续保持硬件无关。
- 物理 hook 路径继续与软件计时路径独立。
- BLE 不直接访问打印机底层串口。
- BLE 不直接修改 Wi-Fi NVS 细节，应调用 Wi-Fi 管理边界。
- BLE 不把 UI 文案、App 状态或 Android 假设写入核心逻辑。

---

## 2. 现状与需要补齐的点

当前已有：

- `TimerCore`：权威计时状态机。
- `CommandAdapter`：可解析 `set/start/pause/resume/stop/reset/config/template`。
- `WebControlAdapter`：HTTP + WebSocket 控制和状态推送。
- `WiFiManager`：AP/STA、扫描、保存/删除凭据。
- `TimerCorePrinterBridge`：软件路径打印桥。
- Web 页面：计时、话语、Wi-Fi、打印机四个入口。

需要补齐：

- BLE GATT 服务。
- 统一设备 API，避免 Web 与 BLE 分叉。
- 状态 JSON builder 从 Web adapter 中抽出，供 BLE 复用。
- 话语池的正式 store 边界。
- 设备端随机话语模式。
- 完成态语义，后续弱化或取消 overtime 产品机制。
- 无线模式：BLE 默认入口，Wi-Fi 可按需开启。

---

## 3. BLE 技术选型

使用：

```ini
h2zero/NimBLE-Arduino
```

原因：

- 适合 ESP32-S3 Arduino 固件。
- 资源占用比默认 BLE 库更低。
- 更适合与 Wi-Fi/WebServer 共存。

`platformio.ini` 需要新增依赖：

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.3.0
    ottowinter/ESPAsyncWebServer-esphome@^3.3.0
    h2zero/NimBLE-Arduino@^2
```

版本号落地时以 PlatformIO Registry 可用版本为准，固定一个可复现版本，不使用裸 `latest`。

---

## 4. 文件结构建议

新增或重构：

```text
firmware/lib/Net/
  BLEAdapter.h
  BLEAdapter.cpp
  BLEConfig.h
  DeviceStatus.h
  DeviceStatus.cpp
  DeviceApi.h
  DeviceApi.cpp

firmware/lib/Message/
  MessageStore.h
  MessageStore.cpp
```

如果暂时不想新建 `Message/`，也可以先放在 `lib/Printer/` 或 `lib/Net/`，但最终应形成独立边界，因为话语池不是网络层，也不是打印机驱动。

---

## 5. GATT 设计

### 5.1 Service

定义在 `BLEConfig.h`：

```cpp
static constexpr const char* TIMEPRINT_SERVICE_UUID = "...";
static constexpr const char* TIMEPRINT_STATUS_UUID = "...";
static constexpr const char* TIMEPRINT_COMMAND_UUID = "...";
static constexpr const char* TIMEPRINT_RESPONSE_UUID = "...";
```

UUID 需要生成正式 128-bit UUID。不要使用示例 UUID 或随机散落在代码里。

### 5.2 Characteristics

| Characteristic | 属性 | 方向 | 用途 |
|---|---|---|---|
| status | read + notify | Device → App | 当前计时和设备简要状态 |
| command | write with response | App → Device | App 发送命令 |
| response | notify | Device → App | 命令结果、列表页响应、错误 |

### 5.3 广播

广播名称建议：

```text
TimePrint-XXXX
```

`XXXX` 使用 MAC 后四位或设备 ID 后四位。

广播数据包含：

- Service UUID。
- 简短设备名。
- 可选固件 major/minor。

不要在广播里放敏感信息。

---

## 6. 设备 API

### 6.1 JSON envelope

BLE 命令统一使用 envelope：

```json
{
  "rid": 1,
  "cmd": "start"
}
```

响应：

```json
{
  "rid": 1,
  "ok": true
}
```

错误：

```json
{
  "rid": 1,
  "ok": false,
  "error": "invalid_command"
}
```

原则：

- `rid` 由 App 生成。
- 固件响应必须带回 `rid`。
- notify 的异步状态不需要 `rid`。
- BLE 和 HTTP/WebSocket 尽量共享同一套 cmd 名称。

### 6.2 状态 JSON

状态字段建议：

```json
{
  "state": "idle",
  "planned_s": 1500,
  "remaining_s": 1500,
  "elapsed_s": 0,
  "focus_count": 12,
  "wifi_mode": "off",
  "ip": "",
  "printer": "stub",
  "firmware": "0.2.0"
}
```

后续取消 overtime 产品机制后：

- 不再把 overtime 作为主交互。
- 可以保留 `overtime_s` 一段兼容期，但 App 不依赖它。
- 增加 `finished` 或 `completed` 状态，具体枚举需在固件任务中统一。

### 6.3 快照类命令

首版覆盖 Web 当前能力：

计时：

- `set`
- `start`
- `pause`
- `resume`
- `stop`
- `reset`

话语：

- `messages_get`
- `message_add`
- `message_delete`
- `message_select`
- `message_mode`

网络：

- `wifi_status`
- `wifi_enable`
- `wifi_disable`
- `wifi_scan`
- `wifi_add`
- `wifi_delete`
- `wifi_reset`

设备：

- `device_info`
- `printer_status`
- `printer_test`
- `device_restart`

### 6.4 分页与长响应

BLE 不应假设大 MTU。

列表响应使用分页：

```json
{
  "rid": 8,
  "ok": true,
  "kind": "messages",
  "page": 0,
  "has_more": false,
  "items": []
}
```

适用：

- 话语列表。
- Wi-Fi 扫描结果。
- 已保存 Wi-Fi 列表。

---

## 7. 完成态与话语策略

### 7.1 完成态

产品方向：后续取消 overtime 机制。

固件规划：

- `TimerCore` 到点后进入完成态。
- 完成态触发 `onPrintSlip`。
- 完成态保留，直到用户重新设置或重置。
- App/Web 看到完成态后展示完成，而不是超时计数。

注意：

- 这是核心行为变更，必须单独做测试。
- 不要在 BLE 任务里顺手改完，应该作为独立固件里程碑。

### 7.2 话语池

固件需要成为话语池权威来源。

数据来源：

- 固件内置预设话语：只读。
- NVS 保存的自定义话语：可新增、删除。

模式：

- fixed：使用当前选中话语。
- random：完成时由设备端从话语池抽取。

抽取原则：

- App/Web 不决定随机结果。
- App/Web 只配置模式或候选范围。
- App 不在线时，完成打印仍然有话语。

---

## 8. 无线模式

### 8.1 目标体验

BLE 是默认入口。

Wi-Fi 是设备能力之一：

- 可以开启。
- 可以关闭。
- 可以用于 Web 备用管理。
- 可以用于局域网控制。

### 8.2 状态

建议定义：

```text
wifi_mode = off | ap | sta | connecting | scanning
```

### 8.3 操作

`wifi_enable`：

- 开启 Wi-Fi 子系统。
- 无凭据时进入 AP 或等待配置，具体策略由固件定。

`wifi_disable`：

- 关闭 STA/AP。
- 保存用户偏好。
- BLE 继续工作。

`wifi_scan`：

- 若 Wi-Fi 关闭，可临时开启 Wi-Fi 扫描。
- 扫描期间 BLE 可能短暂变慢，App 侧显示等待。

`wifi_add`：

- 保存凭据。
- 可能导致重启或重新连接。
- 响应中明确 `restart` 或 `disconnect_expected`。

### 8.4 首版安全说明

首版 BLE 配网为样机策略：

- 不强制配对。
- 不加密 Wi-Fi 密码传输。

必须在文档和 App UI 中标注：

- 适合开发/内测。
- 不适合量产。
- 量产前需要配对、加密或 PIN。

---

## 9. BLEAdapter 设计

### 9.1 职责

`BLEAdapter` 负责：

- 初始化 NimBLE。
- 创建 GATT server。
- 广播服务。
- 接收 command write。
- 调用统一设备 API。
- 发送 response notify。
- 发送 status notify。

`BLEAdapter` 不负责：

- 直接修改 `TimerCore` 内部状态。
- 直接写 NVS。
- 直接控制打印机串口。
- 保存 App 会话状态。

### 9.2 Listener 注册

当前 `TimerCore::kMaxListeners` 是 4，已有：

- SerialStatus
- TimerCorePrinterBridge
- FocusCounter
- WebControlAdapter

新增 BLE 后必须提升容量，建议：

```cpp
static const int kMaxListeners = 6;
```

同时测试 `addListener` 达到容量时的行为。

### 9.3 推送节流

状态推送规则：

- 连接成功后立即发送当前 status。
- 状态变化时立即发送。
- tick 时最多每秒发送一次。
- 无客户端连接时不做 notify。

不要在 BLE 回调里做长时间阻塞操作。

---

## 10. WebControlAdapter 配套重构

为了让 BLE 和 Web 不分叉，需要抽出共享能力：

### 10.1 状态构建

从 `WebControlAdapter::statusJson()` 抽出：

```cpp
String buildStatusJson(const DeviceContext& ctx);
```

Web 和 BLE 都调用它。

### 10.2 命令处理

现有 `CommandAdapter` 可以继续处理计时命令，但全量 Web 能力需要更大的设备命令入口。

建议新增：

```cpp
class DeviceApi {
 public:
  bool handle(JsonDocument& request, JsonDocument& response);
};
```

`DeviceApi` 负责分发：

- TimerCore 命令。
- MessageStore 命令。
- WiFiManager 命令。
- Printer 命令。
- Device info 命令。

`CommandAdapter` 可保留为 TimerCore 兼容层，或逐步并入 `DeviceApi`。

### 10.3 Web 路由兼容

现有 REST 路由不必一次删除。

可以：

- 保留 `/api/status`、`/api/cmd` 等现有接口。
- 内部改为调用 `DeviceApi`。
- 新增 `/api/device` 统一命令入口供后续 Web/App 共用。

---

## 11. 开发里程碑

### F1. 共享状态与设备 API 重构

- 抽出 status builder。
- 新增 `DeviceApi`。
- Web 继续可用。
- 不引入 BLE。

验证：

- `cd firmware && pio test -e native`
- `cd firmware && pio run -e esp32s3`
- Web 计时、Wi-Fi、话语、打印机接口不回归。

### F2. BLE 基础服务

- 加入 NimBLE-Arduino。
- 新增 `BLEConfig` 和 `BLEAdapter`。
- 广播 TimePrint 服务。
- App 或通用 BLE 工具能扫描到设备。

验证：

- ESP32-S3 编译通过。
- 实机可扫描到广播。

### F3. BLE 计时控制

- command write 支持 `set/start/pause/resume/stop/reset`。
- status read/notify 支持当前状态。
- 连接后立即推送状态。

验证：

- BLE 命令与 Web/串口控制结果一致。
- Web 与 BLE 同时连接时状态同步。

### F4. BLE 全量 Web 能力

- 话语池读取/新增/删除/模式。
- Wi-Fi 状态/扫描/添加/删除/重置/开关。
- 打印机状态/自检。
- 设备信息/重启。

验证：

- App 能覆盖 Web 当前功能。
- 所有列表响应支持分页或小包传输。

### F5. 完成态与设备端随机

- `TimerCore` 或上层行为引入完成态。
- 取消 App 主交互对 overtime 的依赖。
- 完成时设备端生成话语。
- 完成即打印。

验证：

- native 增加完成态测试。
- 到点只打印一次。
- App/Web 看到一致完成状态。

### F6. 纯 BLE 默认入口与 Wi-Fi 可关闭

- 设备启动优先 BLE 广播。
- Wi-Fi 可保存为关闭。
- BLE 下可临时开启 Wi-Fi 扫描/配网。
- Web 作为备用管理界面保留。

验证：

- Wi-Fi 关闭时 BLE 仍可连接控制。
- 临时开启 Wi-Fi 扫描后 BLE 不崩溃。
- Wi-Fi 配置动作后的断连/重启响应明确。

---

## 12. 测试计划

native 测试：

- TimerCore 完成态。
- Command/DeviceApi 命令解析。
- MessageStore 增删和随机。
- 状态 JSON 字段。

ESP32-S3 编译：

```bash
cd firmware && pio run -e esp32s3
```

native：

```bash
cd firmware && pio test -e native
```

如果没有 `pio`：

```bash
cd firmware && python -m platformio test -e native
cd firmware && python -m platformio run -e esp32s3
```

实机测试：

- BLE 广播。
- BLE 连接。
- GATT service discovery。
- status read/notify。
- command write。
- Web 和 BLE 同时控制。
- Wi-Fi 开关、扫描、保存。
- 打印测试页。
- 完成态打印。

已知验证缺口：

- 仓库记录过 4D Systems GEN4-ESP32 CH343 在 COM4 上 esptool sync 失败。
- 该问题解决前，不得声称“已烧入实机”。

---

## 13. 风险与约束

- BLE 与 Wi-Fi 共用射频，Wi-Fi 扫描期间 BLE 可能短暂变慢或断开。
- BLE 回调中不能做阻塞操作。
- JSON 长响应必须分页。
- 首版开放 BLE 配网不适合量产。
- 完成态会影响核心行为，必须作为独立里程碑测试。
- `Explore_Codes/` 只能参考思路，不得直接搬入正式固件。

