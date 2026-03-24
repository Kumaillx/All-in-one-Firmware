#include "DataTransfer.h"
byte _client2count = 0;
unsigned long _client2timer;

void handleWifiConnection()
{
    static int wifiFailCounter = 0;

    if (serverData)
    {
        wf.create_new_connection(serverWifiCreds[0].c_str(), serverWifiCreds[1].c_str());
        serverData = false;
    }

    if (!wf.check_connection())
    {
        flags[wf_f] = 0;
        flags[cloud_f] = 0;

        if (wifiFailCounter >= 1)
        {
            wifiFailCounter = 0;
            log_i("Wifi disconnected!");

            digitalWrite(LED_BUILTIN, LOW);
            vTaskDelay(5);
        }

        wifiFailCounter++;
    }
    else
    {

        ssid = WiFi.SSID();
        log_i("Connected to SSID: %s", ssid.c_str());

        if (!wf.check_Internet())
        {
            digitalWrite(LED_BUILTIN, LOW);
            flags[wf_f] = 0;
            flags[cloud_f] = 0;
            log_d("Ping not received");
            WiFi.reconnect();
            vTaskDelay(5);
        }
        else
        {
            digitalWrite(LED_BUILTIN, HIGH);
            flags[wf_f] = 1;
            flags[wf_blink_f] = 1;

            if (!flags[cloud_f] && !flags[cloud_setup_f])
            {
                mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
                mqttClient.setBufferSize(MAX_CHUNK_SIZE_B + 25);

                flags[cloud_setup_f] = 1;
                log_d("Cloud IoT Setup Complete");
            }

            mqttClient.loop();

            vTaskDelay(10); // <- fixes some issues with WiFi stability

            if (!mqttClient.connected())
            {
                flags[cloud_f] = 0;

                for (int i = 0; i < 3; i++)
                {
                    vTaskDelay(10); // <- fixes some issues with WiFi stability
                    configTime(18000, 0, "pool.ntp.org");

                    while (time(nullptr) < cutoff_time) // time less than 1 Jan 2021 12:00 AM
                    {
                        vTaskDelay(1000);
                        log_e("Waiting for time update from NTP server...");
                    }

                    mqttClient.connect(sensorID.c_str());

                    if (mqttClient.connected())
                    {
                        flags[cloud_f] = 1;
                        break;
                    }
                }
            }
        }
    }
}

void handleStorageTransfer()
{
    static String toread = "";
    static int chunksSentCounter = 0;
    static uint8_t currentSlot = 0; // Round-robin between slots

    xSemaphoreTake(semaStorage1, portMAX_DELAY);
    xSemaphoreTake(semaAqData1, portMAX_DELAY);
    long unsent_bytes = storage.get_unsent_data(getTime2());
    log_i("Data to be sent: %ld bytes", unsent_bytes);
    xSemaphoreGive(semaAqData1);
    xSemaphoreGive(semaStorage1);

    if (unsent_bytes > MAX_CHUNK_SIZE_B * NUM_OF_SLOTS)
    {
        log_i("Starting data transfer: %ld bytes pending", unsent_bytes);

        if (flags[cloud_f])
        {
            if (mqttClient.connected())
            {
                xSemaphoreTake(semaStorage1, portMAX_DELAY);

                // Send up to 50 chunks (distributed across all slots)
                for (int i = 0; i < 50 && unsent_bytes > MIN_CHUNK_SIZE_B; i++)
                {

                    xSemaphoreTake(semaAqData1, portMAX_DELAY);

                    // Read from current slot
                    toread = "";
                    toread = storage.read_data(currentSlot);

                    if (toread != "")
                    {

                        if (mqttClient.publish(mqtt_topics_historic[currentSlot].c_str(), toread.c_str()))
                        {
                            flags[cloud_blink_f] = 1;
                            digitalWrite(LED_BUILTIN, LOW);
                            vTaskDelay(100);
                            digitalWrite(LED_BUILTIN, HIGH);

                            log_i("Slot %d: Data sent to cloud", currentSlot + 1);
                            log_d("  Topic: %s", mqtt_topics_historic[currentSlot].c_str());
                            log_d("  Data: %s", toread.c_str());

                            chunksSentCounter++;

                            // Mark data as sent for this slot
                            storage.mark_data(currentSlot, getTime2());
                            vTaskDelay(1);

                            // Update unsent bytes estimate
                            unsent_bytes -= strlen(toread.c_str()) + 4; // +4 for "<>\n"
                        }
                        else
                        {
                            log_e("Slot %d: Failed to publish data", currentSlot + 1);
                            xSemaphoreGive(semaAqData1);
                            break; // Exit loop on publish failure
                        }
                    }
                    else
                    {
                        log_d("Slot %d: No data to read (empty or end of file)", currentSlot + 1);
                    }

                    xSemaphoreGive(semaAqData1);

                    // Move to next slot (round-robin for fairness)
                    currentSlot = (currentSlot + 1) % NUM_OF_SLOTS;

                    vTaskDelay(1); // Yield
                }

                xSemaphoreGive(semaStorage1);
            }
            else
            {
                log_e("MQTT Broker disconnected - MQTT STATE: %d", mqttClient.state());
                flags[cloud_f] = 0;
            }
        }
        else
        {
            log_w("Cloud not connected, cannot transfer data");
        }
    }

    // Sync RTC after sending multiple chunks
    if (chunksSentCounter >= 2 * NUM_OF_SLOTS)
    {
        setRtcTime();
        log_i("Sent %d data chunks to cloud.", chunksSentCounter);
        chunksSentCounter = 0;
        vTaskDelay(1);
    }
}
