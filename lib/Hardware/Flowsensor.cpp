#include "Flowsensor.h"

Flowsensor::Flowsensor() {};
Flowsensor::~Flowsensor() {};

/**
 * 绑定 GPIO 引脚，配置为上拉输入，并注册上升沿外部中断。
 * this 指针作为参数传给静态 ISR，使其能访问具体实例。
 */
void Flowsensor::attach(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
    attachInterruptArg(_pin, pulseISR, this, RISING);
};

/**
 * 设置 K 系数（pulses/L）。
 * 不同型号传感器的 K 系数不同，需根据规格书或标定结果填写。
 */
void Flowsensor::setKFactor(float k) {
    _k = k;
}

/**
 * 静态外部中断服务函数，在上升沿触发。
 * 将 void* 还原为 Flowsensor 指针后，调用实例方法完成计数。
 * @param sensor attachInterruptArg 注册时传入的 this 指针
 */
void IRAM_ATTR Flowsensor::pulseISR(void* sensor) {
    Flowsensor* _sensor = (Flowsensor*)sensor;
    _sensor->updatePulseCount();
}

/**
 * 在中断上下文中将原始脉冲计数加一。
 * 使用 esp_timer_get_time() 做最小间隔过滤，丢弃与上次脉冲间隔 < MIN_PULSE_US 的触发，
 * 以去除上升沿毛刺和 EMI 感应噪声。
 */
void IRAM_ATTR Flowsensor::updatePulseCount() {
    int64_t now = esp_timer_get_time();
    if (now - _lastPulseUs >= MIN_PULSE_US) {
        _lastPulseUs = now;
        pulseCount++;
    }
}

/**
 * 线程安全地读取当前原始脉冲计数。
 * 使用临界区防止与 ISR 并发读写产生数据撕裂。
 * @return 当前脉冲累计值
 */
uint32_t Flowsensor::getCount() {
    portENTER_CRITICAL(&_mux);
    uint32_t count = pulseCount;
    portEXIT_CRITICAL(&_mux);
    return count;
}

/**
 * 线程安全地将原始脉冲计数清零。
 */
void Flowsensor::clearCount() {
    portENTER_CRITICAL(&_mux);
    pulseCount = 0;
    _lastPulseCount = 0;
    portEXIT_CRITICAL(&_mux);
}

/**
 * 流量计算定时回调，在硬件定时器 ISR 中每 1ms 调用一次。
 * 已处于中断上下文，临界区使用 ISR 变体（portENTER_CRITICAL_ISR）。
 *
 * 每隔 FLOW_WINDOW_MS 毫秒执行一次：
 *   1. 读取当前脉冲快照，计算窗口内增量
 *   2. 更新 _deltaPulse（供 getFlowRate 换算瞬时流量）
 *   3. 累加 _totalPulse（供 getTotalVolume 换算累计容积）
 */
void IRAM_ATTR Flowsensor::onTimer() {
    _msCounter++;
    if (_msCounter >= FLOW_WINDOW_MS) {
        _msCounter = 0;

        // 在 ISR 中读取脉冲快照，使用 ISR 变体临界区
        portENTER_CRITICAL_ISR(&_mux);
        uint32_t currentCount = pulseCount;
        portEXIT_CRITICAL_ISR(&_mux);

        uint32_t delta = currentCount - _lastPulseCount;
        _lastPulseCount = currentCount;

        // 仅做整数运算，浮点转换推迟到 get 函数中执行
        _deltaPulse  = delta;
        _totalPulse += delta;
    }
}

/**
 * 获取瞬时流量（mL/min）。
 * 公式：(deltaPulse / 窗口秒数) / K × 1000
 *   = deltaPulse × (1000 / FLOW_WINDOW_MS) × 60 / K × 1000
 * @return 瞬时流量，单位 mL/min
 */
float Flowsensor::getFlowRate() const {
    portENTER_CRITICAL(&_mux);
    uint32_t delta = _deltaPulse;
    portEXIT_CRITICAL(&_mux);
    return (float)delta * (1000.0f / FLOW_WINDOW_MS) * 60.0f / _k * 1000.0f;
}

/**
 * 获取累计总容积（mL）。
 * 公式：totalPulse / K × 1000
 * @return 累计总容积，单位 mL
 */
float Flowsensor::getTotalVolume() const {
    portENTER_CRITICAL(&_mux);
    uint64_t total = _totalPulse;
    portEXIT_CRITICAL(&_mux);
    return (float)total / _k * 1000.0f;
}

/**
 * 清零累计总容积，将 _totalPulse 归零。
 * 线程安全，可在主循环或任务中调用。
 */
void Flowsensor::resetTotalVolume() {
    portENTER_CRITICAL(&_mux);
    _totalPulse = 0;
    portEXIT_CRITICAL(&_mux);
}

void Flowsensor::setTotalVolumeMl(float ml) {
    portENTER_CRITICAL(&_mux);
    _totalPulse = (uint64_t)(ml * _k / 1000.0f);
    portEXIT_CRITICAL(&_mux);
}
