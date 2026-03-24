#include "espCloud.h"
#include "Preferences.h"
#include "FirebaseESP32.h"
#include "rtc.h"
#include "ESP32Ping.h"
#include "ESPWiFi.h"

String mqtt_server = DEFAULT_MQTT_SERVER;
uint16_t mqtt_port = DEFAULT_MQTT_PORT;

String mqtt_topics_live[NUM_OF_SLOTS];
String mqtt_topics_historic[NUM_OF_SLOTS];
String mqtt_topics_livebackup[NUM_OF_SLOTS];

char data_pull_topic[35] = "";
char restart_topic[35] = "";
char logs_topic[35] = "";
char data_push_topic[35] = "";

extern FirebaseData firebaseData;

String dataPath = "";
String clientID = sensorID + "-live";

static String getDeviceNode()
{
  String mac = WiFi.macAddress();
  if (mac.length() == 0)
  {
    // Fallback for edge cases where Wi-Fi isn't initialized yet.
    return sensorID.length() ? sensorID : String("unknown-device");
  }
  return mac;
}

static String makeDevicePath(const char *leaf)
{
  return "/" + getDeviceNode() + "/" + String(leaf);
}

bool isTimeSynced()
{
  unsigned long currentTime = time(nullptr);
  // Unix timestamp for 2020-01-01 00:00:00 is 1577836800
  return (currentTime > cutoff_time);
}

bool isValidDate(const String &date)
{
  if (date.length() != 8)
  {
    return false;
  }

  for (char c : date)
  {
    if (!isdigit(c))
    {
      return false;
    }
  }

  int year, month, day;

  if (sscanf(date.c_str(), "%4d%2d%2d", &year, &month, &day) != 3)
  {
    return false;
  }

  if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31)
  {
    return false;
  }

  return true;
}

void addDataRequest(uint8_t slot, const char *date)
{
  if (slot >= NUM_OF_SLOTS)
  {
    log_e("Invalid slot %d for data request", slot);
    return;
  }

  DataRequest *newPacket = new DataRequest;

  // Check if memory allocation succeeded
  if (newPacket == nullptr)
  {
    log_e("Failed to allocate memory for data request");
    return;
  }

  newPacket->slot = slot; // ✅ Added slot assignment
  strncpy(newPacket->date, date, 8);
  newPacket->date[8] = '\0';
  newPacket->next = nullptr;

  if (!pendingDataRequests)
  {
    pendingDataRequests = newPacket;
  }
  else
  {
    DataRequest *current = pendingDataRequests;
    while (current->next)
    {
      current = current->next;
    }
    current->next = newPacket;
  }

  log_i("Added data request: Slot %d, Date %s", slot + 1, date); // ✅ Updated log
}

void processPayload(const String &payload, uint8_t slot)
{
  if (slot >= NUM_OF_SLOTS)
  {
    log_e("Invalid slot %d in processPayload", slot);
    return;
  }

  // ========================================================================
  // SET SLOT ID COMMAND - Format: /setid/slot1/NEW_ID_VALUE
  // ========================================================================
  if (payload.startsWith("/setid/"))
  {
    String idData = payload.substring(7); // 7 = length of "/setid/"

    log_i("Received setid command: %s", idData.c_str());

    // Parse slot number and new ID
    // Expected format: slot1/NEW_ID_VALUE
    int slashPos = idData.indexOf('/');

    if (slashPos == -1)
    {
      log_e("Invalid setid format. Expected: /setid/slot1/NEW_ID_VALUE");
      dataClient.publish(logs_topic, "Invalid setid format");
      return;
    }

    // Extract slot identifier (e.g., "slot1")
    String slotStr = idData.substring(0, slashPos);

    // Extract new ID value
    String newID = idData.substring(slashPos + 1);
    newID.trim();

    // Parse slot number from "slot1" -> 0, "slot2" -> 1, etc.
    uint8_t targetSlot = 255; // Invalid

    if (slotStr.startsWith("slot") && slotStr.length() > 4)
    {
      int slotNum = slotStr.substring(4).toInt();
      if (slotNum >= 1 && slotNum <= NUM_OF_SLOTS)
      {
        targetSlot = slotNum - 1; // Convert to 0-based index
      }
    }

    if (targetSlot >= NUM_OF_SLOTS)
    {
      log_e("Invalid slot: %s", slotStr.c_str());
      dataClient.publish(logs_topic, "Invalid slot number for setid");
      return;
    }

    if (newID.length() == 0)
    {
      log_e("Empty ID value provided");
      dataClient.publish(logs_topic, "Empty ID value not allowed");
      return;
    }

    // Store new ID in NVS
    String slotKey = "sub-id-" + String(targetSlot);
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SENSOR_INFO, false);
    preferences.putString(slotKey.c_str(), newID);
    preferences.end();

    // Update in-memory variable
    extern String subID[NUM_OF_SLOTS];
    subID[targetSlot] = newID;

    log_i("Slot %d: ID updated to %s", targetSlot + 1, newID.c_str());

    // Send success response
    String successMsg = "Slot " + String(targetSlot + 1) + " ID set to: " + newID + ". Restarting...";
    dataClient.publish(logs_topic, successMsg.c_str());

    // Delay to allow MQTT message to be sent, then restart
    vTaskDelay(1000);
    ESP.restart();

    return;
  }

  // ========================================================================
  // REMOVE SLOT ID COMMAND - Format: /removeid/slot1 or /removeid/all
  // ========================================================================
  else if (payload.startsWith("/removeid/"))
  {
    String removeData = payload.substring(10); // 10 = length of "/removeid/"
    removeData.trim();

    log_i("Received removeid command: %s", removeData.c_str());

    extern String subID[NUM_OF_SLOTS];
    Preferences preferences;

    if (removeData == "all")
    {
      // Remove all slot IDs
      preferences.begin(NVS_NAMESPACE_SENSOR_INFO, false);

      for (uint8_t i = 0; i < NUM_OF_SLOTS; i++)
      {
        String slotKey = "sub-id-" + String(i);
        preferences.remove(slotKey.c_str());
        subID[i] = ""; // Clear in-memory variable
        log_i("Slot %d: ID removed", i + 1);
      }

      preferences.end();

      log_i("All slot IDs removed from NVS");
      dataClient.publish(logs_topic, "All slot IDs removed. Restarting...");

      // Delay to allow MQTT message to be sent, then restart
      vTaskDelay(1000);
      ESP.restart();

      return;
    }

    // Parse specific slot: slot1, slot2, etc.
    uint8_t targetSlot = 255; // Invalid

    if (removeData.startsWith("slot") && removeData.length() > 4)
    {
      int slotNum = removeData.substring(4).toInt();
      if (slotNum >= 1 && slotNum <= NUM_OF_SLOTS)
      {
        targetSlot = slotNum - 1; // Convert to 0-based index
      }
    }

    if (targetSlot >= NUM_OF_SLOTS)
    {
      log_e("Invalid slot for removeid: %s", removeData.c_str());
      dataClient.publish(logs_topic, "Invalid slot number for removeid");
      return;
    }

    // Remove specific slot ID from NVS
    String slotKey = "sub-id-" + String(targetSlot);
    preferences.begin(NVS_NAMESPACE_SENSOR_INFO, false);
    preferences.remove(slotKey.c_str());
    preferences.end();

    // Clear in-memory variable
    subID[targetSlot] = "";

    log_i("Slot %d: ID removed from NVS", targetSlot + 1);

    // Send success response
    String successMsg = "Slot " + String(targetSlot + 1) + " ID removed. Restarting...";
    dataClient.publish(logs_topic, successMsg.c_str());

    // Delay to allow MQTT message to be sent, then restart
    vTaskDelay(1000);
    ESP.restart();

    return;
  }

  // ========================================================================
  // SET CONFIG COMMAND - Format: /setconfig/slot1/20260102.txt
  // ========================================================================
  else if (payload.startsWith("/setconfig/"))
  {
    // Extract everything after "/setconfig/"
    String configData = payload.substring(11); // 11 = length of "/setconfig/"

    log_i("Received setconfig command: %s", configData.c_str());

    // Parse slot number and file path
    // Expected format: slot1/20260102.txt or slot2/20260103.txt
    int slashPos = configData.indexOf('/');

    if (slashPos == -1)
    {
      log_e("Invalid setconfig format. Expected: /setconfig/slot1/20260102.txt");
      dataClient.publish(logs_topic, "Invalid setconfig format");
      return;
    }

    // Extract slot identifier (e.g., "slot1")
    String slotStr = configData.substring(0, slashPos);

    // Extract file path (e.g., "20260102.txt")
    String fileName = configData.substring(slashPos + 1);

    // Parse slot number from "slot1" -> 0, "slot2" -> 1, etc.
    uint8_t targetSlot = 255; // Invalid

    if (slotStr.startsWith("slot") && slotStr.length() > 4)
    {
      int slotNum = slotStr.substring(4).toInt(); // Extract number from "slot1"
      if (slotNum >= 1 && slotNum <= NUM_OF_SLOTS)
      {
        targetSlot = slotNum - 1; // Convert to 0-based index
      }
    }

    if (targetSlot >= NUM_OF_SLOTS)
    {
      log_e("Invalid slot: %s", slotStr.c_str());
      dataClient.publish(logs_topic, "Invalid slot number");
      return;
    }

    // Build full file path: /slot1/20260102.txt
    String slotDir = "/slot" + String(targetSlot + 1);
    String fullFilePath = slotDir + "/" + fileName;

    log_i("Setting config for Slot %d: %s", targetSlot + 1, fullFilePath.c_str());

    if (!flags[sd_f])
    {
      log_e("Storage not available");
      dataClient.publish(logs_topic, "Storage fail - cannot set config");
      return;
    }

    // Verify the target file exists
    if (!SD.exists(fullFilePath))
    {
      log_w("Target file does not exist: %s", fullFilePath.c_str());
      String msg = "File not found: " + fullFilePath;
      dataClient.publish(logs_topic, msg.c_str());
      // Continue anyway to set config - file might be created later
    }

    // Update slot-specific config file
    String configPath = slotDir + "/config.txt";
    File file = SD.open(configPath, FILE_WRITE);

    if (!file)
    {
      log_e("Slot %d: Failed to open config.txt for writing", targetSlot + 1);
      dataClient.publish(logs_topic, "Failed to open config file");
      return;
    }

    // Write new config
    file.seek(0); // Start of file
    file.print(fullFilePath);
    file.println("$");
    file.print(FILE_START_POS); // Reset read position
    file.println("$");
    file.close();

    log_i("Slot %d: config.txt updated to %s", targetSlot + 1, fullFilePath.c_str());

    // Update in-memory variables
    storage.curr_read_file[targetSlot] = fullFilePath;
    storage.curr_read_pos[targetSlot] = FILE_START_POS;

    // Update NVS backup
    configuration__.begin("pointer", false);
    String nvs_key = "file-name" + String(targetSlot);
    configuration__.putString(nvs_key.c_str(), fullFilePath.c_str());
    configuration__.end();

    // Send success response
    String successMsg = "Slot " + String(targetSlot + 1) + " config updated: " + fullFilePath;
    dataClient.publish(logs_topic, successMsg.c_str());

    log_i("Config update complete for Slot %d", targetSlot + 1);

    return;
  }

  // ========================================================================
  // REQUEST APs OR CONFIG FILES - Format: /APs or /config/slot1
  // ========================================================================
  else if (payload.startsWith("/APs") || payload.startsWith("/config"))
  {
    if (payload.startsWith("/APs"))
    {
      // APs file is device-wide, not slot-specific
      String filePath = "/APs.txt";

      char pushTopic[128];
      snprintf(pushTopic, sizeof(pushTopic), "%s/APs", data_push_topic);

      log_i("Requesting APs file: %s", filePath.c_str());
      log_i("Push topic: %s", pushTopic);

      if (sendFile(filePath.c_str(), pushTopic))
      {
        log_i("APs file sent successfully");
      }
      else
      {
        log_e("Failed to send APs file");
        dataClient.publish(logs_topic, "APs file not found or send failed");
      }
      return;
    }
    else if (payload.startsWith("/config"))
    {
      // Parse slot from payload: /config/slot1, /config/slot2, /config/slot3
      uint8_t targetSlot = 255; // Invalid

      // Check if slot is specified: /config/slot1
      if (payload.length() > 7 && payload.indexOf("/slot") != -1)
      {
        int slotPos = payload.indexOf("/slot");
        String slotStr = payload.substring(slotPos + 1); // "slot1"

        if (slotStr.startsWith("slot") && slotStr.length() > 4)
        {
          int slotNum = slotStr.substring(4).toInt(); // Extract number
          if (slotNum >= 1 && slotNum <= NUM_OF_SLOTS)
          {
            targetSlot = slotNum - 1; // Convert to 0-based index
          }
        }
      }

      if (targetSlot >= NUM_OF_SLOTS)
      {
        // No valid slot specified - send all slot configs
        log_i("Sending config files for all slots");

        bool allSent = true;
        for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
        {
          String slotDir = "/slot" + String(slot + 1);
          String configPath = slotDir + "/config.txt";

          char pushTopic[128];
          snprintf(pushTopic, sizeof(pushTopic), "%s/config/slot%d",
                   data_push_topic, slot + 1);

          log_i("Slot %d: Sending config from %s to %s",
                slot + 1, configPath.c_str(), pushTopic);

          if (!sendFile(configPath.c_str(), pushTopic))
          {
            log_e("Slot %d: Failed to send config file", slot + 1);
            allSent = false;
          }

          vTaskDelay(100); // Delay between files
        }

        if (allSent)
        {
          dataClient.publish(logs_topic, "All config files sent successfully");
        }
        else
        {
          dataClient.publish(logs_topic, "Some config files failed to send");
        }
      }
      else
      {
        // Send specific slot config
        String slotDir = "/slot" + String(targetSlot + 1);
        String configPath = slotDir + "/config.txt";

        char pushTopic[128];
        snprintf(pushTopic, sizeof(pushTopic), "%s/config/slot%d",
                 data_push_topic, targetSlot + 1);

        log_i("Slot %d: Sending config from %s to %s",
              targetSlot + 1, configPath.c_str(), pushTopic);

        if (sendFile(configPath.c_str(), pushTopic))
        {
          String msg = "Slot " + String(targetSlot + 1) + " config sent";
          dataClient.publish(logs_topic, msg.c_str());
        }
        else
        {
          log_e("Slot %d: Failed to send config file", targetSlot + 1);
          dataClient.publish(logs_topic, "Config file not found or send failed");
        }
      }
      return;
    }
  }

  else if (payload.indexOf("_") != -1) // 20250512_1
  {

    String filePath = String(payload) + ".txt"; // this gets "20250512_1"
    char cpy_cstring[27];
    strcpy(cpy_cstring, data_push_topic); // /push/sm15-000/20250512_1
    snprintf(cpy_cstring, sizeof(cpy_cstring), "%s%s", cpy_cstring, payload.c_str());
    sendFile(filePath.c_str(), cpy_cstring);
    return;
  }
  else
  {

    log_i("Processing payload for Slot %d: %s", slot + 1, payload.c_str());

    char delimiter = ',';
    int startPos = 0;
    int endPos = payload.indexOf(delimiter);

    int requestCount = 0;

    // Process all dates separated by comma
    while (endPos != -1)
    {
      String date = payload.substring(startPos, endPos);
      date.trim();

      log_d("Extracted date from payload: %s", date.c_str());

      if (isValidDate(date))
      {
        addDataRequest(slot, date.c_str());
        requestCount++;
      }
      else
      {
        log_w("Invalid date format: %s", date.c_str());
      }

      startPos = endPos + 1;
      endPos = payload.indexOf(delimiter, startPos);
    }

    // Process last date (no comma after it)
    String lastDate = payload.substring(startPos);
    lastDate.trim();

    if (lastDate.length() > 0)
    {
      log_d("Extracted last date from payload: %s", lastDate.c_str());

      if (isValidDate(lastDate))
      {
        addDataRequest(slot, lastDate.c_str());
        requestCount++;
      }
      else
      {
        log_w("Invalid date format: %s", lastDate.c_str());
      }
    }

    log_i("Slot %d: Added %d data request(s)", slot + 1, requestCount);
  }
}
void onDataRequest(char *topic, uint8_t *buff, unsigned int size)
{
  log_d("Callback function called with topic %s", topic);

  String topicStr = String(topic);
  String payload = "";

  // Build payload string
  for (unsigned int i = 0; i < size; i++)
  {
    payload += (char)buff[i];
  }

  log_i("Topic: %s, Payload: %s", topic, payload.c_str());

  // ========================================================================
  // Check if it's a pull request for any slot
  // ========================================================================
  bool isPullRequest = false;
  uint8_t targetSlot = 255; // Invalid slot initially

  for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
  {
    // Build expected topic: pull/SM15-000/slot1, pull/SM15-000/slot2, etc.
    char expectedPullTopic[128];
    snprintf(expectedPullTopic, sizeof(expectedPullTopic), "%s/slot%d",
             data_pull_topic, slot + 1);

    if (strcmp(topic, expectedPullTopic) == 0)
    {
      isPullRequest = true;
      targetSlot = slot;
      break;
    }
  }

  if (isPullRequest && targetSlot < NUM_OF_SLOTS)
  {
    log_i("Data pull request for Slot %d", targetSlot + 1);
    log_d("Processing payload: %s", payload.c_str());

    if (payload == "gettime" || payload == "/gettime")
    {
      log_i("Get time request received");

      String currentTime =   getTime();
      // Build response JSON
      String response = "{";
      response += "\"current_time\":\"" + currentTime + "\",";
      response += "}";

      // Publish to logs topic
      if (dataClient.publish(logs_topic, response.c_str()))
      {
        log_i("Time info published: %s", response.c_str());
      }
      else
      {
        log_e("Failed to publish time info");
      }
      return;
    }

    // ====================================================================
    // TIME SYNC - pull/slotX payload: "sync"
    // ====================================================================
    else if (payload == "sync" || payload == "/sync")
    {
      log_i("Time sync request received");

      // Perform NTP sync
      log_i("Syncing time from NTP server...");
      
      configTime(18000, 0, "pool.ntp.org");

      // Wait for time to be set (max 10 seconds)
      int retries = 0;
      bool syncSuccess = false;

      while (retries < 20)
      {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (time(nullptr) > cutoff_time)
        {
          syncSuccess = true;
          break;
        }
        retries++;
      }

      // Build response
      String response = "{";
      response += "\"status\":\"" + String(syncSuccess ? "success" : "error") + "\",";

      if (syncSuccess)
      {
        setRtcTime();
        String syncedTime =
            response += "\"message\":\"Time synced from NTP\",";
        response += "\"current_time\":\"" + syncedTime + "\",";
        log_i("Time synced successfully: %s", syncedTime.c_str());
      }
      else
      {
        response += "\"message\":\"Failed to sync time from NTP\"";
        log_e("Time sync failed");
      }

      response += "}";

      // Publish result
      dataClient.publish(logs_topic, response.c_str());
      return;
    }

    processPayload(payload, targetSlot); // ✅ Pass slot to processPayload
    return;
  }

  // ========================================================================
  // Check if it's a restart request
  // ========================================================================
  if (strcmp(topic, restart_topic) == 0)
  {
    log_i("Restart request received");
    log_d("Processing payload: %s", payload.c_str());

    if (payload == "restart")
    {
      log_w("Restarting ESP32...");
      vTaskDelay(1000);
      ESP.restart();
    }
    else
    {
      log_w("Invalid restart payload: %s", payload.c_str());
    }
    return;
  }

  // ========================================================================
  // Unknown topic
  // ========================================================================
  log_w("Unknown topic received: %s", topic);
}
/**
 * @brief Sends a file to MQTT topic in chunks
 * Improved version with better error handling and memory safety
 *
 * @param fileName - Path to file to send
 * @param topic - MQTT topic to publish to
 * @return true if file sent successfully, false otherwise
 */
bool sendFile(const char *fileName, const char *topic)
{
  if (!SD.exists(fileName))
  {
    log_e("File %s does not exist. Data will not be sent!", fileName);
    if (dataClient.publish(topic, "Not Found"))
    {
      log_d("Published \"Not Found\" message to topic [%s]", topic);
    }
    return false;
  }

  File file = SD.open(fileName, FILE_READ); // ✅ Explicitly specify FILE_READ

  if (!file)
  {
    log_e("File %s could not be opened. Data will not be sent!", fileName);
    if (dataClient.publish(topic, "Not Found"))
    {
      log_d("Published \"Not Found\" message to topic [%s]", topic);
    }
    return false;
  }

  if (file.isDirectory())
  {
    log_e("File %s is a directory. Data will not be sent!", fileName);
    if (dataClient.publish(topic, "Not Found"))
    {
      log_d("Published \"Not Found\" message to topic [%s]", topic);
    }
    file.close();
    return false;
  }

  log_d("File %s will be published on topic [%s]", fileName, topic);

  size_t fileSize = file.size();

  // ✅ Check if file is empty
  if (fileSize == 0)
  {
    log_w("File %s is empty, skipping", fileName);
    file.close();
    return false;
  }

  // ✅ Check if file is too large (optional safety check)
  const size_t MAX_FILE_SIZE = 1024 * 1024; // 1MB limit
  if (fileSize > MAX_FILE_SIZE)
  {
    log_e("File %s is too large (%zu bytes), skipping", fileName, fileSize);
    file.close();
    return false;
  }

  // First packet is file size
  if (dataClient.publish(topic, String(fileSize).c_str()))
  {
    log_d("Published file [%s] size [%zu] at topic [%s]", fileName, fileSize, topic);
  }
  else
  {
    log_e("Failed to publish file size for %s", fileName);
    file.close();
    return false;
  }

  size_t numOfPackets = fileSize / PUSH_PACKET_SIZE;
  size_t lastPacketSize = fileSize % PUSH_PACKET_SIZE;
  if (lastPacketSize)
  {
    numOfPackets += 1;
  }

  log_d("Number of packets to be sent: %zu", numOfPackets);

  file.seek(0);
  size_t startIndex = 0;
  bool allPacketsSent = true;

  // Send all packets except last
  for (size_t packetIndex = 0; packetIndex < numOfPackets - 1; packetIndex++)
  {
    file.seek(startIndex);

    char buffer[PUSH_PACKET_SIZE + 1];
    size_t bytesRead = file.readBytes(buffer, PUSH_PACKET_SIZE);

    // ✅ Check if bytes were actually read
    if (bytesRead != PUSH_PACKET_SIZE)
    {
      log_e("Expected %d bytes but read %zu bytes at packet %zu",
            PUSH_PACKET_SIZE, bytesRead, packetIndex + 1);
      allPacketsSent = false;
      break;
    }

    buffer[PUSH_PACKET_SIZE] = '\0';
    startIndex += bytesRead;

    if (dataClient.publish(topic, buffer))
    {
      log_d("Sent data packet (%zu/%zu) for %s to cloud.",
            packetIndex + 1, numOfPackets, fileName);
    }
    else
    {
      log_e("Failed to send packet %zu/%zu for %s",
            packetIndex + 1, numOfPackets, fileName);
      allPacketsSent = false;
      break;
    }

    vTaskDelay(10); // ✅ Increased delay for MQTT stability
  }

  // Send last packet
  if (allPacketsSent && lastPacketSize > 0)
  {
    file.seek(startIndex);
    char *lastBuff = new char[lastPacketSize + 1];

    // ✅ Check memory allocation
    if (lastBuff == nullptr)
    {
      log_e("Failed to allocate memory for last packet");
      file.close();
      return false;
    }

    size_t bytesRead = file.readBytes(lastBuff, lastPacketSize);

    // ✅ Verify bytes read
    if (bytesRead != lastPacketSize)
    {
      log_e("Expected %zu bytes but read %zu bytes in last packet",
            lastPacketSize, bytesRead);
      delete[] lastBuff;
      file.close();
      return false;
    }

    lastBuff[lastPacketSize] = '\0';

    if (dataClient.publish(topic, lastBuff))
    {
      log_d("Sent data packet (%zu/%zu) for %s to cloud.",
            numOfPackets, numOfPackets, fileName);
    }
    else
    {
      log_e("Failed to send last packet for %s", fileName);
      allPacketsSent = false;
    }

    delete[] lastBuff;
  }

  file.close();

  if (allPacketsSent)
  {
    log_i("File %s sent successfully (%zu packets)", fileName, numOfPackets);
  }
  else
  {
    log_e("File %s transfer incomplete!", fileName);
  }

  return allPacketsSent;
}
/**
 * @brief Processes all pending data requests and sends files to MQTT
 * Sends slot-specific files to slot-specific push topics
 */
void processDataRequests()
{
  if (pendingDataRequests == nullptr)
  {
    log_d("No pending data requests");
    return;
  }

  log_i("Processing pending data requests...");

  DataRequest *current = pendingDataRequests;
  int requestsProcessed = 0;
  int requestsFailed = 0;

  while (current)
  {
    uint8_t slot = current->slot;

    // Build slot-specific file path: /slot1/20260101.txt
    String slotDir = "/slot" + String(slot + 1);
    String fileName = slotDir + "/" + String(current->date) + ".txt";

    // Build slot-specific push topic
    // Format: push/SM15-000/slot1/20260101
    char pushTopic[128];
    snprintf(pushTopic, sizeof(pushTopic), "%s/slot%d/%s",
             data_push_topic, slot + 1, current->date);

    log_i("Slot %d: Sending file %s to topic %s",
          slot + 1, fileName.c_str(), pushTopic);

    log_d("Packet size: %d bytes", PUSH_PACKET_SIZE);
    log_d("Buffer size: %d bytes", PUSH_BUFFER_SIZE);

    // Send the file
    if (sendFile(fileName.c_str(), pushTopic))
    {
      log_i("Slot %d: Successfully sent %s", slot + 1, current->date);
      requestsProcessed++;
    }
    else
    {
      log_e("Slot %d: Failed to send %s", slot + 1, current->date);
      requestsFailed++;
    }

    // Move to next request and delete current
    DataRequest *nextRequest = current->next;
    delete current;
    current = nextRequest;

    vTaskDelay(100); // Delay between file sends
  }

  // Clear the list
  pendingDataRequests = nullptr;

  log_i("Data requests processed: %d succeeded, %d failed",
        requestsProcessed, requestsFailed);
}

bool live_broadcast(String broadcastData, uint8_t slotIndex)
{
  // Validate slot index
  if (slotIndex >= NUM_OF_SLOTS)
  {
    log_e("Invalid slot index: %d", slotIndex);
    return false;
  }

  if (broadcastData == "")
  {
    log_d("Slot %d: Empty String Invalid Data", slotIndex + 1);
    return false;
  }

  if (!WiFi.isConnected())
  {
    log_e("Slot %d: WiFi isn't Connected", slotIndex + 1);
    return false;
  }

  if (!wf.check_Internet())
  {
    log_e("Slot %d: No ping received for live data", slotIndex + 1);
    return false;
  }

  if (!flags[cloud_f])
  {
    log_e("Slot %d: MQTT Client isn't setup", slotIndex + 1);
    return false;
  }

  if (!mqttClient.connected())
  {
    log_e("Slot %d: MQTT Client Disconnected", slotIndex + 1);
    flags[cloud_f] = 0;
    return false;
  }

  if (mqttClient.publish(mqtt_topics_live[slotIndex].c_str(), broadcastData.c_str()))
  {
    flags[cloud_blink_f] = 1; // Blink cloud LED
    log_i("Slot %d: Live data broadcasted on topic: %s", slotIndex + 1, mqtt_topics_live[slotIndex].c_str());
    return true; // Success
  }
  else
  {
    log_e("Slot %d: Error broadcasting live data, check server", slotIndex + 1);
    return false;
  }
}

bool publishDeviceParams()
{
  float slotFactors[NUM_OF_SLOTS];
  float voltageCalibration[NUM_OF_SLOTS];
  float currentCalibration[NUM_OF_SLOTS];

  myNVS::read::slotFactors(slotFactors);

  Preferences voltageCaliberation, currentCaliberation;
  voltageCaliberation.begin("voltage-cal", true);
  currentCaliberation.begin("current-cal", true);
  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String vKey = "cal_factor_" + String(i);
    String cKey = "cur_factor_" + String(i);
    voltageCalibration[i] = voltageCaliberation.getFloat(vKey.c_str(), 1.0);
    currentCalibration[i] = currentCaliberation.getFloat(cKey.c_str(), 1.0);
  }
  voltageCaliberation.end();
  currentCaliberation.end();

  String json = "{";
  json += "\"slotFactors\":[";
  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    json += String(slotFactors[i], 6);
    if (i != NUM_OF_SLOTS - 1)
      json += ",";
  }
  json += "],";

  json += "\"voltageCalibration\":[";
  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    json += String(voltageCalibration[i], 6);
    if (i != NUM_OF_SLOTS - 1)
      json += ",";
  }
  json += "],";

  json += "\"currentCalibration\":[";
  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    json += String(currentCalibration[i], 6);
    if (i != NUM_OF_SLOTS - 1)
      json += ",";
  }
  json += "],";

  json += "\"sd_f\":";
  json += String(flags[sd_f]);
  json += "}";

  char cpy_cstring[25]; //  /push/SM15-9999/params
  strcpy(cpy_cstring, data_push_topic);
  snprintf(cpy_cstring, sizeof(cpy_cstring), "%s/params", cpy_cstring);
  bool ok = dataClient.publish(cpy_cstring, json.c_str());
  if (ok)
  {
    log_d("Published device params to topic [%s]", cpy_cstring);
  }
  else
  {
    log_e("Failed to publish device params to topic [%s]", cpy_cstring);
  }
  return ok;
}

/**
 * @brief Attempts to recover and send backed-up live data for all slots
 * Called when live broadcast is successful to check if there are any
 * backup files that need to be sent to the cloud
 *
 * Checks both files (1.txt and 2.txt) for each slot and sends them if they exist
 */
void attemptLiveDataRecovery()
{

  int totalFilesRecovered = 0;

  // Loop through all slots
  for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
  {
    String slotDir = "/liveData/slot" + String(slot + 1);

    // Check if slot directory exists
    if (!SD.exists(slotDir))
    {
      // log_d("Slot %d: No backup directory found, skipping", slot + 1);
      continue;
    }

    // Check both backup files (1.txt and 2.txt) for this slot
    for (int fileNum = 1; fileNum <= 2; fileNum++)
    {
      String fileName = slotDir + "/" + String(fileNum) + ".txt";

      if (SD.exists(fileName))
      {
        // Build topic for this slot's backup data
        String topic = mqtt_topics_livebackup[slot] + "/" + String(fileNum);

        log_i("Slot %d: Recovering backup file %d...", slot + 1, fileNum);

        if (sendFile(fileName.c_str(), topic.c_str()))
        {
          // Successfully sent, now delete the file
          if (SD.remove(fileName))
          {
            log_i("Slot %d: Successfully sent and deleted backup file %d",
                  slot + 1, fileNum);
            totalFilesRecovered++;
          }
          else
          {
            log_e("Slot %d: Failed to delete backup file %d after sending",
                  slot + 1, fileNum);
          }
        }
        else
        {
          log_e("Slot %d: Failed to send backup file %d", slot + 1, fileNum);
        }

        vTaskDelay(100); // Small delay between file transfers
      }
    }
  }

  if (totalFilesRecovered > 0)
  {
    log_i("Live data recovery complete: %d file(s) recovered", totalFilesRecovered);
  }
}
/******************************************************FIREBASE FUNCTIONS*****************************************************************/
bool initializeDefaultLabels()
{
  uint32_t currentTime = getUnixTime();
  String slotLabels[NUM_OF_SLOTS];
  String labelTimePath = makeDevicePath("lastslotsettime");
  String slotLabelBasePath = makeDevicePath("slotlabels");

  myNVS::write::lastslotsettime(currentTime);

  if (!Firebase.setInt(firebaseData, labelTimePath, currentTime))
  {
    log_e("Failed to set default timestamp");
    return false;
  }

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String labelPath = slotLabelBasePath + "/label" + String(i + 1);
    String labelValue = "label" + String(i + 1);
    slotLabels[i] = labelValue;

    if (!Firebase.setString(firebaseData, labelPath.c_str(), labelValue.c_str()))
    {
      log_e("Failed to set default label for slot %d", i + 1);
      return false;
    }
    log_i("Set default label for slot %d", i + 1);
  }

  myNVS::write::slotLabels(slotLabels);
  return true;
}
bool initializeDefaultSlotFactors()
{
  uint32_t currentTime = getUnixTime();
  float factors[NUM_OF_SLOTS];
  String factorTimePath = makeDevicePath("lastfactorsettime");
  String slotFactorBasePath = makeDevicePath("slotfactors");

  myNVS::write::lastfactorsettime(currentTime);

  if (!Firebase.setInt(firebaseData, factorTimePath, currentTime))
  {
    log_e("Failed to set default timestamp");
    return false;
  }

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String factorPath = slotFactorBasePath + "/factor" + String(i + 1);
    float factorValue = 1.00;
    factors[i] = factorValue;

    if (!Firebase.setFloat(firebaseData, factorPath.c_str(), factorValue))
    {
      log_e("Failed to set default factors for slot %d", i + 1);
      return false;
    }
    log_i("Set default factor for slot %d", i + 1);
  }

  myNVS::write::slotFactors(factors);
  return true;
}

bool copyLabelsToFirebase(const uint32_t &currentTime, String *slotLabels)
{
  log_i("[PROGRESSING...] : COPYING LABELS FROM PREFERENCES TO FIREBASE");
  String slotLabelBasePath = makeDevicePath("slotlabels");
  String labelTimePath = makeDevicePath("lastslotsettime");
  myNVS::read::slotLabels(slotLabels);

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String labelPath = slotLabelBasePath + "/label" + String(i + 1);

    if (!Firebase.setString(firebaseData, labelPath.c_str(), slotLabels[i].c_str()))
    {
      log_e("Failed to copy label for slot %d", i + 1);
      return false;
    }
  }

  return Firebase.setInt(firebaseData, labelTimePath, currentTime);
}
bool copyfactorsToFirebase(const uint32_t &currentTime, float *slotfactors)
{
  log_i("[PROGRESSING...] : COPYING FACTORS FROM PREFERENCES TO FIREBASE");
  String slotFactorBasePath = makeDevicePath("slotfactors");
  String factorTimePath = makeDevicePath("lastfactorsettime");
  myNVS::read::slotFactors(slotfactors);

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String factorPath = slotFactorBasePath + "/factor" + String(i + 1);

    if (!Firebase.setFloat(firebaseData, factorPath.c_str(), slotfactors[i]))
    {
      log_e("Failed to copy factors for slot %d", i + 1);
      return false;
    }
  }

  return Firebase.setInt(firebaseData, factorTimePath, currentTime);
}

bool copyLabelsFromFirebase(const uint32_t &currentTimeFirebase)
{
  log_i("Copying labels from Firebase to Preferences");
  String slotLabels[NUM_OF_SLOTS];
  String slotLabelBasePath = makeDevicePath("slotlabels");

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String labelPath = slotLabelBasePath + "/label" + String(i + 1);

    if (!Firebase.getString(firebaseData, labelPath.c_str(), &slotLabels[i]))
    {
      log_e("Failed to get label for slot %d", i + 1);
      return false;
    }
  }

  myNVS::write::slotLabels(slotLabels);
  myNVS::write::lastslotsettime(currentTimeFirebase);
  return true;
}
bool copyfactorsFromFirebase(const uint32_t &currentTimeFirebase)
{
  log_i("Copying factors from Firebase to Preferences");
  float slotfactors[NUM_OF_SLOTS];
  String slotFactorBasePath = makeDevicePath("slotfactors");

  for (int i = 0; i < NUM_OF_SLOTS; i++)
  {
    String factorPath = slotFactorBasePath + "/factor" + String(i + 1);

    if (!Firebase.getFloat(firebaseData, factorPath.c_str(), &slotfactors[i]))
    {
      log_e("Failed to get factor for slot %d", i + 1);
      return false;
    }
  }

  myNVS::write::slotFactors(slotfactors);
  myNVS::write::lastfactorsettime(currentTimeFirebase);
  return true;
}

bool syncWithFirebase(SyncType type)
{
  uint32_t currentTimeLocal;
  uint32_t currentTimeFirebase;
  uint32_t writeUnix = getUnixTime();
  float slotFactors[NUM_OF_SLOTS];
  String slotLabels[NUM_OF_SLOTS];

  (type == LABELS) ? myNVS::read::slotLabels(slotLabels) : myNVS::read::slotFactors(slotFactors);

  // Define necessary variables for labels or factors
  String basePath, timePath;
  bool alreadyExist;

  if (type == LABELS)
  {
    basePath = makeDevicePath("slotlabels");
    timePath = makeDevicePath("lastslotsettime");
    alreadyExist = Labels_Already_Exist;
  }
  else
  {
    basePath = makeDevicePath("slotfactors");
    timePath = makeDevicePath("lastfactorsettime");
    alreadyExist = Factors_Already_Exist;
  }

  // Check if Firebase structure exists
  if (!Firebase.get(firebaseData, basePath))
  {
    if (type == LABELS)
    {

      if (!alreadyExist)
      {
        log_i("Creating New Firebase Structure for Labels");
        return initializeDefaultLabels();
      }
      else
      {
        log_i("[HURRAY] : LABELS ALREADY EXIST IN NVS. STARTING TO COPY...");
        myNVS::write::lastslotsettime(writeUnix);
        return copyLabelsToFirebase(writeUnix, slotLabels);
      }
    }
    else
    {

      if (!alreadyExist)
      {
        log_i("Creating New Firebase Structure for Factors");
        return initializeDefaultSlotFactors();
      }
      else
      {
        log_i("[HURRAY] : FACTORS ALREADY EXIST IN NVS. STARTING TO COPY...");
        myNVS::write::lastfactorsettime(writeUnix);
        return copyfactorsToFirebase(writeUnix, slotFactors);
      }
    }
  }

  log_i("Checking Sync status");
  if (type == LABELS)
  {
    String maxSlotCheckPath = makeDevicePath("slotlabels") + "/label" + String(NUM_OF_SLOTS);

    if (!Firebase.get(firebaseData, maxSlotCheckPath))
    {
      return initializeDefaultLabels();
    }
  }
  else
  {
    String maxFactorCheckPath = makeDevicePath("slotfactors") + "/factor" + String(NUM_OF_SLOTS);

    if (!Firebase.get(firebaseData, maxFactorCheckPath))
    {
      return initializeDefaultSlotFactors();
    }
  }

  if (!Firebase.getInt(firebaseData, timePath, &currentTimeFirebase))
  {
    log_e("Failed to get timestamp from Firebase");
    return false;
  }

  if (type == LABELS)
    myNVS::read::lastslotsettime(currentTimeLocal);
  else
    myNVS::read::lastfactorsettime(currentTimeLocal);

  if (currentTimeLocal == currentTimeFirebase)
  {
    log_i("Data is in sync");
    return true;
  }

  return (currentTimeLocal > currentTimeFirebase) ? ((type == LABELS) ? copyLabelsToFirebase(currentTimeLocal, slotLabels) : copyfactorsToFirebase(currentTimeLocal, slotFactors)) : ((type == LABELS) ? copyLabelsFromFirebase(currentTimeFirebase) : copyfactorsFromFirebase(currentTimeFirebase));
}
void update_essentials()
{
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "small");
  String tmps_IP = "";
  uint16_t tmps_port = 0;
  String s_IP = makeDevicePath("IP");
  String s_port = makeDevicePath("Port");
  String s_SD = makeDevicePath("SD_Flag");
  String s_RTC = makeDevicePath("RTC_Flag");
  String s_Time = makeDevicePath("lastRestart");
  String currentVersionPath = makeDevicePath("Version");
  String latestVersionPath = "/LatestCustomMeterVersion";
  dataPath = makeDevicePath("deviceID");

  Firebase.getString(firebaseData, s_IP, &tmps_IP);
  Firebase.getInt(firebaseData, s_port, &tmps_port);
  Firebase.getString(firebaseData, latestVersionPath, &latestVersion);
  Firebase.setString(firebaseData, currentVersionPath, String(FIRMWARE_VERSION));
  Firebase.setBool(firebaseData, s_SD, flags[sd_f]);
  Firebase.setBool(firebaseData, s_RTC, flags[rtc_f]);
  Firebase.setString(firebaseData, s_Time, getTime());

  if (tmps_IP == "")
    Firebase.setString(firebaseData, s_IP, mqtt_server);
  else if (tmps_IP != mqtt_server)
  {
    mqtt_server = tmps_IP;
    log_i("%s", mqtt_server.c_str());
  }
  if (!tmps_port)
    Firebase.setInt(firebaseData, s_port, mqtt_port);
  else if (tmps_port != mqtt_port)
  {
    mqtt_port = tmps_port;
    log_i("%d", mqtt_port);
  }
  Firebase.end(firebaseData);
  Firebase.endStream(firebaseData);
}
