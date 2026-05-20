#include "CharlesLaw.h"

const float CharlesLaw::AUTO_TEMP_STEPS[7] = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };

CharlesLaw::CharlesLaw()
    : _bme(nullptr)
    , _volumeCtrl(nullptr)
    , _tempCtrl(nullptr)
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

CharlesLaw::~CharlesLaw() {}

void CharlesLaw::attach(BME280Sensor* bme, VolumeControl* volumeCtrl, TempControl* tempCtrl) {
    if (!bme || !volumeCtrl || !tempCtrl) return;
    _bme        = bme;
    _volumeCtrl = volumeCtrl;
    _tempCtrl   = tempCtrl;
    _attached   = true;
}

void CharlesLaw::attachAirValve(AirValve* airValve) { _airValve = airValve; }

void CharlesLaw::setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl) {
    _largeBottleCapacity = largeBottleMl;
    _smallBottleCapacity = smallBottleMl;
    _componentVolume     = componentVolumeMl;
    _fixedVolume         = smallBottleMl - componentVolumeMl;
}

void CharlesLaw::setStepMode(StepMode mode)          { _stepMode = mode; }
void CharlesLaw::setTotalTargetVolume(float ml)      { _totalTargetVolume = ml; }
void CharlesLaw::setNodeSettleMs(uint32_t ms)        { _nodeSettleMs = ms; }
void CharlesLaw::setInitVolumeTimeoutMs(uint32_t ms) { _initVolumeTimeoutMs = ms; }
void CharlesLaw::setHeatingTimeoutMs(uint32_t ms)    { _heatingTimeoutMs = ms; }

void CharlesLaw::setTempSteps(const float* steps, uint8_t count) {
    _stepCount = min(count, (uint8_t)MAX_STEPS);
    for (uint8_t i = 0; i < _stepCount; i++) _tempSteps[i] = steps[i];
}

bool CharlesLaw::addTempStep(float degC) {
    if (_stepCount >= MAX_STEPS) return false;
    _tempSteps[_stepCount++] = degC;
    return true;
}

void CharlesLaw::clearTempSteps() {
    _stepCount = 0;
    memset(_tempSteps, 0, sizeof(_tempSteps));
}

void CharlesLaw::setPaused(bool paused) {
    if (_paused == paused) return;
    _paused = paused;

    if (paused) {
        _pauseStartMs = millis();
        switch (_state) {
            case INIT_VOLUME:
                if (_volumeCtrl) _volumeCtrl->stop();
                break;
            case STEPPING:
            case STABILIZING:
                if (_tempCtrl) _tempCtrl->stop();
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
        case STABILIZING:
            if (_tempCtrl && _stepCount > 0) {
                _tempCtrl->setTargetTemp(_tempSteps[_stepCount - 1]);
                _tempCtrl->start();
            }
            break;
        default: break;
    }
}

// -----------------------------------------------------------------------
// 流程控制接口
// -----------------------------------------------------------------------

void CharlesLaw::start() {
    if (!_attached) {
        _state = ERROR;
        if (_onError) _onError();
        return;
    }

    _volumeCtrl->stop();
    _tempCtrl->stop();

    _currentStep   = 0;
    _dataCount     = 0;
    _nodeReachedMs = 0;
    _lockedVolume  = 0.0f;

    if (_stepMode == AUTO) _loadAutoSteps();

    if (_airValve) _airValve->open();

    _volumeCtrl->setTargetAirVolume(_totalTargetVolume);
    _volumeCtrl->start();
    _initVolumeStartMs = millis();
    _state = INIT_VOLUME;

    if (_onStart) _onStart();
}

void CharlesLaw::openValve() {
    if (_state != DONE && _state != IDLE && _state != ERROR) return;
    if (_airValve) _airValve->open();
}

void CharlesLaw::closeValve() {
    if (_airValve) _airValve->close();
}

void CharlesLaw::startCooldown(float targetTemp) {
    if (_state != DONE) return;
    if (_tempCtrl) _tempCtrl->startCooldown(targetTemp);
}

void CharlesLaw::stop() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    _tempCtrl->stop();
    if (_airValve) _airValve->open();
    _state = IDLE;
}

void CharlesLaw::reset() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    _tempCtrl->stop();
    if (_airValve) _airValve->open();
    _state         = IDLE;
    _currentStep   = 0;
    _dataCount     = 0;
    _lockedVolume  = 0.0f;
    _paused        = false;
    if (_stepMode == MANUAL) clearTempSteps();
    memset(_data, 0, sizeof(_data));
}

// -----------------------------------------------------------------------
// 私有辅助
// -----------------------------------------------------------------------

void CharlesLaw::_loadAutoSteps() {
    _stepCount = sizeof(AUTO_TEMP_STEPS) / sizeof(AUTO_TEMP_STEPS[0]);
    for (uint8_t i = 0; i < _stepCount; i++) _tempSteps[i] = AUTO_TEMP_STEPS[i];
}

void CharlesLaw::_prepareAndGo() {
    if (_stepMode == AUTO && _stepCount == 0) _loadAutoSteps();
    // 设定最高节点温度，开始加热
    _tempCtrl->setTargetTemp(_tempSteps[_stepCount - 1]);
    _tempCtrl->start();
    _heatingStartMs = millis();
    _state = STEPPING;
}

void CharlesLaw::_recordDataPoint() {
    if (_dataCount >= MAX_STEPS) return;
    float T = _bme->getTemperature() + 273.15f;
    float P = _bme->getPressure();
    float V = _lockedVolume;
    if (isnan(T) || T <= 0.0f || isnan(P)) return;
    _data[_dataCount] = { V, T, P, P / T };
    _dataCount++;
}

void CharlesLaw::_nextStep() {
    _currentStep++;
    if (_currentStep >= _stepCount) {
        // 所有节点采集完毕，停止加热
        _tempCtrl->stop();
        _state = DONE;
        printReport();
        if (_onDone) _onDone();
        return;
    }
    // 继续升温，等下一个节点
    _state = STEPPING;
}

// -----------------------------------------------------------------------
// 主循环
// -----------------------------------------------------------------------

void CharlesLaw::update() {
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
                _lockedVolume = _volumeCtrl->getAirVolume();
                _prepareAndGo();
            }
            break;

        case STEPPING:
            if (millis() - _heatingStartMs >= _heatingTimeoutMs) {
                _tempCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            {
                float currentTemp = _bme->getTemperature();
                if (!isnan(currentTemp) && currentTemp >= _tempSteps[_currentStep]) {
                    _nodeReachedMs = millis();
                    _state = STABILIZING;
                }
            }
            break;

        case STABILIZING:
            if (millis() - _nodeReachedMs >= _nodeSettleMs) {
                _state = RECORDING;
            }
            break;

        case RECORDING: {
            uint8_t before = _dataCount;
            _recordDataPoint();
            const DataPoint* dp = (_dataCount > before) ? &_data[_dataCount - 1] : nullptr;
            if (dp && _onStep) _onStep(_currentStep, *dp);
            _nextStep();
            break;
        }

        default: break;
    }
}

// -----------------------------------------------------------------------
// 报告
// -----------------------------------------------------------------------

void CharlesLaw::printReport() const {
}
