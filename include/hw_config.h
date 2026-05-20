#pragma once

// ─── I2C 引脚 ────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN      21
#define I2C_SCL_PIN      22

// ─── 按键引脚 ────────────────────────────────────────────────────────────────
#define KEY_LEFT_PIN     39
#define KEY_RIGHT_PIN    34
#define KEY_SELECT_PIN   36
#define KEY_BACK_PIN     35

// ─── 流量传感器 ──────────────────────────────────────────────────────────────
#define FLOW_IN_PIN      33    // 进水流量传感器脉冲引脚
#define FLOW_OUT_PIN     32    // 出水流量传感器脉冲引脚

#define FLOW_IN_K        86000.0f   // 进水 K 值 (pulses/L)
#define FLOW_OUT_K       73000.0f   // 出水 K 值 (pulses/L)

// ─── 主水泵（H 桥双向驱动） ──────────────────────────────────────────────────
#define PUMP_IN1         17
#define PUMP_IN2         16
#define PUMP_PWM         4
#define PUMP_LEDC_CH     2

// ─── 冷却水泵（H 桥双向驱动，正转注水 / 反转排水） ──────────────────────────
#define COOLPUMP_IN1     18
#define COOLPUMP_IN2     19
#define COOL_PWM         23
#define COOL_LEDC_CH     1

// ─── 加热器 & 风扇（N-MOS 单向 PWM 驱动） ───────────────────────────────────
#define HEATER_PWM       26
#define FAN_PWM          25
#define HEATER_LEDC_CH   0
#define FAN_LEDC_CH      3

// ─── 气泵（单向 PWM 驱动） ───────────────────────────────────────────────────
#define AIR_PUMP_PWM     27
#define AIR_PUMP_LEDC_CH 4

// ─── 电磁阀 ──────────────────────────────────────────────────────────────────
#define VALVE_PIN        14

// ─── 瓶体物理参数 ────────────────────────────────────────────────────────────
#define LARGE_BOTTLE_ML  325.0f   // 250ml 试剂瓶实际容积
#define SMALL_BOTTLE_ML  135.0f   // 100ml 试剂瓶实际容积
#define COMPONENT_VOL_ML  10.0f   // 元件占用体积

// ─── VolumeControl PID & 限幅 ────────────────────────────────────────────────
#define VOL_PID_KP       20.0f
#define VOL_PID_KI       0.5f
#define VOL_PID_KD       1.0f
#define VOL_DEAD_BAND    0.5f    // mL
#define VOL_MIN_PWM      110
#define VOL_MAX_PWM      255

// ─── PressureControl PID & 限幅 ──────────────────────────────────────────────
#define PRES_PID_KP      5.0f
#define PRES_PID_KI      0.1f
#define PRES_PID_KD      0.1f
#define PRES_DEAD_BAND   0.2f    // hPa
#define PRES_MAX_DELTA_V 5.0f   // mL

// ─── TempControl PID & 参数 ──────────────────────────────────────────────────
#define TEMP_PID_KP            8.0f
#define TEMP_PID_KI            0.2f
#define TEMP_PID_KD           10.0f
#define TEMP_HYSTERESIS        0.5f   // ℃ HOLDING→COOLING 死区
#define TEMP_HOLDING_BAND      3.0f   // ℃ 保留兼容，暂不使用
#define TEMP_MAX_TEMP         80.0f   // ℃ 超温保护阈值
#define TEMP_FAN_COOLDOWN_MS  30000
#define TEMP_SWITCH_DELTA      3.0f   // ℃ HEATING 提前断电距离（需实测标定）
#define TEMP_COAST_BAND        1.0f   // ℃ COASTING→HOLDING 温度误差窗口

// ─── BoyleLaw 实验参数 ───────────────────────────────────────────────────────
#define BOYLE_INIT_TOTAL_VOL  400.0f   // 初始总气体体积 (mL)
#define BOYLE_TARGET_TEMP      25.0f   // 恒温目标 (℃)
#define BOYLE_STABLE_THRESHOLD  0.3f   // hPa
#define BOYLE_STABLE_COUNT        4
#define BOYLE_CIRCULATE_MS     5000
#define BOYLE_SAMPLE_MS         500

// ─── CharlesLaw 实验参数 ─────────────────────────────────────────────────────
#define CHARLES_STABLE_PRES   0.5f    // hPa
#define CHARLES_STABLE_TEMP   0.2f    // ℃
#define CHARLES_STABLE_COUNT    4
#define CHARLES_SAMPLE_MS     500

// ─── GayLussacLaw 实验参数 ───────────────────────────────────────────────────
#define GAY_NODE_SETTLE_MS   1000     // 节点温度越过后等待采集的稳定时间 (ms)
