#include "VolumeControl.h"

VolumeControl::VolumeControl()
    : _flowIn(nullptr)
    , _flowOut(nullptr)
    , _pump(nullptr)
    , _state(IDLE)
    , _targetVolume(0.0f)
    , _startVolume(0.0f)
    , _pumpSpeed(0)
    , _attached(false)
    , _containerVolume(0.0f)
    , _kp(5.0f)
    , _ki(0.5f)
    , _kd(0.0f)
    , _deadBand(0.1f)
    , _minPWM(90)
    , _maxPWM(255)
    , _integral(0.0f)
    , _lastError(0.0f)
    , _lastVolume(0.0f)
    , _pidReady(false)
    , _msCounter(0)
    , _onStart(nullptr)
    , _onComplete(nullptr)
    , _onStop(nullptr)
{
}

VolumeControl::~VolumeControl() {
}

void VolumeControl::attach(Flowsensor* flowInSensor, Flowsensor* flowOutSensor, Pumper* pumpMotor) {
    if (flowInSensor && flowOutSensor && pumpMotor) {
        _flowIn   = flowInSensor;
        _flowOut  = flowOutSensor;
        _pump     = pumpMotor;
        _attached = true;
    }
}

void VolumeControl::setContainerVolume(float liters) {
    _containerVolume = (liters > 0.0f) ? liters : 0.0f;
}

float VolumeControl::_getNetTotalVolume() const {
    if (!_attached) return 0.0f;
    return _flowIn->getTotalVolume() - _flowOut->getTotalVolume();
}

float VolumeControl::getLiquidVolume() const {
    return _getNetTotalVolume();
}

float VolumeControl::getAirVolume() const {
    if (_containerVolume <= 0.0f) return 0.0f;
    float air = _containerVolume - getLiquidVolume();
    return (air > 0.0f) ? air : 0.0f;
}

float VolumeControl::getInFlowTotal() const {
    if (!_attached) return 0.0f;
    return _flowIn->getTotalVolume();
}

float VolumeControl::getOutFlowTotal() const {
    if (!_attached) return 0.0f;
    return _flowOut->getTotalVolume();
}

float VolumeControl::getInFlow() const {
    if (!_attached) return 0.0f;
    return _flowIn->getFlowRate();
}

float VolumeControl::getOutFlow() const {
    if (!_attached) return 0.0f;
    return _flowOut->getFlowRate();
}

void VolumeControl::setTargetVolume(float liters) {
    _targetVolume = (liters >= 0.0f) ? liters : 0.0f;
}

void VolumeControl::setTargetAirVolume(float airLiters) {
    if (_containerVolume <= 0.0f) return;
    float targetLiquid = _containerVolume - airLiters;
    if (targetLiquid < 0.0f) targetLiquid = 0.0f;
    setTargetVolume(targetLiquid);
}

void VolumeControl::setTargetDelta(float deltaLiters) {
    if (!_hasOrigin) return;
    setTargetVolume(_originVolume + deltaLiters);
}

void VolumeControl::setOrigin() {
    _originVolume = getLiquidVolume();
    _hasOrigin    = true;
}

void VolumeControl::clearOrigin() {
    _originVolume = 0.0f;
    _hasOrigin    = false;
}

void VolumeControl::setPID(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void VolumeControl::setDeadBand(float liters) {
    _deadBand = (liters > 0.0f) ? liters : 0.0f;
}

void VolumeControl::setMinPWM(int16_t minPwm) {
    _minPWM = (int16_t)constrain((int)minPwm, 0, 255);
}

void VolumeControl::setMaxPWM(int16_t maxPwm) {
    _maxPWM = (int16_t)constrain((int)maxPwm, 0, 255);
}

void VolumeControl::setPumpSpeed(int16_t speed) {
    _pumpSpeed = constrain(speed, -255, 255);
    if (_state == RUNNING) {
        _pump->setSpeed(_pumpSpeed);
    }
}

void VolumeControl::start() {
    if (!_attached || _targetVolume < 0.0f) {
        _state = ERROR;
        return;
    }

    float currentLiquid = getLiquidVolume();
    _startVolume = currentLiquid;

    if (fabsf(currentLiquid - _targetVolume) <= _deadBand) {
        _state = COMPLETED;
        if (_onComplete) _onComplete();
        return;
    }

    _integral   = 0.0f;
    _lastError  = _targetVolume - currentLiquid;
    _lastVolume = currentLiquid;
    _pidReady   = false;

    _state = RUNNING;
    if (_onStart) _onStart();
}

void VolumeControl::stop() {
    _followMode = false;
    _stopPumpInternal();
    if (_state == RUNNING || _state == COMPLETED) {
        _state = STOPPED;
        if (_onStop) _onStop();
    }
}

void VolumeControl::reset() {
    if (!_attached) return;
    _stopPumpInternal();
    _state        = IDLE;
    _targetVolume = 0.0f;
    _integral     = 0.0f;
    _lastError    = 0.0f;
    _lastVolume   = 0.0f;
    _pidReady     = false;
    _flowIn->clearCount();
    _flowOut->clearCount();
}

void VolumeControl::update() {
    if (!_attached || _state != RUNNING) return;
    if (!_pidReady) return;
    _pidReady = false;

    const float dt = 0.2f;  // 固定 200ms 周期

    float currentVolume = getLiquidVolume();
    float error = _targetVolume - currentVolume;

    if (fabsf(error) <= _deadBand) {
        _stopPumpInternal();
        _integral = 0.0f;
        if (_followMode) return;   // 跟随模式：只停泵，保持 RUNNING 等新目标
        _state = COMPLETED;
        if (_onComplete) _onComplete();
        return;
    }

    // P 项
    float p = _kp * error;

    // I 项（先试算，抗积分饱和后决定是否采纳）
    float iCandidate = _integral + error * dt;

    // D 项：error 数值差分，等价于体积变化率的负值
    float d = _kd * (error - _lastError) / dt;

    float output = p + _ki * iCandidate + d;

    // 抗积分饱和：输出饱和且误差同向时不累积积分
    bool saturatedPos = (output >=  (float)_maxPWM && error > 0.0f);
    bool saturatedNeg = (output <= -(float)_maxPWM && error < 0.0f);
    if (!saturatedPos && !saturatedNeg) {
        _integral = iCandidate;
    }

    // 映射到有效 PWM 区间（跳过泵死区）
    int16_t pwm;
    if (output >= _minPWM) {
        pwm = (int16_t)constrain((int32_t)output,  (int32_t)_minPWM,  (int32_t)_maxPWM);
    } else if (output <= -_minPWM) {
        pwm = (int16_t)constrain((int32_t)output, -(int32_t)_maxPWM, -(int32_t)_minPWM);
    } else {
        // 输出幅度不足以驱动泵，但误差尚未进入死区——以最小 PWM 维持方向
        pwm = (error > 0.0f) ? _minPWM : -_minPWM;
    }

    _pumpSpeed = pwm;
    _pump->setSpeed(pwm);

    _lastError  = error;
    _lastVolume = currentVolume;
}

void IRAM_ATTR VolumeControl::onPidTick() {
    _msCounter++;
    if (_msCounter >= 200) {   // 200ms 周期
        _msCounter = 0;
        _pidReady  = true;     // 浮点 PID 计算推迟到 update() 主循环执行
    }
}

float VolumeControl::getCurrentVolume() const {
    if (!_attached) return 0.0f;
    return getLiquidVolume();
}

float VolumeControl::getRemainingVolume() const {
    if (!_attached || _targetVolume <= 0.0f) return 0.0f;
    return fabsf(_targetVolume - getLiquidVolume());
}

float VolumeControl::getProgressPercent() const {
    if (!_attached || _state == IDLE) return 0.0f;
    float totalDist = fabsf(_targetVolume - _startVolume);
    if (totalDist < 0.001f) return 100.0f;
    float traveled = fabsf(getLiquidVolume() - _startVolume);
    return constrain((traveled / totalDist) * 100.0f, 0.0f, 100.0f);
}

void VolumeControl::_stopPumpInternal() {
    if (_pump) _pump->setSpeed(0);
}
