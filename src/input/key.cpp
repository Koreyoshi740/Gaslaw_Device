#include "key.h"

void Key::attach(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT);
}

bool Key::keyCheck(KeyEvent flag) {
    if (_keyFlag & flag) {
        if (flag != KEY_HOLD)
            _keyFlag &= ~flag;
        return true;
    }
    return false;
}

uint8_t IRAM_ATTR Key::readPin() const {
    return (digitalRead(_pin) == LOW) ? 1 : 0;
}

void IRAM_ATTR Key::keyTick() {
    if (_timer > 0) { _timer--; }

    _tickCount++;
    if (_tickCount < KEY_SAMPLE_TICKS) return;
    _tickCount = 0;

    // 采样
    _lastState = _curState;
    _curState  = readPin();

    // Hold / Down / Up 事件
    if (_curState) _keyFlag |=  KEY_HOLD;
    else           _keyFlag &= ~KEY_HOLD;

    if (_curState && !_lastState) _keyFlag |= KEY_DOWN;
    if (!_curState && _lastState) _keyFlag |= KEY_UP;

    // 状态机：Single / Double / Long / Repeat
    switch (_state) {
        case 0:
            if (_curState) {
                _timer = KEY_LONG_TICKS;
                _state = 1;
            }
            break;
        case 1:
            if (!_curState) {
                _timer = KEY_DOUBLE_TICKS;
                _state = 2;
            } else if (_timer == 0) {
                _timer = KEY_REPEAT_TICKS;
                _keyFlag |= KEY_LONG;
                _state = 4;
            }
            break;
        case 2:
            if (_curState) {
                _keyFlag |= KEY_DOUBLE;
                _state = 3;
            } else if (_timer == 0) {
                _keyFlag |= KEY_SINGLE;
                _state = 0;
            }
            break;
        case 3:
            if (!_curState) _state = 0;
            break;
        case 4:
            if (!_curState) {
                _state = 0;
            } else if (_timer == 0) {
                _timer = KEY_REPEAT_TICKS;
                _keyFlag |= KEY_REPEAT;
            }
            break;
    }
}
