#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <PixelUI.h>
#include <core/ViewManager/ViewManager.h>
#include <core/app/app_system.h>
#include <ui/AppLauncher/AppLauncher.h>
#include "input/key.h"
#include "HardwareManager.h"
#include "apps/experiment/AppBoyle.h"
#include "apps/experiment/AppCharles.h"
#include "apps/experiment/AppGayLussac.h"
#include "apps/sensor/AppEnvironment.h"
#include "apps/debug/AppDebug.h"
#include <phyphoxBle.h>
PhyphoxBLE EXP;
// SH1106 128x64, 硬件 I2C, SDA=21 SCL=22
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

PixelUI ui(u8g2);

Key keyLeft, keyRight, keySelect, keyBack;

hw_timer_t* mainTimer = nullptr;

static uint8_t heartbeatCount = 0;

void IRAM_ATTR onMainTimer() {
    // 硬件控制节拍（流量、PID、温控）
    hwOnTimer();

    // 按键扫描
    keyLeft.keyTick();
    keyRight.keyTick();
    keySelect.keyTick();
    keyBack.keyTick();

    // UI 刷新（每 5ms 触发一次）
    if (++heartbeatCount >= 5) {
        heartbeatCount = 0;
        ui.Heartbeat(5);
    }
}

void setup() {
    Serial.begin(115200);

    

    // u8g2 先初始化：内部调用 Wire.begin() 完成 I2C 总线初始化，并发送 SH1106 初始化序列
    u8g2.begin();
    u8g2.setContrast(255);

    // Wire 已由 u8g2 初始化，再初始化硬件外设（BME280 等 I2C 设备）
    initHardware();

    EXP.start("气体实验定律探究仪");

    keyLeft.attach(KEY_LEFT_PIN);
    keyRight.attach(KEY_RIGHT_PIN);
    keySelect.attach(KEY_SELECT_PIN);
    keyBack.attach(KEY_BACK_PIN);

    // 硬件定时器，每 1ms 触发一次
    mainTimer = timerBegin(0, 80, true);         // 80MHz / 80 = 1MHz
    timerAttachInterrupt(mainTimer, &onMainTimer, true);
    timerAlarmWrite(mainTimer, 1000, true);      // 1000us = 1ms
    timerAlarmEnable(mainTimer);

    // registerApp 在 ui.begin() 之前，与 PixelUI_demo 保持一致
    AppManager::getInstance().registerApp(boyle_app);
    AppManager::getInstance().registerApp(charles_app);
    AppManager::getInstance().registerApp(gaylussac_app);
    AppManager::getInstance().registerApp(app_environment);
    AppManager::getInstance().registerApp(app_debug);
    ui.begin();
    auto launcher = AppLauncher::createAppLauncherView(ui, *ui.getViewManagerPtr());
    ui.getViewManagerPtr()->push(launcher);
}

void loop() {
    bme.update();

    if (keyLeft.keyCheck(KEY_SINGLE))  ui.handleInput(InputEvent::LEFT);
    if (keyLeft.keyCheck(KEY_REPEAT))  ui.handleInput(InputEvent::LEFT_FAST);
    if (keyRight.keyCheck(KEY_SINGLE)) ui.handleInput(InputEvent::RIGHT);
    if (keyRight.keyCheck(KEY_REPEAT)) ui.handleInput(InputEvent::RIGHT_FAST);
    if (keySelect.keyCheck(KEY_SINGLE))                                  ui.handleInput(InputEvent::SELECT);
    if (keySelect.keyCheck(KEY_LONG)  || keySelect.keyCheck(KEY_DOUBLE)) ui.handleInput(InputEvent::BACK);
    if (keyBack.keyCheck(KEY_SINGLE)  || keyBack.keyCheck(KEY_LONG))     ui.handleInput(InputEvent::BACK);

    // 实验状态机更新（浮点运算和传感器读取在主循环中执行）
    volumeCtrl.update();
    pressureCtrl.update();
    tempCtrl.update();
    boyle.update();
    charles.update();
    gayLussac.update();

    ui.renderer();

    static uint32_t lastDebugTime = 0;
    static uint32_t lastPhyphoxTime = 0;
    uint32_t now = millis();
    if (now - lastDebugTime >= 500) {
        lastDebugTime = now;
        static const char* stateNames[] = {"IDLE","HEATING","COASTING","HOLDING","COOLING","ERROR"};
        Serial.printf("Target:%.2f Actual:%.2f Speed:%d State:%s\n",
                      tempCtrl.getTargetTemp(),
                      tempCtrl.getCurrentTemp(),
                      (int)heater.getHeaterSpeed(),
                      stateNames[(int)tempCtrl.getState()]);
    }
    if (now - lastPhyphoxTime >= 500) {
        lastPhyphoxTime = now;
        float temp = bme.getTemperature();
        float pres = bme.getPressure();
        float vol  = volumeCtrl.getAirVolume();
        EXP.write(temp, pres, vol);
    }
}
