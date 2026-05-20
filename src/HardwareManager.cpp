#include "HardwareManager.h"

// ─── Hardware instances ───────────────────────────────────────────────────────
Flowsensor   flowIn;
Flowsensor   flowOut;
Pumper       pump;
Pumper       coolPump;
AirPump      airPump;
AirValve     airValve;
BME280Sensor bme;
Heater       heater;

// ─── Control instances ────────────────────────────────────────────────────────
VolumeControl   volumeCtrl;
PressureControl pressureCtrl;
TempControl     tempCtrl;

// ─── Experiment instances ─────────────────────────────────────────────────────
BoyleLaw     boyle;
CharlesLaw   charles;
GayLussacLaw gayLussac;

// ─── ISR ─────────────────────────────────────────────────────────────────────
void IRAM_ATTR hwOnTimer() {
    flowIn.onTimer();
    flowOut.onTimer();
    volumeCtrl.onPidTick();
    pressureCtrl.onTimer();
    tempCtrl.onTimer();
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void initHardware() {
    // Flow sensors
    flowIn.attach(FLOW_IN_PIN);
    flowIn.setKFactor(FLOW_IN_K);
    flowOut.attach(FLOW_OUT_PIN);
    flowOut.setKFactor(FLOW_OUT_K);

    // Main pump (water in/out)
    pump.attach(PUMP_IN1, PUMP_IN2, PUMP_PWM, PUMP_LEDC_CH);

    // Cooling pump
    coolPump.attach(COOLPUMP_IN1, COOLPUMP_IN2, COOL_PWM, COOL_LEDC_CH);

    // Air pump
    airPump.attach(AIR_PUMP_PWM, AIR_PUMP_LEDC_CH);

    // Air valve (solenoid)
    airValve.attach(VALVE_PIN);

    // BME280
    bme.begin();

    // Heater + fan
    heater.attach(HEATER_PWM, FAN_PWM, HEATER_LEDC_CH, FAN_LEDC_CH);

    // VolumeControl
    volumeCtrl.attach(&flowIn, &flowOut, &pump);
    volumeCtrl.setContainerVolume(LARGE_BOTTLE_ML + SMALL_BOTTLE_ML - COMPONENT_VOL_ML);
    volumeCtrl.setPID(VOL_PID_KP, VOL_PID_KI, VOL_PID_KD);
    volumeCtrl.setDeadBand(VOL_DEAD_BAND);
    volumeCtrl.setMinPWM(VOL_MIN_PWM);
    volumeCtrl.setMaxPWM(VOL_MAX_PWM);

    // PressureControl
    pressureCtrl.attach(&bme, &volumeCtrl);
    pressureCtrl.setPID(PRES_PID_KP, PRES_PID_KI, PRES_PID_KD);
    pressureCtrl.setDeadBand(PRES_DEAD_BAND);
    pressureCtrl.setMaxDeltaV(PRES_MAX_DELTA_V);

    // TempControl
    tempCtrl.attach(&bme, &heater);
    tempCtrl.attachCoolPump(&coolPump);
    tempCtrl.attachAirPump(&airPump);
    tempCtrl.setPID(TEMP_PID_KP, TEMP_PID_KI, TEMP_PID_KD);
    tempCtrl.setHysteresis(TEMP_HYSTERESIS);
    tempCtrl.setHoldingBand(TEMP_HOLDING_BAND);
    tempCtrl.setMaxTemp(TEMP_MAX_TEMP);
    tempCtrl.setFanCooldownMs(TEMP_FAN_COOLDOWN_MS);
    tempCtrl.setSwitchDelta(TEMP_SWITCH_DELTA);
    tempCtrl.setCoastBand(TEMP_COAST_BAND);

    // BoyleLaw
    boyle.attach(&bme, &volumeCtrl, &tempCtrl);
    boyle.attachAirPump(&airPump);
    boyle.attachAirValve(&airValve);
    boyle.setBottleConfig(LARGE_BOTTLE_ML, SMALL_BOTTLE_ML, COMPONENT_VOL_ML);
    boyle.setInitTotalVolume(BOYLE_INIT_TOTAL_VOL);
    boyle.setTargetTemp(BOYLE_TARGET_TEMP);
    boyle.setStableThreshold(BOYLE_STABLE_THRESHOLD, BOYLE_STABLE_COUNT);
    boyle.setCirculateMs(BOYLE_CIRCULATE_MS);
    boyle.setSampleIntervalMs(BOYLE_SAMPLE_MS);

    // CharlesLaw
    charles.attach(&bme, &volumeCtrl, &tempCtrl);
    charles.attachAirValve(&airValve);

    // GayLussacLaw
    gayLussac.attach(&bme, &volumeCtrl, &tempCtrl, &pressureCtrl);
    gayLussac.attachAirValve(&airValve);
    gayLussac.setBottleConfig(LARGE_BOTTLE_ML, SMALL_BOTTLE_ML, COMPONENT_VOL_ML);
    gayLussac.setNodeSettleMs(GAY_NODE_SETTLE_MS);
}
