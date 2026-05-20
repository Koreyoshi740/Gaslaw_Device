#include "Heater.h"

Heater::Heater() {}
Heater::~Heater() {}

/**
 * 绑定加热器和风扇的 PWM 引脚，配置 LEDC 通道并将输出初始化为 0。
 * PWM 参数：频率 20 kHz，8 bit 分辨率（占空比范围 0~255）。
 * @param heaterPWM 加热器 PWM 引脚
 * @param fanPWM    风扇 PWM 引脚
 * @param heaterCh  加热器 LEDC 通道（须与其他外设不冲突）
 * @param fanCh     风扇 LEDC 通道（须与其他外设不冲突）
 */
void Heater::attach(uint8_t heaterPWM, uint8_t fanPWM,
                    uint8_t heaterCh,  uint8_t fanCh)
{
    _heaterPWM = heaterPWM;
    _fanPWM    = fanPWM;
    _heaterCh  = heaterCh;
    _fanCh     = fanCh;
    _attached  = true;

    // 配置 LEDC 通道：20 kHz，8 bit
    ledcSetup(_heaterCh, 20000, 8);
    ledcAttachPin(_heaterPWM, _heaterCh);
    ledcSetup(_fanCh, 20000, 8);
    ledcAttachPin(_fanPWM, _fanCh);

    // 初始状态：关闭
    _heaterState = false;
    _fanState    = false;
    _heaterSpeed = 0;
    _fanSpeed    = 0;

    ledcWrite(_heaterCh, 0);
    ledcWrite(_fanCh,    0);
}

/** 以全速（占空比 255）开启加热器 */
void Heater::heaterOn() {
    setHeaterSpeed(255);
}

/** 关闭加热器（占空比置 0） */
void Heater::heaterOff() {
    setHeaterSpeed(0);
}

/**
 * 设置加热器 PWM 占空比。
 * 占空比为 0 时同步将状态标记为关闭，否则标记为开启。
 * @param speed 占空比（0~255）
 */
void Heater::setHeaterSpeed(uint8_t speed) {
    if (!_attached) return;
    ledcWrite(_heaterCh, speed);
    _heaterSpeed = speed;
    _heaterState = (speed > 0);
}

/** 返回加热器当前是否开启（占空比 > 0） */
bool Heater::isHeaterOn() const {
    return _heaterState;
}

/** 以全速（占空比 255）开启散热风扇 */
void Heater::fanOn() {
    setFanSpeed(255);
}

/** 关闭散热风扇（占空比置 0） */
void Heater::fanOff() {
    setFanSpeed(0);
}

/**
 * 设置风扇 PWM 占空比。
 * 占空比为 0 时同步将状态标记为关闭，否则标记为开启。
 * @param speed 占空比（0~255）
 */
void Heater::setFanSpeed(uint8_t speed) {
    if (!_attached) return;
    ledcWrite(_fanCh, speed);
    _fanSpeed = speed;
    _fanState = (speed > 0);
}

/** 返回散热风扇当前是否开启（占空比 > 0） */
bool Heater::isFanOn() const {
    return _fanState;
}

/** 同时以全速开启加热器和散热风扇 */
void Heater::on() {
    heaterOn();
    fanOn();
}

/** 同时关闭加热器和散热风扇 */
void Heater::off() {
    heaterOff();
    fanOff();
}
