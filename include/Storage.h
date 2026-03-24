#ifndef __STORAGE_H__
#define __STORAGE_H__
#include <Arduino.h> //for using String type
#include <SD.h>
#include "Preferences.h"
#include "config.h"

// TODO: create formula using NUM_OF_SLOTS instead
#if NUM_OF_SLOTS == 2
#define FILE_START_POS 94 // depends on size of header placed at start of file.
#define MIN_CHUNK_SIZE_B 132
#define MIN_CHUNK_SIZE_B_FOR_LIVE 87
#define MAX_CHUNK_SIZE_B 222
#define FILE_HEADER "Time,ID,V1,I1,P1,PF1,THDV1,THDI1,V2,I2,P2,PF2,THDV2,THDI2,V3,I3,P3,PF3,THDV3,THDI3,F,TAP,EP,EN"

#elif NUM_OF_SLOTS == 3
#define FILE_START_POS 94 // depends on size of header placed at start of file.
#define MIN_CHUNK_SIZE_B 132
#define MIN_CHUNK_SIZE_B_FOR_LIVE 87
#define MAX_CHUNK_SIZE_B 222
#define FILE_HEADER "Time,ID,V1,I1,P1,PF1,THDV1,THDI1,V2,I2,P2,PF2,THDV2,THDI2,V3,I3,P3,PF3,THDV3,THDI3,F,TAP,EP,EN"

#else
#define FILE_START_POS 94 // depends on size of header placed at start of file.
#define MIN_CHUNK_SIZE_B 132
#define MIN_CHUNK_SIZE_B_FOR_LIVE 87
#define MAX_CHUNK_SIZE_B 222
#define FILE_HEADER "Time,ID,V1,I1,P1,PF1,THDV1,THDI1,V2,I2,P2,PF2,THDV2,THDI2,V3,I3,P3,PF3,THDV3,THDI3,F,TAP,EP,EN"

#endif

#define CARD_SIZE_LIMIT_MB 30000
#define LOW_SPACE_LIMIT_MB 1024
#define FAIL_LIMIT 3

extern Preferences configuration__;

/*
 * The fomrmat for file name is: YYYYMMDD.txt this should be strictly followed
 */
// WARNING: before calling mark_data, read data should be called. Also, the read data function should
//          be checked for validity (comparing with "") before calling mark_data.

class Storage
{
private:
    bool resume[NUM_OF_SLOTS];

    bool mount_success;
    void remove_oldest_file(const String &folderPath);
    String next_file(String);
    void create_header(File);
    bool APList_exists;
    String curr_SSID;
    String curr_Password;

public:
    long curr_read_pos[NUM_OF_SLOTS];
    int curr_chunk_size[NUM_OF_SLOTS];
    bool init_storage();                                            // Small addition added to init to check for AP list as well.
                                                                    // bool write_data(uint8_t slot, String timenow, String data);
    bool write_AP(String SSID, String Password);                    // Add a new AP to the list on the SD card
    bool rewrite_storage_APs(String SSID[10], String Password[10]); // Erase and rewrite the storage using the String array as paremeters
    String read_data(uint8_t slot);
    bool write_data(uint8_t slot, String timenow, String data);
    void return_APList(String SSID[10], String Password[10]); // Return String arrays for SSIDs and Passwords
    void mark_data(uint8_t slot, String timenow);

    long get_unsent_data(String timenow); // return unsent data in MBs
    void update_config();                 // update config with next file and start pos
    String curr_read_file[NUM_OF_SLOTS];

    bool handleLiveDataFailure(uint8_t slot, String data);
    String findOldestFile(const String &folderPath);
};

extern Storage storage;

#endif