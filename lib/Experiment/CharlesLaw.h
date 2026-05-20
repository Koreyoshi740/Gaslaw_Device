#ifndef CHARLESLAW_H
#define CHARLESLAW_H

#include <Arduino.h>
#include "BME280sensor.h"
#include "VolumeControl.h"
#include "TempControl.h"
#include "AirValve.h"

class CharlesLaw {
public:
    enum State {
        IDLE,
        INIT_VOLUME,        // 开阀，调节体积到目标值，关阀锁定
        STEPPING,           // 下发温度目标，等待加热到达
        STABILIZING,        // 等待P和T双重稳定
        RECORDING,          // 采样记录
        DONE,               // 实验完成，等待外部操作（开阀/降温）
        ERROR
    };

    enum StepMode { AUTO, MANUAL };

    struct DataPoint {
        float V_total;  // 系统总气体体积 (mL)
        float T;        // 温度 (K)
        float P;        // 压强 (hPa)
        float PT;       // P / T，验证量 (hPa/K)
    };

    static const uint8_t MAX_STEPS = 7;

    typedef void (*Callback)(void);
    typedef void (*StepCallback)(uint8_t step, const DataPoint& data);

    CharlesLaw();
    ~CharlesLaw();

    // 绑定依赖模块
    void attach(BME280Sensor* bme, VolumeControl* volumeCtrl, TempControl* tempCtrl);
    void attachAirValve(AirValve* airValve);

    // ---------- 实验参数配置 ----------

    // 瓶子物理参数：大瓶容积、小瓶容积、元件体积（单位 mL）
    // 固定侧气体体积 = 小瓶容积 - 元件体积
    void setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl);

    // 节点模式：AUTO = 内置30~60℃7步，MANUAL = 外部逐一添加（最多7个）
    void setStepMode(StepMode mode);

    // AUTO模式：覆盖默认节点数组
    void setTempSteps(const float* steps, uint8_t count);

    // MANUAL模式：逐一添加节点（3~7个），返回false表示已达上限
    bool addTempStep(float degC);
    void clearTempSteps();

    // 等容目标总体积 (mL)，默认 250mL
    void setTotalTargetVolume(float ml);

    // 温度越过节点后等待采集的稳定时间 (ms)，默认 1000
    void setNodeSettleMs(uint32_t ms);

    // INIT_VOLUME阶段超时保护 (ms)，默认 120000 (2min)
    void setInitVolumeTimeoutMs(uint32_t ms);

    // STEPPING阶段（加热等待）超时保护 (ms)，默认 600000 (10min)
    void setHeatingTimeoutMs(uint32_t ms);

    // ---------- 控制流程 ----------

    // 启动实验：开阀，调节体积到目标值，关阀锁定，进入STEPPING
    void start();

    // DONE后：打开气阀
    void openValve();

    // 手动关闭气阀
    void closeValve();

    // DONE后：启动降温流程（仅在DONE状态有效）
    void startCooldown(float targetTemp);

    void stop();
    void reset();
    void update();

    // 暂停/恢复：暂停时冻结当前阶段执行器，恢复时补偿计时器并重启执行器
    void setPaused(bool paused);
    bool isPaused() const { return _paused; }

    // ---------- 状态查询 ----------
    State    getState()        const { return _state; }
    StepMode getStepMode()     const { return _stepMode; }
    uint8_t  getCurrentStep()  const { return _currentStep; }
    uint8_t  getTotalSteps()   const { return _stepCount; }
    float    getCurrentTargetTemp() const {
        if (_currentStep < _stepCount) return _tempSteps[_currentStep];
        return -1.0f;
    }
    float    getNextTargetTemp() const {
        uint8_t next = _currentStep + 1;
        if (next < _stepCount) return _tempSteps[next];
        return -1.0f;
    }
    bool     isDone()              const { return _state == DONE; }
    float    getFixedVolume()      const { return _fixedVolume; }
    float    getLockedVolume()     const { return _lockedVolume; }
    float    getMaxTotalVolume()   const { return _largeBottleCapacity + _smallBottleCapacity - _componentVolume; }
    bool     isValveOpen()         const { return _airValve && _airValve->isOpen(); }

    const DataPoint& getDataPoint(uint8_t index) const { return _data[index]; }
    uint8_t getDataCount() const { return _dataCount; }

    void printReport() const;

    // ---------- 回调 ----------
    void onStart(Callback cb)             { _onStart = cb; }
    void onStepComplete(StepCallback cb)  { _onStep  = cb; }
    void onDone(Callback cb)              { _onDone  = cb; }
    void onError(Callback cb)             { _onError = cb; }

private:
    BME280Sensor*  _bme;
    VolumeControl* _volumeCtrl;
    TempControl*   _tempCtrl;
    AirValve*      _airValve;
    bool           _attached;

    State    _state;
    StepMode _stepMode;
    uint8_t  _currentStep;
    uint8_t  _stepCount;
    uint8_t  _dataCount;

    float    _tempSteps[MAX_STEPS];

    // 瓶子物理参数
    float    _largeBottleCapacity;   // 大瓶容积 (mL)，默认 325
    float    _smallBottleCapacity;   // 小瓶容积 (mL)，默认 135
    float    _componentVolume;       // 元件占用体积 (mL)，默认 10
    float    _fixedVolume;           // 固定侧气体体积 = 小瓶容积 - 元件体积
    float    _totalTargetVolume;     // 等容目标总体积 (mL)
    float    _lockedVolume;          // 关阀时锁定的实际总气体体积 (mL)
    bool     _paused;
    uint32_t _pauseStartMs;

    uint32_t _nodeSettleMs;        // 温度越过节点后等待采集的时间
    uint32_t _nodeReachedMs;       // 节点到达时间戳

    uint32_t _initVolumeTimeoutMs;
    uint32_t _initVolumeStartMs;
    uint32_t _heatingTimeoutMs;
    uint32_t _heatingStartMs;

    DataPoint _data[MAX_STEPS];

    Callback     _onStart;
    StepCallback _onStep;
    Callback     _onDone;
    Callback     _onError;

    static const float AUTO_TEMP_STEPS[7];

    void _loadAutoSteps();
    void _prepareAndGo();
    void _nextStep();
    void _recordDataPoint();
};

#endif
