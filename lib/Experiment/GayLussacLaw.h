#ifndef GAYLUSSACLAW_H
#define GAYLUSSACLAW_H

#include <Arduino.h>
#include "BME280sensor.h"
#include "VolumeControl.h"
#include "TempControl.h"
#include "PressureControl.h"
#include "AirValve.h"

class GayLussacLaw {
public:
    enum State {
        IDLE,
        INIT_VOLUME,    // 开阀，调节体积到目标值，关阀锁定
        PRES_INIT,      // 读取/等待初始压强，启动恒压控制
        STEPPING,       // 设定最高节点温度，升温过程中逐节点采集
        RECORDING,      // 记录当前节点数据，随即继续 STEPPING
        DONE,
        ERROR
    };

    enum StepMode { AUTO, MANUAL };

    struct DataPoint {
        float V_total;  // 系统总气体体积 (mL)
        float T;        // 温度 (K)
        float P;        // 压强 (hPa)
        float VT;       // V / T，验证量
    };

    static const uint8_t MAX_STEPS = 7;

    typedef void (*Callback)(void);
    typedef void (*StepCallback)(uint8_t step, const DataPoint& data);

    GayLussacLaw();
    ~GayLussacLaw();

    void attach(BME280Sensor* bme, VolumeControl* volumeCtrl,
                TempControl* tempCtrl, PressureControl* pressureCtrl);
    void attachAirValve(AirValve* airValve);

    // ---------- 实验参数配置 ----------

    // 瓶子物理参数：大瓶容积、小瓶容积、元件体积（单位 mL）
    void setBottleConfig(float largeBottleMl, float smallBottleMl, float componentVolumeMl);

    // 等压实验目标总气体体积 (mL)，默认 250mL
    void setTotalTargetVolume(float ml);

    // 节点模式：AUTO = 内置30~60℃7步，MANUAL = 外部逐一添加（最多7个）
    void setStepMode(StepMode mode);

    // AUTO模式：手动覆盖节点数组
    void setTempSteps(const float* steps, uint8_t count);

    // MANUAL模式：逐一添加节点（3~7个），返回false表示已达上限
    bool addTempStep(float degC);
    void clearTempSteps();

    // 自定义恒压目标 (hPa)；不调用则用关阀时的实测压强
    void setTargetPressure(float hPa);

    // 温度越过节点后等待采集的稳定时间 (ms)，默认 1000
    void setNodeSettleMs(uint32_t ms);

    // INIT_VOLUME阶段超时保护 (ms)，默认 120000
    void setInitVolumeTimeoutMs(uint32_t ms);

    // STEPPING加热超时保护 (ms)，默认 600000 (10min)
    void setHeatingTimeoutMs(uint32_t ms);

    // ---------- 控制流程 ----------

    void start();

    // DONE后：打开气阀
    void openValve();

    // 手动关闭气阀
    void closeValve();

    // DONE后：启动降温流程
    void startCooldown(float targetTemp);

    void stop();
    void reset();
    void update();

    void setPaused(bool paused);
    bool isPaused() const { return _paused; }

    // ---------- 状态查询 ----------
    State    getState()          const { return _state; }
    StepMode getStepMode()       const { return _stepMode; }
    uint8_t  getCurrentStep()    const { return _currentStep; }
    uint8_t  getTotalSteps()     const { return _stepCount; }
    float    getCurrentTargetTemp() const {
        if (_currentStep < _stepCount) return _tempSteps[_currentStep];
        return -1.0f;
    }
    float    getNextTargetTemp() const {
        uint8_t next = _currentStep + 1;
        if (next < _stepCount) return _tempSteps[next];
        return -1.0f;
    }
    bool     isDone()            const { return _state == DONE; }
    float    getFixedVolume()    const { return _fixedVolume; }
    float    getLockedVolume()   const { return _lockedVolume; }
    float    getMaxTotalVolume() const { return _largeBottleCapacity + _smallBottleCapacity - _componentVolume; }
    float    getTargetPressure() const { return _targetPressure; }
    bool     isValveOpen()       const { return _airValve && _airValve->isOpen(); }

    const DataPoint& getDataPoint(uint8_t index) const { return _data[index]; }
    uint8_t getDataCount() const { return _dataCount; }

    void printReport() const;

    // ---------- 回调 ----------
    void onStart(Callback cb)             { _onStart = cb; }
    void onStepComplete(StepCallback cb)  { _onStep  = cb; }
    void onDone(Callback cb)              { _onDone  = cb; }
    void onError(Callback cb)             { _onError = cb; }

private:
    BME280Sensor*    _bme;
    VolumeControl*   _volumeCtrl;
    TempControl*     _tempCtrl;
    PressureControl* _pressureCtrl;
    AirValve*        _airValve;
    bool             _attached;

    State    _state;
    StepMode _stepMode;
    uint8_t  _currentStep;
    uint8_t  _stepCount;
    uint8_t  _dataCount;

    float _largeBottleCapacity;
    float _smallBottleCapacity;
    float _componentVolume;
    float _fixedVolume;
    float _totalTargetVolume;
    float _lockedVolume;

    float    _tempSteps[MAX_STEPS];
    float    _targetPressure;
    bool     _customPressure;
    bool     _paused;
    uint32_t _pauseStartMs;

    uint32_t _nodeSettleMs;         // 节点温度越过后等待采集的稳定时间
    uint32_t _nodeReachedMs;        // 节点到达时间戳

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

    void  _loadAutoSteps();
    void  _prepareAndGo();
    float _getTotalVolume() const;
    void  _nextStep();
    void  _recordDataPoint();
};

#endif
