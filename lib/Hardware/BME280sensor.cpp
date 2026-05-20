#include "BME280sensor.h"

BME280Sensor::BME280Sensor()
    : _initialized(false)
    , _cachedTemp(NAN)
    , _cachedPressure(NAN)
    , _lastReadMs(0)
    , _lastTemp(NAN)
    , _stuckCount(0)
{}

BME280Sensor::~BME280Sensor() {}

bool BME280Sensor::begin() {
    _initialized = _bme.begin(0x76, &Wire);
    if (!_initialized) return false;

    _bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                     Adafruit_BME280::SAMPLING_X2,
                     Adafruit_BME280::SAMPLING_X16,
                     Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::FILTER_X4,
                     Adafruit_BME280::STANDBY_MS_0_5);
    return true;
}

static void softReset() {
    Wire.beginTransmission(0x76);
    Wire.write(0xE0);
    Wire.write(0xB6);
    Wire.endTransmission();
    delay(10);
}

void BME280Sensor::update() {
    if (!_initialized) {
        // 未初始化时定期重试
        if (millis() - _lastReadMs >= READ_INTERVAL_MS) {
            _lastReadMs = millis();
            begin();
        }
        return;
    }
    if (millis() - _lastReadMs < READ_INTERVAL_MS) return;
    _lastReadMs = millis();

    float t = _bme.readTemperature();
    float p = _bme.readPressure() / 100.0f;

    // 已知故障特征值：BME280 异常时固定输出 ~20.3℃ / ~730.9 hPa
    bool knownBadTemp = (!isnan(t) && t > 20.0f && t < 20.6f);
    bool knownBadPres = (!isnan(p) && p > 730.0f && p < 731.5f);

    // 连续相同温度（误差 < 0.01℃ 视为相同）
    bool sameAsLast = (!isnan(t) && !isnan(_lastTemp) &&
                       fabsf(t - _lastTemp) < 0.01f);

    if (knownBadTemp || knownBadPres || sameAsLast) {
        if (++_stuckCount >= 3) {
            _stuckCount  = 0;
            _lastTemp    = NAN;
            softReset();
            begin();
            return;
        }
    } else {
        _stuckCount = 0;
        _lastTemp   = t;
        _cachedTemp     = t;
        _cachedPressure = p;
    }
}

float BME280Sensor::getTemperature() {
    if (!_initialized) return NAN;
    return _cachedTemp;
}

float BME280Sensor::getPressure() {
    if (!_initialized) return NAN;
    return _cachedPressure;
}

