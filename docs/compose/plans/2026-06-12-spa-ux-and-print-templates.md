# SPA UX 升级 + 打印模板系统 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use compose:subagent (recommended) or compose:execute to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Web SPA 从简陋原型提升为完整交互体验（预设时间、平滑动画、超时可视化），并新增打印模板系统（鼓励语、模板选择、便签预览）。

**Architecture:** 前端改动集中在 `WebAssets.cpp`（内嵌 SPA），使用 requestAnimationFrame 做动画插值。打印模板系统在固件侧新增 `PrintTemplate` 模块（PROGMEM 存储），通过扩展 `TimerCorePrinterBridge` 支持模板选择。协议扩展在 `CommandAdapter` 中解析新字段。

**Tech Stack:** Vanilla JS + SVG（无框架）、C++17、ArduinoJson、PROGMEM

---

## 文件变更总览

| 文件 | 操作 | 职责 |
|---|---|---|
| `firmware/lib/Net/WebAssets.cpp` | **重写** | 全部 SPA 改动：预设按钮、滑块、动画、超时可视化、模板选择 UI、便签预览 |
| `firmware/lib/Printer/PrintTemplate.h` | **新建** | 模板数据（PROGMEM）、模板列表、按 ID 查找 |
| `firmware/lib/Printer/TimerCorePrinterBridge.h` | **修改** | 增加 `setTemplate()` / 模板传递到 `printSlip` |
| `firmware/lib/Printer/Printer.h` | **修改** | `printSlip` 增加可选 `templateId` 参数 |
| `firmware/lib/Printer/HPD482Printer.h/cpp` | **修改** | 实现带模板的 `printSlip` |
| `firmware/lib/Command/CommandAdapter.h/cpp` | **修改** | 解析 `template` 字段，新增 `preview` 命令 |
| `firmware/lib/Net/WebControlAdapter.h/cpp` | **修改** | 新增 `/api/templates` 和 `/api/preview` 端点 |
| `firmware/src/main.cpp` | **修改** | 注册模板到 bridge，串口 `template` 命令 |

---

### Task 1: SPA — 预设时间按钮 + 滑块

**Covers:** Spec §5.3 "设定时间（数字输入或旋钮控件）"

**Files:**
- Modify: `firmware/lib/Net/WebAssets.cpp`

- [ ] **Step 1: 在 HTML 中添加预设按钮和滑块**

在 `<input id="minutes">` 下方添加：

```html
<div class="presets" style="display:flex;gap:8px;margin:10px 0;flex-wrap:wrap">
  <button class="preset" data-min="5">5 分钟</button>
  <button class="preset" data-min="15">15 分钟</button>
  <button class="preset" data-min="25">25 分钟</button>
  <button class="preset" data-min="45">45 分钟</button>
</div>
<input id="slider" type="range" min="1" max="120" value="25" style="width:100%;margin:8px 0">
<div style="display:flex;justify-content:space-between;font-size:12px;color:var(--muted)">
  <span>1 分钟</span><span id="sliderVal">25 分钟</span><span>120 分钟</span>
</div>
```

- [ ] **Step 2: 添加预设按钮和滑块的 CSS**

```css
.preset{height:34px;padding:0 12px;border:1px solid var(--line);border-radius:16px;
  background:#fff;font-size:13px;cursor:pointer;color:var(--ink);transition:all .15s}
.preset:hover,.preset.active{background:var(--accent);color:#fff;border-color:var(--accent)}
input[type=range]{-webkit-appearance:none;height:6px;border-radius:3px;background:var(--line);outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;
  border-radius:50%;background:var(--accent);cursor:pointer;border:2px solid #fff;
  box-shadow:0 1px 4px rgba(0,0,0,.2)}
```

- [ ] **Step 3: 添加 JS 交互逻辑**

```javascript
// 预设按钮
document.querySelectorAll('.preset').forEach(b => b.addEventListener('click', () => {
  const m = Number(b.dataset.min);
  els.minutes.value = m;
  els.slider.value = m;
  els.sliderVal.textContent = m + ' 分钟';
  document.querySelectorAll('.preset').forEach(x => x.classList.remove('active'));
  b.classList.add('active');
}));

// 滑块 ↔ 输入框联动
els.slider.addEventListener('input', () => {
  const m = Number(els.slider.value);
  els.minutes.value = m;
  els.sliderVal.textContent = m + ' 分钟';
  document.querySelectorAll('.preset').forEach(x => {
    x.classList.toggle('active', Number(x.dataset.min) === m);
  });
});

els.minutes.addEventListener('input', () => {
  const m = Number(els.minutes.value);
  els.slider.value = Math.min(120, Math.max(1, m));
  els.sliderVal.textContent = m + ' 分钟';
});
```

- [ ] **Step 4: 验证**

在浏览器中打开 SPA，检查：
- 点击预设按钮 → 输入框和滑块同步变化
- 拖动滑块 → 输入框和预设按钮高亮同步
- 手动输入 → 滑块同步

---

### Task 2: SPA — 平滑表盘动画

**Covers:** Spec §5.3 "实时可视化表盘（圆形，红色扇形随 remaining_s 收缩）"

**Files:**
- Modify: `firmware/lib/Net/WebAssets.cpp`

- [ ] **Step 1: 重构 `render()` 为动画驱动模式**

将当前的直接渲染改为设置目标值，由 requestAnimationFrame 循环插值：

```javascript
let animState = {
  currentFrac: 1,    // 当前显示的扇形比例
  targetFrac: 1,     // 目标比例（来自 WS 数据）
  currentState: 'idle',
  overtime: 0,
  remaining: 0,
  planned: 0
};

const LERP_SPEED = 0.08; // 每帧插值速度（0~1，越大越快）

function animLoop() {
  // 线性插值
  const diff = animState.targetFrac - animState.currentFrac;
  if (Math.abs(diff) > 0.001) {
    animState.currentFrac += diff * LERP_SPEED;
  } else {
    animState.currentFrac = animState.targetFrac;
  }
  renderFrame();
  requestAnimationFrame(animLoop);
}

function renderFrame() {
  const s = animState;
  els.clock.textContent = s.currentState === 'overtime'
    ? ('+' + mmss(s.overtime))
    : mmss(s.remaining || s.planned);
  els.state.textContent = stateText(s.currentState);
  els.planned.textContent = secondsText(s.planned);
  els.elapsed.textContent = secondsText(s.planned - s.remaining);
  els.remaining.textContent = secondsText(s.remaining);
  els.overtime.textContent = secondsText(s.overtime);
  els.wedge.setAttribute('d', wedgePath(s.currentFrac, s.currentState));
}
```

- [ ] **Step 2: 修改 WS 消息处理**

```javascript
ws.onmessage = e => {
  try {
    const s = JSON.parse(e.data);
    const planned = s.planned_s || 0;
    const remaining = s.remaining_s || 0;
    animState.planned = planned;
    animState.remaining = remaining;
    animState.overtime = s.overtime_s || 0;
    animState.currentState = s.state || 'idle';
    animState.targetFrac = (s.state === 'overtime' || planned === 0)
      ? 0 : Math.max(0, remaining / planned);
  } catch(_) {}
};
```

- [ ] **Step 3: 启动动画循环**

在 script 末尾：
```javascript
requestAnimationFrame(animLoop);
```

- [ ] **Step 4: 验证**

设置 1 分钟计时，观察表盘扇形是否平滑收缩（而非每秒跳一次）。

---

### Task 3: SPA — 超时表盘可视化

**Covers:** Spec §5.3 "overtime 切白色"

**Files:**
- Modify: `firmware/lib/Net/WebAssets.cpp`

- [ ] **Step 1: 添加超时状态的 CSS 动画**

```css
@keyframes pulse-white {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}
.dial-overtime { fill: #f0f0f0; animation: pulse-white 2s ease-in-out infinite; }
```

- [ ] **Step 2: 修改 `wedgePath()` 函数处理超时**

```javascript
function wedgePath(frac, state) {
  const wedge = document.getElementById('wedge');
  if (state === 'overtime') {
    // 超时：整个表盘变白，加脉冲动画
    wedge.classList.add('dial-overtime');
    wedge.classList.remove('dial-rem');
    return 'M50 50 m-47 0 a47 47 0 1 0 94 0 a47 47 0 1 0 -94 0';
  }
  wedge.classList.remove('dial-overtime');
  wedge.classList.add('dial-rem');
  if (frac <= 0) return '';
  if (frac >= 0.999) return 'M50 50 m-47 0 a47 47 0 1 0 94 0 a47 47 0 1 0 -94 0';
  const start = polar(50, 50, 47, 0);
  const end = polar(50, 50, 47, 360 * frac);
  const large = frac > 0.5 ? 1 : 0;
  return `M50 50 L${start[0]} ${start[1]} A47 47 0 ${large} 1 ${end[0]} ${end[1]} Z`;
}
```

- [ ] **Step 3: 修改 `renderFrame()` 中的时钟显示**

超时时时钟用红色显示 `+MM:SS`，并在状态文字旁加"已超时"高亮：

```javascript
if (s.currentState === 'overtime') {
  els.clock.style.color = 'var(--red)';
  els.clock.textContent = '+' + mmss(s.overtime);
} else {
  els.clock.style.color = '';
  els.clock.textContent = mmss(s.remaining || s.planned);
}
```

- [ ] **Step 4: 验证**

设置 1 分钟计时，等进入 overtime，确认：
- 表盘变白并有呼吸脉冲动画
- 时钟显示红色 `+00:XX`

---

### Task 4: 固件 — 打印模板模块

**Covers:** 产品核心——"打印一张便签小纸条" 带鼓励语

**Files:**
- Create: `firmware/lib/Printer/PrintTemplate.h`

- [ ] **Step 1: 创建 PrintTemplate.h**

```cpp
#pragma once

#include <Arduino.h>

namespace timeprint {

struct PrintTemplate {
  const char* id;
  const char* name;        // SPA 显示名
  const char* message;     // 打印在便签上的鼓励语
};

// PROGMEM 模板列表
static const PrintTemplate kTemplates[] PROGMEM = {
  {"default", "默认",   "专注的每一刻都值得记录 :)"},
  {"warm",    "温暖",   "慢慢来，比较快 :)"},
  {"cheer",   "加油",   "你比你想象的更强大 :)"},
  {"zen",     "禅意",   "此刻即永恒 :)"},
  {"simple",  "简洁",   ":)"},
};

static const int kTemplateCount = sizeof(kTemplates) / sizeof(kTemplates[0]);

inline const PrintTemplate* findTemplate(const char* id) {
  for (int i = 0; i < kTemplateCount; ++i) {
    if (strcmp(kTemplates[i].id, id) == 0) return &kTemplates[i];
  }
  return &kTemplates[0]; // 默认 fallback
}

}  // namespace timeprint
```

- [ ] **Step 2: 验证头文件语法**

在 native 环境中 include 此头文件，确认无编译错误。

---

### Task 5: 固件 — 扩展 Printer 接口支持模板

**Covers:** 打印模板系统

**Files:**
- Modify: `firmware/lib/Printer/Printer.h`
- Modify: `firmware/lib/Printer/TimerCorePrinterBridge.h`
- Modify: `firmware/lib/Printer/HPD482Printer.h`
- Modify: `firmware/lib/Printer/HPD482Printer.cpp`

- [ ] **Step 1: 扩展 Printer 接口**

在 `Printer.h` 中添加带模板的重载：

```cpp
#pragma once

#include "TimerCore.h"

namespace timeprint {

struct PrintTemplate;  // 前向声明

class Printer {
 public:
  virtual ~Printer() {}
  virtual void printSlip(const SlipData& slip) = 0;
  virtual void printSlip(const SlipData& slip, const char* message) {
    // 默认实现：忽略 message，走无模板版本
    printSlip(slip);
  }
  virtual void printSimple(const char* message) = 0;
  virtual const char* name() const = 0;
  virtual void testPage() = 0;
};

}  // namespace timeprint
```

- [ ] **Step 2: 扩展 TimerCorePrinterBridge**

```cpp
#pragma once

#include "Printer.h"
#include "TimerCore.h"

namespace timeprint {

class TimerCorePrinterBridge : public ITimerCoreListener {
 public:
  explicit TimerCorePrinterBridge(Printer* printer) : printer_(printer) {}

  void setPrinter(Printer* printer) { printer_ = printer; }
  Printer* printer() const { return printer_; }

  void setTemplateMessage(const char* msg) { templateMessage_ = msg; }
  const char* templateMessage() const { return templateMessage_; }

  void onPrintSlip(const SlipData& slip) override {
    if (!printer_) return;
    if (templateMessage_) {
      printer_->printSlip(slip, templateMessage_);
    } else {
      printer_->printSlip(slip);
    }
  }

 private:
  Printer* printer_;
  const char* templateMessage_ = nullptr;
};

}  // namespace timeprint
```

- [ ] **Step 3: HPD482Printer 实现带模板的 printSlip**

在 `HPD482Printer.h` 中声明：
```cpp
void printSlip(const SlipData& slip, const char* message) override;
```

在 `HPD482Printer.cpp` 中实现：
```cpp
void HPD482Printer::printSlip(const SlipData& slip, const char* message) {
  char planned[8], actual[8], overrun[8];
  formatMMSS(slip.plannedSeconds, planned, sizeof(planned));
  formatMMSS(slip.actualSeconds, actual, sizeof(actual));
  formatMMSS(slip.overrunSeconds, overrun, sizeof(overrun));

  sendCommand("AT+CN=24");
  printText("------------------------");
  char line[48];
  snprintf(line, sizeof(line), "计划: %s", planned);
  printText(line);
  snprintf(line, sizeof(line), "实际: %s", actual);
  printText(line);
  snprintf(line, sizeof(line), "超时: %s", overrun);
  printText(line);
  printText("------------------------");
  if (message && message[0]) {
    printText(message);
  }
  printText("------------------------");
  feedPaperMm(10);
}
```

- [ ] **Step 4: 运行 native 测试确认无回归**

Run: `cd firmware && python -m platformio test -e native`
Expected: 23 tests PASS

---

### Task 6: 固件 — CommandAdapter 支持模板命令

**Covers:** 协议扩展

**Files:**
- Modify: `firmware/lib/Command/CommandAdapter.h`
- Modify: `firmware/lib/Command/CommandAdapter.cpp`

- [ ] **Step 1: 扩展 CommandAdapter 接口**

添加模板回调接口：

```cpp
#pragma once

#include <ArduinoJson.h>
#include "TimerCore.h"

namespace timeprint {

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
using String = std::string;
#endif

class IDeviceConfigStore {
 public:
  virtual ~IDeviceConfigStore() {}
  virtual bool saveWiFiCredentials(const String& ssid, const String& pass) = 0;
};

class ITemplateStore {
 public:
  virtual ~ITemplateStore() {}
  virtual void setTemplateMessage(const char* message) = 0;
};

class CommandAdapter {
 public:
  CommandAdapter(TimerCore* core, IDeviceConfigStore* configStore = nullptr,
                 ITemplateStore* templateStore = nullptr);

  bool handle(JsonDocument& doc);

 private:
  TimerCore* core_;
  IDeviceConfigStore* configStore_;
  ITemplateStore* templateStore_;
};

}  // namespace timeprint
```

- [ ] **Step 2: 实现模板命令解析**

在 `CommandAdapter.cpp` 的 `handle()` 中，在 `"config"` 分支之后添加：

```cpp
} else if (strcmp(command, "template") == 0) {
  if (!templateStore_) return false;
  const char* msg = doc["message"] | "";
  templateStore_->setTemplateMessage(msg);
} else {
```

同时修改构造函数：
```cpp
CommandAdapter::CommandAdapter(TimerCore* core, IDeviceConfigStore* configStore,
                               ITemplateStore* templateStore)
    : core_(core), configStore_(configStore), templateStore_(templateStore) {}
```

- [ ] **Step 3: 更新 test_command 测试**

在 `test_command.cpp` 中添加 FakeTemplateStore 和新测试：

```cpp
struct FakeTemplateStore : public ITemplateStore {
  int sets = 0;
  String lastMessage;
  void setTemplateMessage(const char* msg) override {
    sets++;
    lastMessage = msg;
  }
};
```

修改 setUp 中 adapter 创建：
```cpp
static FakeTemplateStore templateStore;
// ...
adapter = new CommandAdapter(&core, &config, &templateStore);
```

添加测试：
```cpp
void test_template_command_sets_message(void) {
  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"template\",\"message\":\"加油 :)\"}"));
  TEST_ASSERT_EQUAL_INT(1, templateStore.sets);
  TEST_ASSERT_EQUAL_STRING("加油 :)", templateStore.lastMessage.c_str());
}
```

- [ ] **Step 4: 运行测试**

Run: `cd firmware && python -m platformio test -e native`
Expected: 所有测试 PASS（含新增的 template 测试）

---

### Task 7: 固件 — WebControlAdapter 模板集成

**Covers:** 模板选择 + 预览 API

**Files:**
- Modify: `firmware/lib/Net/WebControlAdapter.h`
- Modify: `firmware/lib/Net/WebControlAdapter.cpp`

- [ ] **Step 1: WebControlAdapter 实现 ITemplateStore**

让 WebControlAdapter 同时实现 ITemplateStore，将模板消息传递给 bridge：

在 `WebControlAdapter.h` 中：
```cpp
#include "CommandAdapter.h"
#include "PrintTemplate.h"

class WebControlAdapter : public ITimerCoreListener, public ITemplateStore {
 public:
  // ... 现有接口 ...
  void setTemplateMessage(const char* message) override;
  // ...
 private:
  TimerCorePrinterBridge* bridge_ = nullptr;
  // ...
};
```

- [ ] **Step 2: 修改构造函数接受 bridge 指针**

```cpp
WebControlAdapter::WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi,
                                     TimerCorePrinterBridge* bridge)
    : core_(core), wifi_(wifi), bridge_(bridge),
      commandAdapter_(core, wifi, this)  // this 作为 ITemplateStore
#if defined(ARDUINO_ARCH_ESP32)
      , server_(80), ws_("/ws")
#endif
{}
```

- [ ] **Step 3: 实现 setTemplateMessage**

```cpp
void WebControlAdapter::setTemplateMessage(const char* message) {
  if (bridge_) {
    bridge_->setTemplateMessage(message);
  }
}
```

- [ ] **Step 4: 添加 /api/templates 端点**

在 `registerRoutes()` 中：
```cpp
server_.on("/api/templates", HTTP_GET, [](AsyncWebServerRequest* request) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < kTemplateCount; ++i) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = kTemplates[i].id;
    o["name"] = kTemplates[i].name;
    o["message"] = kTemplates[i].message;
  }
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
});
```

- [ ] **Step 5: 添加 /api/preview 端点**

```cpp
server_.on("/api/preview", HTTP_GET, [this](AsyncWebServerRequest* request) {
  String tplId = request->getParam("template")->value();
  const PrintTemplate* tpl = findTemplate(tplId.c_str());

  JsonDocument doc;
  doc["template_id"] = tpl->id;
  doc["template_name"] = tpl->name;
  doc["message"] = tpl->message;
  doc["planned_s"] = core_ ? core_->plannedSeconds() : 0;

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
});
```

- [ ] **Step 6: 运行 native 测试确认无回归**

Run: `cd firmware && python -m platformio test -e native`
Expected: 所有测试 PASS

---

### Task 8: SPA — 模板选择 + 便签预览

**Covers:** 产品核心功能——模板选择 + 打印预览

**Files:**
- Modify: `firmware/lib/Net/WebAssets.cpp`

- [ ] **Step 1: 添加模板选择 UI**

在按钮区域下方添加：

```html
<div class="panel" style="margin-top:14px">
  <label>打印模板</label>
  <div id="templateList" class="grid" style="grid-template-columns:repeat(3,1fr)"></div>
</div>
```

- [ ] **Step 2: 添加便签预览区域**

```html
<div class="panel preview-panel" style="margin-top:14px">
  <label>便签预览</label>
  <div id="preview" style="font-family:monospace;background:#fafafa;padding:12px;
    border:1px dashed var(--line);border-radius:6px;white-space:pre-line;font-size:14px;
    line-height:1.6;min-height:120px">
    等待计时中...
  </div>
</div>
```

- [ ] **Step 3: 添加预览 CSS**

```css
.preview-panel{border:2px solid var(--accent);background:#f0f7fa}
.tpl-btn{height:36px;border:1px solid var(--line);border-radius:6px;background:#fff;
  font-size:13px;cursor:pointer;color:var(--ink);transition:all .15s}
.tpl-btn:hover,.tpl-btn.active{background:var(--accent);color:#fff;border-color:var(--accent)}
```

- [ ] **Step 4: JS — 加载模板列表并渲染**

```javascript
let templates = [];
let selectedTemplate = 'default';

async function loadTemplates() {
  try {
    const r = await fetch('/api/templates');
    templates = await r.json();
    renderTemplateButtons();
  } catch(e) { console.error('加载模板失败', e); }
}

function renderTemplateButtons() {
  const container = document.getElementById('templateList');
  container.innerHTML = '';
  templates.forEach(t => {
    const btn = document.createElement('button');
    btn.className = 'tpl-btn' + (t.id === selectedTemplate ? ' active' : '');
    btn.textContent = t.name;
    btn.title = t.message;
    btn.onclick = () => {
      selectedTemplate = t.id;
      document.querySelectorAll('.tpl-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      updatePreview();
      // 通知固件当前选择
      ws.send(JSON.stringify({cmd: 'template', message: t.message}));
    };
    container.appendChild(btn);
  });
}
```

- [ ] **Step 5: JS — 更新便签预览**

```javascript
function updatePreview() {
  const tpl = templates.find(t => t.id === selectedTemplate);
  if (!tpl) return;

  const planned = animState.planned || 0;
  const pMin = String(Math.floor(planned / 60)).padStart(2, '0');
  const pSec = String(planned % 60).padStart(2, '0');

  let text = '──────────────────────\n';
  if (planned > 0) {
    const elapsed = (animState.planned - animState.remaining) || 0;
    const eMin = String(Math.floor(elapsed / 60)).padStart(2, '0');
    const eSec = String(elapsed % 60).padStart(2, '0');
    const over = Math.max(0, -animState.remaining);
    const oMin = String(Math.floor(over / 60)).padStart(2, '0');
    const oSec = String(over % 60).padStart(2, '0');
    text += `  预计 planned : ${pMin}:${pSec}\n`;
    text += `  实际 actual  : ${eMin}:${eSec}\n`;
    text += `  超时 overrun : ${oMin}:${oSec}\n`;
  }
  text += '──────────────────────\n';
  text += `  ${tpl.message}\n`;
  text += '──────────────────────';

  document.getElementById('preview').textContent = text;
}
```

- [ ] **Step 6: 在 animLoop 中同步更新预览**

在 `animLoop()` 末尾调用 `updatePreview()`。

- [ ] **Step 7: 页面加载时初始化**

```javascript
loadTemplates();
```

- [ ] **Step 8: 验证**

在浏览器中：
- 页面加载后显示模板按钮列表
- 点击不同模板 → 预览区更新鼓励语
- 设置时间后 → 预览区显示完整便签格式
- 计时过程中 → 预览区实时更新实际用时

---

### Task 9: 固件 — main.cpp 集成

**Covers:** 串口调试 + 模板集成

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: 修改 WebControlAdapter 构造传入 bridge**

```cpp
static WebControlAdapter webControl(&core, &wifiManager, &printerBridge);
```

- [ ] **Step 2: 添加串口 `template` 命令**

在 `handleSerialCommand()` 中添加：

```cpp
} else if (command.startsWith("template ")) {
  String msg = command.substring(9);
  printerBridge.setTemplateMessage(msg.c_str());
  Serial.printf("> 模板已设置: %s\n", msg.c_str());
} else if (command == "template") {
  const char* cur = printerBridge.templateMessage();
  Serial.printf("> 当前模板: %s\n", cur ? cur : "(默认)");
```

- [ ] **Step 3: 更新 help 文本**

```cpp
Serial.println(F("命令: set <分钟> | start | pause | resume | stop | reset | hook | calib | status | template [消息] | wifilist | wifiset | wifireset | help"));
```

- [ ] **Step 4: 运行全部测试**

Run: `cd firmware && python -m platformio test -e native`
Expected: 所有测试 PASS

- [ ] **Step 5: 编译 ESP32 固件**

Run: `cd firmware && python -m platformio run -e esp32s3`
Expected: BUILD SUCCESS

---

### Task 10: 最终验证 + 提交

- [ ] **Step 1: 运行全部 native 测试**

Run: `cd firmware && python -m platformio test -e native`
Expected: 全部 PASS

- [ ] **Step 2: 编译 ESP32 固件**

Run: `cd firmware && python -m platformio run -e esp32s3`
Expected: BUILD SUCCESS

- [ ] **Step 3: 代码审查清单**

- [ ] TimerCore 未被修改
- [ ] PhysicalHook 未被修改
- [ ] 所有新代码在 `lib/` 下有正确命名空间 `timeprint`
- [ ] PROGMEM 用于字符串常量
- [ ] 无 ArduinoJson 动态分配泄漏
- [ ] native 测试覆盖新增逻辑
