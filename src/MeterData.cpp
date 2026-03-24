#include "MeterData.h"
byte _loop_counter = 0;

void MeterData::setPzem(MeterType *pzem)
{
    this->pzem = pzem;
}

void MeterData::setLabel(String label)
{
    this->label = label;
}

void MeterData::read()
{

    float temp_voltage_1 = pzem->voltage_1();
    float temp_voltage_2 = pzem->voltage_2();
    float temp_voltage_3 = pzem->voltage_3();
    float temp_current_1 = pzem->current_1();
    float temp_current_2 = pzem->current_2();
    float temp_current_3 = pzem->current_3();
    float temp_power_1 = pzem->power_1();
    float temp_power_2 = pzem->power_2();
    float temp_power_3 = pzem->power_3();

#if SELECTED_METER == METER_PZEM
    float temp_pf_1 = pzem->pf_1();
    float temp_pf_2 = pzem->pf_2();
    float temp_pf_3 = pzem->pf_3();
    float temp_thd_v1 = pzem->thdv_1();
    float temp_thd_i1 = pzem->thdi_1();
    float temp_thd_v2 = pzem->thdv_2();
    float temp_thd_i2 = pzem->thdi_2();
    float temp_thd_v3 = pzem->thdv_3();
    float temp_thd_i3 = pzem->thdi_3();
    float temp_energy_p = pzem->energyp();
    float temp_energy_n = pzem->energyn();
#else
    float temp_pf_1 = pzem->power_factor_1();
    float temp_pf_2 = pzem->power_factor_2();
    float temp_pf_3 = pzem->power_factor_3();
    float temp_thd_v1 = pzem->thdVoltage1();
    float temp_thd_i1 = pzem->thdCurrent1();
    float temp_thd_v2 = pzem->thdVoltage2();
    float temp_thd_i2 = pzem->thdCurrent2();
    float temp_thd_v3 = pzem->thdVoltage3();
    float temp_thd_i3 = pzem->thdCurrent3();
    float temp_energy_p = pzem->energyP();
    float temp_energy_n = pzem->energyN();
#endif

    float temp_frequency = pzem->frequency();
    float temp_tot_act = pzem->total_power();

    inst_voltage_1 = isnan(temp_voltage_1) ? NAN : temp_voltage_1;
    inst_voltage_2 = isnan(temp_voltage_2) ? NAN : temp_voltage_2;
    inst_voltage_3 = isnan(temp_voltage_3) ? NAN : temp_voltage_3;

    inst_current_1 = isnan(temp_current_1) ? NAN : temp_current_1;
    inst_current_2 = isnan(temp_current_2) ? NAN : temp_current_2;
    inst_current_3 = isnan(temp_current_3) ? NAN : temp_current_3;

    inst_power_1 = isnan(temp_power_1) ? NAN : temp_power_1;
    inst_power_2 = isnan(temp_power_2) ? NAN : temp_power_2;
    inst_power_3 = isnan(temp_power_3) ? NAN : temp_power_3;

    inst_pf_1 = isnan(temp_pf_1) ? NAN : temp_pf_1;
    inst_pf_2 = isnan(temp_pf_2) ? NAN : temp_pf_2;
    inst_pf_3 = isnan(temp_pf_3) ? NAN : temp_pf_3;

    inst_thd_v1 = isnan(temp_thd_v1) ? NAN : temp_thd_v1;
    inst_thd_i1 = isnan(temp_thd_i1) ? NAN : temp_thd_i1;
    inst_thd_v2 = isnan(temp_thd_v2) ? NAN : temp_thd_v2;
    inst_thd_i2 = isnan(temp_thd_i2) ? NAN : temp_thd_i2;
    inst_thd_v3 = isnan(temp_thd_v3) ? NAN : temp_thd_v3;
    inst_thd_i3 = isnan(temp_thd_i3) ? NAN : temp_thd_i3;

    inst_frequency = isnan(temp_frequency) ? NAN : temp_frequency;
    inst_tot_act = isnan(temp_tot_act) ? NAN : temp_tot_act;
    inst_energy_p = isnan(temp_energy_p) ? NAN : temp_energy_p;
    inst_energy_n = isnan(temp_energy_n) ? NAN : temp_energy_n;
}
void MeterData::readRandom()
{
    inst_voltage_1 = 220;
    inst_current_1 = 100;
    inst_power_1 = 22000;
    inst_pf_1 = 1.00;
    inst_frequency = 50.00;
    inst_thd_v1 = 10.00;
    inst_thd_i1 = 5.00;
    inst_voltage_2 = 220;
    inst_current_2 = 300;
    inst_power_2 = 66000;
    inst_pf_2 = 0.99;
    inst_thd_v2 = 10.00;
    inst_thd_i2 = 10.00;
    inst_voltage_3 = 220;
    inst_current_3 = 1000;
    inst_power_3 = 220000;
    inst_pf_3 = 0.95;
    inst_thd_v3 = 30.00;
    inst_thd_i3 = 30.00;
    inst_tot_act = 306000.00;
    inst_energy_p = 10000.00;
    inst_energy_n = 10000.00;
}

String MeterData::getDataString()
{

    return "," +
           String(inst_voltage_1) + "," + String(inst_current_1) + "," + String(inst_power_1) + "," + String(inst_pf_1) + "," + String(inst_thd_v1) + "," + String(inst_thd_i1) + "," +
           String(inst_voltage_2) + "," + String(inst_current_2) + "," + String(inst_power_2) + "," + String(inst_pf_2) + "," + String(inst_thd_v2) + "," + String(inst_thd_i2) + "," +
           String(inst_voltage_3) + "," + String(inst_current_3) + "," + String(inst_power_3) + "," + String(inst_pf_3) + "," + String(inst_thd_v3) + "," + String(inst_thd_i3) + "," +
           String(inst_frequency) + "," + String(inst_tot_act) + "," + String(inst_energy_p) + "," + String(inst_energy_n);
}
