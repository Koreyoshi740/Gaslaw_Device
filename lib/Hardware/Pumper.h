#ifndef PUMPER_H
#define PUMPER_H

#include <Arduino.h>

/**
 * @class Pumper
 * @brief 直流蠕动泵双向 PWM 控制类
 *
 * 通过 H 桥驱动芯片（IN1/IN2 方向控制 + PWM 调速）实现正转、反转、
 * 自由停止（coast）和主动制动（brake）四种工作状态。
 * 使用前须调用 attach() 完成引脚与 LEDC 通道绑定。
 */
class Pumper {
public:
    Pumper();
    ~Pumper();

    /**
     * @brief 绑定电机控制引脚并初始化 LEDC PWM 通道
     * @param IN1 H 桥方向控制引脚 1
     * @param IN2 H 桥方向控制引脚 2
     * @param PWM PWM 调速信号引脚
     * @param ch  使用的 LEDC 通道（0~15，须与其他外设不冲突）
     * @note PWM 频率固定为 20 kHz，分辨率 8 bit（0~255）
     */
    void attach(uint8_t IN1, uint8_t IN2, uint8_t PWM, uint8_t ch);

    void forward();   // 设置电机正转方向（需配合 setSpeed 给定转速）
    void backward();  // 设置电机反转方向（需配合 setSpeed 给定转速）

    /**
     * @brief 设置电机转速与方向
     * @param speed 速度值（-255~255）
     *              正值 → 正转，负值 → 反转，0 → 自由停止
     *              超出范围的值会被 constrain 限幅
     */
    void setSpeed(int16_t speed);

    /**
     * @brief 自由停止（coast）
     * 将 IN1/IN2 均置 LOW，PWM 占空比置 0，电机自然减速停止。
     */
    void stop();

    /**
     * @brief 主动制动（brake）
     * 将 IN1/IN2 均置 HIGH，PWM 占空比置 0，电机短路制动，快速停止。
     */
    void brake();

    /**
     * @brief 判断泵当前是否处于运行或制动状态
     * @return true  当前速度非零，或处于主动制动中
     * @return false 完全停止
     */
    bool isRunning() const;

private:
    uint8_t _IN1;            // H 桥方向控制引脚 1
    uint8_t _IN2;            // H 桥方向控制引脚 2
    uint8_t _PWM;            // PWM 调速引脚
    uint8_t _ch;             // LEDC 通道号
    int16_t _currentSpeed;   // 当前速度（-255~255）
    bool    _braking = false; // 主动制动标志
    bool    isAttached = 0;  // 是否已完成引脚绑定
};

#endif
