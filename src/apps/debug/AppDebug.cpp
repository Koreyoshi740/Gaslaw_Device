#include <core/app/app_system.h>
#include <ui/ListView/ListView.h>
#include <ui/Popup/StatusEntry.h>
#include "HardwareManager.h"
#include "AppDebug.h"

extern PixelUI ui;

// ─── App icon (24×24) ─────────────────────────────────────────────────────────
__attribute__((aligned(4)))
static const unsigned char image_debug_bits[] = {
  0xF0, 0xFF, 0x0F, 0xFC, 0xFF, 0x3F, 0x1E, 0xFF, 0x67, 0x3E, 0xFE, 0x41, 
  0x7F, 0xFC, 0x80, 0x7F, 0xFC, 0xC0, 0x7D, 0x38, 0xE0, 0x19, 0x3C, 0xF0, 
  0x01, 0x18, 0xF8, 0x03, 0x30, 0xFC, 0x07, 0x60, 0xFE, 0xFF, 0xC0, 0xFF, 
  0xFF, 0x80, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0x03, 0xFE, 0xFF, 0x0E, 0xFC, 
  0x7F, 0x1E, 0xF8, 0x3F, 0x3F, 0xF0, 0x9F, 0x3F, 0xE0, 0xC7, 0xFF, 0xC0, 
  0xC2, 0xFF, 0x59, 0xE0, 0xFF, 0x43, 0xF8, 0xFF, 0x27, 0xF0, 0xFF, 0x0F, 
};

// ═══════════════════════════════════════════════════════════════════════════════
// 子页：参数调节（挂在液位调试下）
// ═══════════════════════════════════════════════════════════════════════════════
// PID/死区: ×10 integer for popup display, float mirror for list display (%.1f)
static int32_t g_param_kp    = (int32_t)(VOL_PID_KP   * 10);
static int32_t g_param_ki    = (int32_t)(VOL_PID_KI   * 10);
static int32_t g_param_kd    = (int32_t)(VOL_PID_KD   * 10);
static int32_t g_param_dead  = (int32_t)(VOL_DEAD_BAND * 10);
static float   g_param_kp_f   = VOL_PID_KP;
static float   g_param_ki_f   = VOL_PID_KI;
static float   g_param_kd_f   = VOL_PID_KD;
static float   g_param_dead_f = VOL_DEAD_BAND;
// PWM: direct int32_t 0~255
static int32_t g_param_minpwm = VOL_MIN_PWM;
static int32_t g_param_maxpwm = VOL_MAX_PWM;
// K值: direct real value, step=100 in popup
static int32_t g_param_kin  = (int32_t)FLOW_IN_K;
static int32_t g_param_kout = (int32_t)FLOW_OUT_K;

static ListItem param_items[12];

static void initParamItems() {
    strncpy(param_items[0].title, ">>> 参数调节 <<<", sizeof(param_items[0].title) - 1);

    strncpy(param_items[1].title, "Kp", sizeof(param_items[1].title) - 1);
    param_items[1].extra.float_dot1f_Value = &g_param_kp_f;
    param_items[1].pFunc = []() {
        ui.showPopupProgress(g_param_kp, 0, 200, "Kp", 110, 40, 10000, 1,
            [](int32_t v){ g_param_kp_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(param_items[2].title, "Ki", sizeof(param_items[2].title) - 1);
    param_items[2].extra.float_dot1f_Value = &g_param_ki_f;
    param_items[2].pFunc = []() {
        ui.showPopupProgress(g_param_ki, 0, 100, "Ki", 110, 40, 10000, 1,
            [](int32_t v){ g_param_ki_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(param_items[3].title, "Kd", sizeof(param_items[3].title) - 1);
    param_items[3].extra.float_dot1f_Value = &g_param_kd_f;
    param_items[3].pFunc = []() {
        ui.showPopupProgress(g_param_kd, 0, 100, "Kd", 110, 40, 10000, 1,
            [](int32_t v){ g_param_kd_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(param_items[4].title, "死区", sizeof(param_items[4].title) - 1);
    param_items[4].extra.float_dot1f_Value = &g_param_dead_f;
    param_items[4].pFunc = []() {
        ui.showPopupProgress(g_param_dead, 0, 50, "死区", 110, 40, 10000, 1,
            [](int32_t v){ g_param_dead_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(param_items[5].title, "最小PWM", sizeof(param_items[5].title) - 1);
    param_items[5].extra.intValue = &g_param_minpwm;
    param_items[5].pFunc = []() {
        ui.showPopupProgress(g_param_minpwm, 0, 255, "最小PWM", 110, 40, 10000, 1, nullptr, true, 1);
    };

    strncpy(param_items[6].title, "最大PWM", sizeof(param_items[6].title) - 1);
    param_items[6].extra.intValue = &g_param_maxpwm;
    param_items[6].pFunc = []() {
        ui.showPopupProgress(g_param_maxpwm, 0, 255, "最大PWM", 110, 40, 10000, 1, nullptr, true, 1);
    };

    strncpy(param_items[7].title, "进水K", sizeof(param_items[7].title) - 1);
    param_items[7].extra.intValue = &g_param_kin;
    param_items[7].pFunc = []() {
        ui.showPopupProgress(g_param_kin, 50000, 200000, "进水K", 110, 40, 10000, 1, nullptr, true, 100);
    };

    strncpy(param_items[8].title, "出水K", sizeof(param_items[8].title) - 1);
    param_items[8].extra.intValue = &g_param_kout;
    param_items[8].pFunc = []() {
        ui.showPopupProgress(g_param_kout, 50000, 200000, "出水K", 110, 40, 10000, 1, nullptr, true, 100);
    };

    strncpy(param_items[9].title, "应用PID死区PWM", sizeof(param_items[9].title) - 1);
    param_items[9].pFunc = []() {
        volumeCtrl.setPID(g_param_kp * 0.1f, g_param_ki * 0.1f, g_param_kd * 0.1f);
        volumeCtrl.setDeadBand(g_param_dead * 0.1f);
        volumeCtrl.setMinPWM((int16_t)g_param_minpwm);
        volumeCtrl.setMaxPWM((int16_t)g_param_maxpwm);
        ui.showPopupInfo("参数已应用", "参数调节", 100, 30, 1500);
    };

    strncpy(param_items[10].title, "应用K值", sizeof(param_items[10].title) - 1);
    param_items[10].pFunc = []() {
        float savedIn  = flowIn.getTotalVolume();
        float savedOut = flowOut.getTotalVolume();
        flowIn.setKFactor((float)g_param_kin);
        flowOut.setKFactor((float)g_param_kout);
        flowIn.setTotalVolumeMl(savedIn);
        flowOut.setTotalVolumeMl(savedOut);
        static char buf[48];
        snprintf(buf, sizeof(buf), "进:%.0f 出:%.0f", flowIn.getKFactor(), flowOut.getKFactor());
        ui.showPopupInfo(buf, "K值已应用", 110, 30, 2000);
    };

    strncpy(param_items[11].title, "恢复默认", sizeof(param_items[11].title) - 1);
    param_items[11].pFunc = []() {
        g_param_kp    = (int32_t)(VOL_PID_KP   * 10);  g_param_kp_f   = VOL_PID_KP;
        g_param_ki    = (int32_t)(VOL_PID_KI   * 10);  g_param_ki_f   = VOL_PID_KI;
        g_param_kd    = (int32_t)(VOL_PID_KD   * 10);  g_param_kd_f   = VOL_PID_KD;
        g_param_dead  = (int32_t)(VOL_DEAD_BAND * 10);  g_param_dead_f = VOL_DEAD_BAND;
        g_param_minpwm = VOL_MIN_PWM;
        g_param_maxpwm = VOL_MAX_PWM;
        g_param_kin  = (int32_t)FLOW_IN_K;
        g_param_kout = (int32_t)FLOW_OUT_K;
        volumeCtrl.setPID(VOL_PID_KP, VOL_PID_KI, VOL_PID_KD);
        volumeCtrl.setDeadBand(VOL_DEAD_BAND);
        volumeCtrl.setMinPWM(VOL_MIN_PWM);
        volumeCtrl.setMaxPWM(VOL_MAX_PWM);
        flowIn.setKFactor(FLOW_IN_K);
        flowOut.setKFactor(FLOW_OUT_K);
        ui.showPopupInfo("已恢复默认参数", "参数调节", 110, 30, 1500);
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// 子页：液位调试
// ═══════════════════════════════════════════════════════════════════════════════
static int32_t g_vol_liquid = 50;
static int32_t g_vol_air    = 200;
static int32_t g_vol_delta  = 0;
static bool    g_vol_valve  = false;
static bool    g_vol_drain  = false;

// ─── 液位实时状态（用于 PopupStatus） ────────────────────────────────────────
static const char* volStateStr() {
    switch (volumeCtrl.getState()) {
        case VolumeControl::IDLE:      return "IDLE";
        case VolumeControl::RUNNING:   return "RUNNING";
        case VolumeControl::COMPLETED: return "DONE";
        case VolumeControl::STOPPED:   return "STOP";
        case VolumeControl::ERROR:     return "ERROR";
        default:                       return "?";
    }
}

static const StatusEntry vol_status[] = {
    { "状态",   [](char* b, size_t n){ snprintf(b, n, "%s",    volStateStr()); }},
    { "液位",   [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getLiquidVolume()); }},
    { "气量",   [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getAirVolume()); }},
    { "原点",   [](char* b, size_t n){ volumeCtrl.hasOrigin()
                                            ? snprintf(b, n, "%.1f", volumeCtrl.getOriginVolume())
                                            : snprintf(b, n, "none"); }},
    { "目标",   [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getTargetVolume()); }},
    { "剩余",   [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getRemainingVolume()); }},
    { "进度%",  [](char* b, size_t n){ snprintf(b, n, "%.0f", volumeCtrl.getProgressPercent()); }},
    { "进水流", [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getInFlow()); }},
    { "出水流", [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getOutFlow()); }},
    { "进水总", [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getInFlowTotal()); }},
    { "出水总", [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getOutFlowTotal()); }},
    { "泵PWM",  [](char* b, size_t n){ snprintf(b, n, "%d",   (int)volumeCtrl.getPumpSpeed()); }},
};

static ListItem calib_in_items[6];
static ListItem calib_out_items[6];
static ListItem calib_items[3];

static ListItem vol_items[16];

static void initVolItems() {
    strncpy(vol_items[0].title, ">>> 液位调试 <<<", sizeof(vol_items[0].title) - 1);

    strncpy(vol_items[1].title, "参数调节", sizeof(vol_items[1].title) - 1);
    vol_items[1].nextList       = param_items;
    vol_items[1].nextListLength = 12;

    strncpy(vol_items[2].title, "K值校准", sizeof(vol_items[2].title) - 1);
    vol_items[2].nextList       = calib_items;
    vol_items[2].nextListLength = 3;

    strncpy(vol_items[3].title, "目标液位(mL)", sizeof(vol_items[3].title) - 1);
    vol_items[3].extra.intValue = &g_vol_liquid;
    vol_items[3].pFunc = []() {
        ui.showPopupProgress(g_vol_liquid, 0, 200, "目标液位(mL)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(vol_items[4].title, "目标气量(mL)", sizeof(vol_items[4].title) - 1);
    vol_items[4].extra.intValue = &g_vol_air;
    vol_items[4].pFunc = []() {
        ui.showPopupProgress(g_vol_air, 0, 450, "目标气量(mL)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(vol_items[5].title, "增量(mL)", sizeof(vol_items[5].title) - 1);
    vol_items[5].extra.intValue = &g_vol_delta;
    vol_items[5].pFunc = []() {
        ui.showPopupProgress(g_vol_delta, -200, 200, "增量(mL)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(vol_items[6].title, "气阀", sizeof(vol_items[6].title) - 1);
    vol_items[6].extra.switchValue = &g_vol_valve;
    vol_items[6].pFunc = []() {
        if (g_vol_valve) airValve.open();
        else             airValve.close();
    };

    strncpy(vol_items[7].title, "排水", sizeof(vol_items[7].title) - 1);
    vol_items[7].extra.switchValue = &g_vol_drain;
    vol_items[7].pFunc = []() {
        if (g_vol_drain) pump.setSpeed(-255);
        else             pump.stop();
    };

    strncpy(vol_items[8].title, "设置原点", sizeof(vol_items[8].title) - 1);
    vol_items[8].pFunc = []() {
        volumeCtrl.setOrigin();
        static char buf[32];
        snprintf(buf, sizeof(buf), "原点: %.1f mL", volumeCtrl.getOriginVolume());
        ui.showPopupInfo(buf, "液位", 100, 30, 2000);
    };

    strncpy(vol_items[9].title, "清除原点", sizeof(vol_items[9].title) - 1);
    vol_items[9].pFunc = []() {
        volumeCtrl.clearOrigin();
        ui.showPopupInfo("原点已清除", "液位", 90, 30, 1500);
    };

    strncpy(vol_items[10].title, "执行液位目标", sizeof(vol_items[10].title) - 1);
    vol_items[10].pFunc = []() {
        volumeCtrl.setTargetVolume((float)g_vol_liquid);
        volumeCtrl.start();
        static char buf[32];
        snprintf(buf, sizeof(buf), "目标: %d mL", (int)g_vol_liquid);
        ui.showPopupInfo(buf, "液位目标已启动", 110, 30, 1500);
    };

    strncpy(vol_items[11].title, "执行气量目标", sizeof(vol_items[11].title) - 1);
    vol_items[11].pFunc = []() {
        volumeCtrl.setTargetAirVolume((float)g_vol_air);
        volumeCtrl.start();
        static char buf[32];
        snprintf(buf, sizeof(buf), "目标: %d mL", (int)g_vol_air);
        ui.showPopupInfo(buf, "气量目标已启动", 110, 30, 1500);
    };

    strncpy(vol_items[12].title, "执行增量", sizeof(vol_items[12].title) - 1);
    vol_items[12].pFunc = []() {
        if (!volumeCtrl.hasOrigin()) {
            ui.showPopupInfo("请先设置原点", "液位", 100, 30, 1500);
            return;
        }
        volumeCtrl.setTargetDelta((float)g_vol_delta);
        volumeCtrl.start();
        static char buf[32];
        snprintf(buf, sizeof(buf), "增量: %d mL", (int)g_vol_delta);
        ui.showPopupInfo(buf, "增量已启动", 100, 30, 1500);
    };

    strncpy(vol_items[13].title, "重置", sizeof(vol_items[13].title) - 1);
    vol_items[13].pFunc = []() {
        volumeCtrl.reset();
        airValve.close();
        pump.stop();
        g_vol_valve = false;
        g_vol_drain = false;
        flowIn.resetTotalVolume();
        flowOut.resetTotalVolume();
        ui.showPopupInfo("已重置", "液位", 90, 30, 1200);
    };

    strncpy(vol_items[14].title, "停止", sizeof(vol_items[14].title) - 1);
    vol_items[14].pFunc = []() {
        volumeCtrl.stop();
        airValve.close();
        pump.stop();
        g_vol_valve = false;
        g_vol_drain = false;
        ui.showPopupInfo("已停止", "液位", 90, 30, 1200);
    };

    strncpy(vol_items[15].title, "液位状态", sizeof(vol_items[15].title) - 1);
    vol_items[15].pFunc = []() {
        ui.showPopupStatus(vol_status, 12, "液位状态", 126, 62, 30000);
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// 子页：温控调试
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 温控参数变量 ─────────────────────────────────────────────────────────────
static int32_t g_temp_kp     = (int32_t)(TEMP_PID_KP      * 10);
static int32_t g_temp_ki     = (int32_t)(TEMP_PID_KI      * 10);
static int32_t g_temp_kd     = (int32_t)(TEMP_PID_KD      * 10);
static int32_t g_temp_hyst   = (int32_t)(TEMP_HYSTERESIS  * 10);
static int32_t g_temp_band   = (int32_t)(TEMP_HOLDING_BAND* 10);
static int32_t g_temp_max    = (int32_t)TEMP_MAX_TEMP;
static float   g_temp_kp_f   = TEMP_PID_KP;
static float   g_temp_ki_f   = TEMP_PID_KI;
static float   g_temp_kd_f   = TEMP_PID_KD;
static float   g_temp_hyst_f = TEMP_HYSTERESIS;
static float   g_temp_band_f = TEMP_HOLDING_BAND;

static ListItem temp_param_items[9];

static void initTempParamItems() {
    strncpy(temp_param_items[0].title, ">>> 温控参数 <<<", sizeof(temp_param_items[0].title) - 1);

    strncpy(temp_param_items[1].title, "Kp", sizeof(temp_param_items[1].title) - 1);
    temp_param_items[1].extra.float_dot1f_Value = &g_temp_kp_f;
    temp_param_items[1].pFunc = []() {
        ui.showPopupProgress(g_temp_kp, 0, 500, "Kp", 110, 40, 10000, 1,
            [](int32_t v){ g_temp_kp_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(temp_param_items[2].title, "Ki", sizeof(temp_param_items[2].title) - 1);
    temp_param_items[2].extra.float_dot1f_Value = &g_temp_ki_f;
    temp_param_items[2].pFunc = []() {
        ui.showPopupProgress(g_temp_ki, 0, 100, "Ki", 110, 40, 10000, 1,
            [](int32_t v){ g_temp_ki_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(temp_param_items[3].title, "Kd", sizeof(temp_param_items[3].title) - 1);
    temp_param_items[3].extra.float_dot1f_Value = &g_temp_kd_f;
    temp_param_items[3].pFunc = []() {
        ui.showPopupProgress(g_temp_kd, 0, 200, "Kd", 110, 40, 10000, 1,
            [](int32_t v){ g_temp_kd_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(temp_param_items[4].title, "迟滞带(℃)", sizeof(temp_param_items[4].title) - 1);
    temp_param_items[4].extra.float_dot1f_Value = &g_temp_hyst_f;
    temp_param_items[4].pFunc = []() {
        ui.showPopupProgress(g_temp_hyst, 0, 50, "迟滞带(℃)", 110, 40, 10000, 1,
            [](int32_t v){ g_temp_hyst_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(temp_param_items[5].title, "保温带(℃)", sizeof(temp_param_items[5].title) - 1);
    temp_param_items[5].extra.float_dot1f_Value = &g_temp_band_f;
    temp_param_items[5].pFunc = []() {
        ui.showPopupProgress(g_temp_band, 0, 100, "保温带(℃)", 110, 40, 10000, 1,
            [](int32_t v){ g_temp_band_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(temp_param_items[6].title, "最高温度(℃)", sizeof(temp_param_items[6].title) - 1);
    temp_param_items[6].extra.intValue = &g_temp_max;
    temp_param_items[6].pFunc = []() {
        ui.showPopupProgress(g_temp_max, 30, 100, "最高温度(℃)", 110, 40, 10000, 1, nullptr, true, 1);
    };

    strncpy(temp_param_items[7].title, "应用参数", sizeof(temp_param_items[7].title) - 1);
    temp_param_items[7].pFunc = []() {
        tempCtrl.setPID(g_temp_kp * 0.1f, g_temp_ki * 0.1f, g_temp_kd * 0.1f);
        tempCtrl.setHysteresis(g_temp_hyst * 0.1f);
        tempCtrl.setHoldingBand(g_temp_band * 0.1f);
        tempCtrl.setMaxTemp((float)g_temp_max);
        ui.showPopupInfo("参数已应用", "温控参数", 100, 30, 1500);
    };

    strncpy(temp_param_items[8].title, "恢复默认", sizeof(temp_param_items[8].title) - 1);
    temp_param_items[8].pFunc = []() {
        g_temp_kp = (int32_t)(TEMP_PID_KP * 10);       g_temp_kp_f   = TEMP_PID_KP;
        g_temp_ki = (int32_t)(TEMP_PID_KI * 10);       g_temp_ki_f   = TEMP_PID_KI;
        g_temp_kd = (int32_t)(TEMP_PID_KD * 10);       g_temp_kd_f   = TEMP_PID_KD;
        g_temp_hyst = (int32_t)(TEMP_HYSTERESIS * 10); g_temp_hyst_f = TEMP_HYSTERESIS;
        g_temp_band = (int32_t)(TEMP_HOLDING_BAND*10); g_temp_band_f = TEMP_HOLDING_BAND;
        g_temp_max  = (int32_t)TEMP_MAX_TEMP;
        tempCtrl.setPID(TEMP_PID_KP, TEMP_PID_KI, TEMP_PID_KD);
        tempCtrl.setHysteresis(TEMP_HYSTERESIS);
        tempCtrl.setHoldingBand(TEMP_HOLDING_BAND);
        tempCtrl.setMaxTemp(TEMP_MAX_TEMP);
        ui.showPopupInfo("已恢复默认参数", "温控参数", 110, 30, 1500);
    };
}

// ─── 温控主菜单变量 ───────────────────────────────────────────────────────────
static int32_t g_temp_target    = 25;
static int32_t g_temp_cooldown  = 20;
static bool    g_coolpump_fill  = false;
static bool    g_coolpump_drain = false;
static bool    g_airpump_on     = false;
static bool    g_temp_valve     = false;
static bool    g_fan_on         = false;
static bool    g_heater_on      = false;

// ─── 温控实时状态 ─────────────────────────────────────────────────────────────
static const char* tempStateStr() {
    switch (tempCtrl.getState()) {
        case TempControl::IDLE:    return "IDLE";
        case TempControl::HEATING: return "HEAT";
        case TempControl::HOLDING: return "HOLD";
        case TempControl::COOLING: return "COOL";
        case TempControl::ERROR:   return "ERR";
        default:                   return "?";
    }
}

static const char* tempCooldownStr() {
    switch (tempCtrl.getCooldownState()) {
        case TempControl::COOLDOWN_IDLE:     return "IDLE";
        case TempControl::COOLDOWN_COOLING:  return "COOL";
        case TempControl::COOLDOWN_DRAINING: return "DRAIN";
        case TempControl::COOLDOWN_DONE:     return "DONE";
        default:                             return "?";
    }
}

static const StatusEntry temp_status[] = {
    { "状态",   [](char* b, size_t n){ snprintf(b, n, "%s",    tempStateStr()); }},
    { "降温状态",[](char* b, size_t n){ snprintf(b, n, "%s",   tempCooldownStr()); }},
    { "当前温度",  [](char* b, size_t n){ snprintf(b, n, "%.1f", tempCtrl.getCurrentTemp()); }},
    { "目标温度",  [](char* b, size_t n){ snprintf(b, n, "%.1f", tempCtrl.getTargetTemp()); }},
    { "误差",    [](char* b, size_t n){ snprintf(b, n, "%.2f", tempCtrl.getTempError()); }},
    { "冷却泵",  [](char* b, size_t n){ snprintf(b, n, "%s",   tempCtrl.isCoolPumpRunning() ? "ON" : "OFF"); }},
    { "循环泵",  [](char* b, size_t n){ snprintf(b, n, "%s",   tempCtrl.isAirPumpRunning()  ? "ON" : "OFF"); }},
};

static ListItem temp_items[14];

static void initTempItems() {
    initTempParamItems();

    strncpy(temp_items[0].title, ">>> 温控调试 <<<", sizeof(temp_items[0].title) - 1);

    strncpy(temp_items[1].title, "参数调节", sizeof(temp_items[1].title) - 1);
    temp_items[1].nextList       = temp_param_items;
    temp_items[1].nextListLength = 9;

    strncpy(temp_items[2].title, "目标温度(℃)", sizeof(temp_items[2].title) - 1);
    temp_items[2].extra.intValue = &g_temp_target;
    temp_items[2].pFunc = []() {
        ui.showPopupProgress(g_temp_target, 0, 80, "目标温度(℃)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(temp_items[3].title, "降温目标(℃)", sizeof(temp_items[3].title) - 1);
    temp_items[3].extra.intValue = &g_temp_cooldown;
    temp_items[3].pFunc = []() {
        ui.showPopupProgress(g_temp_cooldown, 0, 80, "降温目标(℃)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(temp_items[4].title, "冷却注水", sizeof(temp_items[4].title) - 1);
    temp_items[4].extra.switchValue = &g_coolpump_fill;
    temp_items[4].pFunc = []() {
        if (g_coolpump_fill) {
            g_coolpump_drain = false;
            coolPump.setSpeed(255);
        } else {
            coolPump.stop();
        }
    };

    strncpy(temp_items[5].title, "冷却排水", sizeof(temp_items[5].title) - 1);
    temp_items[5].extra.switchValue = &g_coolpump_drain;
    temp_items[5].pFunc = []() {
        if (g_coolpump_drain) {
            g_coolpump_fill = false;
            coolPump.setSpeed(-255);
        } else {
            coolPump.stop();
        }
    };

    strncpy(temp_items[6].title, "循环气泵", sizeof(temp_items[6].title) - 1);
    temp_items[6].extra.switchValue = &g_airpump_on;
    temp_items[6].pFunc = []() {
        if (g_airpump_on) tempCtrl.airPumpOn();
        else              tempCtrl.airPumpOff();
    };

    strncpy(temp_items[7].title, "气阀", sizeof(temp_items[7].title) - 1);
    temp_items[7].extra.switchValue = &g_temp_valve;
    temp_items[7].pFunc = []() {
        if (g_temp_valve) airValve.open();
        else              airValve.close();
    };

    strncpy(temp_items[8].title, "风扇", sizeof(temp_items[8].title) - 1);
    temp_items[8].extra.switchValue = &g_fan_on;
    temp_items[8].pFunc = []() {
        if (g_fan_on) heater.fanOn();
        else          heater.fanOff();
    };

    strncpy(temp_items[9].title, "加热片", sizeof(temp_items[9].title) - 1);
    temp_items[9].extra.switchValue = &g_heater_on;
    temp_items[9].pFunc = []() {
        if (g_heater_on) heater.heaterOn();
        else             heater.heaterOff();
    };

    strncpy(temp_items[10].title, "启动加热", sizeof(temp_items[10].title) - 1);
    temp_items[10].pFunc = []() {
        tempCtrl.setTargetTemp((float)g_temp_target);
        tempCtrl.start();
        static char buf[32];
        snprintf(buf, sizeof(buf), "目标: %d ℃", g_temp_target);
        ui.showPopupInfo(buf, "加热已启动", 100, 30, 1500);
    };

    strncpy(temp_items[11].title, "启动降温", sizeof(temp_items[11].title) - 1);
    temp_items[11].pFunc = []() {
        tempCtrl.startCooldown((float)g_temp_cooldown);
        static char buf[32];
        snprintf(buf, sizeof(buf), "目标: %d ℃", g_temp_cooldown);
        ui.showPopupInfo(buf, "降温已启动", 100, 30, 1500);
    };

    strncpy(temp_items[12].title, "停止", sizeof(temp_items[12].title) - 1);
    temp_items[12].pFunc = []() {
        tempCtrl.stop();
        coolPump.stop();
        tempCtrl.airPumpOff();
        heater.heaterOff();
        heater.fanOff();
        airValve.close();
        g_coolpump_fill  = false;
        g_coolpump_drain = false;
        g_airpump_on     = false;
        g_temp_valve     = false;
        g_fan_on         = false;
        g_heater_on      = false;
        ui.showPopupInfo("已停止", "温控", 90, 30, 1200);
    };

    strncpy(temp_items[13].title, "温控状态", sizeof(temp_items[13].title) - 1);
    temp_items[13].pFunc = []() {
        ui.showPopupStatus(temp_status, 7, "温控状态", 126, 62, 30000);
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// 子页：压强调试
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 压强参数变量（PID ×100 精度，死区/最大ΔV ×10） ──────────────────────────
static int32_t g_pres_kp     = (int32_t)(PRES_PID_KP    * 100);
static int32_t g_pres_ki     = (int32_t)(PRES_PID_KI    * 100);
static int32_t g_pres_kd     = (int32_t)(PRES_PID_KD    * 100);
static int32_t g_pres_dead   = (int32_t)(PRES_DEAD_BAND * 10);
static int32_t g_pres_maxdv  = (int32_t)(PRES_MAX_DELTA_V * 10);
static float   g_pres_kp_f   = PRES_PID_KP;
static float   g_pres_ki_f   = PRES_PID_KI;
static float   g_pres_kd_f   = PRES_PID_KD;
static float   g_pres_dead_f = PRES_DEAD_BAND;
static float   g_pres_maxdv_f= PRES_MAX_DELTA_V;

static ListItem pres_param_items[8];

static void initPresParamItems() {
    strncpy(pres_param_items[0].title, ">>> 压强参数 <<<", sizeof(pres_param_items[0].title) - 1);

    strncpy(pres_param_items[1].title, "Kp", sizeof(pres_param_items[1].title) - 1);
    pres_param_items[1].extra.float_dot1f_Value = &g_pres_kp_f;
    pres_param_items[1].pFunc = []() {
        ui.showPopupProgress(g_pres_kp, 0, 1000, "Kp", 110, 40, 10000, 1,
            [](int32_t v){ g_pres_kp_f = v * 0.01f; }, true, 1, 0.01f);
    };

    strncpy(pres_param_items[2].title, "Ki", sizeof(pres_param_items[2].title) - 1);
    pres_param_items[2].extra.float_dot1f_Value = &g_pres_ki_f;
    pres_param_items[2].pFunc = []() {
        ui.showPopupProgress(g_pres_ki, 0, 500, "Ki", 110, 40, 10000, 1,
            [](int32_t v){ g_pres_ki_f = v * 0.01f; }, true, 1, 0.01f);
    };

    strncpy(pres_param_items[3].title, "Kd", sizeof(pres_param_items[3].title) - 1);
    pres_param_items[3].extra.float_dot1f_Value = &g_pres_kd_f;
    pres_param_items[3].pFunc = []() {
        ui.showPopupProgress(g_pres_kd, 0, 500, "Kd", 110, 40, 10000, 1,
            [](int32_t v){ g_pres_kd_f = v * 0.01f; }, true, 1, 0.01f);
    };

    strncpy(pres_param_items[4].title, "死区(hPa)", sizeof(pres_param_items[4].title) - 1);
    pres_param_items[4].extra.float_dot1f_Value = &g_pres_dead_f;
    pres_param_items[4].pFunc = []() {
        ui.showPopupProgress(g_pres_dead, 0, 50, "死区(hPa)", 110, 40, 10000, 1,
            [](int32_t v){ g_pres_dead_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(pres_param_items[5].title, "最大ΔV(mL)", sizeof(pres_param_items[5].title) - 1);
    pres_param_items[5].extra.float_dot1f_Value = &g_pres_maxdv_f;
    pres_param_items[5].pFunc = []() {
        ui.showPopupProgress(g_pres_maxdv, 0, 200, "最大ΔV(mL)", 110, 40, 10000, 1,
            [](int32_t v){ g_pres_maxdv_f = v * 0.1f; }, true, 1, 0.1f);
    };

    strncpy(pres_param_items[6].title, "应用参数", sizeof(pres_param_items[6].title) - 1);
    pres_param_items[6].pFunc = []() {
        pressureCtrl.setPID(g_pres_kp * 0.01f, g_pres_ki * 0.01f, g_pres_kd * 0.01f);
        pressureCtrl.setDeadBand(g_pres_dead * 0.1f);
        pressureCtrl.setMaxDeltaV(g_pres_maxdv * 0.1f);
        ui.showPopupInfo("参数已应用", "压强参数", 100, 30, 1500);
    };

    strncpy(pres_param_items[7].title, "恢复默认", sizeof(pres_param_items[7].title) - 1);
    pres_param_items[7].pFunc = []() {
        g_pres_kp    = (int32_t)(PRES_PID_KP     * 100); g_pres_kp_f    = PRES_PID_KP;
        g_pres_ki    = (int32_t)(PRES_PID_KI     * 100); g_pres_ki_f    = PRES_PID_KI;
        g_pres_kd    = (int32_t)(PRES_PID_KD     * 100); g_pres_kd_f    = PRES_PID_KD;
        g_pres_dead  = (int32_t)(PRES_DEAD_BAND  * 10);  g_pres_dead_f  = PRES_DEAD_BAND;
        g_pres_maxdv = (int32_t)(PRES_MAX_DELTA_V* 10);  g_pres_maxdv_f = PRES_MAX_DELTA_V;
        pressureCtrl.setPID(PRES_PID_KP, PRES_PID_KI, PRES_PID_KD);
        pressureCtrl.setDeadBand(PRES_DEAD_BAND);
        pressureCtrl.setMaxDeltaV(PRES_MAX_DELTA_V);
        ui.showPopupInfo("已恢复默认参数", "压强参数", 110, 30, 1500);
    };
}

// ─── 压强主菜单变量 ───────────────────────────────────────────────────────────
static int32_t g_pres_target     = 1013;
static int32_t g_pres_hysteresis = 5;    // ×10，即 0.5 hPa
static bool    g_pres_valve      = false;
static bool    g_pres_cool_fill  = false;
static bool    g_pres_cool_drain = false;

// ─── 压强实时状态 ─────────────────────────────────────────────────────────────
static const char* presStateStr() {
    switch (pressureCtrl.getState()) {
        case PressureControl::IDLE:    return "IDLE";
        case PressureControl::RUNNING: return "RUN";
        case PressureControl::HOLDING: return "HOLD";
        case PressureControl::ERROR:   return "ERR";
        default:                       return "?";
    }
}

static const StatusEntry pres_status[] = {
    { "状态",   [](char* b, size_t n){ snprintf(b, n, "%s",    presStateStr()); }},
    { "当前压强",  [](char* b, size_t n){ snprintf(b, n, "%.1f", pressureCtrl.getCurrentPressure()); }},
    { "目标压强",  [](char* b, size_t n){ snprintf(b, n, "%.1f", pressureCtrl.getTargetPressure()); }},
    { "误差",    [](char* b, size_t n){ snprintf(b, n, "%.2f", pressureCtrl.getPressureError()); }},
    { "死区",    [](char* b, size_t n){ snprintf(b, n, "%.1f", pressureCtrl.getDeadBand()); }},
    { "液位",    [](char* b, size_t n){ snprintf(b, n, "%.1f", volumeCtrl.getLiquidVolume()); }},
    { "当前温度",  [](char* b, size_t n){ snprintf(b, n, "%.1f", tempCtrl.getCurrentTemp()); }},
};

static ListItem pres_items[11];

static void initPresItems() {
    initPresParamItems();

    strncpy(pres_items[0].title, ">>> 压强调试 <<<", sizeof(pres_items[0].title) - 1);

    strncpy(pres_items[1].title, "参数调节", sizeof(pres_items[1].title) - 1);
    pres_items[1].nextList       = pres_param_items;
    pres_items[1].nextListLength = 8;

    strncpy(pres_items[2].title, "目标压强(hPa)", sizeof(pres_items[2].title) - 1);
    pres_items[2].extra.intValue = &g_pres_target;
    pres_items[2].pFunc = []() {
        ui.showPopupProgress(g_pres_target, 800, 1200, "目标压强(hPa)", 110, 40, 10000, 1, nullptr, true);
    };

    strncpy(pres_items[3].title, "死区(hPa)", sizeof(pres_items[3].title) - 1);
    pres_items[3].extra.float_dot1f_Value = &g_pres_dead_f;
    pres_items[3].pFunc = []() {
        ui.showPopupProgress(g_pres_hysteresis, 0, 50, "死区(hPa)", 110, 40, 10000, 1,
            [](int32_t v){
                g_pres_hysteresis = v;
                g_pres_dead   = v;
                g_pres_dead_f = v * 0.1f;
            }, true, 1, 0.1f);
    };

    strncpy(pres_items[4].title, "气阀", sizeof(pres_items[4].title) - 1);
    pres_items[4].extra.switchValue = &g_pres_valve;
    pres_items[4].pFunc = []() {
        if (g_pres_valve) airValve.open();
        else              airValve.close();
    };

    strncpy(pres_items[5].title, "冷却注水", sizeof(pres_items[5].title) - 1);
    pres_items[5].extra.switchValue = &g_pres_cool_fill;
    pres_items[5].pFunc = []() {
        if (g_pres_cool_fill) { g_pres_cool_drain = false; coolPump.setSpeed(255); }
        else coolPump.stop();
    };

    strncpy(pres_items[6].title, "冷却排水", sizeof(pres_items[6].title) - 1);
    pres_items[6].extra.switchValue = &g_pres_cool_drain;
    pres_items[6].pFunc = []() {
        if (g_pres_cool_drain) { g_pres_cool_fill = false; coolPump.setSpeed(-255); }
        else coolPump.stop();
    };

    strncpy(pres_items[7].title, "启动", sizeof(pres_items[7].title) - 1);
    pres_items[7].pFunc = []() {
        pressureCtrl.setTargetPressure((float)g_pres_target);
        pressureCtrl.setDeadBand(g_pres_hysteresis * 0.1f);
        pressureCtrl.start();
        static char buf[32];
        snprintf(buf, sizeof(buf), "目标: %d hPa", g_pres_target);
        ui.showPopupInfo(buf, "压强已启动", 106, 30, 1500);
    };

    strncpy(pres_items[8].title, "停止", sizeof(pres_items[8].title) - 1);
    pres_items[8].pFunc = []() {
        pressureCtrl.stop();
        airValve.close();
        g_pres_valve = false;
        ui.showPopupInfo("已停止", "压强", 90, 30, 1200);
    };

    strncpy(pres_items[9].title, "复位", sizeof(pres_items[9].title) - 1);
    pres_items[9].pFunc = []() {
        pressureCtrl.reset();
        airValve.close();
        g_pres_valve = false;
        ui.showPopupInfo("已复位", "压强", 90, 30, 1200);
    };

    strncpy(pres_items[10].title, "压强状态", sizeof(pres_items[10].title) - 1);
    pres_items[10].pFunc = []() {
        ui.showPopupStatus(pres_status, 7, "压强状态", 126, 62, 30000);
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// 子页：K值校准
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 进水校准 ────────────────────────────────────────────────────────────────
static bool     g_calib_in_pump       = false;
static uint32_t g_calib_in_start_ms   = 0;
static uint32_t g_calib_in_elapsed_ms = 0;
static uint32_t g_calib_in_pulses     = 0;
static int32_t  g_calib_in_ml         = 100;
static float    g_calib_in_K          = 0.0f;
// 校准泵运行期间保存液位总量，防止校准流量污染液位显示
static float    g_calib_in_saved_in   = 0.0f;
static float    g_calib_in_saved_out  = 0.0f;

static void initCalibInItems() {
    strncpy(calib_in_items[0].title, ">>> 进水校准 <<<", sizeof(calib_in_items[0].title) - 1);

    strncpy(calib_in_items[1].title, "水泵", sizeof(calib_in_items[1].title) - 1);
    calib_in_items[1].extra.switchValue = &g_calib_in_pump;
    calib_in_items[1].pFunc = []() {
        if (g_calib_in_pump) {
            g_calib_in_saved_in  = flowIn.getTotalVolume();
            g_calib_in_saved_out = flowOut.getTotalVolume();
            flowIn.clearCount();
            g_calib_in_start_ms = millis();
            pump.setSpeed(255);
        } else {
            pump.stop();
            g_calib_in_pulses     = flowIn.getCount();
            g_calib_in_elapsed_ms = millis() - g_calib_in_start_ms;
            flowIn.setTotalVolumeMl(g_calib_in_saved_in);
            flowOut.setTotalVolumeMl(g_calib_in_saved_out);
        }
    };

    strncpy(calib_in_items[2].title, "输入水量(mL)", sizeof(calib_in_items[2].title) - 1);
    calib_in_items[2].extra.intValue = &g_calib_in_ml;
    calib_in_items[2].pFunc = []() {
        ui.showPopupProgress(g_calib_in_ml, 50, 100, "实际水量(mL)", 110, 40, 30000, 1, nullptr, true, 1);
    };

    strncpy(calib_in_items[3].title, "计算K值", sizeof(calib_in_items[3].title) - 1);
    calib_in_items[3].pFunc = []() {
        if (g_calib_in_pump) {
            ui.showPopupInfo("请先关闭水泵", "进水校准", 106, 30, 1500);
            return;
        }
        if (g_calib_in_pulses == 0 || g_calib_in_ml < 50) {
            ui.showPopupInfo("数据不足，请先采集", "进水校准", 118, 30, 1500);
            return;
        }
        g_calib_in_K = (float)g_calib_in_pulses / (g_calib_in_ml / 1000.0f);
        static char buf[56];
        snprintf(buf, sizeof(buf), "时间:%.1fs 脉冲:%lu K:%.0f",
                 g_calib_in_elapsed_ms / 1000.0f,
                 (unsigned long)g_calib_in_pulses,
                 g_calib_in_K);
        ui.showPopupInfo(buf, "计算结果(进水)", 124, 32, 8000);
    };

    strncpy(calib_in_items[4].title, "应用K值", sizeof(calib_in_items[4].title) - 1);
    calib_in_items[4].pFunc = []() {
        if (g_calib_in_K <= 0.0f) {
            ui.showPopupInfo("请先计算K值", "进水校准", 106, 30, 1500);
            return;
        }
        float savedMl = flowIn.getTotalVolume();
        flowIn.setKFactor(g_calib_in_K);
        flowIn.setTotalVolumeMl(savedMl);
        g_param_kin = (int32_t)g_calib_in_K;
        static char buf[32];
        snprintf(buf, sizeof(buf), "K=%.0f 已写入", g_calib_in_K);
        ui.showPopupInfo(buf, "进水K已应用", 110, 30, 2500);
    };

    strncpy(calib_in_items[5].title, "重置", sizeof(calib_in_items[5].title) - 1);
    calib_in_items[5].pFunc = []() {
        pump.stop();
        if (g_calib_in_pump) {
            flowIn.setTotalVolumeMl(g_calib_in_saved_in);
            flowOut.setTotalVolumeMl(g_calib_in_saved_out);
        }
        g_calib_in_pump       = false;
        g_calib_in_start_ms   = 0;
        g_calib_in_elapsed_ms = 0;
        g_calib_in_pulses     = 0;
        g_calib_in_ml         = 100;
        g_calib_in_K          = 0.0f;
        ui.showPopupInfo("已重置", "进水校准", 90, 30, 1200);
    };
}

// ─── 出水校准 ────────────────────────────────────────────────────────────────
static bool     g_calib_out_pump       = false;
static uint32_t g_calib_out_start_ms   = 0;
static uint32_t g_calib_out_elapsed_ms = 0;
static uint32_t g_calib_out_pulses     = 0;
static int32_t  g_calib_out_ml         = 100;
static float    g_calib_out_K          = 0.0f;
// 校准泵运行期间保存液位总量，防止校准流量污染液位显示
static float    g_calib_out_saved_in   = 0.0f;
static float    g_calib_out_saved_out  = 0.0f;

static void initCalibOutItems() {
    strncpy(calib_out_items[0].title, ">>> 出水校准 <<<", sizeof(calib_out_items[0].title) - 1);

    strncpy(calib_out_items[1].title, "水泵", sizeof(calib_out_items[1].title) - 1);
    calib_out_items[1].extra.switchValue = &g_calib_out_pump;
    calib_out_items[1].pFunc = []() {
        if (g_calib_out_pump) {
            g_calib_out_saved_in  = flowIn.getTotalVolume();
            g_calib_out_saved_out = flowOut.getTotalVolume();
            flowOut.clearCount();
            g_calib_out_start_ms = millis();
            pump.setSpeed(-255);
        } else {
            pump.stop();
            g_calib_out_pulses     = flowOut.getCount();
            g_calib_out_elapsed_ms = millis() - g_calib_out_start_ms;
            flowIn.setTotalVolumeMl(g_calib_out_saved_in);
            flowOut.setTotalVolumeMl(g_calib_out_saved_out);
        }
    };

    strncpy(calib_out_items[2].title, "输入水量(mL)", sizeof(calib_out_items[2].title) - 1);
    calib_out_items[2].extra.intValue = &g_calib_out_ml;
    calib_out_items[2].pFunc = []() {
        ui.showPopupProgress(g_calib_out_ml, 50, 100, "实际水量(mL)", 110, 40, 30000, 1, nullptr, true, 1);
    };

    strncpy(calib_out_items[3].title, "计算K值", sizeof(calib_out_items[3].title) - 1);
    calib_out_items[3].pFunc = []() {
        if (g_calib_out_pump) {
            ui.showPopupInfo("请先关闭水泵", "出水校准", 106, 30, 1500);
            return;
        }
        if (g_calib_out_pulses == 0 || g_calib_out_ml < 50) {
            ui.showPopupInfo("数据不足，请先采集", "出水校准", 118, 30, 1500);
            return;
        }
        g_calib_out_K = (float)g_calib_out_pulses / (g_calib_out_ml / 1000.0f);
        static char buf[56];
        snprintf(buf, sizeof(buf), "时间:%.1fs 脉冲:%lu K:%.0f",
                 g_calib_out_elapsed_ms / 1000.0f,
                 (unsigned long)g_calib_out_pulses,
                 g_calib_out_K);
        ui.showPopupInfo(buf, "计算结果(出水)", 124, 32, 8000);
    };

    strncpy(calib_out_items[4].title, "应用K值", sizeof(calib_out_items[4].title) - 1);
    calib_out_items[4].pFunc = []() {
        if (g_calib_out_K <= 0.0f) {
            ui.showPopupInfo("请先计算K值", "出水校准", 106, 30, 1500);
            return;
        }
        float savedMl = flowOut.getTotalVolume();
        flowOut.setKFactor(g_calib_out_K);
        flowOut.setTotalVolumeMl(savedMl);
        g_param_kout = (int32_t)g_calib_out_K;
        static char buf[32];
        snprintf(buf, sizeof(buf), "K=%.0f 已写入", g_calib_out_K);
        ui.showPopupInfo(buf, "出水K已应用", 110, 30, 2500);
    };

    strncpy(calib_out_items[5].title, "重置", sizeof(calib_out_items[5].title) - 1);
    calib_out_items[5].pFunc = []() {
        pump.stop();
        if (g_calib_out_pump) {
            flowIn.setTotalVolumeMl(g_calib_out_saved_in);
            flowOut.setTotalVolumeMl(g_calib_out_saved_out);
        }
        g_calib_out_pump       = false;
        g_calib_out_start_ms   = 0;
        g_calib_out_elapsed_ms = 0;
        g_calib_out_pulses     = 0;
        g_calib_out_ml         = 100;
        g_calib_out_K          = 0.0f;
        ui.showPopupInfo("已重置", "出水校准", 90, 30, 1200);
    };
}

// ─── K值校准父菜单 ────────────────────────────────────────────────────────────

static void initCalibItems() {
    initCalibInItems();
    initCalibOutItems();

    strncpy(calib_items[0].title, ">>> K值校准 <<<", sizeof(calib_items[0].title) - 1);

    strncpy(calib_items[1].title, "进水校准", sizeof(calib_items[1].title) - 1);
    calib_items[1].nextList       = calib_in_items;
    calib_items[1].nextListLength = 6;

    strncpy(calib_items[2].title, "出水校准", sizeof(calib_items[2].title) - 1);
    calib_items[2].nextList       = calib_out_items;
    calib_items[2].nextListLength = 6;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 顶层菜单
// ═══════════════════════════════════════════════════════════════════════════════
static ListItem root_items[4];

static void initRootItems() {
    strncpy(root_items[0].title, ">>> 硬件调试 <<<", sizeof(root_items[0].title) - 1);

    strncpy(root_items[1].title, "液位调试", sizeof(root_items[1].title) - 1);
    root_items[1].nextList       = vol_items;
    root_items[1].nextListLength = 16;

    strncpy(root_items[2].title, "温控调试", sizeof(root_items[2].title) - 1);
    root_items[2].nextList       = temp_items;
    root_items[2].nextListLength = 14;

    strncpy(root_items[3].title, "压强调试", sizeof(root_items[3].title) - 1);
    root_items[3].nextList       = pres_items;
    root_items[3].nextListLength = 11;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ListView 子类
// ═══════════════════════════════════════════════════════════════════════════════
class AppDebugView : public ListView {
public:
    AppDebugView(PixelUI& ui) : ListView(ui, root_items, 4) {}
    void onLoad() override {}
    void onSave() override {
        // 液位
        volumeCtrl.stop();
        pump.stop();
        g_vol_valve = false;
        g_vol_drain = false;
        // 温控
        tempCtrl.stop();
        coolPump.stop();
        tempCtrl.airPumpOff();
        heater.heaterOff();
        heater.fanOff();
        g_coolpump_fill  = false;
        g_coolpump_drain = false;
        g_airpump_on     = false;
        g_temp_valve     = false;
        g_fan_on         = false;
        g_heater_on      = false;
        // 压强
        pressureCtrl.stop();
        coolPump.stop();
        g_pres_cool_fill  = false;
        g_pres_cool_drain = false;
        g_pres_valve = false;
        // 共用气阀最后统一关
        airValve.close();
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// AppItem 注册
// ═══════════════════════════════════════════════════════════════════════════════
AppItem app_debug{
    .title  = "硬件调试",
    .bitmap = image_debug_bits,
    .createApp = [](PixelUI& ui, void*) -> std::shared_ptr<IApplication> {
        static bool initialized = false;
        if (!initialized) {
            initParamItems();
            initCalibItems();
            initVolItems();
            initTempItems();
            initPresItems();
            initRootItems();
            initialized = true;
        }
        return std::make_shared<AppDebugView>(ui);
    },
};
