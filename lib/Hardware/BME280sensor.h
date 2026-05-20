#ifndef BME280SENSOR_H
#define BME280SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

class BME280Sensor {
public:
    BME280Sensor();
    ~BME280Sensor();

    bool begin();

    // 主循环调用，以固定间隔读取传感器并缓存，含卡死检测与软复位
    void update();

    float getTemperature();
    float getPressure();

private:
    Adafruit_BME280 _bme;
    bool     _initialized;

    float    _cachedTemp;
    float    _cachedPressure;
    uint32_t _lastReadMs;
    static const uint32_t READ_INTERVAL_MS = 500;

    float   _lastTemp;
    uint8_t _stuckCount;
};

#endif
