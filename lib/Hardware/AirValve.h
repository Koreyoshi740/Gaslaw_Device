#ifndef AIRVALVE_H
#define AIRVALVE_H

#include <Arduino.h>

/**
 * @class AirValve
 * @brief 电磁气阀数字控制类
 *
 * 通过 AOD4184 MOSFET 驱动常闭型电磁阀（Normally Closed）：
 *   - HIGH → MOSFET 导通 → 阀门开启（通气）
 *   - LOW  → MOSFET 截止 → 阀门关闭（断电自动关闭）
 *
 * 使用前须调用 attach() 完成引脚配置。
 */
class AirValve {
public:
    AirValve();
    ~AirValve();

    /**
     * @brief 配置控制引脚，并将阀门初始化为关闭状态
     * @param pin 连接 MOSFET 栅极驱动电路的 GPIO 引脚号
     */
    void attach(uint8_t pin);

    void open();          // 开启气阀（输出 HIGH，MOSFET 导通）
    void close();         // 关闭气阀（输出 LOW，MOSFET 截止，断电自动关闭）
    bool isOpen() const;  // 返回气阀当前状态（true = 开启）

private:
    uint8_t _pin;       // 控制引脚号
    bool    _state;     // 阀门当前状态（true = 开启）
    bool    _attached;  // 是否已完成引脚配置
};

#endif
