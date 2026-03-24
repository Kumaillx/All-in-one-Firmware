#include "Restart.h"

void incrementRestartCount()
{
    int prevRestartCount;
    uint32_t prevRestartTime;
    myNVS::read::restartData(prevRestartCount, prevRestartTime);
    uint32_t currRestartTime = (uint32_t)(time(nullptr));

    log_i("Previous Restart Count: %d", prevRestartCount);
    log_i("Previous Restart Time: %zu", prevRestartTime);
    log_i("Current Restart Time: %zu", currRestartTime);

    if (prevRestartTime == 0 || currRestartTime - prevRestartTime > RESTART_THRESH_SECS)
    {
        myNVS::write::restartData(0, currRestartTime);
    }
    else
    {
        myNVS::write::restartData(++prevRestartCount, prevRestartTime);
    }
}

bool reachedRestartLimit()
{
    int restartCount;
    uint32_t restartTime;
    myNVS::read::restartData(restartCount, restartTime);

    if (restartCount > RESTART_COUNT_LIMIT)
    {
        return true;
    }

    return false;
}

void resetRestartCount()
{
    myNVS::write::restartData(0, (uint32_t)(time(nullptr)));
}

void onRestart()
{
    incrementRestartCount();

    if (flags[sd_f] && flags[rtc_f] && wf.check_connection() && dataClient.connected() && reachedRestartLimit())
    {
        log_d("Restarted %d times in %d minutes...", RESTART_COUNT_LIMIT, RESTART_THRESH_SECS / 60);

        bool allFilesUploaded = true;

        // Send current files and config for ALL slots
        for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
        {
            String currFile = storage.curr_read_file[slot];
            
            if (currFile == "" || !SD.exists(currFile))
            {
                log_w("Slot %d: No valid file to upload, skipping", slot + 1);
                continue;
            }

            // Build push topic for current file
            // Remove .txt extension: /slot1/20260102.txt -> /slot1/20260102
            String currFilePushTopic = RESTART_PUSH_TOPIC + sensorID + 
                                      currFile.substring(0, currFile.length() - 4);

            // Build config file path and topic
            String slotDir = "/slot" + String(slot + 1);
            String configFile = slotDir + "/config.txt";
            String configPushTopic = RESTART_PUSH_TOPIC + sensorID + 
                                    "/slot" + String(slot + 1) + "/config";

            log_i("Slot %d: Uploading current file and config...", slot + 1);

            // Upload current data file and config
            if (sendFile(currFile.c_str(), currFilePushTopic.c_str()) && 
                sendFile(configFile.c_str(), configPushTopic.c_str()))
            {
                log_i("Slot %d: Files uploaded successfully", slot + 1);

                // Rename current file to avoid re-processing
                uint8_t _num = 1;
                String newCurrFile = currFile.substring(0, currFile.length() - 4) + 
                                    "_" + _num + ".txt";
                
                while (SD.exists(newCurrFile))
                {
                    _num++;
                    newCurrFile = currFile.substring(0, currFile.length() - 4) + 
                                 "_" + _num + ".txt";
                    
                    if (_num > 99) // Safety check
                    {
                        log_e("Slot %d: Too many renamed files!", slot + 1);
                        break;
                    }
                }

                if (SD.rename(currFile, newCurrFile))
                {
                    log_i("Slot %d: Renamed file %s to %s", 
                          slot + 1, currFile.c_str(), newCurrFile.c_str());
                }
                else
                {
                    log_e("Slot %d: Failed to rename %s", slot + 1, currFile.c_str());
                    allFilesUploaded = false;
                }

                // Delete slot-specific config file
                if (SD.remove(configFile))
                {
                    log_i("Slot %d: Deleted config.txt", slot + 1);
                }
                else
                {
                    log_e("Slot %d: Failed to delete config.txt", slot + 1);
                }
            }
            else
            {
                log_e("Slot %d: Failed to upload files", slot + 1);
                allFilesUploaded = false;
            }

            vTaskDelay(100); // Small delay between slots
        }

        // If all files uploaded successfully, recreate configs
        if (allFilesUploaded)
        {
            log_i("All slot files uploaded. Creating new configs...");
            storage.update_config();
            resetRestartCount();
        }
        else
        {
            log_w("Some slot files failed to upload. Not resetting restart count.");
        }
    }
}
