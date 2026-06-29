#ifndef BOYLELAW_H
#define BOYLELAW_H

#include <Arduino.h>
#include "BME280sensor.h"
#include "VolumeControl.h"
#include "TempControl.h"
#include "AirPump.h"
#include "AirValve.h"

class BoyleLaw {
public:
    enum State {
        IDLE,
        INIT_VOLUME,        // 初始注水到起始刻度
        TEMP_CHECKING,      // 注水完成后观察温度稳定性
        WAITING_TEMP,       // 等待温度稳定（可跳过）
        COLLECTING,         // 连贯压缩，途经各节点时自动采集
        DONE,               // 所有步骤完成
        ERROR
    };

    enum StepMode { AUTO, MANUAL };

    struct DataPoint {
        float V_total;   // 系统总气体体积 (mL)
        float P;         // 压强 (hPa)
        float T;         // 温度 (K)
        float PV;        // P × V_total，验证量
    };

    static const uint8_t MAX_STEPS = 7;

    typedef void (*Callback)(void);
    typedef void (*StepCallback)(uint8_t step, const DataPoint& data);

    BoyleLaw();
    ~BoyleLaw();

    // 绑定依赖模块
    void attach(BME280Sensor* bme, VolumeControl* volumeCtrl, TempControl* tempCtrl);
    void attachAirPump(AirPump* airPump);
    void attachAirValve(AirValve* airValve);

    // ---------- 实验参数配置 ----------

    // 瓶子物理参数：大瓶容积、小瓶容积、元件体积（单位 mL）
    // 固定侧气体体积 = 小瓶容积 - 元件体积
    // 系统最大总气体体积 = 大瓶容积 + 小瓶容积 - 元件体积
    // 默认：325mL + 135mL - 10mL = 450mL
    void setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl);

    // 初始状态总气体体积 (mL)，默认 450mL（大瓶全空）
    void setInitTotalVolume(float ml);

    // 节点模式：AUTO = 动态生成7步，MANUAL = 外部逐一添加（最多7个）
    void setStepMode(StepMode mode);

    // AUTO 模式：手动覆盖节点数组（总体积，单位 mL）
    void setVolumeSteps(const float* totalVolSteps, uint8_t count);

    // MANUAL 模式：逐一添加节点（总体积，3~7个），返回false表示已达上限
    bool addVolumeStep(float totalVolumeMl);
    void clearVolumeSteps();

    // 等温目标温度 (℃)
    void setTargetTemp(float temp);

    // 是否启用恒温控制，false 则跳过 WAITING_TEMP
    void enableTempControl(bool enable);

    // 压强稳定判断：连续 stableCount 次采样变化 < stableThreshold(hPa)
    void setStableThreshold(float hPa, uint8_t count);

    // 节点采集容差 (mL)，默认 0.5
    void setCollectTolerance(float ml);

    // 气泵循环时间 (ms)，默认 5000
    void setCirculateMs(uint32_t ms);

    // 采样间隔 (ms)，默认 500
    void setSampleIntervalMs(uint32_t ms);

    // 温度稳定性检测窗口 (ms)，默认 3000
    void setTempCheckDuration(uint32_t ms);

    // 视为温度大幅波动的阈值 (℃)，默认 1.0
    void setTempInstabilityThreshold(float degC);

    // STABILIZING 阶段超时保护 (ms)，默认 60000
    void setStabilizeTimeoutMs(uint32_t ms);

    // ---------- 控制流程 ----------

    // 启动实验：载入节点，注水到初始位置，进入FILLING
    void start();

    void stop();
    void reset();

    // 暂停/恢复：暂停时 update() 直接返回，并冻结底层执行器
    void setPaused(bool paused);
    bool isPaused() const { return _paused; }

    // 气阀手动控制
    void openValve();
    void closeValve();

    void update();

    // ---------- 状态查询 ----------
    State    getState()           const { return _state; }
    StepMode getStepMode()        const { return _stepMode; }
    uint8_t  getCurrentStep()     const { return _currentStep; }
    uint8_t  getTotalSteps()      const { return _stepCount; }
    bool     isDone()             const { return _state == DONE; }
    float    getFixedVolume()     const { return _fixedVolume; }
    float    getCurrentTargetVolume() const {
        if (_currentStep < _stepCount) return _volumeSteps[_currentStep];
        return 0.0f;
    }
    float    getNextTargetVolume() const {
        uint8_t next = _currentStep + 1;
        if (next < _stepCount) return _volumeSteps[next];
        return 0.0f;
    }
    float    getMaxTotalVolume()  const { return _largeBottleCapacity + _smallBottleCapacity - _componentVolume; }
    bool     isValveOpen()        const { return _airValve && _airValve->isOpen(); }
    bool     isTempEnabled()      const { return _tempEnabled; }
    float    getTempCheckStartTemp() const { return _tempCheckStartTemp; }

    const DataPoint& getDataPoint(uint8_t index) const {
        if (index >= _dataCount) return _data[0];
        return _data[index];
    }
    uint8_t getDataCount() const { return _dataCount; }

    void printReport() const;

    // ---------- 回调 ----------
    void onStart(Callback cb)              { _onStart = cb; }
    void onStepComplete(StepCallback cb)   { _onStep  = cb; }
    void onDone(Callback cb)               { _onDone  = cb; }
    void onError(Callback cb)              { _onError = cb; }

private:
    BME280Sensor*  _bme;
    VolumeControl* _volumeCtrl;
    TempControl*   _tempCtrl;
    AirPump*       _airPump;
    AirValve*      _airValve;
    bool           _attached;

    State    _state;
    StepMode _stepMode;
    uint8_t  _currentStep;
    uint8_t  _stepCount;
    uint8_t  _dataCount;

    // 瓶子物理参数
    float   _largeBottleCapacity;   // 250ml瓶实际容积 (mL)，默认 325
    float   _smallBottleCapacity;   // 100ml瓶实际容积 (mL)，默认 135
    float   _componentVolume;       // 元件占用体积 (mL)，默认 10
    float   _fixedVolume;           // 固定侧气体体积 = 小瓶容积 - 元件体积

    float   _initTotalVolume;       // 初始状态总气体体积 (mL)
    float   _targetTemp;
    bool    _tempEnabled;
    bool    _paused;                // 暂停标志，update() 短路
    uint32_t _pauseStartMs;         // 暂停开始时间，用于恢复时补偿计时器

    float   _volumeSteps[MAX_STEPS]; // 体积节点（总体积，单位 mL）
    float   _stableThreshold;
    uint8_t _stableCount;
    uint8_t _stableCounter;
    float   _lastPressure;
    float   _collectTolerance;           // 节点采集容差 (mL)
    bool    _stepCollected[MAX_STEPS];   // 各节点是否已采集

    uint32_t _timerMs;
    uint32_t _circulateMs;
    uint32_t _sampleIntervalMs;
    uint32_t _tempCheckDuration;
    float    _tempCheckStartTemp;
    uint32_t _tempCheckStartMs;
    float    _tempInstabilityThreshold;
    uint32_t _stabilizeTimeoutMs;
    uint32_t _stabilizeStartMs;

    DataPoint _data[MAX_STEPS];

    Callback     _onStart;
    StepCallback _onStep;
    Callback     _onDone;
    Callback     _onError;

    void _loadAutoSteps();
    void _prepareAndGo();
    float _getTotalVolume() const;
    void  _nextStep();
    void  _recordDataPoint();
};

#endif
