#ifndef __COMPILE_INVERTER_
#define __COMPILE_INVERTER_


#define inverter_DISABLE    0
#define inverter_ENABLE     1

#define PRINT_RED       "\e[1;31m" // in Visual Studio Code: add monitor_raw = yes to platform.ini
#define PRINT_GREEN     "\e[0;32m"
#define PRINT_PURPLE    "\e[0;35m"
#define PRINT_RESET     "\e[1;37m" // White


#define modbusReadingsBlockSize 25
#define modbusBlock01Start      101
#define modbusBlock02Start      514
#define modbusBlock03Start      588
#define modbusBlock04Start      619
#define modbusBlock05Start      653
#define modbusNoOfBlocks        5

void inverter_BatchReadRegisters(uint16_t uiFirstRegister, int iLen);

int inverter_get_iBatt_V_Nominel_V_x100(void);
int inverter_get_iBatteryCapacity_Ah(void);
int inverter_get_iBatteryMaxCharge_A(void);
int inverter_get_iBatteryMaxDischarge_A(void);
int inverter_get_iBatteryMinSOC_pct(void);
long inverter_get_lTotalProduction_Wh_x100(void);
long inverter_get_lTotalConsumption_Wh_x100(void);
int inverter_get_iCurrentProduction_W(void);
int inverter_get_iCurrentTotalConsumption_W(void);
int inverter_get_iCurrentGridPower_W(void);
int inverter_get_iCurrentAvailableProduction_W(void);
int inverter_get_iBatt_SoC_pct(void);
int inverter_get_iBatt_ChargeToday_Wh_x100(void);
signed int inverter_get_siBatteryPower_W(void);

bool inverter_getPVSellingState(void);
void inverter_setSellingFromPV(uint8_t ucNewState) ;
void inverter_setSellingFromBattery(uint8_t ucNewState);
void inverter_setUseFromBattery(uint8_t ucNewState);
void setChargeFromGrid(uint8_t ucNewState, uint16_t uiChargeRate_A);
void disableCharge(void) ;
void enableCharge(void) ;


void inverter_comSetup() ;


#endif