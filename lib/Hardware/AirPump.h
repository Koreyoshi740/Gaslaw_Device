#ifndef AIRPUMP_H
#define AIRPUMP_H

#include <Arduino.h>

/**
 * @class AirPump
 * @brief 气泵 PWM 控制类
 *
 * 通过 ESP32 LEDC 外设输出 PWM 信号，驱动 AOD4184 MOSFET 控制气泵转速。
 * 气泵为单向驱动（无方向控制），仅通过占空比调节流量。
 * 使用前须调用 attach() 完成引脚与 LEDC 通道绑定。
 */
class AirPump {
public:
    AirPump();
    ~AirPump();

    /**
     * @brief 绑定 PWM 引脚并初始化 LEDC 通道
     * @param pwmPin  气泵 PWM 信号引脚号
     * @param channel 使用的 LEDC 通道（0~15，须与其他外设不冲突）
     * @note PWM 频率固定为 20 kHz，分辨率 8 bit（0~255）
     */
    void attach(uint8_t pwmPin, uint8_t channel);

    void on();                       // 以全速（占空比 255）开启气泵
    void off();                      // 关闭气泵（占空比置 0）

    /**
     * @brief 设置气泵 PWM 占空比
     * @param speed 占空比（0~255），0 表示停止
     */
    void setSpeed(uint8_t speed);

    bool    isOn()     const;   // 返回气泵当前是否开启（speed > 0）
    uint8_t getSpeed() const;   // 返回当前占空比（0~255）

private:
    uint8_t _pwmPin;    // PWM 信号引脚号
    uint8_t _channel;   // LEDC 通道号
    uint8_t _speed;     // 当前占空比（0~255）
    bool    _state;     // 运行状态（speed > 0 为 true）
    bool    _attached;  // 是否已完成引脚绑定
};

#endif
