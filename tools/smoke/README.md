# 烟测脚本

这些脚本用于验证单元测试和编译检查无法完全覆盖的台架行为。

## 串口启动

```powershell
python tools/smoke/serial_boot_smoke.py --port COM3
```

预期结果：启动文本包含 TimePrint 固件骨架、命令列表、Web 服务启动信息和设备访问地址。

## HTTP 控制

先让电脑加入 `TimePrint-XXXX` WiFi AP，然后运行：

```powershell
python tools/smoke/http_control_smoke.py --base-url http://192.168.4.1
```

脚本会验证 `GET /api/status`，以及 `POST /api/cmd` 下的
`set/start/pause/resume/stop/reset`。

## 串口 WiFi 配网

通过串口把开发板从 AP 模式配置到本地 2.4 GHz WiFi：

```powershell
python tools/smoke/serial_wifi_provision.py --port COM3 --ssid "<ssid>" --password "<password>"
```

开发板重新连接后，脚本会打印 STA IP。
