#include "GayLussacLaw.h"

const float GayLussacLaw::AUTO_TEMP_STEPS[7] = {
    30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f
};

GayLussacLaw::GayLussacLaw()
    : _bme(nullptr)
    , _volumeCtrl(nullptr)
    , _tempCtrl(nullptr)
    , _pressureCtrl(nullptr)
    , _airValve(nullptr)
    , _attached(false)
    , _state(IDLE)
    , _stepMode(AUTO)
    , _currentStep(0)
    , _stepCount(0)
    , _dataCount(0)
    , _largeBottleCapacity(325.0f)
    , _smallBottleCapacity(135.0f)
    , _componentVolume(10.0f)
    , _fixedVolume(125.0f)
    , _totalTargetVolume(250.0f)
    , _lockedVolume(0.0f)
    , _targetPressure(0.0f)
    , _customPressure(false)
    , _paused(false)
    , _pauseStartMs(0)
    , _nodeSettleMs(1000)
    , _nodeReachedMs(0)
    , _initVolumeTimeoutMs(120000)
    , _initVolumeStartMs(0)
    , _heatingTimeoutMs(600000)
    , _heatingStartMs(0)
    , _onStart(nullptr)
    , _onStep(nullptr)
    , _onDone(nullptr)
    , _onError(nullptr)
{
    memset(_tempSteps, 0, sizeof(_tempSteps));
    memset(_data, 0, sizeof(_data));
}

GayLussacLaw::~GayLussacLaw() {}

void GayLussacLaw::attach(BME280Sensor* bme, VolumeControl* volumeCtrl,
                          TempControl* tempCtrl, PressureControl* pressureCtrl) {
    if (!bme || !volumeCtrl || !tempCtrl || !pressureCtrl) return;
    _bme          = bme;
    _volumeCtrl   = volumeCtrl;
    _tempCtrl     = tempCtrl;
    _pressureCtrl = pressureCtrl;
    _attached     = true;
}

void GayLussacLaw::attachAirValve(AirValve* airValve) { _airValve = airValve; }

void GayLussacLaw::setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl) {
    _largeBottleCapacity = largeBottleMl;
    _smallBottleCapacity = smallBottleMl;
    _componentVolume     = componentVolumeMl;
    _fixedVolume         = smallBottleMl - componentVolumeMl;
}

void GayLussacLaw::setTotalTargetVolume(float ml)      { _totalTargetVolume   = ml; }
void GayLussacLaw::setStepMode(StepMode mode)          { _stepMode            = mode; }
void GayLussacLaw::setNodeSettleMs(uint32_t ms)        { _nodeSettleMs        = ms; }
void GayLussacLaw::setInitVolumeTimeoutMs(uint32_t ms) { _initVolumeTimeoutMs = ms; }
void GayLussacLaw::setHeatingTimeoutMs(uint32_t ms)    { _heatingTimeoutMs    = ms; }

void GayLussacLaw::setTargetPressure(float hPa) {
    _targetPressure = hPa;
    _customPressure = true;
}

void GayLussacLaw::setTempSteps(const float* steps, uint8_t count) {
    _stepCount = min(count, (uint8_t)MAX_STEPS);
    for (uint8_t i = 0; i < _stepCount; i++) _tempSteps[i] = steps[i];
}

bool GayLussacLaw::addTempStep(float degC) {
    if (_stepCount >= MAX_STEPS) return false;
    _tempSteps[_stepCount++] = degC;
    return true;
}

void GayLussacLaw::clearTempSteps() {
    _stepCount = 0;
    memset(_tempSteps, 0, sizeof(_tempSteps));
}

void GayLussacLaw::setPaused(bool paused) {
    if (_paused == paused) return;
    _paused = paused;

    if (paused) {
        _pauseStartMs = millis();
        switch (_state) {
            case INIT_VOLUME:
                if (_volumeCtrl) _volumeCtrl->stop();
                break;
            case STEPPING:
            case RECORDING:
                if (_tempCtrl)     _tempCtrl->stop();
                if (_pressureCtrl) _pressureCtrl->stop();
                break;
            default: break;
        }
        return;
    }

    uint32_t pauseDur = millis() - _pauseStartMs;
    _initVolumeStartMs += pauseDur;
    _heatingStartMs    += pauseDur;
    _nodeReachedMs     += pauseDur;

    switch (_state) {
        case INIT_VOLUME:
            if (_volumeCtrl) {
                _volumeCtrl->setTargetAirVolume(_totalTargetVolume);
                _volumeCtrl->start();
            }
            break;
        case STEPPING:
        case RECORDING:
            if (_pressureCtrl) {
                _pressureCtrl->setTargetPressure(_targetPressure);
                _pressureCtrl->start();
            }
            if (_tempCtrl && _stepCount > 0) {
                _tempCtrl->setTargetTemp(_tempSteps[_stepCount - 1]);
                _tempCtrl->start();
            }
            break;
        default: break;
    }
}

// -----------------------------------------------------------------------
// 控制流程
// -----------------------------------------------------------------------

void GayLussacLaw::start() {
    if (!_attached) {
        _state = ERROR;
        if (_onError) _onError();
        return;
    }

    _volumeCtrl->stop();
    _pressureCtrl->stop();
    _tempCtrl->stop();

    _currentStep  = 0;
    _dataCount    = 0;
    _lockedVolume = 0.0f;
    _nodeReachedMs = 0;

    if (_stepMode == AUTO) _loadAutoSteps();

    if (_airValve) _airValve->open();

    _volumeCtrl->setTargetAirVolume(_totalTargetVolume);
    _volumeCtrl->start();
    _initVolumeStartMs = millis();
    _state = INIT_VOLUME;

    if (_onStart) _onStart();
}

void GayLussacLaw::openValve() {
    if (_state != DONE && _state != IDLE && _state != ERROR) return;
    if (_airValve) _airValve->open();
}

void GayLussacLaw::closeValve() {
    if (_airValve) _airValve->close();
}

void GayLussacLaw::startCooldown(float targetTemp) {
    if (_state != DONE) return;
    if (_tempCtrl) _tempCtrl->startCooldown(targetTemp);
}

void GayLussacLaw::stop() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    _pressureCtrl->stop();
    _tempCtrl->stop();
    if (_airValve) _airValve->open();
    _state = IDLE;
}

void GayLussacLaw::reset() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    _pressureCtrl->stop();
    _tempCtrl->stop();
    if (_airValve) _airValve->open();
    _state        = IDLE;
    _currentStep  = 0;
    _dataCount    = 0;
    _lockedVolume = 0.0f;
    _paused       = false;
    if (_stepMode == MANUAL) clearTempSteps();
    memset(_data, 0, sizeof(_data));
}

// -----------------------------------------------------------------------
// 私有辅助
// -----------------------------------------------------------------------

void GayLussacLaw::_loadAutoSteps() {
    _stepCount = sizeof(AUTO_TEMP_STEPS) / sizeof(AUTO_TEMP_STEPS[0]);
    for (uint8_t i = 0; i < _stepCount; i++) _tempSteps[i] = AUTO_TEMP_STEPS[i];
}

// 关阀后：确定恒压目标，启动 pressureCtrl，然后加热到最高节点
void GayLussacLaw::_prepareAndGo() {
    if (_stepMode == AUTO && _stepCount == 0) _loadAutoSteps();

    // 确定恒压目标
    if (!_customPressure) {
        _targetPressure = _bme->getPressure();
    }
    _pressureCtrl->setTargetPressure(_targetPressure);
    _pressureCtrl->start();
    _state = PRES_INIT;
}

float GayLussacLaw::_getTotalVolume() const {
    return _volumeCtrl->getAirVolume();
}

void GayLussacLaw::_recordDataPoint() {
    if (_dataCount >= MAX_STEPS) return;
    float V = _getTotalVolume();
    float T = _bme->getTemperature() + 273.15f;
    float P = _bme->getPressure();
    if (isnan(T) || T <= 0.0f || isnan(P)) return;
    _data[_dataCount] = { V, T, P, V / T };
    _dataCount++;
}

void GayLussacLaw::_nextStep() {
    _currentStep++;
    if (_currentStep >= _stepCount) {
        // 所有节点采集完毕
        _pressureCtrl->stop();
        _tempCtrl->stop();
        _state = DONE;
        printReport();
        if (_onDone) _onDone();
        return;
    }
    // 继续等待升温到下一节点
    _state = STEPPING;
}

// -----------------------------------------------------------------------
// 主循环
// -----------------------------------------------------------------------

void GayLussacLaw::update() {
    if (!_attached || _state == IDLE || _state == DONE || _state == ERROR) return;
    if (_paused) return;

    switch (_state) {

        case INIT_VOLUME:
            if (millis() - _initVolumeStartMs >= _initVolumeTimeoutMs) {
                _volumeCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            if (!_volumeCtrl->isRunning()) {
                if (_airValve) _airValve->close();
                _lockedVolume = _getTotalVolume();
                _prepareAndGo();
            }
            break;

        case PRES_INIT:
            // 等 pressureCtrl 进入 HOLDING（压强稳定到目标），再启动加热
            if (_pressureCtrl->getState() == PressureControl::ERROR) {
                _pressureCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            if (_pressureCtrl->getState() == PressureControl::HOLDING) {
                // 开始加热，目标 = 最高节点温度
                _tempCtrl->setTargetTemp(_tempSteps[_stepCount - 1]);
                _tempCtrl->start();
                _heatingStartMs = millis();
                _state = STEPPING;
            }
            break;

        case STEPPING:
            // 加热超时保护
            if (millis() - _heatingStartMs >= _heatingTimeoutMs) {
                _pressureCtrl->stop();
                _tempCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            // tempCtrl 超温或出错
            if (_tempCtrl->getState() == TempControl::ERROR) {
                _pressureCtrl->stop();
                _tempCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            {
                float currentTemp = _bme->getTemperature();
                if (!isnan(currentTemp) && currentTemp >= _tempSteps[_currentStep]) {
                    _nodeReachedMs = millis();
                    _state = RECORDING;
                }
            }
            break;

        case RECORDING:
            // 等待节点稳定时间后采集
            if (millis() - _nodeReachedMs >= _nodeSettleMs) {
                uint8_t before = _dataCount;
                _recordDataPoint();
                const DataPoint* dp = (_dataCount > before) ? &_data[_dataCount - 1] : nullptr;
                if (dp && _onStep) _onStep(_currentStep, *dp);
                _nextStep();
            }
            break;

        default: break;
    }
}

// -----------------------------------------------------------------------
// 报告
// -----------------------------------------------------------------------

void GayLussacLaw::printReport() const {
}
