#ifndef __ESP_CLOUD_H__
#define __ESP_CLOUD_H__

#include <Arduino.h>
#include "Storage.h"
#include "PubSubClient.h"
#include "config.h"
#include "myNVS.h"
#include "WiFi.h"


#define PUSH_PACKET_SIZE 2048
#define PUSH_BUFFER_SIZE (((PUSH_PACKET_SIZE / 128) + 1) * 128)

#if NUM_OF_SLOTS == 15
#define MQTT_TOPIC "EiG/SmartMeter15"
#elif NUM_OF_SLOTS == 12
#define MQTT_TOPIC "EiG/SmartMeter12"
#elif NUM_OF_SLOTS == 9
#define MQTT_TOPIC "EiG/SmartMeter9"
#elif NUM_OF_SLOTS == 8
#define MQTT_TOPIC "EiG/SmartMeter8"
#elif NUM_OF_SLOTS == 6
#define MQTT_TOPIC "EiG/SmartMeter6"
#elif NUM_OF_SLOTS == 3
#define MQTT_TOPIC "EiG/TCMMeter3"
#elif NUM_OF_SLOTS == 1
#define MQTT_TOPIC "EiG/SmartMeter"
#endif


struct DataRequest
{
    uint8_t slot;           // Slot index (0 to NUM_OF_SLOTS-1)
    char date[9];           // Date string YYYYMMDD + null terminator
    DataRequest *next;      // Linked list pointer
};

enum SyncType
{
    LABELS,
    FACTORS
};
extern String mqtt_server;
extern PubSubClient mqttClient;
extern uint16_t mqtt_port;
extern String latestVersion;
extern String sensorID;
extern String dataPath ;

extern PubSubClient dataClient;

extern char data_pull_topic[35];
extern char restart_topic[35];
extern char logs_topic[35];
extern char data_push_topic[35];

extern DataRequest *pendingDataRequests;


void onDataRequest(char *topic, uint8_t *buff, unsigned int size);
bool sendFile(const char *fileName, const char *topic);
void processDataRequests();
bool live_broadcast(String broadcastData, uint8_t slotIndex);
bool publishDeviceParams();

//Label Configuration Functions
bool initializeDefaultLabels();
bool initializeDefaultSlotFactors();
bool copyLabelsToFirebase(const uint32_t &currentTime, String *slotLabels);
bool copyLabelsFromFirebase(const uint32_t &currentTimeFirebase);
bool copyfactorsToFirebase(const uint32_t &currentTime, float *slotfactors);
bool copyfactorsFromFirebase(const uint32_t &currentTimeFirebase);
//Single functions to handle syncing of Labels&Factors
bool syncWithFirebase(SyncType type);
void attemptLiveDataRecovery();
void update_essentials(void);


extern String mqtt_topics_live[NUM_OF_SLOTS]; 
extern String mqtt_topics_historic[NUM_OF_SLOTS]; // Historic data topics
extern String mqtt_topics_livebackup[NUM_OF_SLOTS];


#endif