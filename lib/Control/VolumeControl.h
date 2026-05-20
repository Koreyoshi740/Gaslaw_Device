#ifndef VOLUMECONTROL_H
#define VOLUMECONTROL_H

#include <Arduino.h>
#include "Flowsensor.h"
#include "Pumper.h"

class VolumeControl {
public:
    enum State {
        IDLE,
        RUNNING,
        COMPLETED,
        STOPPED,
        ERROR
    };

    typedef void (*Callback)(void);

    VolumeControl();
    ~VolumeControl();

    // 关联两个流量传感器和泵
    void attach(Flowsensor* flowInSensor, Flowsensor* flowOutSensor, Pumper* pumpMotor);

    // ---------- 容器参数 ----------
    void setContainerVolume(float ml);
    float getContainerVolume() const { return _containerVolume; }

    // ---------- 体积查询（基于净体积） ----------
    float getLiquidVolume() const;          // 当前净液体体积（进水 - 出水，mL）
    float getAirVolume() const;             // 空气体积 = 容器总容积 - 净液体体积（mL）

    // 获取各传感器独立累计值（调试用）
    float getInFlowTotal() const;
    float getOutFlowTotal() const;
    float getInFlow() const;
    float getOutFlow() const;

    // ---------- 目标设定 ----------
    void setTargetVolume(float ml);         // 目标液位绝对值（mL）
    void setTargetAirVolume(float airMl);   // 通过目标空气体积间接设定液位（mL）
    void setTargetDelta(float deltaMl);     // 相对原点的增量（mL），需先调用 setOrigin()

    // ---------- 原点 ----------
    void setOrigin();                        // 将当前液位设为原点
    void clearOrigin();                      // 清除原点（回到绝对模式）
    float getOriginVolume() const { return _originVolume; }
    bool  hasOrigin() const { return _hasOrigin; }

    // ---------- 控制 ----------
    void setPumpSpeed(int16_t speed);
    void setFollowMode(bool enable) { _followMode = enable; }
    bool isFollowMode() const { return _followMode; }
    void start();
    void stop();
    void reset();
    void update();                           // 需在主循环中周期性调用

    // ---------- PID 参数 ----------
    void setPID(float kp, float ki, float kd);
    void setDeadBand(float ml);             // 停泵死区（mL），默认 2.0
    void setMinPWM(int16_t minPwm);         // 最小有效 PWM（克服泵死区），默认 90
    void setMaxPWM(int16_t maxPwm);         // 最大 PWM，默认 255

    // ---------- 定时器节拍 ----------
    void IRAM_ATTR onPidTick();              // 在硬件定时器 ISR 中每 1ms 调用

    // ---------- 状态查询 ----------
    State getState() const { return _state; }
    bool isIdle() const { return _state == IDLE; }
    bool isRunning() const { return _state == RUNNING; }
    float getCurrentVolume() const;          // 本次已泵送净体积（mL）
    float getRemainingVolume() const;        // 剩余待泵送净体积（mL）
    float getProgressPercent() const;
    int16_t getPumpSpeed() const { return _pumpSpeed; }
    float getTargetVolume() const { return _targetVolume; }
    float getTargetAirVolume() const { return _containerVolume - _targetVolume; }

    // ---------- 回调 ----------
    void onStart(Callback cb)   { _onStart = cb; }
    void onComplete(Callback cb){ _onComplete = cb; }
    void onStop(Callback cb)    { _onStop = cb; }

private:
    Flowsensor* _flowIn;
    Flowsensor* _flowOut;
    Pumper*     _pump;
    State       _state;
    float       _targetVolume;
    float       _startVolume   = 0.0f;
    float       _originVolume  = 0.0f;
    bool        _hasOrigin     = false;
    int16_t     _pumpSpeed     = 0;
    bool        _attached;
    float       _containerVolume;
    bool        _followMode    = false;

    // PID 参数
    float   _kp       = 5.0f;
    float   _ki       = 0.5f;
    float   _kd       = 0.0f;
    float   _deadBand = 2.0f;    // mL
    int16_t _minPWM   = 90;
    int16_t _maxPWM   = 255;

    // PID 运行状态
    float             _integral   = 0.0f;
    float             _lastError  = 0.0f;
    float             _lastVolume = 0.0f;
    volatile bool     _pidReady   = false;
    volatile uint32_t _msCounter  = 0;

    Callback _onStart;
    Callback _onComplete;
    Callback _onStop;

    void _stopPumpInternal();
    float _getNetTotalVolume() const;
};

#endif