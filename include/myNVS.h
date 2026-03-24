#ifndef __MYNVS_H__
#define __MYNVS_H__

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"


#define BUFF_LEN_NVS_KEY 16

#define NVS_NAMESPACE_SENSOR_INFO "sm-app"
#define NVS_KEY_SENSOR_NAME "sensor-name"

#define NVS_NAMESPACE_SENSOR_subINFO "sm-subapp"
#define NVS_KEY_SENSOR_subNAME "sensor-subname-mac-exten"

#define NVS_NAMESPACE_SLOT_LABELS "slot-labels"
#define NVS_NAMESPACE_SLOT_FACTORS "slot-factors"
#define NVS_NAMESPACE_VOLT_FACTORS  "voltage-cal"

#define NVS_NAMESPACE_RESTART_INFO "restart-info"
#define NVS_KEY_RESTART_COUNT "count"
#define NVS_KEY_RESTART_TIME "time"

#define NVS_NAMESPACE_MQTT_INFO "mqtt-app"
#define NVS_KEY_MQTT_SERVER "ip_address"
#define NVS_KEY_MQTT_PORT "port"

#define NVS_NAMESPACE_FILE_INFO "pointer"
#define NVS_KEY_FILE_NAME "file-name"

#define NVS_NAMESPACE_LAST_SET "slot-set-time"
#define NVS_NAMESPACE_LAST_FACTOR_SET "factor-set-time"
#define NVS_NAMESPACE_LAST_FILE_SET "file-set-time"

extern bool Factors_Already_Exist;
extern bool Labels_Already_Exist;
extern bool Filename_Already_Exist;

/// @brief Read and write to Non-Volatile Storage using arguments.
namespace myNVS
{
    namespace read
    {
        void slotLabels(String slot_labels[NUM_OF_SLOTS]);
        void lastslotsettime(uint32_t &current_slot_set_time);
        void lastfactorsettime(uint32_t &current_factor_set_time);
        void lastfilesettime(uint32_t &current_file_set_time);  
        void slotFactors(float slot_factors[NUM_OF_SLOTS]);
        void restartData(int &restart_count, uint32_t &restart_time);
        void mqttData(String &mqtt_server, uint16_t &mqtt_port);
        void sensorName(String &sensor_name);
        void sensorsubName(String &sensor_name);
        void currentFileName(String &file_name);
        
    }

    namespace write
    {
        void slotLabels(const String slot_labels[NUM_OF_SLOTS]);
        void lastslotsettime(const uint32_t current_slot_set_time);
        void lastfactorsettime(const uint32_t current_factor_set_time);
        void lastfilesettime(const uint32_t current_file_set_time); 
        void slotFactors(const float slot_factors[NUM_OF_SLOTS]);
        void restartData(int restart_count, uint32_t restart_time);
        void mqttData(String mqtt_server, uint16_t mqtt_port);
        void sensorName(String sensor_name);
        void sensorsubName(String sensor_name);
        void currentFileName(String file_name);
    }
}

#endif