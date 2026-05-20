#ifndef PRESSURECONTROL_H
#define PRESSURECONTROL_H

#include <Arduino.h>
#include "BME280sensor.h"
#include "VolumeControl.h"

class PressureControl {
public:
    enum State { IDLE, RUNNING, HOLDING, ERROR };

    typedef void (*Callback)(void);

    PressureControl();
    ~PressureControl();

    void attach(BME280Sensor* bme, VolumeControl* volumeCtrl);

    void  setTargetPressure(float hPa);
    float getTargetPressure() const { return _targetPressure; }

    // PID 参数（Kp/Ki/Kd），输出量为体积增量 ΔV (mL)
    void setPID(float kp, float ki, float kd);

    // 每步最大体积增量 (mL)，默认 50.0（滚动目标模式下相当于速率上限）
    void setMaxDeltaV(float ml);

    // 死区 (hPa)：|error| < deadBand 时进入 HOLDING，默认 0.5
    void  setDeadBand(float hPa);
    float getDeadBand() const { return _deadBand; }

    // 兼容旧接口
    void  setHysteresis(float hPa) { setDeadBand(hPa); }
    float getHysteresis()    const { return _deadBand; }

    // 状态查询
    float getCurrentPressure();
    float getPressureError() const;
    State getState()   const { return _state; }
    bool  isRunning()  const { return _state == RUNNING; }

    void start();
    void stop();
    void reset();

    void update();
    void IRAM_ATTR onTimer();

    void onStart(Callback cb) { _onStart = cb; }
    void onReach(Callback cb) { _onReach = cb; }
    void onStop(Callback cb)  { _onStop  = cb; }

private:
    BME280Sensor*  _bme;
    VolumeControl* _volumeCtrl;
    bool           _attached;
    State          _state;

    float _targetPressure;
    float _deadBand;

    float _kp;
    float _ki;
    float _kd;
    float _integral;
    float _lastError;
    float _maxDeltaV;

    volatile uint32_t _tickMs;
    volatile bool     _doUpdate;

    static const uint32_t SAMPLE_PERIOD_MS = 200;

    Callback _onStart;
    Callback _onReach;
    Callback _onStop;

    bool _reachFired;

    void _applyPID(float currentP);
};

#endif
