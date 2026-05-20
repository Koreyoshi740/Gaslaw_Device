#pragma once
#include <Arduino.h>

// 调用 keyTick() 的定时器周期（ms）
static constexpr uint16_t KEY_TICK_MS       = 1;
// 采样间隔：每 SAMPLE_TICKS 次 tick 采样一次
static constexpr uint16_t KEY_SAMPLE_TICKS  = 50;
// 双击最大间隔（采样次数）—— 设为 0 禁用双击，松开即触发 SINGLE
static constexpr uint16_t KEY_DOUBLE_TICKS  = 200;
// 长按触发阈值（采样次数）
static constexpr uint16_t KEY_LONG_TICKS    = 500 ;
// 连发间隔（采样次数）
static constexpr uint16_t KEY_REPEAT_TICKS  = 70 ;

enum KeyEvent : uint8_t {
    KEY_HOLD   = 0x01,
    KEY_DOWN   = 0x02,
    KEY_UP     = 0x04,
    KEY_SINGLE = 0x08,
    KEY_DOUBLE = 0x10,
    KEY_LONG   = 0x20,
    KEY_REPEAT = 0x40,
};

class Key {
public:
    void attach(uint8_t pin);

    // 放在硬件定时器 ISR 中调用，每 KEY_TICK_MS 调用一次
    void IRAM_ATTR keyTick();

    // 检查并消费事件，Hold 事件不消费（持续有效）
    bool keyCheck(KeyEvent flag);

private:
    uint8_t  _pin        = 0;
    volatile uint8_t  _keyFlag    = 0;
    uint16_t _tickCount  = 0;
    uint16_t _timer      = 0;
    uint8_t  _state      = 0;
    uint8_t  _curState   = 0;
    uint8_t  _lastState  = 0;

    uint8_t IRAM_ATTR readPin() const;
    void    sample();
};
