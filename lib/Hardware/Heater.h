#ifndef HEATER_H
#define HEATER_H

#include <Arduino.h>

/**
 * @class Heater
 * @brief 加热器 + 散热风扇 PWM 控制类
 *
 * 通过 ESP32 LEDC 外设输出 PWM 信号，驱动 AOD4184 MOSFET
 * 分别控制加热丝和散热风扇的功率。
 * 使用前须调用 attach() 完成引脚与 LEDC 通道绑定。
 */
class Heater {
public:
    Heater();
    ~Heater();

    /**
     * @brief 绑定控制引脚并初始化 LEDC PWM 通道
     * @param heaterPWM 加热器 PWM 信号引脚号
     * @param fanPWM    散热风扇 PWM 信号引脚号
     * @param heaterCh  加热器使用的 LEDC 通道（0~15）
     * @param fanCh     散热风扇使用的 LEDC 通道（0~15）
     * @note PWM 频率固定为 20 kHz，分辨率 8 bit（0~255）
     */
    void attach(uint8_t heaterPWM, uint8_t fanPWM,
                uint8_t heaterCh,  uint8_t fanCh);

    // ── 加热器控制 ────────────────────────────────────────────
    void heaterOn();                      // 以最大占空比（255）开启加热器
    void heaterOff();                     // 关闭加热器（占空比置 0）
    void setHeaterSpeed(uint8_t speed);   // 设置加热器占空比（0~255）
    bool isHeaterOn() const;              // 返回加热器当前是否开启
    uint8_t getHeaterSpeed() const { return _heaterSpeed; }

    // ── 散热风扇控制 ─────────────────────────────────────────
    void fanOn();                         // 以最大占空比（255）开启风扇
    void fanOff();                        // 关闭风扇（占空比置 0）
    void setFanSpeed(uint8_t speed);      // 设置风扇占空比（0~255）
    bool isFanOn() const;                 // 返回风扇当前是否开启

    // ── 组合控制 ─────────────────────────────────────────────
    void on();    // 同时以全速开启加热器和风扇
    void off();   // 同时关闭加热器和风扇

private:
    uint8_t _heaterPWM;     // 加热器 PWM 引脚
    uint8_t _fanPWM;        // 风扇 PWM 引脚

    uint8_t _heaterSpeed;   // 当前加热器占空比（0~255）
    uint8_t _fanSpeed;      // 当前风扇占空比（0~255）
    uint8_t _heaterCh;      // 加热器 LEDC 通道号
    uint8_t _fanCh;         // 风扇 LEDC 通道号

    bool _heaterState;      // 加热器开启状态（speed > 0 为 true）
    bool _fanState;         // 风扇开启状态（speed > 0 为 true）
    bool _attached;         // 是否已完成引脚绑定
};

#endif
