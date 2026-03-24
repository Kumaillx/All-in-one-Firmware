#include <Storage.h>
#include <rtc.h>
#include <myNVS.h>

Preferences configuration__;

/**
 * @brief init_storage initialises storage (SD card).
 * It checks whether the card is mounted or not, detects the
 * card type and size, if connected. read the required data
 * from the files already available in the SD card. Moreover,
 * this function creates the APs and config file in the memory
 * if not already avaiable before initialization. If the config
 * file is available and empty, it checks for the file name in
 *  the flash memory and start that file from start.
 *
 * @return returns true only if the storage is initialized properly.
 */
bool Storage::init_storage()
{
    log_i("Initializing SD card... ");
    mount_success = false;

    if (!SD.begin())
    {
        log_e("Card Initialization Failed ");
        return mount_success;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE)
    {
        log_e("No SD card attached ");
        return mount_success;
    }

    log_d("init_storage() -> storage.cpp -> SD Card Type: ");
    if (cardType == CARD_MMC)
        log_d("MMC ");
    else if (cardType == CARD_SD)
        log_d("SDSC ");
    else if (cardType == CARD_SDHC)
        log_d("SDHC ");
    else
    {
        log_d("UNKNOWN ");
        return mount_success;
    }

    uint64_t cardSize = SD.totalBytes() / (1024 * 1024);
    log_i("SD Card Size: %lluMB", cardSize);

    // Initialize per-slot defaults
    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
        curr_read_file[slot] = "";
        curr_read_pos[slot] = FILE_START_POS;
        resume[slot] = false;
    }

    // ========================================================================
    // CREATE SLOT DIRECTORIES IF THEY DON'T EXIST
    // ========================================================================
    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
        String slotDir = "/slot" + String(slot + 1);

        if (!SD.exists(slotDir))
        {
            if (SD.mkdir(slotDir))
            {
                log_i("Created directory: %s", slotDir.c_str());
            }
            else
            {
                log_e("Failed to create directory: %s", slotDir.c_str());
            }
        }
        else
        {
            log_d("Directory exists: %s", slotDir.c_str());
        }
    }

    // ========================================================================
    // LOAD OR CREATE PER-SLOT CONFIG FILES
    // ========================================================================
    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
        String configPath = "/slot" + String(slot + 1) + "/config.txt";

        if (SD.exists(configPath))
        {
            // Config file exists - load it
            File file = SD.open(configPath, FILE_READ);

            if (file && file.available())
            {
                log_d("Slot %d: Loading config from %s", slot + 1, configPath.c_str());

                String temp = "";
                char c = file.read();

                // Read curr_read_file
                while (c != '$' && file.available())
                {
                    temp += c;
                    c = file.read();
                }
                curr_read_file[slot] = temp;

                if (file.available())
                    file.read(); // skip newline after $

                // Read curr_read_pos
                temp = "";
                if (file.available())
                {
                    c = file.read();
                    while (c != '$' && file.available())
                    {
                        temp += c;
                        c = file.read();
                    }
                    curr_read_pos[slot] = atol(temp.c_str());
                }

                resume[slot] = (curr_read_file[slot] != "");

                log_i("Slot %d -> File: %s Pos: %lu",
                      slot + 1,
                      curr_read_file[slot].c_str(),
                      curr_read_pos[slot]);

                file.close();

                // Update NVS backup
                configuration__.begin("pointer", false);
                String nvs_key = "file-name" + String(slot);
                String nvs_file = configuration__.getString(nvs_key.c_str(), "");
                if (nvs_file != curr_read_file[slot])
                {
                    configuration__.putString(nvs_key.c_str(), curr_read_file[slot].c_str());
                }
                configuration__.end();
            }
            else
            {
                // File exists but empty - try to load from NVS
                file.close();
                log_w("Slot %d: Config file empty! Loading from NVM...", slot + 1);

                configuration__.begin("pointer", false);
                curr_read_file[slot] = configuration__.getString(("file-name" + String(slot)).c_str(), "");
                configuration__.end();

                curr_read_pos[slot] = FILE_START_POS;
                resume[slot] = (curr_read_file[slot] != "");

                if (resume[slot])
                {
                    log_d("Slot %d -> Loaded from NVM: %s", slot + 1, curr_read_file[slot].c_str());

                    // Save back to slot config
                    File file2 = SD.open(configPath, FILE_WRITE);
                    if (file2)
                    {
                        file2.print(curr_read_file[slot]);
                        file2.println("$");
                        file2.print(curr_read_pos[slot]);
                        file2.println("$");
                        file2.close();
                        log_d("Slot %d: config.txt updated from NVM", slot + 1);
                    }
                }
            }
        }
        else
        {
            // Config file does not exist - create it
            log_w("Slot %d: Config not found! Creating %s", slot + 1, configPath.c_str());

            curr_read_file[slot] = "";
            curr_read_pos[slot] = FILE_START_POS;
            resume[slot] = false;

            File file = SD.open(configPath, FILE_WRITE);
            if (file)
            {
                file.print(curr_read_file[slot]);
                file.println("$");
                file.print(curr_read_pos[slot]);
                file.println("$");
                file.close();
                log_d("Slot %d: config.txt created", slot + 1);
            }
            else
            {
                log_e("Slot %d: Failed to create config file!", slot + 1);
            }
        }
    }

    // New addition to the init function
    if (SD.exists("/APs.txt")) // Check for APs.txt on SD card
    {
        File APList = SD.open("/APs.txt", FILE_READ);
        if (APList.available())
        {
            log_i("AP storage found!");
            APList.close();
            APList_exists = true;
        }
        else
        {
            log_i("AP storage is missing. Recreating file ");
            APList_exists = false;
        }
    }
    else
    {
        log_i("AP storage not found. Creating new file ");
        APList_exists = false;
    }

    if (!APList_exists)
    {
        String APCred = String(DEFAULT_SSID) + "," + String(DEFAULT_PASSWORD);
        File APList = SD.open("/APs.txt", FILE_WRITE);
        APList.print('<');
        APList.print(APCred); // Default entry for the APlist
        APList.print('>');
        APList.close();
        APList_exists = true;
    }

    mount_success = true;
    return mount_success;
}

/**
 * @brief write_data function writes data to the storage.
 * - to separate a chunk of data from another, it encapsulates data in '<>'
 * - the first thing it performs is to check if card has free space greater than LOW_SPACE_LIMIT_MB
 *   if the space is less, delete the oldest file
 * - if this is the first time of writing data, the write data function also creates the config.txt file
 *
 * @param timenow is the current time in format YYYYMMDD
 * @param data the string of data that needs to be written in the storage
 * @return return value is true if the write operation is successful
 */
bool Storage::write_data(uint8_t slot, String timenow, String data)
{
    if (slot >= NUM_OF_SLOTS)
    {
        log_e("Invalid slot %d", slot);
        return false;
    }

    if (data == "")
        return false;

    bool write_success = false;
    if (!mount_success)
    {
        log_e("Storage mount failed or not mounted! Try again ");
        return write_success;
    }

    // Check space (check once for all slots, not per slot)
    if ((CARD_SIZE_LIMIT_MB - SD.usedBytes() / 1048576) < LOW_SPACE_LIMIT_MB)
    {
        log_w("Space low! Deleting oldest file ");
        // Delete oldest file from any slot directory
        this->remove_oldest_file("/slot1");
        if ((CARD_SIZE_LIMIT_MB - SD.usedBytes() / 1048576) < LOW_SPACE_LIMIT_MB)
            this->remove_oldest_file("/slot2");
        if ((CARD_SIZE_LIMIT_MB - SD.usedBytes() / 1048576) < LOW_SPACE_LIMIT_MB)
            this->remove_oldest_file("/slot3");
    }

    // Build slot-specific path: /slot1/20260102.txt
    String slotDir = "/slot" + String(slot + 1);
    String path = slotDir + "/" + timenow + ".txt";
    File file;

    if (!SD.exists(path))
    {
        file = SD.open(path, FILE_APPEND);
        if (!file)
        {
            log_e("Slot %d: Failed to open file for writing: %s", slot + 1, path.c_str());
            return write_success;
        }
        create_header(file);
    }
    else
    {
        file = SD.open(path, FILE_APPEND);
        if (!file)
        {
            log_e("Slot %d: Failed to open file for writing: %s", slot + 1, path.c_str());
            return write_success;
        }
    }

    String temp = "<" + data + ">";
    log_d("Chunk size: %d bytes", temp.length());
    if (temp.length() > MIN_CHUNK_SIZE_B)
    {
        if (file.println(temp))
        {
            log_d("Slot %d: File written to %s", slot + 1, path.c_str());
            write_success = true;
        }
        else
        {
            log_e("Slot %d: Write failed", slot + 1);
            file.close();
            return write_success;
        }

        // Update slot-specific config if first time (resume=false)
        if (!resume[slot])
        {
            // ❌ OLD CODE (WRONG):
            // String name = file.name();
            // curr_read_file[slot] = "/" + name;

            // ✅ NEW CODE (CORRECT):
            // file.name() returns something like "slot1/20260102.txt" (without leading /)
            // We need to ensure it has the full path: /slot1/20260102.txt
            curr_read_file[slot] = path; // Use the path variable we already built!

            // Update this slot's config file
            String configPath = slotDir + "/config.txt";
            File file2 = SD.open(configPath, FILE_WRITE);
            if (file2)
            {
                file2.print(curr_read_file[slot]);
                file2.println("$");
                file2.print(curr_read_pos[slot]);
                file2.println("$");
                file2.close();
                log_d("Slot %d: config.txt updated with file: %s", slot + 1, curr_read_file[slot].c_str());
            }
            else
            {
                log_e("Slot %d: Failed to update config.txt", slot + 1);
            }

            // Update NVS backup for this slot
            configuration__.begin("pointer", false);
            configuration__.putString(("file-name" + String(slot)).c_str(), curr_read_file[slot]);
            configuration__.end();

            resume[slot] = true;
        }
    }

    file.close();
    return write_success;
}

/**
 * @brief This function adds an access point to the SD card file APs.txt
 * as well as creates the file APs.txt if not found
 *
 * @param SSID is the name of Wifi that is to be added in the APs list
 * @param Password is the password for respective SSID
 *
 * @return true if access point is succesfully added to the file
 *
 */
bool Storage::write_AP(String SSID, String Password) // made with small edits to the write_data function
{
    bool write_success = false;
    if (mount_success)
    {
        String path = "/APs.txt";
        File file;
        if (!SD.exists(path))
        {
            file = SD.open(path, FILE_WRITE);
            if (!file)
            {
                log_e("Failed to open file for writing ");
                return write_success;
            }
        }
        else
        {
            file = SD.open(path, FILE_APPEND);
            if (!file)
            {
                log_e("Failed to open file for writing ");
                return write_success;
            }
        }
        if (file.print("<"))
        {
            file.print(SSID + ",");
            file.print(Password);
            file.println(">");
            log_d("File written ");
            write_success = true;
        }
        else
        {
            log_e("Write failed ");
        }
        file.close();
        return write_success;
    }
    else
    {
        log_e("Storage mount failed or not mounted! Try again ");
        return write_success;
    }
}

/**
 * @brief This function clears the APs.txt file and adds new credentials according to
 * the String passed to it. It is called whenever the AP list exceeds a set
 * limit or a saved SSID is being connected to with a different password.
 *
 * @param SSID is a list of SSIDs that needs to be written in the APs.txt file
 *  with a maximum capacity of 10 SSIDs
 * @param Password is the list of passwords to the corresponsing entries of SSID
 *  stored in the SSID array
 *
 * @return True if success
 */
bool Storage::rewrite_storage_APs(String SSID[10], String Password[10])
{
    bool write_success = false;
    int i = 0;
    if (mount_success)
    {
        String path = "/APs.txt";
        File file;
        if (!SD.exists(path))
        {
            file = SD.open(path, FILE_WRITE);
            if (!file)
            {
                log_e("Failed to open file for writing ");
                return write_success;
            }
        }
        else
        {
            file = SD.open(path, FILE_WRITE);
            if (!file)
            {
                log_e("Failed to open file for writing ");
                return write_success;
            }
        }
        while (!SSID[i].isEmpty() && i < 10)
        {
            if (file.print("<"))
            {
                file.print(SSID[i] + ",");
                file.print(Password[i]);
                file.print(">");
                log_d("File written %s and %s ", SSID[i].c_str(), Password[i].c_str());
                i++;
            }
            else
            {
                log_e("Write failed ");
                file.close();
                SD.remove(path);
                return false;
            }
        }
        write_success = true;
        file.close();
        return write_success;
    }
    else
    {
        log_e("Storage mount failed or not mounted! Try again ");
        return write_success;
    }
}

/**
 * @brief places CSV header on the file. This CSV header is placed on the top of
 * each file (when a file is created)
 *
 * @param File is a parameter of type File in which teh header needs to be placed
 */
void Storage::create_header(File file)
{

    file.println(FILE_HEADER);
}

String Storage::findOldestFile(const String &folderPath)
{
    File root = SD.open(folderPath);
    if (!root || !root.isDirectory())
    {
        log_e("Failed to open directory: %s", folderPath.c_str());
        return "";
    }

    String oldestFile = "";
    while (File file = root.openNextFile())
    {
        if (!file.isDirectory())
        {
            String fileName = file.name();
            file.close();
            if (fileName.endsWith(".txt"))
            {
                fileName = fileName.substring(0, fileName.length() - 4);
                if (oldestFile == "" || fileName < oldestFile)
                {
                    oldestFile = fileName;
                }
            }
        }
    }
    return oldestFile;
}

/**
 * @brief remove_oldest_file() function removes the oldest file based on filename.
 * - it converts the filenames to integers and loops over all files to check the smallest.
 * - then the smallest files is removed.
 */
void Storage::remove_oldest_file(const String &folderPath)
{
    String oldestFile = findOldestFile(folderPath);
    if (oldestFile.isEmpty())
    {
        log_w("No files found to remove in %s", folderPath.c_str());
        return;
    }

    String filePath = folderPath + oldestFile + ".txt";
    if (SD.remove(filePath))
    {
        log_d("Successfully removed file: %s", filePath.c_str());
    }
    else
    {
        log_e("Failed to remove file: %s", filePath.c_str());
    }
}

/**
 * @brief read_data() function reads the data starting with < character from a specific slot
 * - it returns data by removing encapsulation "<>"
 * - it also updates the size of current chunk of data.
 * - to check the start of data, it checks the next characters for '<'. if this is not found
 *   then it returns false (failure)
 * - in case the file has ended before detection of start character, that means there is no more data to be read in file.
 * - if data start is not at the curr_read_pos, then it also updates the curr_read_pos variable to start of data.
 * - if the starting character is found, then it loops over the data to check the end character '>'
 *   till the max_chunk_size_b limit
 *
 * @param slot - slot index (0 to NUM_OF_SLOTS-1)
 * @return returns the string without encapsulation "<>"
 */
String Storage::read_data(uint8_t slot)
{
    if (slot >= NUM_OF_SLOTS)
    {
        log_e("Invalid slot %d", slot);
        return "";
    }

    log_d("Slot %d: Reading file: %s at pos: %lu", slot + 1, curr_read_file[slot].c_str(), curr_read_pos[slot]);

    File file = SD.open(curr_read_file[slot], FILE_READ);
    String toread = "";
    bool invalid_data = 0;

    if (!file)
    {
        log_e("Slot %d: Failed to open file for reading", slot + 1);
        log_e("Slot %d: Current Read File %s does not exist!", slot + 1, curr_read_file[slot].c_str());

        // Extract date from current file for comparison
        String currFileDate = curr_read_file[slot];
        int lastSlash = currFileDate.lastIndexOf('/');
        if (lastSlash >= 0)
        {
            currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9); // Get YYYYMMDD
        }

        if (getTime2().toDouble() > currFileDate.toDouble())
        {
            curr_read_file[slot] = next_file(curr_read_file[slot]);

            // Skip non-existent files
            while (!SD.exists(curr_read_file[slot]) && getTime2().toDouble() > currFileDate.toDouble())
            {
                log_d("Slot %d: Moving onto next file, %s does not exist", slot + 1, curr_read_file[slot].c_str());
                curr_read_file[slot] = next_file(curr_read_file[slot]);

                // Update date for comparison
                currFileDate = curr_read_file[slot];
                lastSlash = currFileDate.lastIndexOf('/');
                if (lastSlash >= 0)
                {
                    currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9);
                }

                vTaskDelay(1); // Yield during loop
            }

            curr_read_pos[slot] = FILE_START_POS;

            // Update slot-specific config file
            String slotDir = "/slot" + String(slot + 1);
            String configPath = slotDir + "/config.txt";
            File conf_file = SD.open(configPath, FILE_WRITE);
            if (!conf_file)
            {
                log_e("Slot %d: Failed to open file for saving config", slot + 1);
                return "";
            }
            conf_file.print(curr_read_file[slot]);
            conf_file.println("$");
            conf_file.print(String(curr_read_pos[slot]));
            conf_file.println("$");
            conf_file.close();
            log_d("Slot %d: Config updated to file: %s, pos: %lu", slot + 1, curr_read_file[slot].c_str(), curr_read_pos[slot]);
        }
        return "";
    }

    // Check if file has enough data
    file.seek(file.size() - 3);
    char temp = file.read();

    if ((file.size() < FILE_START_POS + MIN_CHUNK_SIZE_B) ||
        (file.size() - curr_read_pos[slot] < MIN_CHUNK_SIZE_B) ||
        (temp != '>' && file.size() - curr_read_pos[slot] < MAX_CHUNK_SIZE_B))
    {
        // Extract date from current file
        String currFileDate = curr_read_file[slot];
        int lastSlash = currFileDate.lastIndexOf('/');
        if (lastSlash >= 0)
        {
            currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9);
        }

        if (getTime2().toDouble() > currFileDate.toDouble())
        {
            curr_read_file[slot] = next_file(curr_read_file[slot]);

            while (!SD.exists(curr_read_file[slot]) && getTime2().toDouble() > currFileDate.toDouble())
            {
                log_d("Slot %d: Moving onto next file, %s does not exist", slot + 1, curr_read_file[slot].c_str());
                curr_read_file[slot] = next_file(curr_read_file[slot]);

                currFileDate = curr_read_file[slot];
                lastSlash = currFileDate.lastIndexOf('/');
                if (lastSlash >= 0)
                {
                    currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9);
                }

                vTaskDelay(1);
            }

            curr_read_pos[slot] = FILE_START_POS;

            String slotDir = "/slot" + String(slot + 1);
            String configPath = slotDir + "/config.txt";
            File conf_file = SD.open(configPath, FILE_WRITE);
            if (!conf_file)
            {
                log_e("Slot %d: Failed to open file for saving config", slot + 1);
                file.close();
                return "";
            }
            conf_file.print(curr_read_file[slot]);
            conf_file.println("$");
            conf_file.print(String(curr_read_pos[slot]));
            conf_file.println("$");
            conf_file.close();
            log_d("Slot %d: Config updated to file: %s, pos: %lu", slot + 1, curr_read_file[slot].c_str(), curr_read_pos[slot]);
            file.close();
            return "";
        }
        else
        {
            log_e("Slot %d: No new Data in the file %s", slot + 1, curr_read_file[slot].c_str());
            file.close();
            return "";
        }
    }

    curr_chunk_size[slot] = 0;
    file.seek(curr_read_pos[slot]);

    int i = 0;
    // Loop checks for the start of valid data frame
    while (true)
    {
        if (file.available())
        {
            char c = file.read();
            if (c == '<') // Valid data string started
            {
                curr_chunk_size[slot] = 1;
                curr_read_pos[slot] += i;
                break;
            }
            i++;
        }
        else // File has no data, failsafe
        {
            log_e("Slot %d: Changing the position to previous value", slot + 1);
            curr_read_pos[slot] -= MAX_CHUNK_SIZE_B;
            file.seek(curr_read_pos[slot]);
        }

        if (i > 100)
        { // Safety check
            vTaskDelay(1);
            i = 0;
        }
    }

    // Loop reads the data string
    while (true)
    {
        if (file.available())
        {
            char c = file.read();
            curr_chunk_size[slot]++;

            if (curr_chunk_size[slot] > MAX_CHUNK_SIZE_B)
            {
                curr_chunk_size[slot] = 0;
                toread = "";

                // Solution to Issue # 11
                String currWriteFile = "/slot" + String(slot + 1) + "/" + getTime2() + ".txt";
                if (curr_read_file[slot] != currWriteFile)
                {
                    int j = 0;
                    file.seek(curr_read_pos[slot] + 1);
                    while (file.available())
                    {
                        char c = file.read();
                        if (c == '<')
                        {
                            curr_chunk_size[slot] = 1;
                            curr_read_pos[slot] += j;
                            break;
                        }
                        j++;
                        if (j % 50 == 0)
                        {
                            vTaskDelay(1); // Yield periodically
                        }
                    }
                }

                if (curr_chunk_size[slot] == 0)
                {
                    log_i("Slot %d: Valid data not found for reading!", slot + 1);
                    file.close();
                    return "";
                }
            }
            else if (c == '<')
            {
                curr_read_pos[slot] += curr_chunk_size[slot];
                curr_chunk_size[slot] = 1;
                toread = "";
            }
            else if (c != '>')
            {
                toread += c;
            }
            else
            {
                if (curr_chunk_size[slot] < MIN_CHUNK_SIZE_B)
                {
                    curr_read_pos[slot] += curr_chunk_size[slot] + 1;
                    invalid_data = 1;
                }
                break; // '>' found! terminating iterations
            }
        }
        else // End character '>' not found till end of file
        {
            log_e("Slot %d: File ended before data read completed. Data corrupt!", slot + 1);
            curr_chunk_size[slot] = 0;
            file.close();
            return "";
        }
    }

    file.close();

    if (invalid_data)
    {
        invalid_data = 0;

        String slotDir = "/slot" + String(slot + 1);
        String configPath = slotDir + "/config.txt";
        File conf_file = SD.open(configPath, FILE_WRITE);
        if (!conf_file)
        {
            log_e("Slot %d: Failed to open file for saving config", slot + 1);
            return "";
        }
        conf_file.print(curr_read_file[slot]);
        conf_file.println("$");
        conf_file.print(String(curr_read_pos[slot]));
        conf_file.println("$");
        conf_file.close();
        log_d("Slot %d: Config updated to position: %lu", slot + 1, curr_read_pos[slot]);
    }
    else
    {
        log_d("Slot %d: Parsed successfully, chunk size: %d", slot + 1, curr_chunk_size[slot]);
    }

    return toread;
}
// ============================================================================
// UPDATED update_config() FUNCTION
// ============================================================================

void Storage::update_config()
{
    log_i("Updating config for all %d slots...", NUM_OF_SLOTS);

    // Loop over all slots
    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
        String slotDir = "/slot" + String(slot + 1);

        // Extract date from current file for comparison
        String currFileDate = curr_read_file[slot];
        int lastSlash = currFileDate.lastIndexOf('/');
        if (lastSlash >= 0)
        {
            currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9); // Get YYYYMMDD
        }

        // Update the curr_read_file for this slot if time has passed
        if (getTime2().toDouble() > currFileDate.toDouble())
        {
            curr_read_file[slot] = next_file(curr_read_file[slot]);

            while (!SD.exists(curr_read_file[slot]))
            {
                // Update date for next iteration
                currFileDate = curr_read_file[slot];
                lastSlash = currFileDate.lastIndexOf('/');
                if (lastSlash >= 0)
                {
                    currFileDate = currFileDate.substring(lastSlash + 1, lastSlash + 9);
                }

                // Check if we've gone beyond current date
                if (getTime2().toDouble() <= currFileDate.toDouble())
                {
                    log_d("Slot %d: Reached current date, stopping file search", slot + 1);
                    break;
                }

                log_d("Slot %d: Moving onto next file, %s does not exist",
                      slot + 1, curr_read_file[slot].c_str());
                curr_read_file[slot] = next_file(curr_read_file[slot]);

                vTaskDelay(1); // Yield during loop
            }

            // Update NVS for this slot
            configuration__.begin("pointer", false);
            String nvs_key = "file-name" + String(slot);
            String nvs_file = configuration__.getString(nvs_key.c_str(), "");
            if (nvs_file != curr_read_file[slot])
            {
                configuration__.putString(nvs_key.c_str(), curr_read_file[slot].c_str());
                log_d("Slot %d: NVS updated with file: %s", slot + 1, curr_read_file[slot].c_str());
            }
            configuration__.end();
        }

        // Reset the read position for this slot
        curr_read_pos[slot] = FILE_START_POS;

        // Save to slot-specific config file
        String configPath = slotDir + "/config.txt";
        File config = SD.open(configPath, FILE_WRITE);
        if (!config)
        {
            log_e("Slot %d: Failed to open %s for saving config", slot + 1, configPath.c_str());
            continue; // Move to next slot
        }

        config.print(curr_read_file[slot]);
        config.println("$");
        config.print(String(curr_read_pos[slot]));
        config.println("$");
        config.close();

        log_i("Slot %d: Config updated - File: %s, Pos: %lu",
              slot + 1, curr_read_file[slot].c_str(), curr_read_pos[slot]);
    }

    log_i("All slot configs updated successfully");
}

/**
 * @brief This funtion cycles through the APs stored in the SD card and stores them in
 * a String array. Since the String array parameter degenerates to a pointer it
 * changes the original referenced parameter and thus returns the list of
 * credentials in two arrays. The maximum limit is 10 entries.
 *
 * @return Returns nothing but the SSID_List and Password_List (global arrays) are
 * updated as a reference pointer to them
 */
void Storage::return_APList(String SSID_List[10], String Password_List[10])
{
    int i = 0;
    while (!SSID_List[i].isEmpty() || !Password_List[i].isEmpty())
    {
        SSID_List[i].clear();
        Password_List[i].clear();
        i++;
    }
    i = 0;
    if (mount_success && APList_exists)
    {
        char temp = '\0';
        int16_t max_iter_limit = 0;
        int16_t i = 0;
        bool read_st = false;
        bool SSID_rd = false;
        bool Password_rd = false;
        File AP = SD.open("/APs.txt", FILE_READ);

        while (1)
        {
            if (AP.available() && !read_st)
            {
                temp = AP.read();
                if (temp == '<' && max_iter_limit < 10)
                {
                    log_d("Start of frame found ");
                    temp = AP.read();
                    max_iter_limit = 0;
                    read_st = true;
                }

                if (max_iter_limit > 10) // could not find the start of frame
                {
                    log_e("Start of frame missing ");
                    return;
                }
                else
                {
                    max_iter_limit++;
                }
            }

            if (AP.available() && read_st)
            {
                while (temp != ',' && max_iter_limit < 40)
                {
                    curr_SSID += temp;
                    temp = AP.read();
                    max_iter_limit++;
                    if (max_iter_limit >= 40) // username greater than assigned limit. Maybe define global variables??
                    {
                        log_e("Invalid username ");
                        break;
                    }
                    if (temp == ',')
                    {
                        SSID_rd = true;
                        log_d("%s ", curr_SSID.c_str());
                    }
                }
                temp = AP.read();
                while (temp != '>' && max_iter_limit < 40)
                {
                    curr_Password += temp;
                    temp = AP.read();
                    max_iter_limit++;
                    if (max_iter_limit >= 40)
                    {
                        log_e("Invalid password ");
                        break;
                    }
                    if (temp == '>')
                    {
                        Password_rd = true;
                        log_d("%s ", curr_Password.c_str());
                    }
                }
            }
            if (SSID_rd && Password_rd) // One frame of credentials has been read
            {
                if (i < 10)
                {
                    // Trim any leading/trailing whitespace or CR/LF from parsed credentials
                    curr_SSID.trim();
                    curr_Password.trim();

                    SSID_List[i] = curr_SSID;
                    Password_List[i] = curr_Password;
                    i++;
                    log_d("New AP returned ");
                }
                else
                {
                    log_w("Maximum number of APs stored in SD card. Please free up space ");
                }
                curr_SSID = "";
                curr_Password = "";
                Password_rd = false;
                SSID_rd = false;
                read_st = false;
                max_iter_limit = 0;
            }
            if (!AP.available())
            {
                log_d("End of data reached ");
                break;
            }
        }
        AP.close();
        i = 0;
        return;
    }
}

/**
 * @brief mark_data updates the curr_read_pos in slot-specific config.txt
 * - if the remaining data in file is less than MIN_CHUNK_SIZE_B it also updates the filename
 *
 * @param slot - slot index (0 to NUM_OF_SLOTS-1)
 * @param timenow is a string in the format YYYYMMDD. This string the date of the
 * file whose current pointer is to be marked
 */
void Storage::mark_data(uint8_t slot, String timenow)
{
    if (slot >= NUM_OF_SLOTS)
    {
        log_e("Invalid slot %d", slot);
        return;
    }

    String slotDir = "/slot" + String(slot + 1);
    String curr_write_file = slotDir + "/" + timenow + ".txt";

    log_d("Slot %d: Marking current chunk of data", slot + 1);

    File file = SD.open(curr_read_file[slot], FILE_READ);
    if (!file)
    {
        log_e("Slot %d: Failed to open file for marking", slot + 1);
        return;
    }

    file.seek(curr_read_pos[slot]);
    char c = file.read();

    if (c == '<')
    {
        log_i("Slot %d: Size: %d, Pos: %lu, Chunk: %d, Write: %s, Read: %s",
              slot + 1, file.size(), curr_read_pos[slot], curr_chunk_size[slot],
              curr_write_file.c_str(), curr_read_file[slot].c_str());

        // Check if this is the end of file
        if ((file.size() - (curr_read_pos[slot] + curr_chunk_size[slot]) < MIN_CHUNK_SIZE_B) &&
            (curr_write_file != curr_read_file[slot]))
        {
            file.close();
            log_d("Slot %d: File completed! Moving to next file", slot + 1);
            log_d("  Current Write File: %s", curr_write_file.c_str());
            log_d("  Current Read File: %s", curr_read_file[slot].c_str());

            String next_filename = next_file(curr_read_file[slot]);

            // Skip non-existent files
            while (!SD.exists(next_filename) && (next_filename < curr_write_file))
            {
                log_d("Slot %d: Skipping non-existent file: %s", slot + 1, next_filename.c_str());
                next_filename = next_file(next_filename);
                vTaskDelay(1); // Yield during loop
            }

            if (SD.exists(next_filename))
            {
                curr_read_pos[slot] = FILE_START_POS;
                curr_read_file[slot] = next_filename;

                // Update NVS backup
                configuration__.begin("pointer", false);
                configuration__.putString(("file-name" + String(slot)).c_str(), curr_read_file[slot]);
                configuration__.end();

                myNVS::write::lastfilesettime(getUnixTime());

                log_i("Slot %d: Moved to next file: %s", slot + 1, curr_read_file[slot].c_str());
            }
            else
            {
                log_w("Slot %d: Next file does not exist yet: %s", slot + 1, next_filename.c_str());
            }
        }
        else
        {
            // Update position within current file
            curr_read_pos[slot] += curr_chunk_size[slot] + 1;
            file.close();
            log_d("Slot %d: Updated position to %lu", slot + 1, curr_read_pos[slot]);
        }

        // Save to slot-specific config file
        String configPath = slotDir + "/config.txt";
        file = SD.open(configPath, FILE_WRITE);
        if (!file)
        {
            log_e("Slot %d: Failed to open config file for saving", slot + 1);
            return;
        }

        file.print(curr_read_file[slot]);
        file.println("$");
        file.print(String(curr_read_pos[slot]));
        file.println("$");
        file.close();

        log_d("Slot %d: Data marked and config updated!", slot + 1);
    }
    else
    {
        file.close();
        log_e("Slot %d: Valid data not found for marking (expected '<', got '%c')", slot + 1, c);
    }
}

/**
 * @brief next_file gives the complete path of next file
 *
 * @param curr_file is a string (complete path) of the current file being written or read
 * @return The string containing the path of next file
 */
String Storage::next_file(String curr_file)
{
    // Extract slot directory if present (e.g., "/slot1/20260102.txt")
    String slotPrefix = "";
    int slashPos = curr_file.indexOf('/', 1); // Find second slash

    if (slashPos > 0)
    {
        // Has slot directory: /slot1/20260102.txt
        slotPrefix = curr_file.substring(0, slashPos); // "/slot1"
        curr_file = curr_file.substring(slashPos);     // "/20260102.txt"
    }

    // Extract date from filename (now works for both /20260102.txt and after slot removal)
    String syear = curr_file.substring(1, 5);
    String smonth = curr_file.substring(5, 7);
    String sday = curr_file.substring(7, 9);

    int iyear = syear.toInt();
    int imonth = smonth.toInt();
    int iday = sday.toInt();

    String next_file = getNextDay(iyear, imonth, iday);

    // Rebuild with slot prefix if it existed
    if (slotPrefix != "")
    {
        next_file = slotPrefix + "/" + next_file + ".txt";
    }
    else
    {
        next_file = "/" + next_file + ".txt";
    }

    return next_file;
}

/**
 * @brief get_unsent_data returns the total unsent data in bytes across all slots.
 * It reads from each slot's current file and position till the present location
 * if wifi is available.
 *
 * @param timenow is the current date in format YYYYMMDD
 *
 * @return total number of unsent bytes across all slots as long datatype
 */
long Storage::get_unsent_data(String timenow)
{
    long total_unsent_bytes = 0;

    for (uint8_t slot = 0; slot < NUM_OF_SLOTS; slot++)
    {
        String slotDir = "/slot" + String(slot + 1);
        String configPath = slotDir + "/config.txt";
        String curr_write_file = slotDir + "/" + timenow + ".txt";

        if (!SD.exists(configPath))
        {

            log_w("Slot %d: Config file not found, skipping", slot + 1);
            vTaskDelay(1); // ✅ ADD THIS
            continue;
        }

        File file = SD.open(configPath, FILE_READ);
        if (!file)
        {
            log_e("Slot %d: Failed to open config file", slot + 1);
            vTaskDelay(1); // ✅ ADD THIS
            continue;
        }

        // Read filename and file position from slot config
        String filename = "";
        long filepos = FILE_START_POS;

        {
            char c = file.read();
            String temp = "";

            // Read filename
            while (c != '$' && file.available())
            {
                temp += c;
                c = file.read();
            }
            filename = temp; // This should be the FULL path like /slot1/20260102.txt

            if (file.available())
                file.read(); // skip newline after $

            // Read file position
            temp = "";
            if (file.available())
            {
                c = file.read();
                while (c != '$' && file.available())
                {
                    temp += c;
                    c = file.read();
                }
                filepos = atol(temp.c_str());
            }
        }
        file.close();

        // ✅ ADD THIS: Ensure filename has slot prefix if missing
        if (filename != "" && !filename.startsWith(slotDir))
        {
            // Config has old format without slot prefix, fix it
            if (filename.startsWith("/"))
            {
                filename = slotDir + filename; // /20260102.txt -> /slot1/20260102.txt
            }
            else
            {
                filename = slotDir + "/" + filename;
            }
        }

        if (filename == "")
        {
            log_d("Slot %d: No data to send", slot + 1);
            vTaskDelay(1); // ✅ ADD THIS
            continue;
        }

        long slot_unsent_bytes = 0;

        if (SD.exists(filename))
        {
            file = SD.open(filename);
            if (file)
            {
                slot_unsent_bytes = file.size() - filepos;
                file.close();
                log_d("Slot %d: Current file %s has %ld unsent bytes",
                      slot + 1, filename.c_str(), slot_unsent_bytes);
            }
        }

        vTaskDelay(1); // ✅ ADD THIS - yield before loop

        // Add all subsequent files until current write file
        String nextFilename = filename;

        while (nextFilename < curr_write_file)
        {
            String prevFilename = nextFilename; // ✅ ADD THIS
            nextFilename = next_file(nextFilename);

            // ✅ ADD SAFETY CHECK
            if (nextFilename == prevFilename || nextFilename == "")
            {
                break;
            }

            if (SD.exists(nextFilename))
            {
                file = SD.open(nextFilename);
                if (file)
                {
                    long fileBytes = file.size() - FILE_START_POS;
                    slot_unsent_bytes += fileBytes;
                    file.close();
                    log_d("Slot %d: File %s has %ld bytes",
                          slot + 1, nextFilename.c_str(), fileBytes);
                }
            }

            vTaskDelay(1);
        }

        total_unsent_bytes += slot_unsent_bytes;

        vTaskDelay(1); // ✅ ADD THIS - yield between slots
    }

    return total_unsent_bytes;
}

/**
 * @brief Handles live data broadcast failures by storing data to backup files
 * Each slot has its own backup file that alternates every 10 minutes
 *
 * File structure:
 * /liveData/slot1/1.txt (first 10 min)
 * /liveData/slot1/2.txt (next 10 min)
 * /liveData/slot2/1.txt
 * /liveData/slot2/2.txt
 * /liveData/slot3/1.txt
 * /liveData/slot3/2.txt
 *
 * @param slot - slot index (0 to NUM_OF_SLOTS-1)
 * @param data - live data string to be stored
 * @return true if data written successfully, false otherwise
 */
bool Storage::handleLiveDataFailure(uint8_t slot, String data)
{
    if (slot >= NUM_OF_SLOTS)
    {
        log_e("Invalid slot %d", slot);
        return false;
    }

    // ✅ OPTION 3: Use initialization list with proper size (if C++11)
    static int fileIndex[NUM_OF_SLOTS] = {1};             // First element 1, rest 0 (then fix with loop)
    static bool isLogging[NUM_OF_SLOTS] = {};             // All false
    static unsigned long lastFailTime[NUM_OF_SLOTS] = {}; // All 0
    static bool initialized = false;

    if (!initialized)
    {
        for (uint8_t i = 0; i < NUM_OF_SLOTS; i++)
        {
            fileIndex[i] = 1; // Ensure all start at 1
        }
        initialized = true;
    }

    unsigned long currentTime = millis();

    // Initialize timing for this slot if first failure
    if (!isLogging[slot])
    {
        lastFailTime[slot] = currentTime;
        isLogging[slot] = true;
    }

    // Switch files every 10 minutes for this slot
    if (currentTime - lastFailTime[slot] >= LIVE_DATA_BACKUP_TIME * 60 * MS_IN_SECOND)
    {
        // Toggle between file 1 and 2 for this slot
        fileIndex[slot] = (fileIndex[slot] == 1) ? 2 : 1;

        String slotDir = "/liveData/slot" + String(slot + 1);
        String filePath = slotDir + "/" + String(fileIndex[slot]) + ".txt";

        // Delete the new current file if it exists (to start fresh)
        if (SD.exists(filePath))
        {
            if (SD.remove(filePath))
            {
                log_i("Slot %d: Deleted old backup file %s", slot + 1, filePath.c_str());
            }
        }

        lastFailTime[slot] = currentTime; // Reset the timer
        log_i("Slot %d: Switched to backup file %d", slot + 1, fileIndex[slot]);
    }

    // Ensure the slot-specific liveData directory exists
    String slotDir = "/liveData/slot" + String(slot + 1);
    if (!SD.exists(slotDir))
    {
        // Create parent directory first if needed
        if (!SD.exists("/liveData"))
        {
            if (!SD.mkdir("/liveData"))
            {
                log_e("Failed to create /liveData directory");
                return false;
            }
        }

        // Create slot-specific directory
        if (!SD.mkdir(slotDir))
        {
            log_e("Slot %d: Failed to create directory %s", slot + 1, slotDir.c_str());
            return false;
        }
        log_i("Slot %d: Created backup directory %s", slot + 1, slotDir.c_str());
    }

    String filePath = slotDir + "/" + String(fileIndex[slot]) + ".txt";

    File file;
    if (!SD.exists(filePath))
    {
        // Create file fresh with WRITE mode
        file = SD.open(filePath, FILE_WRITE);
        if (!file)
        {
            log_e("Slot %d: Failed to create %s", slot + 1, filePath.c_str());
            return false;
        }
        log_i("Slot %d: Creating new backup file with header", slot + 1);
        this->create_header(file);
        file.close();
    }

    // Reopen in append mode for normal writing
    file = SD.open(filePath, FILE_APPEND);
    if (!file)
    {
        log_e("Slot %d: Failed to open %s for append", slot + 1, filePath.c_str());
        return false;
    }

    // Write the data
    String temp = "<" + data + ">";
    bool writeSuccess = false;

    if (temp.length() > MIN_CHUNK_SIZE_B_FOR_LIVE)
    {
        if (file.println(temp))
        {
            log_d("Slot %d: Backup data written to %s", slot + 1, filePath.c_str());
            // log_v("  Data: %s", data.c_str());
            writeSuccess = true;
        }
        else
        {
            log_e("Slot %d: Backup write failed", slot + 1);
        }
    }
    else
    {
        log_w("Slot %d: Data too small for backup (%d bytes)", slot + 1, temp.length());
    }

    file.close();
    return writeSuccess;
}
Storage storage; // storage object created for storing data