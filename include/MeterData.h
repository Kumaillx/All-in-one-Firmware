#ifndef __METERDATA_H__
#define __METERDATA_H__

#include <ArduinoJson.h>
#include <config.h>

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

// Abstracted class to handle data acquisition of PZEM module.
class MeterData
{
    // Functions
public:
    // void resetSums();
    void setPzem(MeterType *pzem);
    void setLabel(String label);

    void read();
    void readRandom();

    String getDataString();

    // getters
    float getInstVoltage() { return (inst_voltage_1); }
    float getInstCurrent() { return inst_current_1; }
    float getInstPower() { return inst_power_1; }
    float getInstPf() { return inst_pf_1; }

private:
    // void addInstToSum();

    // Variables
private:
    MeterType *pzem;

    float energy_p, energy_n;
    float inst_voltage_1, inst_voltage_2, inst_voltage_3;
    float inst_current_1, inst_current_2, inst_current_3;
    float inst_power_1, inst_power_2, inst_power_3;
    float inst_pf_1, inst_pf_2, inst_pf_3;
    float inst_frequency;
    float inst_thd_v1, inst_thd_i1;
    float inst_thd_v2, inst_thd_i2;
    float inst_thd_v3, inst_thd_i3;
    float inst_tot_act;
    float inst_energy_p, inst_energy_n;

    unsigned int samples;
    String label;
};

extern byte _loop_counter;
#endif