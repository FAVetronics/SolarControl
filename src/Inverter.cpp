#include <ModbusMaster.h>
#include "inverter.h"
#include "userData.h"

// Adresses: Deye 3p modbus address list.docx

// Settings:
#define modbus_LIMIT_CONTROL_MODE     142
#define modbusSet_SELLING_FIRST       0
#define modbusSet_ZERO_EXPORT_TO_LOAD 1
#define modbusSet_ZERO_EXPORT_TO_CT   2

#define modbus_SOLAR_SELL             145
#define modbusSet_SOLAR_SELL_ENABLE   1
#define modbusSet_SOLAR_SELL_DISABLE  0

#define modbus_Max_A_charge         108
#define modbus_Max_A_discharge      109

#define modbus_Time_point_1_capacity_pct    166
#define modbus_Time_point_2_capacity_pct    166
#define modbus_Time_point_3_capacity_pct    166
#define modbus_Time_point_4_capacity_pct    166
#define modbus_Time_point_5_capacity_pct    166
#define modbus_Time_point_6_capacity_pct    166
    
#define modbus_Time_point_1_charge_enable   172 // Bit0: grid charging enable Bit1: gen charging enable
#define modbus_Time_point_2_charge_enable   173
#define modbus_Time_point_3_charge_enable   174
#define modbus_Time_point_4_charge_enable   175
#define modbus_Time_point_5_charge_enable   176
#define modbus_Time_point_6_charge_enable   177

// Readings:
#define modbusBatt_V_Nominel_V_x100         101
#define modbusBatteryCapacity_Ah            102
#define modbusBatteryMaxCharge_A            108
#define modbusBatteryMaxDischarge_A         109
#define modbusBatteryMinSOC_pct             117

//                                          514
#define modbusTotalGridBuyPower_lo_kWh_x10  522
#define modbusTotalGridBuyPower_hi_kWh_x10  523
#define modbusTotalGridSellPower_lo_kWh_x10 524
#define modbusTotalGridSellPower_hi_kWh_x10 525
#define modbusTotalLoadPower_lo_kWh_x10     527
#define modbusTotalLoadPower_hi_kWh_x10     528
#define modbusTotalPVPower_lo_kWh_x10       534
#define modbusTotalPVPower_hi_kWh_x10       535

//                                          588
#define modbusBatteryOutputPower            590

#define modbusGridTotalPower                619

#define modbusLoadTotalPower                653
#define modbusPV1Power                      672
#define modbusPV2Power                      673


#define MAX_COM_ATTEMPTS_EXCEEDED 255


int imodbusValues[2][modbusReadingsBlockSize*modbusNoOfBlocks];

bool bBatchReadValuesValid = false;




void storeModbusBatchReading(int *iVal, int indexFirst) {
  int offset;
  if (indexFirst >= modbusBlock05Start) {
    offset = 4 * modbusReadingsBlockSize;
    bBatchReadValuesValid = true; // all values has been updated
  }
  else if (indexFirst >= modbusBlock04Start) offset = 3 * modbusReadingsBlockSize;
  else if (indexFirst >= modbusBlock03Start) offset = 2 * modbusReadingsBlockSize;
  else if (indexFirst >= modbusBlock02Start) offset = 1 * modbusReadingsBlockSize;
  else offset = 0;
  for (int n = 0; n < modbusReadingsBlockSize; n++) {
    imodbusValues[0][n+offset] = indexFirst + n;
    imodbusValues[1][n+offset] = *(iVal + n);
  }
}

int iGetModbusBatchReading(int index) {
  int iResult = 12345;
  if (!bBatchReadValuesValid) iResult = 0;
  for (int n = 0; n < modbusReadingsBlockSize*modbusNoOfBlocks; n++) {
    if (imodbusValues[0][n] == index) {iResult = imodbusValues[1][n]; break;}
  }
  return iResult;
}


//** Modbus **
// instantiate ModbusMaster object
ModbusMaster node;

void preTransmission()
{
  // We use auto dir change
  //digitalWrite(MAX485_RE_NEG, 1);
  //digitalWrite(MAX485_DE, 1);
}
void postTransmission()
{
  // We use auto dir change
  //digitalWrite(MAX485_RE_NEG, 0);
  //digitalWrite(MAX485_DE, 0);
}

/////////////////////////////////////////////////////////////
// Inverter functions                                      //
/////////////////////////////////////////////////////////////

//** Requests **//

uint16_t ReadSingleRegister(uint16_t uiRegister) {
  uint8_t result;
  uint8_t ucTryCnt = 3;
  do {
    result = node.readHoldingRegisters(uiRegister, 1);
    delay(10); // allow inverter to ??
  } while ((result != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error reading register " + String(uiRegister) + " - (" + String(result, HEX) + ")");
  }
  return node.getResponseBuffer(0x00);
}


long ReadAddDualRegister_(uint16_t uiFirstRegister) {
  uint8_t result;
  uint8_t ucTryCnt = 3;
  do {
    result = node.readHoldingRegisters(uiFirstRegister, 2);
    delay(10); // allow inverter to ??
  } while ((result != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error reading dual register " + String(uiFirstRegister) + " - (" + String(result, HEX) + ")");
  }
  return node.getResponseBuffer(0x00) + node.getResponseBuffer(0x01);
}

long ReadAddDualRegister(uint16_t uiFirstRegister) {
  return (long)iGetModbusBatchReading(uiFirstRegister) + (long)iGetModbusBatchReading(uiFirstRegister+1);
}


long ReadDualRegister_(uint16_t uiFirstRegister) {
  uint8_t result;
  uint8_t ucTryCnt = 3;
  do {
    result = node.readHoldingRegisters(uiFirstRegister, 2);
    delay(10); // allow inverter to ??
  } while ((result != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error reading dual register " + String(uiFirstRegister) + " - (" + String(result, HEX) + ")");
  }
  return node.getResponseBuffer(0x00) + (node.getResponseBuffer(0x01) * 0x10000);
}

long ReadDualRegister(uint16_t uiFirstRegister) {
  return (long)iGetModbusBatchReading(uiFirstRegister) + ((long)iGetModbusBatchReading(uiFirstRegister+1) * 0x10000);
}

void inverter_BatchReadRegisters(uint16_t uiFirstRegister, int iLen) {
#ifdef NO_INVERTER
  return;
#endif
  uint8_t result;
  uint8_t ucTryCnt = 3;
  do {
    result = node.readHoldingRegisters(uiFirstRegister, iLen);
    delay(10); // allow inverter to ??
  } while ((result != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error reading batch registers from " + String(uiFirstRegister));
  }
  int iResp[modbusReadingsBlockSize];
  for (int n = 0; n<modbusReadingsBlockSize; n++){
    iResp[n] = node.getResponseBuffer(n);
  }
  storeModbusBatchReading(iResp, uiFirstRegister);
}

#ifdef NO_INVERTER
int  extCurrentProduction_W = 0;
int  extCurrentConsumption_W = 0;
int  extGridPower_W = 0;
int  extBattPower_W = 0;
int  extSOC_pct = 0;
bool bValuesSetFromExt = false;
// get inverter values from external source - test only
void inverter_set_extCurrentProduction_W(int iVal){
  extCurrentProduction_W = iVal;
}
void inverter_set_extCurrentConsumption_W(int iVal){
  extCurrentConsumption_W = iVal;
}
void inverter_set_extGridPower_W(int iVal){
  extGridPower_W = iVal;
}
void inverter_set_extBattPower_W(int iVal){
  extBattPower_W = iVal;
}
void inverter_set_extSOC_pct(int iVal){
  extSOC_pct = iVal;
  bValuesSetFromExt = true;
}
#endif


int inverter_get_iBatt_V_Nominel_V_x100(void){
#ifdef NO_INVERTER
  return random(10000);
#endif
//    return ReadSingleRegister(modbusBatt_V_Nominel_V_x100);
    return iGetModbusBatchReading(modbusBatt_V_Nominel_V_x100);
}

int inverter_get_iBatteryCapacity_Ah(void){
#ifdef NO_INVERTER
  return random(10000);
#endif
    //return ReadSingleRegister(modbusBatteryCapacity_Ah);
    return iGetModbusBatchReading(modbusBatteryCapacity_Ah);
}

int inverter_get_iBatteryMaxCharge_A(void){
#ifdef NO_INVERTER
  return random(10000);
#endif
    //return ReadSingleRegister(modbusBatteryMaxCharge_A);
    return iGetModbusBatchReading(modbusBatteryMaxCharge_A);
}

int inverter_get_iBatteryMaxDischarge_A(void){
#ifdef NO_INVERTER
  return random(10000);
#endif
    //return ReadSingleRegister(modbusBatteryMaxDischarge_A);
    return iGetModbusBatchReading(modbusBatteryMaxDischarge_A);
}

int inverter_get_iBatteryMinSOC_pct(void){
#ifdef NO_INVERTER
  return random(10000);
#endif
    //return ReadSingleRegister(modbusBatteryMinSOC_pct);
    return iGetModbusBatchReading(modbusBatteryMinSOC_pct);
}


long inverter_get_lTotalProduction_Wh_x100(void){
#ifdef NO_INVERTER
  static long res=1; 
  return res += random(10);
#endif
    long lProduction_Wh_x100 = 0;
    lProduction_Wh_x100 = ReadDualRegister(modbusTotalPVPower_lo_kWh_x10);
    return (lProduction_Wh_x100);
}

long inverter_get_lTotalConsumption_Wh_x100(void){
#ifdef NO_INVERTER
  static long res=1; 
  return res += random(10);
#endif
    long lConsumption_Wh_x100 = 0;
    lConsumption_Wh_x100 = ReadDualRegister(modbusTotalLoadPower_lo_kWh_x10);
    return (lConsumption_Wh_x100);
}

int inverter_get_iCurrentProduction_W(void){
#ifdef NO_INVERTER
  if (bValuesSetFromExt) return extCurrentProduction_W;
  else return random(10000);
#endif
    int iProduction_W = 0;
    iProduction_W = (int)ReadAddDualRegister(modbusPV1Power);
    //Serial.println("Tyvkaervej:\t" + String(ReadSingleRegister(modbusPV1Power));
    //Serial.println("Bygaden:\t"+String(ReadSingleRegister(modbusPV2Power));
    return (iProduction_W);
}

int inverter_get_iCurrentTotalConsumption_W(void){
#ifdef NO_INVERTER
  if (bValuesSetFromExt) return extCurrentConsumption_W;
  else return random(10000);
#endif
    //return ReadSingleRegister(modbusLoadTotalPower);
    return iGetModbusBatchReading(modbusLoadTotalPower);
}

int inverter_get_iCurrentGridPower_W(void){
#ifdef NO_INVERTER
  if (bValuesSetFromExt) return extGridPower_W;
  else return random(10000);
#endif
    //return ReadSingleRegister(modbusGridTotalPower);
    return iGetModbusBatchReading(modbusGridTotalPower);
}


int inverter_get_iCurrentAvailableProduction_W(void){
    int iProd_W = inverter_get_iCurrentProduction_W();
    int iConsump_W = inverter_get_iCurrentTotalConsumption_W();
    Serial.println("Current production:\t"+String(iProd_W)+" (W)");
    Serial.println("Current consumption:\t" + String(iConsump_W)+" (W)");
    int iAvailable_W = iProd_W - iConsump_W;
    if (iAvailable_W < 0) iAvailable_W = 0;
    Serial.println("Available for charging:\t\t" + String(iAvailable_W)+" (W)");
    return (iAvailable_W);
}

int inverter_get_iBatt_SoC_pct(void){
#ifdef NO_INVERTER
  if (bValuesSetFromExt) return extSOC_pct;
  else return random(100);
#endif
    //return ReadSingleRegister(588);
    return iGetModbusBatchReading(588);
}

int inverter_get_iBatt_ChargeToday_Wh_x100(void){
#ifdef NO_INVERTER
  return random(1000);
#endif
    //return ReadSingleRegister(514);
    return iGetModbusBatchReading(514);
}
    
signed int inverter_get_siBatteryPower_W(void){
#ifdef NO_INVERTER
  if (bValuesSetFromExt) return extBattPower_W;
  else return random(10000);
#endif
  //return (signed int)ReadSingleRegister(modbusBatteryOutputPower);
  return (signed int)iGetModbusBatchReading(modbusBatteryOutputPower);
}



//** Commands **//

uint8_t writeMultipleRegister(uint16_t uiRegister, uint8_t ucNoOfData) {
#ifdef NO_INVERTER
  return 0;
#endif
  uint8_t ucResult;
  uint8_t ucTryCnt = 3;
  do {
    ucResult = node.writeMultipleRegisters(uiRegister, ucNoOfData);
  } while ((ucResult != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error writing to register " + String(uiRegister) + " - (" + String(ucResult, HEX) + ")");
  }
  // Verify
  ucTryCnt = 3;
  do {
    ucResult = node.readHoldingRegisters(uiRegister, ucNoOfData);
    delay(10); 
  } while ((ucResult != node.ku8MBSuccess) && (ucTryCnt-- > 0));
  if (ucTryCnt == MAX_COM_ATTEMPTS_EXCEEDED) {
    Serial.print("Error reading register " + String(uiRegister) + " for verify - (" + String(ucResult, HEX) + ")");
  }
  ucResult = 0;
  Serial.print("Readback from " + String(uiRegister) + ": ");
  for (uint8_t n = 0; n < ucNoOfData; n++)
  {
    Serial.print(String(node.getResponseBuffer(n)) + " ");
//    if (node.getResponseBuffer(n) != node.getTransmitBuffer getResponseBuffer(n)) result = error
  }
  Serial.println("");
  return ucResult;
}

uint8_t writeSingleRegister(uint16_t uiRegister, uint16_t uiData) {
#ifdef NO_INVERTER
  return 0;
#endif
    node.setTransmitBuffer(0, uiData);
    return writeMultipleRegister(uiRegister, 1);
}



uint8_t ucCurrentlySellingFromPV = -1;

bool inverter_getPVSellingState(void) {
  return (ucCurrentlySellingFromPV == 1);
}

void inverter_setSellingFromPV(uint8_t ucNewState) {
  if (ucNewState != ucCurrentlySellingFromPV) {
    uint16_t uiData;
    ucCurrentlySellingFromPV = ucNewState; // OBS read back data to verify before setting this
#ifndef NO_INVERTER
    if (ucNewState == inverter_DISABLE) uiData = modbusSet_SOLAR_SELL_DISABLE;
    else uiData = modbusSet_SOLAR_SELL_ENABLE;
    node.setTransmitBuffer(0, uiData);
    writeSingleRegister(modbus_SOLAR_SELL, uiData);
#endif
  }
  if (ucNewState == 0) Serial.print(PRINT_RED);
  else Serial.print(PRINT_GREEN);
  Serial.print("\tSelling from PV: " + String(ucNewState));
  Serial.print(PRINT_RESET);
 }


void inverter_setSellingFromBattery(uint8_t ucNewState) {
  static uint8_t ucCurrentState = -1;
  if (ucNewState != ucCurrentState) {
    uint16_t uiData;
    ucCurrentState = ucNewState; // OBS read back data to verify before setting this
#ifndef NO_INVERTER
    if (ucNewState == inverter_DISABLE) uiData = modbusSet_ZERO_EXPORT_TO_CT;
    else uiData = modbusSet_SELLING_FIRST;
    node.setTransmitBuffer(0, uiData);
    writeSingleRegister(modbus_LIMIT_CONTROL_MODE, uiData);
#endif
  }
  if (ucNewState == 0) Serial.print(PRINT_RED);
  else Serial.print(PRINT_GREEN);
  Serial.print("\tSelling from battery: " + String(ucNewState));
  Serial.println(PRINT_RESET);
}


void inverter_setUseFromBattery(uint8_t ucNewState) {
  static uint8_t ucCurrentState = -1;
  if (ucNewState != ucCurrentState) {
    uint16_t uiData;
    ucCurrentState = ucNewState; // OBS read back data to verify before setting this
#ifndef NO_INVERTER
    if (ucNewState == inverter_DISABLE) uiData = 0;
    else uiData = batt_MaxDechargeRate_A;
    node.setTransmitBuffer(0, uiData);
    writeSingleRegister(modbus_Max_A_discharge, uiData);
#endif
  }
  if (ucNewState == 0) Serial.print(PRINT_RED);
  else Serial.print(PRINT_GREEN);
  Serial.print("\tUse from battery: " + String(ucNewState));
  Serial.println(PRINT_RESET);
}


void setChargeFromGrid(uint8_t ucNewState, uint16_t uiChargeRate_A) {
  static uint8_t ucCurrentState = -1;
  uint8_t ucCapacity;
  bool bChargeEnable;
  if (ucNewState != ucCurrentState) {
    if (ucNewState == inverter_ENABLE) {
      Serial.println("\tEnabling charge of battery from grid");
      ucCapacity = ucBatt_MaxSOC;
      bChargeEnable = true;
    }
    else {
      Serial.println("\tDisabling charge of battery from grid");
      ucCapacity = ucBatt_MinSOC;
      bChargeEnable = false;
    }
 #ifdef NO_INVERTER
    return;
#endif
   for (uint8_t ix=0;ix<6; ix++) {
      node.setTransmitBuffer(ix, uint16_t(ucCapacity));
    }
    writeMultipleRegister(modbus_Time_point_1_capacity_pct, 6);
    delay(10);
    if (bChargeEnable) {
      if (uiChargeRate_A < batt_MaxChargeRate_A) node.setTransmitBuffer(0, uiChargeRate_A);
      else node.setTransmitBuffer(0, uint16_t(batt_MaxChargeRate_A));
      writeMultipleRegister(modbus_Max_A_charge, 1);
    }
    delay(10);
    for (uint8_t ix=0;ix<6; ix++) {
      node.setTransmitBuffer(ix, uint16_t(bChargeEnable));
    }
    writeMultipleRegister(modbus_Time_point_1_charge_enable, 6);
    ucCurrentState = ucNewState; // OBS read back data to verify before setting this
  }
}



void disableCharge(void) {
  uint16_t uiData = 0; // max charge rate = 0A
  Serial.print(PRINT_RED);
  Serial.print("\tDisabling charge of battery");
  Serial.println(PRINT_RESET);
#ifdef NO_INVERTER
    return;
#endif
  node.setTransmitBuffer(0, uiData);
  writeSingleRegister(modbus_Max_A_charge, uiData);
}

void enableCharge(void) {
  uint16_t uiData = batt_MaxChargeRate_A;
  Serial.print(PRINT_GREEN);
  Serial.print("\tEnabling charge of battery");
  Serial.println(PRINT_RESET);
#ifdef NO_INVERTER
    return;
#endif
  node.setTransmitBuffer(0, uiData);
  writeSingleRegister(modbus_Max_A_charge, uiData);
}



#if defined(ESP32S3)
#define RX1_PIN D7 // verify pin numbers
#define TX1_PIN D6
#endif

void inverter_comSetup() {
#ifdef NO_INVERTER
  return;
#endif
  //** modbus **//
  //pinMode(MAX485_RE_NEG, OUTPUT); We use auto dir change
  //pinMode(MAX485_DE, OUTPUT);
  // Init in receive mode
  //digitalWrite(MAX485_RE_NEG, 0);
  //digitalWrite(MAX485_DE, 0);
#if defined(ESP32S3)
  Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);
  // Modbus slave ID 1
  node.begin(1, Serial1);
#elif defined(ESP32)
  Serial2.begin(9600);
  // Modbus slave ID 1
  node.begin(1, Serial2);
#endif
  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

}