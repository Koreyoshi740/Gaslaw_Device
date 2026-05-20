#include "AirValve.h"

/** 构造函数：将所有成员初始化为安全默认值 */
AirValve::AirValve()
    : _pin(0)
    , _state(false)
    , _attached(false)
{}

AirValve::~AirValve() {}

/**
 * 配置控制引脚为推挽输出，并将阀门初始化为关闭状态（LOW）。
 * 常闭型阀门断电后自动关闭，符合故障安全原则。
 * @param pin GPIO 引脚号
 */
void AirValve::attach(uint8_t pin) {
    _pin      = pin;
    _attached = true;

    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);   // 默认关闭（断电安全）
    _state = false;
}

/**
 * 开启气阀：输出 HIGH，MOSFET 导通，电磁阀通电开启。
 * attach() 未调用时直接返回。
 */
void AirValve::open() {
    if (!_attached) return;
    digitalWrite(_pin, HIGH);
    _state = true;
}

/**
 * 关闭气阀：输出 LOW，MOSFET 截止，电磁阀断电自动关闭。
 * attach() 未调用时直接返回。
 */
void AirValve::close() {
    if (!_attached) return;
    digitalWrite(_pin, LOW);
    _state = false;
}

/** 返回气阀当前是否开启（true = 开启） */
bool AirValve::isOpen() const {
    return _state;
}
