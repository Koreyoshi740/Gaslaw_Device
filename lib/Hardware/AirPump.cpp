#include "AirPump.h"

/** 构造函数：将所有成员初始化为安全默认值 */
AirPump::AirPump()
    : _pwmPin(0)
    , _channel(0)
    , _speed(0)
    , _state(false)
    , _attached(false)
{}

AirPump::~AirPump() {}

/**
 * 绑定 PWM 引脚，配置 LEDC 通道并将输出初始化为 0（停止）。
 * PWM 参数：频率 20 kHz，8 bit 分辨率（占空比范围 0~255）。
 * @param pwmPin  气泵 PWM 引脚
 * @param channel LEDC 通道号
 */
void AirPump::attach(uint8_t pwmPin, uint8_t channel) {
    _pwmPin   = pwmPin;
    _channel  = channel;
    _attached = true;

    // 配置 LEDC 通道：20 kHz，8 bit
    ledcSetup(_channel, 20000, 8);
    ledcAttachPin(_pwmPin, _channel);

    // 初始状态：停止
    _speed = 0;
    _state = false;
    ledcWrite(_channel, 0);
}

/** 以全速（占空比 255）开启气泵 */
void AirPump::on() {
    setSpeed(255);
}

/** 关闭气泵（占空比置 0） */
void AirPump::off() {
    setSpeed(0);
}

/**
 * 设置气泵 PWM 占空比。
 * 占空比为 0 时将状态标记为停止，否则标记为运行。
 * attach() 未调用时直接返回，不执行任何操作。
 * @param speed 占空比（0~255）
 */
void AirPump::setSpeed(uint8_t speed) {
    if (!_attached) return;
    ledcWrite(_channel, speed);
    _speed = speed;
    _state = (speed > 0);
}

/** 返回气泵当前是否运行（占空比 > 0） */
bool AirPump::isOn() const {
    return _state;
}

/** 返回当前 PWM 占空比（0~255） */
uint8_t AirPump::getSpeed() const {
    return _speed;
}
