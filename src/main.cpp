/*
 * Copyright 2023, NeuBolt Energy Services. All rights reserved.
 * Developed by Muhammad Shayan Khawar (shayan123.sk@gmail.com), Muhammad Hassaan(hassaan.afzaal16@gmail.com),
 *              Atta ul Haleem (attaul.haleem21@gmail.com), Tayyab Zafar (Tayyabxfr@gmail.com), Kumail Ali (m.kumail80@gmail.com)
 *
 * Main file for ESP32 based SmartMeter's master code
 *
 * This file is the property of NeuBolt Energy Services. Its sole private
 * ownership remains with the Company and it should not be shared with anyone without the consent
 * of respective authority.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <ESP32Ping.h>

#include "config.h"

#if SELECTED_METER == METER_M1M20
#include <M1M20.h>
typedef M1M20Meter MeterType;
#elif SELECTED_METER == METER_PZEM
#include <PZEM004Tv30.h>
typedef PZEM004Tv30 MeterType;
#elif SELECTED_METER == METER_TNM360
#include <TNM360.h>
typedef TNM360Meter MeterType;
#endif

#include <FirebaseESP32.h>
#include <SoftwareSerial.h>
#include "ESPWiFi.h"
#include "Storage.h"
#include "rtc.h"
#include "espCloud.h"
#include "MeterData.h"
#include "webServer.h"
#include "myNVS.h"
#include "Restart.h"
#include "DataTransfer.h"

#ifdef OTA_UPDATE
#include "OTA.h"
#endif

SoftwareSerial swSer(25, 26);

const char *pullTopic = "pull/";
const char *restartTopic = "restart/";
const char *logsTopic = "logs/";
const char *pushTopic = "push/";

// Add both topic types // Live data topics

DataRequest *pendingDataRequests = nullptr;

FirebaseData firebaseData;
WiFiClient espClient1;
WiFiClient espClient2;
PubSubClient mqttClient(espClient1);
PubSubClient dataClient(espClient2);
String sensorID = "";
String subID[NUM_OF_SLOTS];
String currenTime = "", currenTimeFirebase = "";
String slot_labels[NUM_OF_SLOTS];

const uint8_t LEDS[] = {AQ_LED, STORAGE_LED, WIFI_LED, CLOUD_LED};

MeterType pzem[NUM_OF_SLOTS];
byte flags[14];

String serverWifiCreds[2];
MeterData meterData[NUM_OF_SLOTS];

void vAcquireData(void *pvParameters);
// void vLiveBroadcast(void *pvParameters);
void vStorage(void *pvParameters);
void vDataPull(void *pvParameters);
void vWifiTransfer(void *pvParameters);
void vStatusLed(void *pvParameters);

TaskHandle_t dataTask, storageTask, wifiTask, ledTask, pullTask;

SemaphoreHandle_t semaAqData1, semaStorage1, semaWifi1;

#ifdef OLED_DISPLAY
Adafruit_SH1106 display(SDA, SCL);
TaskHandle_t oledTask;
bool oled_data = false;
String time_oled = "";
String voltage_oled[NUM_OF_SLOTS], current_oled[NUM_OF_SLOTS], power_oled[NUM_OF_SLOTS], pf_oled[NUM_OF_SLOTS] = {};
#endif

// Replace individual towrite variables with arrays
String towrite_slot[NUM_OF_SLOTS];      // Data to be written at storage intervals
String towrite_inst_slot[NUM_OF_SLOTS]; // Live/instant data
String toTransfer_slot[NUM_OF_SLOTS];   // Data to be transferred via MQTT
String towrite_s_slot[NUM_OF_SLOTS];    // Temporary copy during storage task

String latestVersion = "";

bool FirebaseFlag = false;

unsigned long reboottime;

byte _stg_fail_count = 0;
byte _rtc_fail_count = 0;

static String addrToStr(uint8_t addr)
{
  char buf[3];
  snprintf(buf, sizeof(buf), "%02X", addr);
  return String(buf);
}

void setup()
{
  Serial.begin(115200);
  swSer.begin(115200);

  log_d("\nCurrent Version: " FIRMWARE_VERSION "\nDescription: " FIRMWARE_DESCRIPTION "\nCommit Date: " COMMIT_DATE);

#ifdef RESET_ESP_STORAGE

  Preferences preferences;
  preferences.begin(NVS_NAMESPACE_SENSOR_INFO, false);
  preferences.clear();
  preferences.end();

  log_i("Reset ESP Preferences!");
  delay(500);
  ESP.restart();
#endif

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
#if SELECTED_METER == METER_M1M20
    pzem[i] = MeterType(Serial2, 16, 17, -1, 0x01 + i);
#elif SELECTED_METER == METER_TNM360
    pzem[i] = MeterType(Serial2, 16, 17, -1, 0x01 + i);
#elif SELECTED_METER == METER_PZEM
    pzem[i] = MeterType(Serial2, 16, 17, 0x01 + i);
#endif
  }

  flags[aq_f] = 1;
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(AQ_LED, OUTPUT);
  pinMode(STORAGE_LED, OUTPUT);
  pinMode(WIFI_LED, OUTPUT);
  pinMode(CLOUD_LED, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(AQ_LED, LOW);
  digitalWrite(STORAGE_LED, LOW);
  digitalWrite(WIFI_LED, LOW);
  digitalWrite(CLOUD_LED, LOW);

  myNVS::read::mqttData(mqtt_server, mqtt_port);
#ifdef OLED_DISPLAY
  display.begin(SH1106_SWITCHCAPVCC, 0x3C); // Address 0x3D for 128x64
  log_d("Display Initialized");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("The first step to a more sustainable future");
  display.display();
#endif

  while (storage.init_storage() == false && _stg_fail_count < FAIL_LIMIT)
  {
    _stg_fail_count++;
    flags[sd_f] = 0;
    log_e("Storage initialization failed!");
  }
  if (_stg_fail_count >= FAIL_LIMIT)
  {
    log_e("Storage failed after 3 tries...!");
  }
  else
  {
    log_d("Storage initialization success!");
    flags[sd_f] = 1;
  }

  if (wf.init())
  {
    if (wf.check_connection())
    {
      digitalWrite(LED_BUILTIN, HIGH);
      log_d("Initialized wifi successfully");
      flags[wf_f] = 1;
      ssid = WiFi.SSID();
    }
  }
  else
  {
    log_e("Wifi not Initialized");
  }

  String mac = WiFi.macAddress();
  mac.toLowerCase();
  mac.replace(":", ""); // AABBCCDDEEFF
  log_i("MAC Address: %s", mac.c_str());
  // mac address to lower case
  sensorID = mac;
  log_d("sensorID is %s", sensorID.c_str());

  while (initRTC() == false && _rtc_fail_count < FAIL_LIMIT)
  {
    _rtc_fail_count++;
    flags[rtc_f] = 0; // the system time would be 000 from start. The data would be ? I guess 1/1/2000, or maybe 1970..
    if (flags[sd_f])
    { // if storage is working
      // Loop through each slot's config file
      for (int i = 0; i < NUM_OF_SLOTS; i++)
      {
        String curr_file = storage.curr_read_file[i];

        log_d("Current Read file: %s", curr_file.c_str());

        // Skip empty file entries
        if (curr_file.length() < 9)
        {
          log_w("Slot %d curr_read_file too short or empty, skipping", i + 1);
          continue;
        }

        // ✅ Extract date from full path: /slot1/20260102.txt
        // Find the last '/' to get the filename
        int lastSlash = curr_file.lastIndexOf('/');
        if (lastSlash == -1)
        {
          log_w("Slot %d: Invalid file path format: %s", i + 1, curr_file.c_str());
          continue;
        }

        // Extract filename: 20260102.txt
        String filename = curr_file.substring(lastSlash + 1);

        // Remove .txt extension if present
        if (filename.endsWith(".txt"))
        {
          filename.remove(filename.length() - 4);
        }

        log_d("Slot %d: Extracted filename: %s", i + 1, filename.c_str());

        // Ensure string has exactly 8 characters for YYYYMMDD
        if (filename.length() == 8)
        {
          String syear = filename.substring(0, 4);
          String smonth = filename.substring(4, 6);
          String sday = filename.substring(6, 8);

          int iyear = syear.toInt();
          int imonth = smonth.toInt();
          int iday = sday.toInt();

          log_i("Slot %d: year %d, month %d, day %d", i + 1, iyear, imonth, iday);

          _set_esp_time(iyear, imonth, iday);
          flags[rtc_f] = 1;
          log_i("RTC set from slot %d file: %s", i + 1, curr_file.c_str());
          break; // Stop after first valid slot
        }
        else
        {
          log_w("Slot %d file invalid (wrong date length): %s", i + 1, filename.c_str());
        }
      }
    }
  }

  if (_rtc_fail_count >= FAIL_LIMIT)
  {
    log_e("RTC failed after 3 tries...!");
  }
  else
  {
    _set_esp_time();
    flags[rtc_f] = 1;
    log_d("Current Time : %s", getTime().c_str());
  }

  {
    // ✅ Read all slot IDs from NVS
    bool allIDsLoaded = true;

    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
      
    #ifdef TESTING_METER_IDS
      //for bypassing api request for testing purposes
      String meterKey0 = "sub-id-" + String(slot);
      // String meterKey1 = "sub-id-" + String(1);
      // String meterKey2 = "sub-id-" + String(2);
      Preferences pref;
      pref.begin(NVS_NAMESPACE_SENSOR_INFO, false);
      pref.putString(meterKey0.c_str(), "test010112aabbccddeeff0101");
      // preferences.putString(meterKey1.c_str(), "test010112aabbccddeeff0102");
      // preferences.putString(meterKey2.c_str(), "test010112aabbccddeeff0103");
      pref.end();
    #endif
      String slotKey = "sub-id-" + String(slot);
      Preferences preferences;
      preferences.begin(NVS_NAMESPACE_SENSOR_INFO, true);
      subID[slot] = preferences.getString(slotKey.c_str(), "");
      preferences.end();

      if (subID[slot] != "")
      {
        log_i("Slot %d ID found: %s", slot + 1, subID[slot].c_str());
      }
      else
      {
        log_w("Slot %d ID not found", slot + 1);
        allIDsLoaded = false;
      }
    }

    if (allIDsLoaded)
    {
      log_i("All slot IDs loaded successfully!");
    }
    else
    {
      // IDs not found - request from backend API
      log_i("Some IDs missing! Requesting from backend API...");

      while (!wf.check_connection() || !wf.check_Internet())
      {
        flags[wf_f] = 0;
        log_e("No wifi available, waiting for connection");
        delay(1000);
      }

      flags[wf_f] = 1;
      digitalWrite(LED_BUILTIN, HIGH);

      // ✅ Get MAC address and remove colons
      String macAddress = WiFi.macAddress();
      macAddress.toLowerCase();
      macAddress.replace(":", ""); // AABBCCDDEEFF
      log_i("MAC Address: %s", macAddress.c_str());
      // mac address to lower case
      sensorID = macAddress;
      log_d("sensorID is %s", sensorID.c_str());

      HTTPClient http;
      bool allRequestsSuccessful = true;

      // ✅ Make HTTP GET request for each slot
      for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
      {
        if (subID[slot] != "")
        {
          log_i("Slot %d: ID already exists, skipping", slot + 1);
          continue;
        }

        log_i("Slot %d: Requesting ID from API...", slot + 1);

        // Calculate slave_id
        uint8_t slaveId = 0x01 + slot;
        char slaveIdStr[3];
        snprintf(slaveIdStr, sizeof(slaveIdStr), "%02d", slaveId);

        // Build GET URL
        String apiURL = "https://enfoenviz.com/api/meter_lookup?";

        apiURL += "brand_id=" + String(METER_BRANDMODEL);
        macAddress.toLowerCase();
        apiURL += "&mac_address=" + macAddress;
        apiURL += "&slave_id=" + String(slaveIdStr);
        apiURL += "&extension_id=" + String(METER_EXTENSION);

        log_i("Slot %d: API URL: %s", slot + 1, apiURL.c_str());

        // ✅ Create new HTTPClient instance for each request
        HTTPClient http;
        http.begin(apiURL);

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0)
        {
          String response = http.getString();
          log_i("Slot %d: HTTP Response code: %d", slot + 1, httpResponseCode);
          log_i("Slot %d: Response: %s", slot + 1, response.c_str());

          if (httpResponseCode == 200)
          {
            String fullID = "";

            // ✅ Parse JSON response
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, response);

            if (!error)
            {
              // ✅ Check if status is success
              if (doc.containsKey("status") && doc["status"] == "success")
              {
                // ✅ Extract meter_id
                if (doc.containsKey("meter_id"))
                {
                  fullID = doc["meter_id"].as<String>();
                  log_i("Slot %d: Successfully got meter_id: %s", slot + 1, fullID.c_str());

                  // ✅ Optional: Log components for debugging
                  if (doc.containsKey("components"))
                  {
                    JsonObject components = doc["components"];
                  }
                }
                else
                {
                  log_e("Slot %d: meter_id not found in response", slot + 1);
                  allRequestsSuccessful = false;
                }
              }
              else
              {
                log_e("Slot %d: API returned non-success status", slot + 1);
                if (doc.containsKey("message"))
                {
                  log_e("  Message: %s", doc["message"].as<const char *>());
                }
                allRequestsSuccessful = false;
              }
            }
            else
            {
              log_e("Slot %d: JSON parsing failed: %s", slot + 1, error.c_str());
              log_e("  Response: %s", response.c_str());
              allRequestsSuccessful = false;
            }

            // ✅ Validate and save ID
            if (fullID.length() > 0)
            {
              subID[slot] = fullID;
              log_i("Slot %d: ID obtained: %s", slot + 1, subID[slot].c_str());
              // Save to NVS
              String slotKey = "sub-id-" + String(slot);
              Preferences preferences;
              preferences.begin(NVS_NAMESPACE_SENSOR_INFO, false);
              preferences.putString(slotKey.c_str(), fullID);
              preferences.end();

              log_i("Slot %d: ID saved to NVS: %s", slot + 1, fullID.c_str());
            }
            else
            {
              log_e("Slot %d: Empty ID received from API", slot + 1);
              allRequestsSuccessful = false;
            }
          }
          else
          {
            log_e("Slot %d: API returned error code: %d", slot + 1, httpResponseCode);
            log_e("Slot %d: Response: %s", slot + 1, response.c_str());
            allRequestsSuccessful = false;
          }
        }
        else
        {
          log_e("Slot %d: HTTP GET failed! Error: %s",
                slot + 1, http.errorToString(httpResponseCode).c_str());
          allRequestsSuccessful = false;
        }

        http.end();      // ✅ Clean up connection
        vTaskDelay(500); // Delay between requests
      }

      // ✅ Check if all requests succeeded
      if (allRequestsSuccessful)
      {
        log_i("All slot IDs retrieved successfully. Restarting...");
        vTaskDelay(1000);
        ESP.restart();
      }
      else
      {
        log_e("Failed to get all slot IDs from backend. Restarting in 10 seconds...");
        vTaskDelay(10000);
        ESP.restart();
      }
    }
  }

  wf.start_soft_ap();

  // // MQTT topics for handling data requests
  snprintf(data_pull_topic, sizeof(data_pull_topic), "%s%s", pullTopic, sensorID.c_str()); // pull/pjns0111885721471B640100
  snprintf(restart_topic, sizeof(restart_topic), "%s%s", restartTopic, sensorID.c_str());  // restart/pjns0111885721471B640100
  snprintf(logs_topic, sizeof(logs_topic), "%s%s", logsTopic, sensorID.c_str());           // logs/pjns0111885721471B640100
  snprintf(data_push_topic, sizeof(data_push_topic), "%s%s", pushTopic, sensorID.c_str()); // push/pjns0111885721471B640100

  log_i("data pull topic %s", data_pull_topic);
  log_i("data push topic %s", data_push_topic);
  log_i("restart topic %s", restart_topic);
  log_i("logs topic %s", logs_topic);

  flags[cloud_f] = 0;
  flags[cloud_blink_f] = 0;
  flags[cloud_setup_f] = 0;
  if (wf.check_connection())
  {
    if (wf.check_Internet())
    {
      flags[wf_f] = 1;
      digitalWrite(LED_BUILTIN, HIGH);
      update_essentials();

      myNVS::write::mqttData(mqtt_server, mqtt_port);
#ifdef OTA_UPDATE
      downloadUpdate();
#endif
      setRtcTime();
      log_i("Current Time is set to %s", getTime().c_str());
      byte _retry = 0;
      while (getUnixTime() < cutoff_time && _retry < FAIL_LIMIT)
      {
        log_e("RTC time is incorrect, retrying...");
        setRtcTime();
        vTaskDelay(2000);
        _retry++;
      }
      if (_retry >= FAIL_LIMIT)
      {
        log_e("RTC time setting failed after 3 tries...!");
        flags[rtc_f] = 0;
      }
      else
      {
        log_d("RTC time set successfully!");
        flags[rtc_f] = 1;
      }
      log_i("Mqtt Server : %s  Mqtt Port : %d", mqtt_server.c_str(), mqtt_port);
      mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
      mqttClient.setBufferSize(MAX_CHUNK_SIZE_B + 100);
      flags[cloud_setup_f] = 1;

      if (!mqttClient.connected())
      {
        if (mqttClient.connect(sensorID.c_str()))
        {

          if (mqttClient.connected())
          {
            flags[cloud_f] = 1;
            log_d("Cloud IoT Setup Complete");
          }
        }
        else
        {
          log_e("mqtt didn't connect");
        }
      }
      else
      {
        log_d("mqtt not connected");
      }

      dataClient.setServer(mqtt_server.c_str(), mqtt_port);
      dataClient.setBufferSize(PUSH_BUFFER_SIZE);
      dataClient.setCallback(onDataRequest);
      String deviceName = sensorID + "-data";

      if (dataClient.connect(deviceName.c_str()))
      {

        for (uint8_t i = 0; i < NUM_OF_SLOTS; i++)
        {
          String baseTopic = "enfoTopic/" + String(subID[i]);
          log_i("Historic Topic is %s", baseTopic.c_str());

          // Store both variants
          mqtt_topics_historic[i] = baseTopic;
          // mqtt_topics_livebackup[i] = baseTopic + "/LiveBackup";

          String pullTopicSlot = String(pullTopic) + String(sensorID) + "/slot" + String(i + 1);

          if (dataClient.subscribe(pullTopicSlot.c_str()))
          {
            log_i("Subscribed to pull topic: %s", pullTopicSlot.c_str());
          }
          if (dataClient.subscribe(restart_topic))
          {
            log_d("Subscribed to topic %s", restart_topic);
          }
        }
      }
    }
    else
    {
      flags[wf_f] = 0;
      log_i("Internet not availaible or wifi disconnected");
      WiFi.reconnect();
    }
  }
  else
  {
    log_i("Internet not availaible");
    flags[wf_f] = 0;
  }

  onRestart();

  semaAqData1 = xSemaphoreCreateBinary();
  semaStorage1 = xSemaphoreCreateBinary();
  semaWifi1 = xSemaphoreCreateBinary();

  xSemaphoreGive(semaWifi1); // Initially allow DataPull to run
  xSemaphoreGive(semaAqData1);

  reboottime = time(nullptr);

  xTaskCreatePinnedToCore(vStatusLed, "Status LED", 5000, NULL, 2, &ledTask, 1);
  xTaskCreatePinnedToCore(vAcquireData, "Data Acquisition", 6000, NULL, 5, &dataTask, 1);
  // xTaskCreatePinnedToCore(vLiveBroadcast, "Live Broadcast", 5000, NULL, 2, &liveBroadcastTask, 0);
  xTaskCreatePinnedToCore(vStorage, "Storage Handler", 6000, NULL, 3, &storageTask, 1);
  xTaskCreatePinnedToCore(vDataPull, "Data pull on Wifi", 7000, NULL, 2, &pullTask, 0);
  xTaskCreatePinnedToCore(vWifiTransfer, "Transfer data on Wifi", 7000, NULL, 4, &wifiTask, 0);
#ifdef OLED_DISPLAY
  xTaskCreatePinnedToCore(vOLEDDisplay, "OLED Display", 5000, NULL, 2, &oledTask, 0);
#endif
}

void loop()
{
  vTaskDelay(10);
}

void vAcquireData(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  unsigned long start_time = millis();
  int cycle_count = 0;

  // Read slot labels from NVS
  String slot_labels[NUM_OF_SLOTS];
  myNVS::read::slotLabels(slot_labels);

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    meterData[i].setPzem(&pzem[i]); // Set the address of the PZEM module
    meterData[i].setLabel(slot_labels[i]);
  }

  for (;;)
  {
    cycle_count++;

    if (cycle_count > DATA_STORAGE_TIME * MS_IN_SECOND / (DATA_ACQUISITION_TIME * MS_IN_SECOND))
    {
      cycle_count = 1;
    }

    if (reboottime + (12 * 60 * 60) <= time(nullptr))
    {
      ESP.restart();
    }

    xSemaphoreTake(semaAqData1, portMAX_DELAY); // semaphore to check if sending of data over storage has returned
    log_i("Cycle number: %d", cycle_count);

    _loop_counter = 0;
    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
#ifdef DUMMY_DATA
      meterData[i].readRandom();
#else
      meterData[i].read();
      vTaskDelay(50);
#endif
    }

    unsigned long elapsed_time = millis() - start_time;
    log_i("Elapsed Time: %lu s", elapsed_time / 1000);

    // Check if storage cycle reached
    if (cycle_count == (DATA_STORAGE_TIME * MS_IN_SECOND / (DATA_ACQUISITION_TIME * MS_IN_SECOND)) ||
        (elapsed_time / 1000 >= DATA_STORAGE_TIME))
    {
      for (int i = 0; i < NUM_OF_SLOTS; i++)
      {
        // Prepare storage string
        towrite_slot[i] = getTime() + "," + subID[i]; // towrite_slot[i] = getTime() + "," + subID[i];
        towrite_slot[i] += meterData[i].getDataString();
      }

      start_time = millis();
      cycle_count = 0;
      _loop_counter = 0;

      // Log all slot data for debug
      for (int i = 0; i < NUM_OF_SLOTS; i++)
      {
        log_i("Slot %d: %s", i + 1, towrite_slot[i].c_str());
      }

      // Update transfer variables if connected to cloud
      if (flags[wf_f] && flags[cloud_f])
      {
        for (int i = 0; i < NUM_OF_SLOTS; i++)
        {
          toTransfer_slot[i] = towrite_slot[i];
        }
      }
    }

    xSemaphoreGive(semaAqData1);
    xSemaphoreGive(semaStorage1);

    vTaskDelay(1);
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    log_v("Stack usage of acquireData Task: %d", (int)uxHighWaterMark);

    vTaskDelayUntil(&xLastWakeTime, (DATA_ACQUISITION_TIME * MS_IN_SECOND));
  }
}

/**
 * @brief This function stores the data acquired from smartmeter.
 * The execution of this task is controlled by the semaphore semaStorage1. This
 * semaphore is given by the acquire data task. Since the semaphore is given
 * on every chunk of data generated so, the execution frequency of transmission
 * is same as that of string generation i.e. 60 seconds (DATA ACQ TIME * DATA STORAGE TIME).
 *
 * @param pvParameters void
 */
void vStorage(void *pvParameters)
{
  for (;;)
  { // infinite loop
    if (flags[sd_f])
    { // storage is working
      vTaskDelay(1);
      xSemaphoreTake(semaStorage1, portMAX_DELAY); // for syncing task with acquire

      // Check if any slot has data to write
      bool hasDataToWrite = false;
      for (int i = 0; i < NUM_OF_SLOTS; i++)
      {
        if (towrite_slot[i] != "")
        {
          hasDataToWrite = true;
          break;
        }
      }

      if (hasDataToWrite)
      {
        // Make copy of all slot data and stop data acquisition
        xSemaphoreTake(semaAqData1, portMAX_DELAY);

        for (int i = 0; i < NUM_OF_SLOTS; i++)
        {
          towrite_s_slot[i] = towrite_slot[i];
          towrite_slot[i] = "";
        }

        xSemaphoreGive(semaAqData1);
        vTaskDelay(1);

        if (getUnixTime() > cutoff_time)
        {
          flags[rtc_f] = 1;

          bool writeAllSuccess = true;
          int successCount = 0;
          int failCount = 0;

          // Write data for each slot
          for (int i = 0; i < NUM_OF_SLOTS; i++)
          {
            if (towrite_s_slot[i] != "")
            {
              if (storage.write_data(i, getTime2(), towrite_s_slot[i]))
              {
                flags[sd_f] = 1;
                successCount++;
              }
              else
              {
                flags[sd_f] = 0;
                failCount++;
                writeAllSuccess = false;
                log_e("Slot %d: Storage write failed!", i + 1);
              }
            }
          }

          // Update flags based on results
          if (writeAllSuccess && successCount > 0)
          {
            flags[sd_f] = 1;
            flags[sd_blink_f] = 1;
            log_i("Storage cycle complete: %d/%d slots written successfully", successCount, NUM_OF_SLOTS);
          }
          else if (successCount > 0)
          {
            // Partial success - keep storage flag on
            flags[sd_f] = 1;
            log_w("Partial storage success: %d succeeded, %d failed", successCount, failCount);
          }
          else
          {
            // Complete failure
            log_e("Complete storage failure: All %d slots failed!", NUM_OF_SLOTS);
          }
        }
        else
        {
          log_e("RTC time is incorrect! Data not logging...");
          flags[rtc_f] = 0;
        }

        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        log_v("Stack usage of Storage Task: %d", (int)uxHighWaterMark);
      }

      xSemaphoreGive(semaStorage1);
    }
    else
    {
      // Storage failed - handle restart
      int restartCount;
      uint32_t restartTime;
      myNVS::read::restartData(restartCount, restartTime);
      log_d("Restart counter: %d", restartCount);

      if (restartCount == 4)
      {
        log_d("Restart count reached 4, restarting ESP after 10 mins...");
        vTaskDelay(2 * DATA_STORAGE_TIME * MS_IN_SECOND); // storage is not working, delay 10 minutes
      }

      log_i("ESP Restarting Due to storage failure...");
      ESP.restart();
    }
  } // end for
} // end vStorage task

void vDataPull(void *pvParameters)
{
  for (;;)
  {
    // Wait for Wi-Fi connection semaphore
    xSemaphoreTake(semaWifi1, portMAX_DELAY);
    vTaskDelay(1);

    if (flags[wf_f])
    {
      if (dataClient.loop())
      {
        if (flags[sd_f] && pendingDataRequests)
        {
          xSemaphoreTake(semaStorage1, portMAX_DELAY);
          processDataRequests();
          xSemaphoreGive(semaStorage1);
          vTaskDelay(1000);
        }
      }
      else
      {
        dataClient.setServer(mqtt_server.c_str(), mqtt_port);
        dataClient.setBufferSize(PUSH_BUFFER_SIZE);
        dataClient.setCallback(onDataRequest);
        String deviceName = sensorID + "-data";

        if (dataClient.connect(deviceName.c_str()))
        {

          for (uint8_t i = 0; i < NUM_OF_SLOTS; i++)
          {
            // Build base topic
            String baseTopic = "enfoTopic/" + subID[i];
            // irhj/01/11/885721471B64/01/00
            log_i("baseTopic is %s", baseTopic.c_str());

            // Store both variants
            mqtt_topics_historic[i] = baseTopic;
            // mqtt_topics_live[i] = baseTopic + "/live";
            // mqtt_topics_livebackup[i] = baseTopic + "/LiveBackup";

            String pullTopicSlot = String(pullTopic) + String(sensorID) + "/slot" + String(i + 1);

            if (dataClient.subscribe(pullTopicSlot.c_str()))
            {
              log_i("Subscribed to pull topic: %s", pullTopicSlot.c_str());
            }
            if (dataClient.subscribe(restart_topic))
            {
              log_d("Subscribed to topic %s", restart_topic);
            }
          }
        }
        else
        {
          vTaskDelay(1000);
        }
      }
      vTaskDelay(10);
    }
    xSemaphoreGive(semaWifi1);
  }
}

void vWifiTransfer(void *pvParameters)
{

  for (;;)
  {
    unsigned long now = millis();
    xSemaphoreTake(semaWifi1, portMAX_DELAY);
    handleWifiConnection(); // Ensure Wi-Fi is connected
    xSemaphoreGive(semaWifi1);
    vTaskDelay(1);
    if (flags[wf_f])
    {
      if (flags[png_f])
      {
        if (flags[sd_f])
        {
          handleStorageTransfer();
        }
      }
      else
        log_e("Ping not received");
    }

    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    log_v("Stack usage of Wifi Task: %d", (int)uxHighWaterMark);
    vTaskDelay(DATA_ACQUISITION_TIME * MS_IN_SECOND);
  }
}

void vStatusLed(void *pvParameters)
{
  int _blink_count[] = {0, 0, 0, 0};
  const int max_blink = 3;
  for (;;)
  {                                         // infinite loop
    for (int _flag = 0; _flag < 4; _flag++) // Blink Index
    {
      uint8_t _ledPin = LEDS[_flag];
      if (flags[_flag])
      {
        digitalWrite(_ledPin, !digitalRead(_ledPin));
        if (_flag != cloud_blink_f)
        {
          _blink_count[_flag]++;
          if (_blink_count[_flag] > max_blink)
          {
            _blink_count[_flag] = 0;
            flags[_flag] = 0;
          }
        }
        else
          flags[_flag] = 0;
      }
      else
        digitalWrite(_ledPin, flags[_flag + 4]);
    }
    vTaskDelay(100);
  }
}
