#ifndef __DATATRANSFER_H__
#define __DATATRANSFER_H__

#include <Arduino.h>
#include <PubSubClient.h>
#include "rtc.h"
#include "config.h"
#include "espCloud.h"
#include "ESPWiFi.h"
#include "ESP32Ping.h"

extern String sensorID;

extern String serverWifiCreds[2];
extern PubSubClient mqttClient;
extern String towrite, toTransfer;
extern SemaphoreHandle_t semaAqData1, semaStorage1, semaWifi1, semaPull1;


void handleWifiConnection();
void handleStorageTransfer();

#endif