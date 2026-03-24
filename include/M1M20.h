/*
  M1M20.h - Modbus RTU reader for ABB M1M20 meter.
  Refactored to follow the structure of the PZEM004Tv30 library.
*/

#ifndef M1M20_H
#define M1M20_H

#include <Arduino.h>

#define M1M20_BAUD_RATE 19200  // Confirm with meter; manual supports 9600-115200
#define M1M20_DEFAULT_ADDR 0x01
#define UPDATE_TIME 250 // Time between updates

// Struct to hold all meter values
struct m1m20_values {
    float voltage_1;
    float voltage_2;
    float voltage_3;
    float current_1;
    float current_2;
    float current_3;
    float power_1;
    float power_2;
    float power_3;
    float power_factor_1;
    float power_factor_2;
    float power_factor_3;
    float total_power;
    float frequency;
    float energyP; // Total Active Energy Import (kWh)
    float energyN; // Total Active Energy Export (kWh)
    float thd_v1;
    float thd_i1;
    float thd_v2;
    float thd_v3;
    float thd_i2;
    float thd_i3;
    // Add other values as needed
};

class M1M20Meter {
public:
    M1M20Meter();
#if defined(ESP32)
    M1M20Meter(HardwareSerial &port, uint8_t receivePin, uint8_t transmitPin, int8_t enablePin = -1, uint8_t addr = M1M20_DEFAULT_ADDR);
#else
    M1M20Meter(HardwareSerial &port, uint8_t addr = M1M20_DEFAULT_ADDR);
#endif
    M1M20Meter(Stream &port, uint8_t addr = M1M20_DEFAULT_ADDR);
    ~M1M20Meter();

    bool updateValues(); // Update main electrical values
    bool updateFrequency(); // Update frequency value
    bool updateTHD(); // Update THD values
    bool updatePowerFactor(); // Update Power Factor values

    // Getters that will update values if necessary
    float voltage_1();
    float voltage_2();
    float voltage_3();

    float current_1();
    float current_2();
    float current_3();

    float power_1();
    float power_2();
    float power_3();

    float power_factor_1();
    float power_factor_2();
    float power_factor_3();
    float total_power();

    float frequency();
    float energyP();
    float energyN();

    float thdVoltage1();
    float thdVoltage2();
    float thdVoltage3();
    float thdCurrent1();
    float thdCurrent2();
    float thdCurrent3();

    // Getters that return the last read value
    float getVoltage_1();
    float getVoltage_2();
    float getVoltage_3();
    float getCurrent_1();
    float getCurrent_2();
    float getCurrent_3();
    float getPower_1();
    float getPower_2();
    float getPower_3();
    float getTotal_power();
    float getPower_factor_1();
    float getPower_factor_2();
    float getPower_factor_3();
    float getFrequency();
    float getEnergyP();
    float getEnergyN();
    float getThdVoltage1();
    float getThdVoltage2();
    float getThdVoltage3();
    float getThdCurrent1();
    float getThdCurrent2();
    float getThdCurrent3();

    bool isConnected();

    // Low level helpers
    bool readRegister16(uint16_t reg, uint16_t &out);
    bool readRegister32(uint16_t reg, uint32_t &out);  // For unsigned 32-bit
    bool readRegisterSigned32(uint16_t reg, int32_t &out);  // For signed 32-bit (powers)
    bool readRegister64(uint16_t reg, uint64_t &out);  // For 64-bit (energy)

private:
    Stream* _serial;
    uint8_t _addr;
    unsigned long _lastRead;
    bool _isConnected;

#if defined(ESP32)
    int8_t _enablePin = -1;
#endif

    m1m20_values _currentValues;

    void init(Stream* port, uint8_t addr);

    // Frame readers
    bool readRealtimeFrame();
    bool readFrequencyFrame();
    bool readTHDFrame();
    bool readPowerFactorFrame();

    bool sendRead(uint16_t startAddr, uint16_t qty, uint8_t* resp, uint16_t respLen, unsigned long timeout = 200);
    uint16_t receive(uint8_t* resp, uint16_t len);

    // CRC
    uint16_t CRC16(const uint8_t* data, uint16_t len);
    void setCRC(uint8_t* buf, uint16_t len);
    bool checkCRC(const uint8_t* buf, uint16_t len);
};

#endif