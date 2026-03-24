#include "myNVS.h"

bool Factors_Already_Exist = true;
bool Labels_Already_Exist = true;
bool Filename_Already_Exist = true;

void myNVS::read::slotLabels(String slot_labels[NUM_OF_SLOTS])
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SLOT_LABELS);

    for (int i = 0; i < NUM_OF_SLOTS; i++)
    { 
        char key[BUFF_LEN_NVS_KEY];
        itoa(i, key, 10);

        if (!preferences.isKey(key))
        {
            preferences.putString(key, "");
            Labels_Already_Exist = false;
        }

        slot_labels[i] = preferences.getString(key, "");
    }

    preferences.end();
}

void myNVS::write::slotLabels(const String slot_labels[NUM_OF_SLOTS])
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SLOT_LABELS);

    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
        char key[BUFF_LEN_NVS_KEY];
        itoa(i, key, 10);
        preferences.putString(key, slot_labels[i]);
    }

    preferences.end();
}

void myNVS::read::slotFactors(float slot_factors[NUM_OF_SLOTS])
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SLOT_FACTORS);
    

    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
        char key[BUFF_LEN_NVS_KEY];
        itoa(i, key, 10);

        if (!preferences.isKey(key))
        {
            preferences.putFloat(key, 1.0f);
            Factors_Already_Exist = false;
        }

        slot_factors[i] = preferences.getFloat(key, 1.0f);
    }

    preferences.end();
}

void myNVS::write::slotFactors(const float slot_factors[NUM_OF_SLOTS])
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SLOT_FACTORS);

    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
        char key[BUFF_LEN_NVS_KEY];
        itoa(i, key, 10);

        preferences.putFloat(key, slot_factors[i]);
    }

    preferences.end();
}

void myNVS::read::restartData(int &restart_count, uint32_t &restart_time)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_RESTART_INFO);

    if (!preferences.isKey(NVS_KEY_RESTART_COUNT))
    {
        preferences.putInt(NVS_KEY_RESTART_COUNT, 0);
    }

    if (!preferences.isKey(NVS_KEY_RESTART_TIME))
    {
        preferences.putUInt(NVS_KEY_RESTART_TIME, 0);
    }

    restart_count = preferences.getInt(NVS_KEY_RESTART_COUNT, 0);
    restart_time = preferences.getUInt(NVS_KEY_RESTART_TIME, 0);
    preferences.end();
}

void myNVS::write::restartData(int restart_count, uint32_t restart_time)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_RESTART_INFO);
    preferences.putInt(NVS_KEY_RESTART_COUNT, restart_count);
    preferences.putUInt(NVS_KEY_RESTART_TIME, restart_time);
    preferences.end();
}

void myNVS::read::mqttData(String &mqtt_server, uint16_t &mqtt_port)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_MQTT_INFO);

    if (!preferences.isKey(NVS_KEY_MQTT_SERVER))
    {
        preferences.putString(NVS_KEY_MQTT_SERVER, DEFAULT_MQTT_SERVER);
    }

    if (!preferences.isKey(NVS_KEY_MQTT_PORT))
    {
        preferences.putInt(NVS_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    }

    mqtt_server = preferences.getString(NVS_KEY_MQTT_SERVER, DEFAULT_MQTT_SERVER);
    mqtt_port = preferences.getInt(NVS_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    preferences.end();
}

void myNVS::write::mqttData(String mqtt_server, uint16_t mqtt_port)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_MQTT_INFO);
    preferences.putString(NVS_KEY_MQTT_SERVER, mqtt_server);
    preferences.putInt(NVS_KEY_MQTT_PORT, mqtt_port);
    preferences.end();
}

void myNVS::read::sensorName(String &sensor_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SENSOR_INFO, true);
    sensor_name = preferences.getString(NVS_KEY_SENSOR_NAME, "");
    preferences.end();
}

void myNVS::write::sensorName(String sensor_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SENSOR_INFO);
    preferences.putString(NVS_KEY_SENSOR_NAME, sensor_name);
    preferences.end();
}

void myNVS::read::sensorsubName(String &sensor_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SENSOR_subINFO, true);
    sensor_name = preferences.getString(NVS_KEY_SENSOR_subNAME, "");
    preferences.end();
}

void myNVS::write::sensorsubName(String sensor_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_SENSOR_subINFO);
    preferences.putString(NVS_KEY_SENSOR_subNAME, sensor_name);
    preferences.end();
}

void myNVS::read::currentFileName(String &file_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_FILE_INFO, true);
    file_name = preferences.getString(NVS_KEY_FILE_NAME, "");
    (file_name != "")? Filename_Already_Exist = true : Filename_Already_Exist = false; 
    preferences.end();
}

void myNVS::write::currentFileName(String file_name)
{
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_FILE_INFO);
    preferences.putString(NVS_KEY_FILE_NAME, file_name);
    preferences.end();
}
void myNVS::read::lastslotsettime( uint32_t &current_slot_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_SET,true);

    current_slot_set_time = preferences.getUInt("lasttime", 0);

    preferences.end();
}
void myNVS::read::lastfilesettime(uint32_t &current_file_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_FILE_SET,true);

    current_file_set_time = preferences.getUInt("lastfiletime", 0);

    preferences.end();
}
void myNVS::write::lastfilesettime(const uint32_t current_file_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_FILE_SET);

    preferences.putUInt("lastfiletime",current_file_set_time);

    preferences.end();
    
}

void myNVS::read::lastfactorsettime( uint32_t &current_factor_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_FACTOR_SET,true);

    current_factor_set_time = preferences.getUInt("factortime", 0);

    preferences.end();
}
void myNVS::write::lastslotsettime(const uint32_t current_slot_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_SET);

    preferences.putUInt("lasttime",current_slot_set_time);

    preferences.end();
    
}
void myNVS::write::lastfactorsettime(const uint32_t current_factor_set_time)
{
    Preferences preferences;

    preferences.begin(NVS_NAMESPACE_LAST_FACTOR_SET);

    preferences.putUInt("factortime",current_factor_set_time);

    preferences.end();
    
}

