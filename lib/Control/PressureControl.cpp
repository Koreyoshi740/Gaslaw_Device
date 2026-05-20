#include "PressureControl.h"

PressureControl::PressureControl()
    : _bme(nullptr)
    , _volumeCtrl(nullptr)
    , _attached(false)
    , _state(IDLE)
    , _targetPressure(0.0f)
    , _deadBand(0.5f)
    , _kp(0.25f)
    , _ki(0.05f)
    , _kd(0.0f)
    , _integral(0.0f)
    , _lastError(0.0f)
    , _maxDeltaV(50.0f)    // 滚动目标模式：每100ms最大体积移动量上限
    , _tickMs(0)
    , _doUpdate(false)
    , _onStart(nullptr)
    , _onReach(nullptr)
    , _onStop(nullptr)
    , _reachFired(false)
{}

PressureControl::~PressureControl() {}

void PressureControl::attach(BME280Sensor* bme, VolumeControl* volumeCtrl) {
    if (!bme || !volumeCtrl) return;
    _bme        = bme;
    _volumeCtrl = volumeCtrl;
    _attached   = true;
}

void PressureControl::setTargetPressure(float hPa) { _targetPressure = hPa; }

void PressureControl::setPID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void PressureControl::setMaxDeltaV(float ml) {
    if (ml > 0.0f) _maxDeltaV = ml;
}

void PressureControl::setDeadBand(float hPa) {
    if (hPa > 0.0f) _deadBand = hPa;
}

float PressureControl::getCurrentPressure() {
    if (!_attached) return NAN;
    return _bme->getPressure();
}

float PressureControl::getPressureError() const {
    if (!_attached) return NAN;
    return _bme->getPressure() - _targetPressure;
}

void PressureControl::start() {
    if (!_attached || _targetPressure <= 0.0f) {
        _state = ERROR;
        return;
    }

    float currentP = _bme->getPressure();
    if (isnan(currentP) || currentP <= 0.0f) {
        _state = ERROR;
        return;
    }

    _reachFired = false;
    _integral   = 0.0f;
    _lastError  = _targetPressure - currentP;  // 避免首次D项跳变
    _state      = RUNNING;
    _volumeCtrl->setFollowMode(true);

    _applyPID(currentP);

    if (_onStart) _onStart();
}

void PressureControl::stop() {
    if (_state == IDLE) return;
    _volumeCtrl->stop();
    _state = IDLE;
    if (_onStop) _onStop();
}

void PressureControl::reset() {
    _volumeCtrl->reset();
    _state      = IDLE;
    _reachFired = false;
    _integral   = 0.0f;
    _lastError  = 0.0f;
}

// -----------------------------------------------------------------------
// PID核心（滚动目标模式）：
//   外环 100ms 触发一次，输出 ΔV(mL) 作为"本轮期望体积移动量"
//   直接更新 volumeControl 的目标并保持其运行，不等完成
//   error > 0 → 压强偏低 → ΔV < 0（压缩气体升压）
//   error < 0 → 压强偏高 → ΔV > 0（释放气体降压）
// -----------------------------------------------------------------------
void PressureControl::_applyPID(float currentP) {
    static const float dt = SAMPLE_PERIOD_MS / 1000.0f;  // 0.1 s

    float error = _targetPressure - currentP;

    if (fabsf(error) <= _deadBand) {
        _state = HOLDING;
        _volumeCtrl->stop();   // 进死区才停泵
        if (!_reachFired) {
            _reachFired = true;
            if (_onReach) _onReach();
        }
        return;
    }

    _state = RUNNING;
    _reachFired = false;  // 离开死区后允许再次触发 onReach

    float p          = _kp * error;
    float d          = _kd * (error - _lastError) / dt;
    float iCandidate = _integral + error * dt;

    // 反向：压强误差与体积增量方向相反
    // error > 0 → 压强偏低 → ΔV < 0（压缩气体升压）
    // error < 0 → 压强偏高 → ΔV > 0（释放气体降压）
    float rawOutput = -(p + _ki * iCandidate + d);
    float output    = constrain(rawOutput, -_maxDeltaV, _maxDeltaV);

    // 抗积分饱和
    bool satNeg = (rawOutput < -_maxDeltaV && error > 0.0f);
    bool satPos = (rawOutput >  _maxDeltaV && error < 0.0f);
    if (!satNeg && !satPos) _integral = iCandidate;

    _lastError = error;

    Serial.printf("[PressureCtrl] P=%.2f target=%.2f err=%.2f out=%.2f mL\n",
                  currentP, _targetPressure, error, output);

    float currentAir = _volumeCtrl->getAirVolume();
    float targetAir  = constrain(currentAir + output, 0.001f, _volumeCtrl->getContainerVolume());
    _volumeCtrl->setTargetAirVolume(targetAir);
    if (!_volumeCtrl->isRunning()) _volumeCtrl->start();
}

void IRAM_ATTR PressureControl::onTimer() {
    _tickMs++;
    if (_tickMs >= SAMPLE_PERIOD_MS) {
        _tickMs   = 0;
        _doUpdate = true;
    }
}

void PressureControl::update() {
    if (!_attached || (_state != RUNNING && _state != HOLDING)) return;
    if (!_doUpdate) return;
    _doUpdate = false;

    // 滚动目标模式：不等 volumeControl 停止，直接采样并更新目标
    float currentP = _bme->getPressure();
    if (isnan(currentP)) return;

    _applyPID(currentP);
}
