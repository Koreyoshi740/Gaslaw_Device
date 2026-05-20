#include "BoyleLaw.h"

static const float AUTO_VOLUME_STEPS[7] = {
    400.0f, 375.0f, 350.0f, 325.0f, 300.0f, 275.0f, 250.0f
};

BoyleLaw::BoyleLaw()
    : _bme(nullptr)
    , _volumeCtrl(nullptr)
    , _tempCtrl(nullptr)
    , _airPump(nullptr)
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
    , _initTotalVolume(450.0f)
    , _targetTemp(25.0f)
    , _tempEnabled(false)
    , _paused(false)
    , _pauseStartMs(0)
    , _stableThreshold(0.3f)
    , _stableCount(4)
    , _stableCounter(0)
    , _lastPressure(0.0f)
    , _timerMs(0)
    , _circulateMs(5000)
    , _sampleIntervalMs(1000)
    , _tempCheckDuration(3000)
    , _tempCheckStartTemp(0.0f)
    , _tempCheckStartMs(0)
    , _tempInstabilityThreshold(1.0f)
    , _stabilizeTimeoutMs(60000)
    , _stabilizeStartMs(0)
    , _onStart(nullptr)
    , _onStep(nullptr)
    , _onDone(nullptr)
    , _onError(nullptr)
{
    memset(_volumeSteps, 0, sizeof(_volumeSteps));
    memset(_data, 0, sizeof(_data));
}

BoyleLaw::~BoyleLaw() {}

void BoyleLaw::attach(BME280Sensor* bme, VolumeControl* volumeCtrl, TempControl* tempCtrl) {
    if (!bme || !volumeCtrl || !tempCtrl) return;
    _bme        = bme;
    _volumeCtrl = volumeCtrl;
    _tempCtrl   = tempCtrl;
    _attached   = true;
}

void BoyleLaw::attachAirPump(AirPump* airPump)   { _airPump  = airPump; }
void BoyleLaw::attachAirValve(AirValve* airValve) { _airValve = airValve; }

void BoyleLaw::setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl) {
    _largeBottleCapacity = largeBottleMl;
    _smallBottleCapacity = smallBottleMl;
    _componentVolume     = componentVolumeMl;
    _fixedVolume         = smallBottleMl - componentVolumeMl;
}

void BoyleLaw::setInitTotalVolume(float ml)        { _initTotalVolume  = ml; }
void BoyleLaw::setTargetTemp(float temp)           { _targetTemp       = temp; }
void BoyleLaw::setCirculateMs(uint32_t ms)         { _circulateMs      = ms; }
void BoyleLaw::setSampleIntervalMs(uint32_t ms)    { _sampleIntervalMs = ms; }
void BoyleLaw::setTempCheckDuration(uint32_t ms)   { _tempCheckDuration = ms; }
void BoyleLaw::setTempInstabilityThreshold(float degC) { _tempInstabilityThreshold = degC; }
void BoyleLaw::setStabilizeTimeoutMs(uint32_t ms)  { _stabilizeTimeoutMs = ms; }
void BoyleLaw::enableTempControl(bool enable)      { _tempEnabled      = enable; }
void BoyleLaw::setStepMode(StepMode mode)          { _stepMode         = mode; }

void BoyleLaw::setPaused(bool paused) {
    if (_paused == paused) return;
    _paused = paused;

    if (paused) {
        _pauseStartMs = millis();
        switch (_state) {
            case INIT_VOLUME:
            case STEPPING:
                if (_volumeCtrl) _volumeCtrl->stop();
                break;
            default: break;
        }
        return;
    }

    uint32_t pauseDur = millis() - _pauseStartMs;
    _tempCheckStartMs += pauseDur;
    _timerMs          += pauseDur;
    _stabilizeStartMs += pauseDur;

    switch (_state) {
        case INIT_VOLUME:
            if (_volumeCtrl) {
                _volumeCtrl->setTargetAirVolume(_initTotalVolume);
                _volumeCtrl->start();
            }
            break;
        case STEPPING:
            if (_volumeCtrl && _currentStep < _stepCount) {
                _volumeCtrl->setTargetAirVolume(_volumeSteps[_currentStep]);
                _volumeCtrl->start();
            }
            break;
        default: break;
    }
}

void BoyleLaw::setStableThreshold(float hPa, uint8_t count) {
    _stableThreshold = hPa;
    _stableCount     = count;
}

void BoyleLaw::setVolumeSteps(const float* totalVolSteps, uint8_t count) {
    _stepCount = min(count, (uint8_t)MAX_STEPS);
    for (uint8_t i = 0; i < _stepCount; i++) {
        _volumeSteps[i] = totalVolSteps[i];
    }
}

bool BoyleLaw::addVolumeStep(float totalVolumeMl) {
    if (_stepCount >= MAX_STEPS) return false;
    if (totalVolumeMl <= _fixedVolume) return false;
    _volumeSteps[_stepCount++] = totalVolumeMl;
    return true;
}

void BoyleLaw::clearVolumeSteps() {
    _stepCount = 0;
    memset(_volumeSteps, 0, sizeof(_volumeSteps));
}

void BoyleLaw::openValve() {
    if (_airValve) {
        _airValve->open();
    }
}

void BoyleLaw::closeValve() {
    if (_airValve) {
        _airValve->close();
    }
}

void BoyleLaw::start() {
    if (!_attached) {
        _state = ERROR;
        if (_onError) _onError();
        return;
    }

    _currentStep   = 0;
    _dataCount     = 0;
    _stableCounter = 0;
    _lastPressure  = 0.0f;

    // Load steps now so UI can read them during FILLING
    if (_stepMode == AUTO) _loadAutoSteps();

    float initVarVol = _initTotalVolume - _fixedVolume;
    _volumeCtrl->setTargetAirVolume(_initTotalVolume);
    _volumeCtrl->start();
    _state = INIT_VOLUME;

    if (_onStart) _onStart();
}

void BoyleLaw::stop() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    if (_airPump)  _airPump->off();
    if (_airValve) _airValve->open();
    _state = IDLE;
}

void BoyleLaw::reset() {
    if (!_attached) { _state = IDLE; return; }
    _volumeCtrl->stop();
    if (_airPump)  _airPump->off();
    if (_airValve) _airValve->open();
    if (_tempCtrl) _tempCtrl->stop();
    _state         = IDLE;
    _currentStep   = 0;
    _dataCount     = 0;
    _stableCounter = 0;
    _paused        = false;
    if (_stepMode == MANUAL) clearVolumeSteps();
    memset(_data, 0, sizeof(_data));
}

// -----------------------------------------------------------------------
// 私有辅助
// -----------------------------------------------------------------------

void BoyleLaw::_loadAutoSteps() {
    _stepCount = sizeof(AUTO_VOLUME_STEPS) / sizeof(AUTO_VOLUME_STEPS[0]);
    for (uint8_t i = 0; i < _stepCount; i++) {
        _volumeSteps[i] = AUTO_VOLUME_STEPS[i];
    }
}

void BoyleLaw::_prepareAndGo() {
    if (_stepMode == AUTO && _stepCount == 0) _loadAutoSteps(); // fallback if not loaded yet
    _nextStep();
}

float BoyleLaw::_getTotalVolume() const {
    return _volumeCtrl->getAirVolume();
}

void BoyleLaw::_recordDataPoint() {
    if (_dataCount >= MAX_STEPS) return;
    float P = _bme->getPressure();
    float T = _bme->getTemperature() + 273.15f;
    float V = _getTotalVolume();
    _data[_dataCount] = { V, P, T, P * V };
    _dataCount++;
}

void BoyleLaw::_nextStep() {
    if (_currentStep >= _stepCount) {
        if (_tempCtrl) _tempCtrl->stop();
        _state = DONE;
        printReport();
        if (_onDone) _onDone();
        return;
    }

    float totalVol = _volumeSteps[_currentStep];
    float varVol   = totalVol - _fixedVolume;
    _volumeCtrl->setTargetAirVolume(totalVol);
    _volumeCtrl->start();
    _state = STEPPING;
}

// -----------------------------------------------------------------------
// 主循环
// -----------------------------------------------------------------------

void BoyleLaw::update() {
    if (!_attached || _state == IDLE || _state == DONE || _state == ERROR) return;
    if (_paused) return;

    switch (_state) {

        case INIT_VOLUME:
            if (!_volumeCtrl->isRunning()) {
                if (_airValve) {
                    _airValve->close();
                }
                _tempCheckStartTemp = _bme->getTemperature();
                _tempCheckStartMs   = millis();
                _state = TEMP_CHECKING;
            }
            break;

        case TEMP_CHECKING:
            if (millis() - _tempCheckStartMs >= _tempCheckDuration) {
                if (_tempEnabled) {
                    _tempCtrl->setTargetTemp(_targetTemp);
                    _tempCtrl->start();
                    _state = WAITING_TEMP;
                } else {
                    _prepareAndGo();
                }
            }
            break;

        case WAITING_TEMP:
            if (_tempCtrl->getState() == TempControl::HOLDING) {
                _prepareAndGo();
            } else if (_tempCtrl->getState() == TempControl::ERROR) {
                _tempCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
            }
            break;

        case STEPPING:
            if (!_volumeCtrl->isRunning()) {
                _lastPressure     = _bme->getPressure();
                _timerMs          = millis();
                _stabilizeStartMs = millis();
                _stableCounter    = 0;
                _state            = STABILIZING;
            }
            break;

        case STABILIZING:
            if (millis() - _stabilizeStartMs >= _stabilizeTimeoutMs) {
                if (_tempCtrl) _tempCtrl->stop();
                _state = ERROR;
                if (_onError) _onError();
                break;
            }
            if (millis() - _timerMs >= _sampleIntervalMs) {
                _timerMs = millis();
                float currentP = _bme->getPressure();
                if (isnan(currentP)) break;

                if (fabsf(currentP - _lastPressure) < _stableThreshold) {
                    _stableCounter++;
                    if (_stableCounter >= _stableCount) {
                        _state = RECORDING;
                    }
                } else {
                    _stableCounter = 0;
                }
                _lastPressure = currentP;
            }
            break;

        case RECORDING: {
            uint8_t before = _dataCount;
            _recordDataPoint();
            if (_dataCount == before) {
                _currentStep++;
                _nextStep();
                break;
            }
            const DataPoint& dp = _data[_dataCount - 1];
            if (_onStep) _onStep(_currentStep, dp);
            _currentStep++;
            _nextStep();
            break;
        }

        default: break;
    }
}

// -----------------------------------------------------------------------
// 报告
// -----------------------------------------------------------------------

void BoyleLaw::printReport() const {
}
