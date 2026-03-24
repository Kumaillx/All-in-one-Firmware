#include "TNM360.h"
#include <stdio.h>

// REGISTERS (Real-time)
#define REG_VOLTAGE_L1 0x00
#define REG_VOLTAGE_L2 0x01
#define REG_VOLTAGE_L3 0x02
#define REG_CURRENT_L1 0x03
#define REG_CURRENT_L2 0x04
#define REG_CURRENT_L3 0x05
#define REG_POWER_TOTAL 0x07
#define REG_POWER_L1    0x08
#define REG_POWER_L2    0x09
#define REG_POWER_L3    0x0A

#define REG_POWER_FACTOR_TOTAL 0x13 // uint16, scale 0.01
#define REG_POWER_FACTOR_L1 0x14
#define REG_POWER_FACTOR_L2 0x15
#define REG_POWER_FACTOR_L3 0x16

// Frequency
#define REG_FREQ 0x1A

// CT Ratio
#define REG_CT_RATIO 0x5A

// THD 
#define REG_THD_V1 0x70
#define REG_THD_V2 0x71
#define REG_THD_V3 0x72
#define REG_THD_I1 0x73
#define REG_THD_I2 0x74
#define REG_THD_I3 0x75


// Energy (two 16-bit registers each, big-endian)
#define REG_TOTAL_IMPORTED_ENERGY_H 0x1D
#define REG_TOTAL_IMPORTED_ENERGY_L 0x1E
#define REG_TOTAL_EXPORTED_ENERGY_H 0x1F
#define REG_TOTAL_EXPORTED_ENERGY_L 0x20


// Scale factors given in manual
static constexpr float SCALE_VOLT = 0.1f;
static constexpr float SCALE_CURR = 0.01f;
static constexpr float SCALE_POWER = 0.01f;
static constexpr float SCALE_ENERGY = 0.01f;
static constexpr float SCALE_FREQ = 0.01f;
static constexpr float SCALE_THD = 0.1f;
static constexpr float  SCALE_POWER_FACTOR = 0.001f;  

// CRC table from PZEM004T
static const uint16_t crcTable[] PROGMEM = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

//-----------------------------------------------    Helpers -----------------------------------------------

// Helper for unsigned 16-bit (voltage, current, power factor, frequency)
static uint16_t toUint16(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

// Helper for signed 16-bit (active power)
static int16_t toInt16(const uint8_t* buf) {
    return (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

// Helper to extract a 32-bit unsigned integer (Big Endian, energy)
static uint32_t toUint32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

// Helper for signed 32-bit
static int32_t toInt32(const uint8_t* buf) {
    return ((int32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

// Helper for 64-bit unsigned (energy)
static uint64_t toUint64(const uint8_t* buf) {
    return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) | ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) | ((uint64_t)buf[6] << 8) | buf[7];
}

// ------------------------------------------------   Constructors -----------------------------------------------

TNM360Meter::TNM360Meter() {
    init(nullptr, TNM360_DEFAULT_ADDR);
}

#if defined(ESP32)
TNM360Meter::TNM360Meter(HardwareSerial &port, uint8_t receivePin, uint8_t transmitPin, int8_t enablePin, uint8_t addr) {
    port.begin(TNM360_BAUD_RATE, SERIAL_8E1, receivePin, transmitPin);
    this->_enablePin = enablePin;
    if (this->_enablePin != -1) {
        pinMode(this->_enablePin, OUTPUT);
        digitalWrite(this->_enablePin, LOW); // Listen mode
    }
    init(&port, addr);
}
#else
TNM360Meter::TNM360Meter(HardwareSerial &port, uint8_t addr) {
    port.begin(TNM360_BAUD_RATE, SERIAL_8E1);
    init(&port, addr);
}
#endif

TNM360Meter::TNM360Meter(Stream &port, uint8_t addr) {
    init(&port, addr);
}

TNM360Meter::~TNM360Meter() {}

void TNM360Meter::init(Stream* port, uint8_t addr) {
    this->_serial = port;
    this->_addr = addr;
    this->_lastRead = 0;
    this->_lastRead -= UPDATE_TIME;
    this->_isConnected = false;
}

bool TNM360Meter::updateValues() {
    if (_lastRead + UPDATE_TIME > millis()) {
        return true;
    }

    if (!readRealtimeFrame()) {
        _isConnected = false;
        return false;
    }

    uint32_t raw_energy_p;
    if (!readRegister32(REG_TOTAL_IMPORTED_ENERGY_H, raw_energy_p)) {
        _isConnected = false;
        return false;
    }
    _currentValues.energyP = raw_energy_p * SCALE_ENERGY;

    uint32_t raw_energy_n;
    if (!readRegister32(REG_TOTAL_EXPORTED_ENERGY_H, raw_energy_n)) {
        _isConnected = false;
        return false;
    }
    _currentValues.energyN = raw_energy_n * SCALE_ENERGY;

    _isConnected = true;
    _lastRead = millis();
    return true;
}

bool TNM360Meter::updateFrequency() {
    return readFrequencyFrame();
}

bool TNM360Meter::updateTHD() {
    return readTHDFrame();
}

bool TNM360Meter::updatePowerFactor() {
    return readPowerFactorFrame();
}

float TNM360Meter::voltage_1() { if (!updateValues()) return NAN; return _currentValues.voltage_1; }
float TNM360Meter::voltage_2() { if (!updateValues()) return NAN; return _currentValues.voltage_2; }
float TNM360Meter::voltage_3() { if (!updateValues()) return NAN; return _currentValues.voltage_3; }

float TNM360Meter::current_1() { if (!updateValues()) return NAN; return _currentValues.current_1; }
float TNM360Meter::current_2() { if (!updateValues()) return NAN; return _currentValues.current_2; }
float TNM360Meter::current_3() { if (!updateValues()) return NAN; return _currentValues.current_3; }

float TNM360Meter::power_1() { if (!updateValues()) return NAN; return _currentValues.power_1; }
float TNM360Meter::power_2() { if (!updateValues()) return NAN; return _currentValues.power_2; }
float TNM360Meter::power_3() { if (!updateValues()) return NAN; return _currentValues.power_3; }
float TNM360Meter::total_power() { if (!updateValues()) return NAN; return _currentValues.total_power; }
    
float TNM360Meter::power_factor_1() { if (!updatePowerFactor()) return NAN; return _currentValues.power_factor_1; }
float TNM360Meter::power_factor_2() { if (!updatePowerFactor()) return NAN; return _currentValues.power_factor_2; }
float TNM360Meter::power_factor_3() { if (!updatePowerFactor()) return NAN; return _currentValues.power_factor_3; }

float TNM360Meter::frequency() { if (!updateFrequency()) return NAN; return _currentValues.frequency; }
float TNM360Meter::energyP() { if (!updateValues()) return NAN; return _currentValues.energyP; }
float TNM360Meter::energyN() { if (!updateValues()) return NAN; return _currentValues.energyN; }

float TNM360Meter::thdVoltage1() { if (!updateTHD()) return NAN; return _currentValues.thd_v1; }
float TNM360Meter::thdCurrent1() { if (!updateTHD()) return NAN; return _currentValues.thd_i1; }

float TNM360Meter::thdVoltage2() { if (!updateTHD()) return NAN; return _currentValues.thd_v2; }
float TNM360Meter::thdVoltage3() { if (!updateTHD()) return NAN; return _currentValues.thd_v3; }
float TNM360Meter::thdCurrent2() { if (!updateTHD()) return NAN; return _currentValues.thd_i2; }
float TNM360Meter::thdCurrent3() { if (!updateTHD()) return NAN; return _currentValues.thd_i3; }

float TNM360Meter::getVoltage_1() { return _currentValues.voltage_1; }
float TNM360Meter::getVoltage_2() { return _currentValues.voltage_2; }
float TNM360Meter::getVoltage_3() { return _currentValues.voltage_3; }

float TNM360Meter::getCurrent_1() { return _currentValues.current_1; }
float TNM360Meter::getCurrent_2() { return _currentValues.current_2; }
float TNM360Meter::getCurrent_3() { return _currentValues.current_3; }

float TNM360Meter::getPower_1() { return _currentValues.power_1; }
float TNM360Meter::getPower_2() { return _currentValues.power_2; }
float TNM360Meter::getPower_3() { return _currentValues.power_3; }

float TNM360Meter::getTotal_power() { return _currentValues.total_power; }

float TNM360Meter::getPower_factor_1() { return _currentValues.power_factor_1; }
float TNM360Meter::getPower_factor_2() { return _currentValues.power_factor_2; }
float TNM360Meter::getPower_factor_3() { return _currentValues.power_factor_3; }

float TNM360Meter::getFrequency() { return _currentValues.frequency; }
float TNM360Meter::getEnergyP() { return _currentValues.energyP; }
float TNM360Meter::getEnergyN() { return _currentValues.energyN; }

float TNM360Meter::getThdVoltage1() { return _currentValues.thd_v1; }
float TNM360Meter::getThdCurrent1() { return _currentValues.thd_i1; }
float TNM360Meter::getThdVoltage2() { return _currentValues.thd_v2; }
float TNM360Meter::getThdCurrent2() { return _currentValues.thd_i2; }
float TNM360Meter::getThdVoltage3() { return _currentValues.thd_v3; }
float TNM360Meter::getThdCurrent3() { return _currentValues.thd_i3; }

bool TNM360Meter::isConnected() { return _isConnected; }

bool TNM360Meter::readRealtimeFrame() {
    // Read CT Ratio
    uint16_t ct_ratio = 10;
    readRegister16(REG_CT_RATIO, ct_ratio);
    if (ct_ratio == 0) {
        ct_ratio = 10; // Fallback to avoid mult by 0 if something is wrong (10 means 1.0 multiplier)
    }
    
    // The meter stores CT ratio in tenths (e.g. 10 = 1.0, 50 = 5.0).
    // Divide by 10 to shift the decimal one point to the left.
    float ct_multiplier = ct_ratio / 10.0f;

    // Read voltages (3 registers: L1, L2, L3 - each is a single 16-bit register)
    uint8_t volt_resp[5 + 6];  // 3 regs = 6 bytes data
    if (!sendRead(REG_VOLTAGE_L1, 3, volt_resp, sizeof(volt_resp))) {
        return false;
    }
    const uint8_t* data = &volt_resp[3];
    _currentValues.voltage_1 = toUint16(&data[0]) * SCALE_VOLT;
    _currentValues.voltage_2 = toUint16(&data[2]) * SCALE_VOLT;
    _currentValues.voltage_3 = toUint16(&data[4]) * SCALE_VOLT;

    // Read currents (3 registers: L1, L2, L3 - each is a single 16-bit register)
    uint8_t curr_resp[5 + 6];
    if (!sendRead(REG_CURRENT_L1, 3, curr_resp, sizeof(curr_resp))) {
        return false;
    }
    data = &curr_resp[3];
    _currentValues.current_1 = toUint16(&data[0]) * SCALE_CURR * ct_multiplier;
    _currentValues.current_2 = toUint16(&data[2]) * SCALE_CURR * ct_multiplier;
    _currentValues.current_3 = toUint16(&data[4]) * SCALE_CURR * ct_multiplier;

    // Read powers (4 registers: total, L1, L2, L3 - each is a signed 16-bit register)
    uint8_t power_resp[5 + 8];  // 4 regs = 8 bytes
    if (!sendRead(REG_POWER_TOTAL, 4, power_resp, sizeof(power_resp))) {
        return false;
    }
    data = &power_resp[3];
    _currentValues.total_power = toInt16(&data[0]) * SCALE_POWER * ct_ratio * 100.0f;
    _currentValues.power_1 = toInt16(&data[2]) * SCALE_POWER * ct_ratio * 100.0f;
    _currentValues.power_2 = toInt16(&data[4]) * SCALE_POWER * ct_ratio * 100.0f;
    _currentValues.power_3 = toInt16(&data[6]) * SCALE_POWER * ct_ratio * 100.0f;

    return true;
}

bool TNM360Meter::readFrequencyFrame() {
    uint16_t raw;
    if (!readRegister16(REG_FREQ, raw)) {
        return false;
    }
    _currentValues.frequency = raw * SCALE_FREQ;
    return true;
}

bool TNM360Meter::readTHDFrame() {
    uint16_t rv1, rv2, rv3, ri1, ri2, ri3;
    if (!readRegister16(REG_THD_V1, rv1)) return false;
    if (!readRegister16(REG_THD_V2, rv2)) return false;
    if (!readRegister16(REG_THD_V3, rv3)) return false;
    if (!readRegister16(REG_THD_I1, ri1)) return false;
    if (!readRegister16(REG_THD_I2, ri2)) return false;
    if (!readRegister16(REG_THD_I3, ri3)) return false;

    _currentValues.thd_v1 = rv1 * SCALE_THD;
    _currentValues.thd_v2 = rv2 * SCALE_THD;
    _currentValues.thd_v3 = rv3 * SCALE_THD;

    _currentValues.thd_i1 = ri1 * SCALE_THD;
    _currentValues.thd_i2 = ri2 * SCALE_THD;
    _currentValues.thd_i3 = ri3 * SCALE_THD;

    return true;
}

bool TNM360Meter::readPowerFactorFrame() {
    uint8_t buf[5 + 6];  // 3 regs x 2 bytes = 6 data bytes
    if (!sendRead(REG_POWER_FACTOR_L1, 3, buf, sizeof(buf))) return false;
    const uint8_t* data = &buf[3];
    _currentValues.power_factor_1 = toUint16(&data[0]) * SCALE_POWER_FACTOR;
    _currentValues.power_factor_2 = toUint16(&data[2]) * SCALE_POWER_FACTOR;
    _currentValues.power_factor_3 = toUint16(&data[4]) * SCALE_POWER_FACTOR;
    return true;
}

bool TNM360Meter::readRegister16(uint16_t reg, uint16_t &out) {
    uint8_t buf[7]; // 5 header+crc + 2 data
    if (!sendRead(reg, 1, buf, sizeof(buf))) return false;
    out = toUint16(&buf[3]);
    return true;
}

bool TNM360Meter::readRegisterSigned16(uint16_t reg, int16_t &out) {
    uint8_t buf[7];
    if (!sendRead(reg, 1, buf, sizeof(buf))) return false;
    out = toInt16(&buf[3]);
    return true;
}

bool TNM360Meter::readRegister32(uint16_t reg, uint32_t &out) {
    uint8_t buf[9]; // 5 + 4 +2
    if (!sendRead(reg, 2, buf, sizeof(buf))) return false;
    out = toUint32(&buf[3]);
    return true;
}

bool TNM360Meter::readRegisterSigned32(uint16_t reg, int32_t &out) {
    uint8_t buf[9];
    if (!sendRead(reg, 2, buf, sizeof(buf))) return false;
    out = toInt32(&buf[3]);
    return true;
}

bool TNM360Meter::readRegister64(uint16_t reg, uint64_t &out) {
    uint8_t buf[3 + 8 + 2]; // addr + fc + bytecount + 8 data bytes + crc
    if (!sendRead(reg, 4, buf, sizeof(buf))) return false;
    out = toUint64(&buf[3]);
    return true;
}

bool TNM360Meter::sendRead(uint16_t startAddr, uint16_t qty, uint8_t *resp, uint16_t respLen, unsigned long timeout) {
    if (qty == 0 || qty > 125) return false;

    uint8_t req[8];
    req[0] = _addr;
    req[1] = 0x03; // Read Holding Registers
    req[2] = (startAddr >> 8) & 0xFF;
    req[3] = (startAddr) & 0xFF;
    req[4] = (qty >> 8) & 0xFF;
    req[5] = (qty) & 0xFF;

    setCRC(req, 8);

#if defined(ESP32)
    if(_enablePin != -1) {
        digitalWrite(_enablePin, HIGH); // Enable Transmission
        delay(1);
    }
#endif
    
    _serial->write(req, 8);
    _serial->flush();

#if defined(ESP32)
    if(_enablePin != -1) {
        digitalWrite(_enablePin, LOW);  // Disable Transmission (Listen Mode)
    }
#endif

    uint16_t expected_len = 5 + 2 * qty;
    if (respLen < expected_len) return false; // a bit of safety

    uint16_t read_len = receive(resp, expected_len, timeout);
    
    return read_len == expected_len;
}

uint16_t TNM360Meter::receive(uint8_t *resp, uint16_t len, unsigned long timeout) {
    unsigned long startTime = millis();
    uint16_t index = 0;
    while ((index < len) && (millis() - startTime < timeout)) {
        if (_serial->available() > 0) {
            resp[index++] = (uint8_t)_serial->read();
        }
        yield();
    }

    if (!checkCRC(resp, index)) {
        return 0;
    }

    return index;
}

bool TNM360Meter::checkCRC(const uint8_t *buf, uint16_t len) {
    if (len <= 2) return false;
    uint16_t crc = CRC16(buf, len - 2);
    return ((uint16_t)buf[len - 2] | (uint16_t)buf[len - 1] << 8) == crc;
}

void TNM360Meter::setCRC(uint8_t *buf, uint16_t len) {
    if (len <= 2) return;
    uint16_t crc = CRC16(buf, len - 2);
    buf[len - 2] = crc & 0xFF;
    buf[len - 1] = (crc >> 8) & 0xFF;
}

uint16_t TNM360Meter::CRC16(const uint8_t *data, uint16_t len) {
    uint8_t nTemp;
    uint16_t crc = 0xFFFF;

    while (len--) {
        nTemp = *data++ ^ crc;
        crc >>= 8;
        crc ^= (uint16_t)pgm_read_word(&crcTable[nTemp]);
    }
    return crc;
}