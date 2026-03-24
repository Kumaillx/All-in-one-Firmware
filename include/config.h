#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>

#define METER_M1M20 1
#define METER_PZEM 2
#define METER_TNM360 3

// Select the meter type here:
#define SELECTED_METER METER_M1M20

#define FIRMWARE_VERSION "1.5.2"
#define FIRMWARE_DESCRIPTION "some mac optimizations"
#define COMMIT_DATE "4 Feb 2026"

// #define OTA_UPDATE

//#define DUMMY_DATA
//  #define OLED_DISPLAY
//  #define RESET_ESP_STORAGE
#define TESTING_METER_IDS

#define DEFAULT_SSID "NeuBolt"

#define DEFAULT_PASSWORD "12344321"
#define SOFT_AP_PASSWORD "123456789"

#define DEFAULT_MQTT_SERVER "156.67.214.113"
#define DEFAULT_MQTT_PORT 1883
#define PORT_FOR_API 8000

#define FIREBASE_HOST "abb-meters-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyDzLsvHQ3whWi9-1OTDWlNE9tDkcEi484Y"

#define WIFI_SCAN_FAILURE_LIMIT 3
#define STORAGE_WRITE_FAILURE_LIMIT 3

#define MS_IN_SECOND 1000
#define DATA_ACQUISITION_TIME 3  // perform action every "DATA_ACQUISITION_TIME" milliseconds
#define DATA_STORAGE_TIME 30     // store the string every "DATA_STORAGE_TIME" seconds
#define LIVE_DATA_BACKUP_TIME 10 // backup live data every "LIVE_DATA_BACKUP_TIME" minutes

// LED pins
#define AQ_LED 32
#define STORAGE_LED 33
#define WIFI_LED 25
#define CLOUD_LED 13

#define METER_BRANDMODEL "21"
#define METER_EXTENSION "01" // hardcoded extension

enum flags_
{
    aq_blink_f,
    sd_blink_f,
    wf_blink_f,
    cloud_blink_f,
    aq_f,
    sd_f,
    wf_f,
    cloud_f,
    cloud_setup_f,
    rtc_f,
    png_f
};
extern byte flags[14];

#endif