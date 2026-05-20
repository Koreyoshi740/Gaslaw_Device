#include "Pumper.h"

Pumper::Pumper() {};
Pumper::~Pumper() {};

/**
 * 绑定 H 桥控制引脚，配置方向引脚为推挽输出，
 * 配置 LEDC 通道（20 kHz，8 bit），并调用 stop() 确保初始状态安全。
 * @param IN1 方向控制引脚 1
 * @param IN2 方向控制引脚 2
 * @param PWM PWM 调速引脚
 * @param ch  LEDC 通道号
 */
void Pumper::attach(uint8_t IN1, uint8_t IN2, uint8_t PWM, uint8_t ch) {
    _IN1 = IN1;
    _IN2 = IN2;
    _PWM = PWM;
    _ch  = ch;
    pinMode(_IN1, OUTPUT);
    pinMode(_IN2, OUTPUT);
    // 配置 LEDC 通道：20 kHz，8 bit
    ledcSetup(_ch, 20000, 8);
    ledcAttachPin(_PWM, _ch);
    isAttached = 1;
    stop();   // 初始化为自由停止状态
};

/**
 * 设置电机为正转方向：IN1=HIGH，IN2=LOW。
 * 仅设置方向，不输出 PWM，需配合 setSpeed() 使用。
 */
void Pumper::forward() {
    if (!isAttached) return;
    digitalWrite(_IN1, HIGH);
    digitalWrite(_IN2, LOW);
}

/**
 * 设置电机为反转方向：IN1=LOW，IN2=HIGH。
 * 仅设置方向，不输出 PWM，需配合 setSpeed() 使用。
 */
void Pumper::backward() {
    if (!isAttached) return;
    digitalWrite(_IN1, LOW);
    digitalWrite(_IN2, HIGH);
}

/**
 * 根据速度值设置方向并输出对应占空比：
 *   speed > 0 → forward() + PWM = speed
 *   speed < 0 → backward() + PWM = -speed
 *   speed = 0 → stop()
 * 输入值超出 -255~255 范围时自动限幅。
 * @param speed 速度（-255~255）
 */
void Pumper::setSpeed(int16_t speed) {
    if (!isAttached) return;
    _currentSpeed = constrain(speed, -255, 255);
    if (_currentSpeed > 0) {
        forward();
        ledcWrite(_ch, _currentSpeed);
    } else if (_currentSpeed < 0) {
        backward();
        ledcWrite(_ch, -_currentSpeed);
    } else {
        stop();
    }
}

/**
 * 自由停止（coast）：IN1/IN2 均置 LOW，PWM 占空比为 0。
 * 电机输出端开路，靠自身惯性自然减速。
 */
void Pumper::stop() {
    if (!isAttached) return;
    digitalWrite(_IN1, LOW);
    digitalWrite(_IN2, LOW);
    ledcWrite(_ch, 0);
    _currentSpeed = 0;
    _braking = false;
}

/**
 * 主动制动（brake）：IN1/IN2 均置 HIGH，PWM 占空比为 0。
 * 电机绕组短路，产生反向制动力矩，快速停转。
 */
void Pumper::brake() {
    if (!isAttached) return;
    digitalWrite(_IN1, HIGH);
    digitalWrite(_IN2, HIGH);
    ledcWrite(_ch, 0);
    _currentSpeed = 0;
    _braking = true;
}

/**
 * 判断泵当前是否处于运行或主动制动状态。
 * @return true  速度非零或正在主动制动
 * @return false 完全停止（coast 状态）
 */
bool Pumper::isRunning() const {
    return _currentSpeed != 0 || _braking;
}
