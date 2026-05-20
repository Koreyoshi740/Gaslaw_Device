#ifndef FLOWSENSOR_H
#define FLOWSENSOR_H

#include <Arduino.h>

/**
 * @class Flowsensor
 * @brief 流量传感器驱动类
 *
 * 通过外部中断统计脉冲数，配合硬件定时器定时计算瞬时流量与累计容积。
 * 脉冲计数与流量计算均使用临界区保护，线程安全。
 *
 * 使用方式：
 *   1. 调用 attach() 绑定引脚并注册中断
 *   2. 在硬件定时器 ISR 中每 1ms 调用一次 onTimer()
 *   3. 通过 getFlowRate() / getTotalVolume() 读取流量数据
 */
class Flowsensor {
public:
    Flowsensor();
    ~Flowsensor();

    /**
     * @brief 绑定传感器引脚并注册上升沿外部中断
     * @param pin 连接流量传感器信号线的 GPIO 引脚号
     */
    void attach(uint8_t pin);

    /**
     * @brief 设置传感器 K 系数（每升脉冲数，单位：pulses/L）
     * @param k K 系数，默认值 140000.0（即 140000 pulses/L）
     */
    void setKFactor(float k);

    /**
     * @brief 获取当前 K 系数
     * @return 当前 K 系数（pulses/L）
     */
    float getKFactor() const { return _k; }

    /**
     * @brief 外部中断服务函数（静态），在上升沿触发时累加脉冲计数
     * @param sensor 指向所属 Flowsensor 实例的指针（通过 attachInterruptArg 传入）
     * @note 必须放在 IRAM 中以满足 ISR 执行速度要求
     */
    static void IRAM_ATTR pulseISR(void* sensor);

    /**
     * @brief 获取当前累计脉冲总数（线程安全）
     * @return 自上次 clearCount() 后的脉冲计数
     */
    uint32_t getCount();

    /**
     * @brief 清零原始脉冲计数器（线程安全）
     */
    void clearCount();

    /**
     * @brief 流量计算定时回调，须在硬件定时器 ISR 中每 1ms 调用一次
     *
     * 内部维护毫秒累加器，每经过 FLOW_WINDOW_MS 毫秒计算一次：
     *   - 窗口内脉冲增量 → _deltaPulse（供 getFlowRate 换算）
     *   - 累计总脉冲     → _totalPulse（供 getTotalVolume 换算）
     *
     * @note 必须放在 IRAM 中，使用 ISR 变体临界区（portENTER_CRITICAL_ISR）
     */
    void IRAM_ATTR onTimer();

    /**
     * @brief 获取瞬时流量
     * @return 瞬时流量，单位：mL/min
     *         计算公式：(deltaPulse / 窗口秒数) / K × 1000
     */
    float getFlowRate() const;

    /**
     * @brief 获取累计总容积
     * @return 累计总容积，单位：mL
     *         计算公式：totalPulse / K × 1000
     */
    float getTotalVolume() const;

    /**
     * @brief 清零累计总容积（将 _totalPulse 归零，线程安全）
     */
    void resetTotalVolume();
    void setTotalVolumeMl(float ml);

private:
    /**
     * @brief 在中断上下文中将脉冲计数加一（由 pulseISR 调用）
     * @note 必须放在 IRAM 中
     */
    void IRAM_ATTR updatePulseCount();

    uint8_t _pin;                           // 传感器信号引脚号

    volatile uint32_t pulseCount = 0;       // 原始脉冲累计计数（由 ISR 自增）
    // 最小脉冲间隔过滤：拒绝与上次有效脉冲时间间隔 < MIN_PULSE_US 的触发，
    // 滤除上升沿毛刺/EMI 噪声。K=77000 时最大合理流速 <1000Hz，500µs 安全裕量充足。
    volatile int64_t  _lastPulseUs = 0;
    static constexpr int64_t MIN_PULSE_US = 5000;
    float _k = 140000.0f;                   // K 系数（pulses/L），默认 140000

    volatile uint32_t _lastPulseCount = 0;  // 上一个计算窗口结束时的脉冲快照
    volatile uint32_t _msCounter = 0;       // 毫秒累加器，用于判断是否到达计算窗口
    volatile uint32_t _deltaPulse = 0;      // 最近窗口内脉冲增量，供 getFlowRate() 换算
    volatile uint64_t _totalPulse = 0;      // 累计总脉冲数（uint64_t 防长时间运行溢出）

    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED; // 临界区自旋锁，保护共享变量

    // 流量计算时间窗口（ms），与 PID 控制周期对齐
    static constexpr uint32_t FLOW_WINDOW_MS = 200;
};

#endif
