#include "TempControl.h"

TempControl::TempControl()
    : _bme(nullptr)
    , _heater(nullptr)
    , _coolPump(nullptr)
    , _airPump(nullptr)
    , _attached(false)
    , _state(IDLE)
    , _targetTemp(25.0f)
    , _hysteresis(0.5f)
    , _holdingBand(3.0f)
    , _maxTemp(80.0f)
    , _kp(8.0f)
    , _ki(0.02f)
    , _kd(10.0f)
    , _integral(0.0f)
    , _lastError(0.0f)
    , _switchDelta(5.0f)
    , _effectiveSwitchDelta(5.0f)
    , _coastBand(2.0f)
    , _coastTimeout(60000)
    , _coastStartMs(0)
    , _coastPrevTemp(NAN)
    , _cooldownTargetTemp(25.0f)
    , _cooldownDrainingTime(120000)
    , _cooldownMaxTime(1800000)
    , _cooldownStartMs(0)
    , _cooldownState(COOLDOWN_IDLE)
    , _circulationInterval(10000)
    , _circulationDuration(3000)
    , _lastCirculationMs(0)
    , _circulationStartMs(0)
    , _holdCirculationInterval(15000)
    , _holdCirculationDuration(2000)
    , _holdLastCirculationMs(0)
    , _holdCirculationStartMs(0)
    , _tickMs(0)
    , _doUpdate(false)
    , _fanCooldownMs(30000)
    , _heaterOffMs(0)
    , _reachFired(false)
    , _fanCooling(false)
    , _onStart(nullptr)
    , _onReach(nullptr)
    , _onStop(nullptr)
    , _onOverTemp(nullptr)
    , _onCooldownComplete(nullptr)
{}

TempControl::~TempControl() {}

void TempControl::attach(BME280Sensor* bme, Heater* heater) {
    if (!bme || !heater) return;
    _bme     = bme;
    _heater  = heater;
    _attached = true;
    _heater->off();
}

void TempControl::attachCoolPump(Pumper* coolPump) { _coolPump = coolPump; }
void TempControl::attachAirPump(AirPump* airPump)  { _airPump  = airPump;  }

void TempControl::setTargetTemp(float temp)       { _targetTemp    = temp; }
void TempControl::setHysteresis(float hysteresis) { if (hysteresis > 0.0f) _hysteresis = hysteresis; }
void TempControl::setMaxTemp(float maxTemp)        { _maxTemp       = maxTemp; }
void TempControl::setFanCooldownMs(uint32_t ms)    { _fanCooldownMs = ms; }
void TempControl::setSwitchDelta(float degC)       { if (degC > 0.0f) _switchDelta = degC; }
void TempControl::setCoastBand(float degC)         { if (degC > 0.0f) _coastBand   = degC; }
void TempControl::setCoastTimeout(uint32_t ms)     { _coastTimeout  = ms; }

void TempControl::setPID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void TempControl::setHoldingBand(float degC) {
    if (degC > 0.0f) _holdingBand = degC;
}

void TempControl::setCooldownDrainingTime(uint32_t ms) { _cooldownDrainingTime = ms; }
void TempControl::setCooldownMaxTime(uint32_t ms)      { _cooldownMaxTime = ms; }

void TempControl::setCirculationInterval(uint32_t ms) { _circulationInterval = ms; }
void TempControl::setCirculationDuration(uint32_t ms) { _circulationDuration = ms; }

void TempControl::setHoldCirculationInterval(uint32_t ms) { _holdCirculationInterval = ms; }
void TempControl::setHoldCirculationDuration(uint32_t ms) { _holdCirculationDuration = ms; }

void TempControl::coolPumpOn() {
    if (_coolPump) {
        if (_cooldownState != COOLDOWN_IDLE) stopCooldown();
        _coolPump->setSpeed(255);
    }
}

void TempControl::coolPumpOff() {
    if (_coolPump) {
        if (_cooldownState != COOLDOWN_IDLE) stopCooldown();
        _coolPump->stop();
    }
}

void TempControl::startCooldown(float targetTemp) {
    if (!_coolPump) return;
    _cooldownTargetTemp = targetTemp;
    _cooldownState = COOLDOWN_COOLING;
    _cooldownStartMs = millis();
    if (_heater)  _heater->heaterOff();
    if (_heater)  _heater->fanOn();
    _coolPump->setSpeed(255);
}

void TempControl::stopCooldown() {
    if (_coolPump) _coolPump->stop();
    if (_heater)  _heater->fanOff();
    _cooldownState = COOLDOWN_IDLE;
    if (_state == ERROR) _state = IDLE;
}

void TempControl::airPumpOn()  { if (_airPump) _airPump->on();  }
void TempControl::airPumpOff() { if (_airPump) _airPump->off(); }

float TempControl::getCurrentTemp() {
    if (!_attached) return NAN;
    return _bme->getTemperature();
}

float TempControl::getTempError() const {
    if (!_attached) return NAN;
    return _bme->getTemperature() - _targetTemp;
}

// ---------- 超温保护 ----------
void TempControl::_triggerOverTemp() {
    _heater->heaterOff();
    _heater->fanOn();
    if (_airPump) _airPump->on();
    if (_coolPump && _cooldownState == COOLDOWN_IDLE) {
        startCooldown(25.0f);
    }
    _state = ERROR;
    if (_onOverTemp) _onOverTemp();
}

// ---------- 进入 HEATING ----------
void TempControl::_enterHeating() {
    _heater->heaterOn();
    _heater->fanOn();
    if (_airPump) {
        _airPump->off();
        _lastCirculationMs = millis();
    }
    _state = HEATING;
}

// ---------- 进入 COASTING ----------
void TempControl::_enterCoasting() {
    _heater->heaterOff();
    _heater->fanOn();
    if (_airPump) _airPump->off();
    _coastStartMs   = millis();
    _coastPrevTemp  = _bme->getTemperature();
    _state = COASTING;
}

// ---------- 进入 HOLDING ----------
void TempControl::_enterHolding() {
    _heater->fanOn();
    // 重置 PID 状态
    float currentTemp = _bme->getTemperature();
    _integral  = 0.0f;
    _lastError = isnan(currentTemp) ? 0.0f : (_targetTemp - currentTemp);
    _state = HOLDING;
    if (!_reachFired) { _reachFired = true; if (_onReach) _onReach(); }
}

// ---------- 进入 COOLING ----------
void TempControl::_enterCooling() {
    _heater->heaterOff();
    _heater->fanOn();
    _state = COOLING;
}

// ---------- PID 核心（仅 HOLDING 使用） ----------
// error > 0 偏低 → 加热；error < 0 偏高 → 输出 0 等待自然散热，不主动切 COOLING
void TempControl::_applyPID(float currentTemp) {
    static const float dt = SAMPLE_PERIOD_MS / 1000.0f;

    float error = _targetTemp - currentTemp;

    // 超出迟滞带才切 COOLING
    if (error < -_hysteresis * 4.0f) {
        _enterCooling();
        return;
    }

    // 温度偏高但未超迟滞带：停止加热，等自然散热
    if (error < 0.0f) {
        _heater->heaterOff();
        _integral  = 0.0f;
        _lastError = error;
        return;
    }

    float p          = _kp * error;
    float d          = _kd * (error - _lastError) / dt;
    float iCandidate = _integral + error * dt;

    float rawOutput = p + _ki * iCandidate + d;
    float output    = constrain(rawOutput, 0.0f, 255.0f);

    // 抗积分饱和
    bool satHigh = (rawOutput > 255.0f && error > 0.0f);
    bool satLow  = (rawOutput < 0.0f   && error < 0.0f);
    if (!satHigh && !satLow) _integral = iCandidate;

    _lastError = error;
    _heater->setHeaterSpeed((uint8_t)output);
}

void TempControl::start() {
    if (!_attached) { _state = ERROR; return; }

    float currentTemp = _bme->getTemperature();
    if (isnan(currentTemp)) { _state = ERROR; return; }

    if (currentTemp >= _maxTemp) {
        _triggerOverTemp();
        return;
    }

    if (_fanCooling && _state == COOLING) {
        _heater->fanOff();
        _fanCooling = false;
    }

    _reachFired = false;
    _fanCooling  = false;

    float error = _targetTemp - currentTemp;

    // 误差 > 5℃ 时按比例放大提前断电距离，保持 switchDelta/5 的比例
    _effectiveSwitchDelta = _switchDelta;

    if (error > _effectiveSwitchDelta) {
        // 误差大于提前断电距离，正常从 HEATING 开始
        _enterHeating();
    } else if (error > 0.0f) {
        // 误差已在 effectiveSwitchDelta 以内，直接进 COASTING 让惯性消耗
        _enterCoasting();
    } else if (error < -_hysteresis) {
        _enterCooling();
    } else {
        _enterHolding();
    }

    if (_onStart) _onStart();
}

void TempControl::stop() {
    if (_cooldownState != COOLDOWN_IDLE) stopCooldown();
    if (_state == IDLE) return;
    _heater->heaterOff();
    _heater->fanOn();
    _heaterOffMs = millis();
    _fanCooling  = true;
    _state = COOLING;
    if (_onStop) _onStop();
}

void TempControl::emergencyStop() {
    _heater->off();
    if (_coolPump) _coolPump->stop();
    if (_airPump) _airPump->off();
    _fanCooling    = false;
    _cooldownState = COOLDOWN_IDLE;
    _state         = IDLE;
}

void IRAM_ATTR TempControl::onTimer() {
    _tickMs++;
    if (_tickMs >= SAMPLE_PERIOD_MS) {
        _tickMs   = 0;
        _doUpdate = true;
    }
}

void TempControl::update() {
    if (!_attached) return;

    // ── 1. stop() 触发的散热延迟 ──
    if (_fanCooling && _state == COOLING) {
        if (millis() - _heaterOffMs >= _fanCooldownMs) {
            _heater->fanOff();
            _fanCooling = false;
            _state      = IDLE;
        }
        return;
    }

    // ── 2. 冷却水泵状态机 ──
    if (_cooldownState != COOLDOWN_IDLE && _coolPump) {
        uint32_t now         = millis();
        float    currentTemp = _bme->getTemperature();

        if (_cooldownState == COOLDOWN_COOLING) {
            if (now - _cooldownStartMs >= _cooldownMaxTime) {
                _coolPump->stop();
                _cooldownState = COOLDOWN_IDLE;
                if (_state == ERROR) _state = IDLE;
            } else if (isnan(currentTemp)) {
                _coolPump->stop();
                _cooldownState = COOLDOWN_IDLE;
                if (_state == ERROR) _state = IDLE;
            } else if (currentTemp <= _cooldownTargetTemp) {
                _coolPump->setSpeed(-255);
                _cooldownStartMs = now;
                _cooldownState   = COOLDOWN_DRAINING;
                if (_state == ERROR) {
                    _heater->fanOff();
                    if (_airPump) _airPump->off();
                }
            }
        } else if (_cooldownState == COOLDOWN_DRAINING) {
            if (now - _cooldownStartMs >= _cooldownDrainingTime) {
                _coolPump->stop();
                if (_state != ERROR) {
                    if (_heater) _heater->fanOff();
                }
                _cooldownState = COOLDOWN_IDLE;
                if (_state == ERROR) _state = IDLE;
                if (_onCooldownComplete) _onCooldownComplete();
            }
        }
    }

    // ── 3. ERROR 状态：等待降温流程完成 ──
    if (_state == ERROR) return;

    // ── 5. 主温控逻辑（定时采样）──
    if (_state != HEATING && _state != COASTING && _state != HOLDING && _state != COOLING) return;
    if (!_doUpdate) return;
    _doUpdate = false;

    float currentTemp = _bme->getTemperature();
    if (isnan(currentTemp)) return;

    if (currentTemp >= _maxTemp) {
        _triggerOverTemp();
        return;
    }

    if (_state == HEATING) {
        // 到达提前断电点，进 COASTING
        if (currentTemp >= _targetTemp - _effectiveSwitchDelta) {
            _enterCoasting();
        }

    } else if (_state == COASTING) {
        uint32_t now = millis();
        float error  = _targetTemp - currentTemp;

        // 计算 dT/dt
        float rate = 0.0f;
        if (!isnan(_coastPrevTemp)) {
            rate = (currentTemp - _coastPrevTemp) / (SAMPLE_PERIOD_MS / 1000.0f);
        }
        _coastPrevTemp = currentTemp;

        bool tempStable  = (rate < 0.1f && rate > -0.1f);  // 升温趋于停止
        bool inBand      = (fabsf(error) <= _coastBand);
        bool timedOut    = (now - _coastStartMs >= _coastTimeout);

        if (timedOut || (tempStable && inBand)) {
            // 温度在目标附近且稳定，或超时 → 切 HOLDING
            _enterHolding();
        } else if (error < -_coastBand && tempStable) {
            // 惯性已消耗但温度仍明显偏高 → 直接切 COOLING
            _enterCooling();
        }

    } else if (_state == HOLDING) {
        _applyPID(currentTemp);

    } else {    // COOLING
        if (currentTemp <= _targetTemp + _hysteresis) {
            _enterHolding();
        }
    }
}
