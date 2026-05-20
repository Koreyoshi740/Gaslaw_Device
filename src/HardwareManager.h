#pragma once

#include <hw_config.h>

// Hardware
#include <AirPump.h>
#include <AirValve.h>
#include <BME280sensor.h>
#include <Flowsensor.h>
#include <Heater.h>
#include <Pumper.h>

// Control
#include <VolumeControl.h>
#include <PressureControl.h>
#include <TempControl.h>

// Experiment
#include <BoyleLaw.h>
#include <CharlesLaw.h>
#include <GayLussacLaw.h>

// ─── Hardware instances ───────────────────────────────────────────────────────
extern Flowsensor    flowIn;
extern Flowsensor    flowOut;
extern Pumper        pump;
extern Pumper        coolPump;
extern AirPump       airPump;
extern AirValve      airValve;
extern BME280Sensor  bme;
extern Heater        heater;

// ─── Control instances ────────────────────────────────────────────────────────
extern VolumeControl   volumeCtrl;
extern PressureControl pressureCtrl;
extern TempControl     tempCtrl;

// ─── Experiment instances ─────────────────────────────────────────────────────
extern BoyleLaw      boyle;
extern CharlesLaw    charles;
extern GayLussacLaw  gayLussac;

// ─── Timer callbacks (called from ISR, IRAM_ATTR required) ───────────────────
void IRAM_ATTR hwOnTimer();   // drive flowIn/flowOut/volumeCtrl/pressureCtrl/tempCtrl

void initHardware();
