#pragma once
#include <stdint.h>
#include <stddef.h>

// TimerCore: 纯逻辑计时状态机。
// - 零硬件依赖,可在 PC(native)与 ESP32 上编译。
// - 与输入来源无关(物理旋钮 / WiFi / BLE 都只调用同一套 event)。
// - 与输出去向无关(打印机 / WebSocket / BLE notify 都通过 ITimerCoreListener 接收)。
// - 不依赖 wall-clock:通过 tick() 推进每 1 秒,保证可确定性测试。

namespace timeprint {

enum class State : uint8_t {
  Idle,      // 待机
  Running,   // 计时中(红盘收缩)
  Overtime,  // 超时(红->白,计超时量)
  Paused,    // 暂停
  Finished   // 停止瞬态(发出便签后立即回到 Idle)
};

enum class Feedback : uint8_t {
  Start,
  TimeUp,  // 剩余时间归零的瞬间(红->白切换点)
  Stop,
  Reset
};

struct TickInfo {
  uint32_t plannedSeconds;
  uint32_t elapsedSeconds;
  uint32_t remainingSeconds;
  uint32_t overrunSeconds;
};

struct SlipData {
  uint32_t plannedSeconds;  // 预计用时
  uint32_t actualSeconds;   // 实际用时
  uint32_t overrunSeconds;  // 超时量
};

// 输出 adapter 实现此接口并注册到 core。所有方法默认空实现,按需重写。
class ITimerCoreListener {
 public:
  virtual ~ITimerCoreListener() {}
  virtual void onStateChanged(State /*from*/, State /*to*/) {}
  virtual void onTick(const TickInfo& /*info*/) {}
  virtual void onPrintSlip(const SlipData& /*slip*/) {}
  virtual void onFeedback(Feedback /*fb*/) {}
};

class TimerCore {
 public:
  static const int kMaxListeners = 4;

  TimerCore();

  bool addListener(ITimerCoreListener* listener);

  // ---- 事件(与来源无关)----
  void setTime(uint32_t minutes);  // 仅 Idle 态生效
  void start();                    // Idle 且已设时间 -> Running
  void pause();                    // Running/Overtime -> Paused
  void resume();                   // Paused -> 之前的态
  void stop();                     // 计时中 -> 发便签 -> Idle
  void reset();                    // 任意态 -> Idle 并清零
  void tick();                     // 推进 1 秒

  // ---- 只读访问 ----
  State state() const { return state_; }
  uint32_t plannedSeconds() const { return plannedSeconds_; }
  uint32_t elapsedSeconds() const { return elapsedSeconds_; }
  uint32_t remainingSeconds() const;
  uint32_t overrunSeconds() const;

 private:
  void transitionTo(State next);
  void emitTick();
  void emitFeedback(Feedback fb);
  void emitSlip();

  State state_;
  State pausedFrom_;  // 暂停前是 Running 还是 Overtime
  uint32_t plannedSeconds_;
  uint32_t elapsedSeconds_;
  ITimerCoreListener* listeners_[kMaxListeners];
  int listenerCount_;
};

}  // namespace timeprint
