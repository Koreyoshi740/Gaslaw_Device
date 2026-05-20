#ifndef TEMPCONTROL_H
#define TEMPCONTROL_H

#include <Arduino.h>
#include "BME280sensor.h"
#include "Heater.h"
#include "Pumper.h"
#include "AirPump.h"

class TempControl {
public:
    enum State { IDLE, HEATING, COASTING, HOLDING, COOLING, ERROR };
    enum CooldownState { COOLDOWN_IDLE, COOLDOWN_COOLING, COOLDOWN_DRAINING, COOLDOWN_DONE };

    typedef void (*Callback)(void);

    TempControl();
    ~TempControl();

    void attach(BME280Sensor* bme, Heater* heater);
    void attachCoolPump(Pumper* coolPump);
    void attachAirPump(AirPump* airPump);

    void setTargetTemp(float temp);
    void setHysteresis(float hysteresis);
    void setPID(float kp, float ki, float kd);
    void setHoldingBand(float degC);
    void setMaxTemp(float maxTemp);

    // HEATING 提前断电距离：currentTemp >= target - switchDelta 时切 COASTING
    void setSwitchDelta(float degC);
    // COASTING→HOLDING 允许的温度误差窗口
    void setCoastBand(float degC);
    // COASTING 最长等待时间 ms
    void setCoastTimeout(uint32_t ms);

    void startCooldown(float targetTemp);
    void stopCooldown();
    void setCooldownDrainingTime(uint32_t ms);
    void setCooldownMaxTime(uint32_t ms);
    CooldownState getCooldownState() const { return _cooldownState; }

    void setCirculationInterval(uint32_t ms);
    void setCirculationDuration(uint32_t ms);

    // HOLDING 阶段气泵间歇参数
    void setHoldCirculationInterval(uint32_t ms);
    void setHoldCirculationDuration(uint32_t ms);

    void coolPumpOn();
    void coolPumpOff();
    void airPumpOn();
    void airPumpOff();

    void setFanCooldownMs(uint32_t ms);

    float getCurrentTemp();
    float getTargetTemp()  const { return _targetTemp; }
    float getTempError()   const;
    State getState()       const { return _state; }
    bool  isHeating()      const { return _state == HEATING; }
    bool  isCoolPumpRunning() const { return _coolPump && _coolPump->isRunning(); }
    bool  isAirPumpRunning()  const { return _airPump && _airPump->isOn(); }
    bool  isCooldownActive() const { return _cooldownState != COOLDOWN_IDLE; }

    void start();
    void stop();
    void emergencyStop();

    void IRAM_ATTR onTimer();
    void update();

    void onStart(Callback cb)            { _onStart  = cb; }
    void onReach(Callback cb)            { _onReach  = cb; }
    void onStop(Callback cb)             { _onStop   = cb; }
    void onOverTemp(Callback cb)         { _onOverTemp = cb; }
    void onCooldownComplete(Callback cb) { _onCooldownComplete = cb; }

private:
    BME280Sensor* _bme;
    Heater*       _heater;
    Pumper*       _coolPump;
    AirPump*      _airPump;
    bool          _attached;
    State         _state;

    float    _targetTemp;
    float    _hysteresis;
    float    _holdingBand;
    float    _maxTemp;

    // PID 参数（仅 HOLDING 使用）
    float    _kp;
    float    _ki;
    float    _kd;
    float    _integral;
    float    _lastError;

    // HEATING→COASTING 提前断电距离 ℃
    float    _switchDelta;
    float    _effectiveSwitchDelta;  // start() 时按误差比例计算的实际断电距离

    // COASTING 参数
    float    _coastBand;
    uint32_t _coastTimeout;
    uint32_t _coastStartMs;
    float    _coastPrevTemp;

    // 冷却水泵参数
    float    _cooldownTargetTemp;
    uint32_t _cooldownDrainingTime;
    uint32_t _cooldownMaxTime;
    uint32_t _cooldownStartMs;
    CooldownState _cooldownState;

    // HEATING 气泵间歇循环
    uint32_t _circulationInterval;
    uint32_t _circulationDuration;
    uint32_t _lastCirculationMs;
    uint32_t _circulationStartMs;

    // HOLDING 气泵间歇循环（独立参数，防止过热）
    uint32_t _holdCirculationInterval;
    uint32_t _holdCirculationDuration;
    uint32_t _holdLastCirculationMs;
    uint32_t _holdCirculationStartMs;

    volatile uint32_t _tickMs;
    volatile bool     _doUpdate;
    static const uint32_t SAMPLE_PERIOD_MS = 500;

    uint32_t _fanCooldownMs;
    uint32_t _heaterOffMs;

    bool _reachFired;
    bool _fanCooling;

    Callback _onStart;
    Callback _onReach;
    Callback _onStop;
    Callback _onOverTemp;
    Callback _onCooldownComplete;

    void _triggerOverTemp();
    void _enterHeating();
    void _enterCoasting();
    void _enterHolding();
    void _enterCooling();
    void _applyPID(float currentTemp);
};

#endif
