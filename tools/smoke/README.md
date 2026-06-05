# Smoke Tests

These scripts verify bench behavior that unit tests and compile checks cannot
fully prove.

## Serial Boot

```powershell
python tools/smoke/serial_boot_smoke.py --port COM3
```

Expected result: boot text includes the TimePrint scaffold, command list, Web
server startup, and the device URL.

## HTTP Control

First join the `TimePrint-XXXX` WiFi AP from the host machine, then run:

```powershell
python tools/smoke/http_control_smoke.py --base-url http://192.168.4.1
```

The script exercises `GET /api/status` plus `POST /api/cmd` for
`set/start/pause/resume/stop/reset`.

## Serial WiFi Provisioning

To move the board from AP mode onto a local 2.4 GHz WiFi network:

```powershell
python tools/smoke/serial_wifi_provision.py --port COM3 --ssid "<ssid>" --password "<password>"
```

The script prints the STA IP once the board reconnects.
