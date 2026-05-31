#include <ArduinoJson.h>
#include <NTPClient.h>
#include <SolarCalculator.h>
#include "inverter.h"
#include "userData.h"
#include <WiFi.h>
#include "web.h"
#include "ArduinoNvs.h"
#include "LED.h"


/******* Defines *******/

#define GMT_PLUS_1      3600


#if use_billigkwh
#define jsonBufferSize 2000
#elif use_elprisenligenu
#define jsonBufferSize 8192
#endif                                


#define UPDATE_TEMPERATEURE_INTERVAL_ms (1000 * 60 * 15) // 15 min
#define ONE_HOUR_ms (1000 * 60 * 60) // 60 min
#define UPDATE_INVERTER_READINGS_INTERVAL_ms (200)

//Basic inverter data
int batt_ChargeToday_kWh_x10;
int batt_SoC_pct = 79;
int batt_CurrentCapacity_Wh;
int batt_FreeCapacity_Wh;


/******* Variables *******/

DynamicJsonDocument currentPrices(jsonBufferSize); // allocates memory for json
float priceNowBuy;
float payoutNowSell;

float chargeValue=0;

unsigned long lastUpdateTemperatureTime = UPDATE_TEMPERATEURE_INTERVAL_ms;
unsigned long lastPricesUpdateTime = ONE_HOUR_ms;
unsigned long lastUpdateInverterReadingsTime = UPDATE_INVERTER_READINGS_INTERVAL_ms;

// Historical data
long lLastTotalConsumption_Wh_x100;
int iHourlyConsumption_Wh_x100[24];
long lLastTotalProduction_Wh_x100;
int iHourlyProduction_Wh_x100[24];
float fHourlytDry[24];
long lLastTotalCharge_Wh_x100;
int iActualHourlyChargeRate_Wh_x100[24];






/*** time/date ***/
WiFiUDP myNTPUDP;
const char* myNTPServer = "europe.pool.ntp.org";
//const char* myNTPServer = "time.google.com";

// Returns the day-of-month of the last Sunday in the given month/year
static int lastSundayOfMonth(int year, int month) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month; // day 0 of next month = last day of current month
    t.tm_mday = 0;
    mktime(&t);
    return t.tm_mday - t.tm_wday; // subtract weekday offset (0=Sun)
}

// EU DST: CEST active from last Sunday of March 01:00 UTC to last Sunday of October 01:00 UTC
static bool isCEST(time_t utcEpoch) {
    struct tm *t = gmtime(&utcEpoch);
    int month = t->tm_mon + 1;
    int day   = t->tm_mday;
    int hour  = t->tm_hour;
    int year  = t->tm_year + 1900;

    if (month < 3 || month > 10) return false;
    if (month > 3 && month < 10) return true;

    int lastSun = lastSundayOfMonth(year, month);
    if (month == 3)  return (day > lastSun) || (day == lastSun && hour >= 1);
    /* month == 10 */ return (day < lastSun) || (day == lastSun && hour < 1);
}

long myTimeOffset = GMT_PLUS_1; // updated at runtime to GMT+1 (CET) or GMT+2 (CEST)

/**
 * Adding support to year, month, day from NTPClient
 * Credit: https://github.com/arduino-libraries/NTPClient/issues/36
 * Credit: https://www.geeksforgeeks.org/inheritance-in-c/
 */
class ExtendedNTPClient : public NTPClient {
public:
    ExtendedNTPClient(WiFiUDP &udp, const char *server, long timeOffset)
        : NTPClient(udp, server, timeOffset) {}

    int getYear() {
        time_t rawtime = this->getEpochTime();
        struct tm *ti;
        ti = localtime(&rawtime);
        int year = ti->tm_year + 1900;
        return year;
    }

    int getMonth() {
        time_t rawtime = this->getEpochTime();
        struct tm *ti;
        ti = localtime(&rawtime);
        int month = ti->tm_mon + 1; // tm_mon starts from 0
        return month;
    }

    int getDate() {
        time_t rawtime = this->getEpochTime();
        struct tm *ti;
        ti = localtime(&rawtime);
        int day = ti->tm_mday;
        return day;
    }
};
ExtendedNTPClient timeDateClient(myNTPUDP, myNTPServer, myTimeOffset);







/****** Defrosting *******/
#define DEFROST_RELAY_STATE_ACTIVE    0
#define DEFROST_RELAY_STATE_INACTIVE  1
#define DEFROST_RELAY_STATE_UNKNOWN   2

void checkDefrosting(void){
  float tDry = get_fDMI_temp(PAR_TEMP_DRY);
  Serial.printf("tDry: %f\n", tDry);
  uint8_t ucDefrostRelayState;
  bool bActivateHeat = ((tDry < 2) && (tDry > -2));
  if (bActivateHeat) ucDefrostRelayState = DEFROST_RELAY_STATE_ACTIVE;
  else ucDefrostRelayState = DEFROST_RELAY_STATE_INACTIVE;
  web_SetDefrostRelay(ucDefrostRelayState == DEFROST_RELAY_STATE_ACTIVE);
}




void updateHistory(void){
    // Update historical data
    String sNVSkey;
    uint8_t ucThisHour = timeDateClient.getHours();
    Serial.println("Index: "+(String)ucThisHour);
    // Consumption
    long lTotalConsumption_Wh_x100 = inverter_get_lTotalConsumption_Wh_x100();
    long lConsumptionLastHour_Wh_x100 = lTotalConsumption_Wh_x100 - lLastTotalConsumption_Wh_x100;
    if ((lConsumptionLastHour_Wh_x100 < 0) || (lConsumptionLastHour_Wh_x100 > lInverterMaxProd_w)) lConsumptionLastHour_Wh_x100 = 1; // use 1w to indicate this situation
    lLastTotalConsumption_Wh_x100 = lTotalConsumption_Wh_x100;
    sNVSkey = "lLastTotalConsumption";
    NVS.setInt(sNVSkey, (uint32_t)lTotalConsumption_Wh_x100);
    iHourlyConsumption_Wh_x100[ucThisHour] = /*filter*/(lConsumptionLastHour_Wh_x100);
    sNVSkey = "Cons"+String(ucThisHour);
    NVS.setInt(sNVSkey, iHourlyConsumption_Wh_x100[ucThisHour]);
    Serial.print("Consumption (W): ");
    for (int n = 0; n<24; n++){
      if (n == ucThisHour) Serial.print(PRINT_PURPLE);
      Serial.print((String)(iHourlyConsumption_Wh_x100[n] * 100) + ", ");
      if (n == ucThisHour) Serial.print(PRINT_RESET);
    }
    Serial.println("");

    // Production
    long lTotalProduction_Wh_x100 = inverter_get_lTotalProduction_Wh_x100();
    long lProductionLastHour_Wh_x100 = lTotalProduction_Wh_x100 - lLastTotalProduction_Wh_x100;
    if ((lProductionLastHour_Wh_x100 < 0) || (lProductionLastHour_Wh_x100 > lInverterMaxProd_w)) lProductionLastHour_Wh_x100 = 2; // use 2w to indicate this situation
    lLastTotalProduction_Wh_x100 = lTotalProduction_Wh_x100;
    sNVSkey = "lLastTotalProduction";
    NVS.setInt(sNVSkey, (uint32_t)lTotalProduction_Wh_x100);
    iHourlyProduction_Wh_x100[ucThisHour] = /*filter*/(lProductionLastHour_Wh_x100);
    sNVSkey = "Prod"+String(ucThisHour);
    NVS.setInt(sNVSkey, iHourlyProduction_Wh_x100[ucThisHour]);
    Serial.print("Production (W): ");
    for (int n = 0; n<24; n++){
      if (n == ucThisHour) Serial.print(PRINT_PURPLE);
      Serial.print((String)(iHourlyProduction_Wh_x100[n] * 100) + ", ");
      if (n == ucThisHour) Serial.print(PRINT_RESET);
    }
    Serial.println("");
    
    // Charge rate
    long lTotalCharge_Wh_x100 = inverter_get_iBatt_ChargeToday_Wh_x100(); // should be total total - not total today
    long lPVChargeLastHour_Wh_x100 = lTotalCharge_Wh_x100 - lLastTotalCharge_Wh_x100;     // Grid charge should be sutracted
    if (lPVChargeLastHour_Wh_x100 < 0) lPVChargeLastHour_Wh_x100 = 3; // use 3 to indicate this situation
    lLastTotalCharge_Wh_x100 = lTotalCharge_Wh_x100;
    sNVSkey = "lLastTotalCharge";
    NVS.setInt(sNVSkey, (uint32_t)lTotalCharge_Wh_x100);
    iActualHourlyChargeRate_Wh_x100[ucThisHour] = /*filter*/(lPVChargeLastHour_Wh_x100);
    sNVSkey = "ChRate"+String(ucThisHour);
    NVS.setInt(sNVSkey, iActualHourlyChargeRate_Wh_x100[ucThisHour]);
    Serial.print("Charge (W): ");
    for (int n = 0; n<24; n++){
      if (n == ucThisHour) Serial.print(PRINT_PURPLE);
      Serial.print((String)(iActualHourlyChargeRate_Wh_x100[n] * 100) + ", ");
      if (n == ucThisHour) Serial.print(PRINT_RESET);
    }
    Serial.println("");

    // tDry
    float tDry = get_fDMI_temp(PAR_TEMP_DRY);
    fHourlytDry[ucThisHour] = tDry;
    sNVSkey = "tDry"+String(ucThisHour);
    NVS.setFloat(sNVSkey, fHourlytDry[ucThisHour]);
    Serial.print("tDry: ");
    for (int n = 0; n<24; n++){
      if (n == ucThisHour) Serial.print(PRINT_PURPLE);
      Serial.print((String)fHourlytDry[n]+", ");
      if (n == ucThisHour) Serial.print(PRINT_RESET);
    }
    Serial.println("");
}

 

float get_fElectricityPrice_kr(int iOffsetFromNow_hour) {
  int currentHour = timeDateClient.getHours();
  int iRequestedHour = currentHour + iOffsetFromNow_hour;
  if (iRequestedHour > 23) iRequestedHour -= 24;
  float priceNow = float(currentPrices[0]["priser"][iRequestedHour]);
  //Serial.print("The current consumer price is: "+String(priceNow)+" kr ");
  if (bCommercialPlant) {
    priceNow = (priceNow * 0.8) - elafgift; // Deduct Moms and Elafgift on commercial plants
    //Serial.print("- As a commercial plant we will be paying "+String(priceNow)+" kr excl. taxes ");
  }
  return priceNow;
}

float get_fElectricityPriceMin_kr(void) {
  float fLowestPrice_kr = 999.0;
  for (int n = 0; n <= 23; n++) {
    if (float(currentPrices[0]["priser"][n]) < fLowestPrice_kr) fLowestPrice_kr = float(currentPrices[0]["priser"][n]);
  }
  if (bCommercialPlant) {
    fLowestPrice_kr = (fLowestPrice_kr * 0.8) - elafgift; // Deduct Moms and Elafgift on commercial plants
  }
  return fLowestPrice_kr;
}

float get_fElectricityPriceMax_kr(void) {
  float fHighestPrice_kr = 0.0;
  for (int n = 0; n <= 23; n++) {
    if (float(currentPrices[0]["priser"][n]) > fHighestPrice_kr) fHighestPrice_kr = float(currentPrices[0]["priser"][n]);
  }
  if (bCommercialPlant) {
    fHighestPrice_kr = (fHighestPrice_kr * 0.8) - elafgift; // Deduct Moms and Elafgift on commercial plants
  }
  return fHighestPrice_kr;
}



float getPayoutNowSell() {
  int currentHour = timeDateClient.getHours();
#if use_billigkwh
  float payoutNow = float(currentPrices[0]["spotExMoms"][currentHour]);
  payoutNow -= indfodningsTarrifNetselskab_DKK + indfodningsTarrifEnerginet_DKK + balanceTarrifEnerginet_DKK + balanceTarrifLeverandoer_DKK;
#elif use_elprisenligenu
  float payoutNow = currentPrices[currentHour]["DKK_per_kWh"];
  payoutNow -= indfodningsTarrifNetselskab_DKK + indfodningsTarrifEnerginet_DKK + balanceTarrifEnerginet_DKK + balanceTarrifLeverandoer_DKK;
#endif
  String year = String(timeDateClient.getYear());
  String month = String(timeDateClient.getMonth());
  String day = String(timeDateClient.getDate());
  String hour = String(currentHour);
  Serial.println("Spot price is kr "+String((float)currentPrices[0]["spotExMoms"][currentHour])+" now ("+day+"/"+month+"-"+year+" from "+hour+" to "+String(currentHour+1)+")");
  if (payoutNow < 0) {
    Serial.println("so we will be paying kr "+String(payoutNow*-1)+" / kWh for selling");
  }
  else  {
    Serial.println("so we will receive kr "+String(payoutNow)+" / kWh for selling");
  }
  return payoutNow;
}


void sort_prices(float*fSortData, uint8_t*iOriginalIndex, const uint8_t ucStartHour, const uint8_t ucEndHour)
{
  float hold;
  int holdIndex;
  bool swaps = true;

  // bubble sort
  while (swaps)
  {
    swaps = false;
    // number of comparisons per pass
    for (int i = ucStartHour; i < ucEndHour; i++)
    {
      // compare adjacent elements and swap them if left element is larger than next element to its right
      if (fSortData[i] > fSortData[i + 1])
      {
        swaps = true;
        hold = fSortData[i]; // stores left element, to be swapped into position to right
        fSortData[i] = fSortData[i + 1];
        fSortData[i + 1] = hold;
        holdIndex = iOriginalIndex[i]; // stores left element, to be swapped into position to right
        iOriginalIndex[i] = iOriginalIndex[i + 1];
        iOriginalIndex[i + 1] = holdIndex;
      }
    }
  }
  // move to start of array if nesseccary
  if (ucStartHour > 0) {
    for (int i = ucStartHour; i <= ucEndHour; i++)
    {
      fSortData[i - ucStartHour] = fSortData[i];
      iOriginalIndex[i - ucStartHour] = iOriginalIndex[i];
    }
  }

#ifdef VERBOSE
    // Print the sorted data for debug
  Serial.print("Sorted data = { ");
  for (int a = 0; a <= ucEndHour-ucStartHour; a++) {
    Serial.print(iOriginalIndex[a]);
    Serial.print(":");
    Serial.print(fSortData[a]);
    if (a < ucEndHour-ucStartHour) Serial.print(", ");
  }
  Serial.println(" }");  // end of data set
#endif
}



 void getPossibleSunHoursSunriseSunset(int* sunStart, int*sunEnd)
 {
  // Get today's sunrise and sunset in UTC
  double transit, sunrise, sunset;
  // Using YMD from NTC - could also have been extracted from billigkwh.dk reply
  time_t epochTime = timeDateClient.getEpochTime();
 //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;

  // Calculate the times of sunrise, transit, and sunset, in hours (UTC)
  calcSunriseSunset(currentYear, currentMonth, monthDay, latitude, longitude, transit, sunrise, sunset);
  *sunStart = sunrise + PVDelayFromSunrise_min;
  *sunEnd = sunset + PVDelayFromSunset_min;

  Serial.println();
  Serial.println("Today the sun got up at "+String(sunrise)+" and it will set at "+sunset+" - Giving PV active hours from "+*sunStart+" to "+*sunEnd);
  Serial.println();
 }


 void getPossibleSunHours(int* sunStart, int*sunEnd)
 {
  const int iMinValueToCallItProduction_Wh_x100 = 5;
  // Find first occourence of production (before noon):
  *sunStart = 12;
  for (uint8_t n=0; n<=12; n++){
    if (iHourlyProduction_Wh_x100[n] > iMinValueToCallItProduction_Wh_x100) {
      *sunStart = n;
      break;
    }
  }
  // Find last occourence of production (after noon):
  *sunEnd = 12;
  for (uint8_t n=23; n>=12; n--){
    if (iHourlyProduction_Wh_x100[n] > iMinValueToCallItProduction_Wh_x100) {
      *sunEnd = n;
      break;
    }
  }
  Serial.println();
  Serial.println("We've seen prodution higher than "+String(iMinValueToCallItProduction_Wh_x100*100)+" Wh from "+(String)*sunStart+" to "+(String)*sunEnd);
  Serial.println();
 }



void getBasicInverterData(void){
    batt_ChargeToday_kWh_x10 = inverter_get_iBatt_ChargeToday_Wh_x100();
    batt_SoC_pct = inverter_get_iBatt_SoC_pct();
    batt_CurrentCapacity_Wh = long(batt_SoC_pct * batt_100pct_Wh) / 100;
    batt_FreeCapacity_Wh = long((100-batt_SoC_pct) * batt_100pct_Wh) / 100;
    Serial.println ("Battery SOC is " + String(batt_SoC_pct) + "%, so it holds " + String(batt_CurrentCapacity_Wh) + "Wh and has room for " + String(batt_FreeCapacity_Wh) + " Wh");
}



#define ucMaxNoOfGridChargedHours 12
#define FREE_HOUR 25
uint8_t ucGridChargedForUseHours[ucMaxNoOfGridChargedHours] = {FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR,FREE_HOUR};

void erase_ucGridChargedForUseHours(void) {
  for (uint8_t n = 0; n < ucMaxNoOfGridChargedHours; n++){
    ucGridChargedForUseHours[n] = FREE_HOUR;
  }
}


void avoidSellOnNegativePrices(void){
  // Don't sell PV power if we have to pay to get rid of it
  if (!web_getbPricesValid()) { 
    Serial.println("avoidSellOnNegativePrices disabled"); 
    return; 
  }
  Serial.println("");
  float payout = getPayoutNowSell();
  if (payout < 0.01) inverter_setSellingFromPV(inverter_DISABLE);
  else inverter_setSellingFromPV(inverter_ENABLE);
  Serial.println("");
}



//--- Charge from PV in lowest cost hours if possible ---
void SetCharging(void) {
    int iEstimatedPVChargeRate_Wh;
    int currentHour = timeDateClient.getHours();

/*
Fremgangsmåde med forecast:
Hvis vi sælger fra batteriet, skal vi ikke lade.
start med at se hvor meget vi forventer at kunne lade i timer med negative priser:
(forecast for hour a - historical consumption for hour a) + (forecast for hour b - historical consumption for hour b) + ...
Hvis det er tilstrækkeligt til at lade batteriet helt op, lader vi bare i de timer.
Hvis det ikke er nok, itererer vi over de timer med lavest pris, indtil vi har nok.
*/


/*    // calc estimated possible charge rate based on actual charge last hour (to even out diif in weather) and available for charge, this hour, 24 hours ago
    if (currentHour > 0) iEstimatedPVChargeRate_Wh = (iActualHourlyChargeRate_Wh_x100[currentHour-1] + (iHourlyProduction_Wh_x100[currentHour] - iHourlyConsumption_Wh_x100[currentHour]))/2;
    else iEstimatedPVChargeRate_Wh = (iActualHourlyChargeRate_Wh_x100[23] + iHourlyProduction_Wh_x100[0] - iHourlyConsumption_Wh_x100[0])/2; // properbly always zero
    if (iEstimatedPVChargeRate_Wh < 0) iEstimatedPVChargeRate_Wh = 0;
    iEstimatedPVChargeRate_Wh = iEstimatedPVChargeRate_Wh * 100;
*/
// iEstimatedPVChargeRate_Wh = (int)((fIrradianceFlux_j_m2[this hour] - fIrradianceFlux_j_m2[prev hour]) / fSolarfSolarIrradianceFluxFactor);
// temp. use hardcoded 3/4 max charge rate: 
    iEstimatedPVChargeRate_Wh = ((batt_MaxChargeRate_A * batt_V_Nominel) / 4) * 3;

    Serial.println();
    Serial.println ("Estimated power available for charge this hour ("+String(currentHour)+"): " + String(iEstimatedPVChargeRate_Wh) + " Wh");

    uint8_t ucTimeNeededToFullyCharge_hours;
    if (iEstimatedPVChargeRate_Wh > 0) ucTimeNeededToFullyCharge_hours = uint8_t(round((batt_FreeCapacity_Wh / iEstimatedPVChargeRate_Wh) + 0.5));
    else ucTimeNeededToFullyCharge_hours = 24;
    Serial.println ("With that charge rate, we need " + String(ucTimeNeededToFullyCharge_hours) + " hours to charge fully");  // this is purely based on prices - should also take estimated charge rate into account (find lowes price - how much can we charge there - then next lowest and so on)

    int PVActiveStart_h;
    int PVActiveEnd_h;
    int startHour;
    bool charginCanBeStarted = false;
    getPossibleSunHours(&PVActiveStart_h, &PVActiveEnd_h);
    if (currentHour < PVActiveStart_h) startHour = PVActiveStart_h;
    else startHour = currentHour;
    int endHour = PVActiveEnd_h;
    if (startHour < endHour) {
      if ((endHour - startHour) <= ucTimeNeededToFullyCharge_hours) {
        Serial.println("There are only " + String(endHour - startHour) + " hours left today");
        charginCanBeStarted = true;
      }
      else {
        float loPrice[24];
        uint8_t loHour[24];
        // make a copy of price data
        for (int n= 0; n<24; n++) {
          loPrice[n] = currentPrices[0]["spotExMoms"][n];
          loHour[n] = n;
        }
        // remove already passed hours
        for (int a = 0; a < currentHour; a++) {
          loPrice[a] = 9999;
        }
        // remove hours after PV sunset
        for (int a = PVActiveEnd_h; a < 24; a++) {
          loPrice[a] = 9999;
        }
        sort_prices(loPrice, loHour, 0, 23);
        Serial.println("These hours are most optimal for charging today:"); // this is purely based on prices - should also take estimated charge rate into account (find lowes price - how much can we charge there - then next lowest and so on)
        int cnt = 0;
        while (cnt < ucTimeNeededToFullyCharge_hours) {
          Serial.println("\t" + String(loHour[cnt]) + " (net. kr " + String(loPrice[cnt]) + ")");
          if (loHour[cnt] == currentHour) charginCanBeStarted = true;
          cnt += 1;
        }
      }
    }
    else {
      Serial.println("Charging from PV not possible today");
      charginCanBeStarted = true;
    }

    if (!inverter_getPVSellingState()) {
      charginCanBeStarted = true;
      Serial.println("We're not selling, so we migth as well charge no matter what..");
    }
    if (charginCanBeStarted) {
      enableCharge();
    }
    else { 
      disableCharge();
    }
    Serial.println("");
  }





void setup() {
  Serial.begin(115200); 
// With waiting for Serial, the program won't start until the terminal program has started  
//  while (!Serial) {
//    ;  // wait for serial port to connect. Needed for native USB port only
//  }

  LED_init();
  web_init();
  inverter_comSetup();
  // alive blink
  pinMode(LED_BUILTIN, OUTPUT);
  timeDateClient.begin();
  timeDateClient.update();
  myTimeOffset = isCEST(timeDateClient.getEpochTime() - myTimeOffset) ? 7200 : 3600;
  timeDateClient.setTimeOffset(myTimeOffset);
  // init history
  NVS.begin();
  String sNVSkey;

  Serial.print("Stored Consumption (W): ");
  for (int n = 0; n<24; n++){
    sNVSkey = "Cons"+String(n);
    iHourlyConsumption_Wh_x100[n] = NVS.getInt(sNVSkey); 
    Serial.print((String)(iHourlyConsumption_Wh_x100[n] * 100) + ", ");
  }
  Serial.println();
  sNVSkey = "lLastTotalConsumption";
  lLastTotalConsumption_Wh_x100 = NVS.getInt(sNVSkey);

  Serial.print("Stored Production (W): ");
  for (int n = 0; n<24; n++){
    sNVSkey = "Prod"+String(n);
    iHourlyProduction_Wh_x100[n] = NVS.getInt(sNVSkey); 
    Serial.print((String)(iHourlyProduction_Wh_x100[n] * 100) + ", ");
  }
  Serial.println();
  sNVSkey = "lLastTotalProduction";
  lLastTotalProduction_Wh_x100 = NVS.getInt(sNVSkey);

  Serial.print("Stored Charge (W): ");
  for (int n = 0; n<24; n++){
    sNVSkey = "ChRate"+String(n);
    iActualHourlyChargeRate_Wh_x100[n] = NVS.getInt(sNVSkey); 
    Serial.print((String)(iActualHourlyChargeRate_Wh_x100[n] * 100) + ", ");
  }
  Serial.println();
  sNVSkey = "lLastTotalCharge";
  lLastTotalCharge_Wh_x100 = NVS.getInt(sNVSkey);

  for (int n = 0; n<ucMaxNoOfGridChargedHours; n++){
    sNVSkey = "Ch4Use"+String(n);
    ucGridChargedForUseHours[n] = NVS.getInt(sNVSkey); 
  }


  Serial.print("Stored tDry: ");
  for (int n = 0; n<24; n++){
    sNVSkey = "tDry"+String(n);
    fHourlytDry[n] = NVS.getFloat(sNVSkey); 
    Serial.print((String)fHourlytDry[n] + ", ");
  }
  Serial.println();
  Serial.print(PRINT_RESET);

 }






void loop() {

  LED_update();

  if ((millis() - lastUpdateTemperatureTime) > UPDATE_TEMPERATEURE_INTERVAL_ms) {
    lastUpdateTemperatureTime = millis();
    checkDefrosting();
    Serial.println("");
  }
  
  if ((millis() - lastUpdateInverterReadingsTime) > UPDATE_INVERTER_READINGS_INTERVAL_ms) {
    lastUpdateInverterReadingsTime = millis();
    static uint8_t ucBlock = 1;
    switch (ucBlock++)
    {
    case 1:
      inverter_BatchReadRegisters(modbusBlock01Start, modbusReadingsBlockSize);
      break;
    case 2:
      inverter_BatchReadRegisters(modbusBlock02Start, modbusReadingsBlockSize);
      break;
    case 3:
      inverter_BatchReadRegisters(modbusBlock03Start, modbusReadingsBlockSize);
      break;
    case 4:
      inverter_BatchReadRegisters(modbusBlock04Start, modbusReadingsBlockSize);
      break;
    case 5:
      inverter_BatchReadRegisters(modbusBlock05Start, modbusReadingsBlockSize);
      break;
    
    default:
      ucBlock = 1;
      break;
    }
   
  }

  
  static uint8_t ucPrevHour = 99;
  uint8_t ucCurrentHour = timeDateClient.getHours();
  if (ucCurrentHour != ucPrevHour) {
    ucPrevHour = ucCurrentHour;
    timeDateClient.update();
    long newOffset = isCEST(timeDateClient.getEpochTime() - myTimeOffset) ? 7200 : 3600;
    if (newOffset != myTimeOffset) {
      myTimeOffset = newOffset;
      timeDateClient.setTimeOffset(myTimeOffset);
      timeDateClient.update();
    }
    Serial.println("\n\n----------------------------------------------------------------------------");
    Serial.println("Day: " + String(timeDateClient.getDay())  + " time: " + timeDateClient.getFormattedTime());

    UpdateNordPoolPrices(&currentPrices, timeDateClient.getYear(), timeDateClient.getMonth(), timeDateClient.getDate());
    updateHistory();
    getBasicInverterData();
    avoidSellOnNegativePrices();
    SetCharging();
  }
#ifdef OLD_STUF
    /*
    Grid charge:
    Se 11 timer frem.
    Find laveste pris. Find førstkommende time herfra hvor det kan betale sig at sælge. Marker timer som taget.
    Gentag på næste hele time.
    I PV nattetimer: enable grid charge i "lav" timer.
    I PV dagtimer: enable grid charge i "lav" timer, medmindre batterier kan nå at blive opladet fra PV med gældende PV ladehastighed, inden "høj" time.
    */
     if (batt_SoC_pct <= (2 * ucBatt_MinSOC)) erase_ucGridChargedForUseHours(); // We're low on capacity so see if we can find some new hours to charge for
    // charge from grid in best hours if price is negative:
    Serial.println("\nShould we charge from grid?");
    bool bChargeFromGrid = false;
    uint8_t ucHourToUseTHisCharge;
    if (priceNowBuy <= 0) {
      Serial.println("Yes, Price is negative.");
      bChargeFromGrid = true; //Missing the part with "best hours" to optimize if battery can only absorb some of the free power
    }
    else {
      // Sort prices
      float fSortedPrices[24];
      uint8_t ucSortedHours[24];
      for (uint8_t n= 0; n<24; n++) {
        fSortedPrices[n] = currentPrices[0]["priser"][n];
        ucSortedHours[n] = n;
      }
      sort_prices(fSortedPrices, ucSortedHours, ucCurrentHour, 23); // maybe we should look longer into the future?
      // return data example from sort_prices(): Sorted data = { 23:0.85, 12:0.96, 22:0.96, 11:0.97, 13:0.99, 10:1.00, 21:1.04, 14:1.09, 20:1.20, 15:1.25, 16:1.49, 19:1.49, 17:1.86, 18:1.86 }
      // Test this hour
      /* Test only */Serial.println ("\tPrice\t   Cost to buy now");
      uint8_t ucNoOfHoursFound = 0;
      uint8_t n = 0;
      while (n < 24 - ucCurrentHour) {
        float fPriceHour_n = fSortedPrices[n];
        float fChargeCostIncrease = 0.0;
        bool bPosEarnHourFound = false;
        if (bCommercialPlant) {
          fPriceHour_n = (fPriceHour_n * 0.8) - elafgift; // Deduct Moms and Elafgift on commercial plants
        }
        // Add increased cost the longer time there is until we will use it
        if (ucSortedHours[n] > ucCurrentHour) fChargeCostIncrease = (ucSortedHours[n] - ucCurrentHour - 1) * batt_ChargeCostIncreasePerHour_DKR_kWh;
        float fCostNowBuy = priceNowBuy + batt_WearCost_DKR_kWh + fChargeCostIncrease;
        /* Test only */Serial.print ("kl " + String(ucSortedHours[n]) + ":\t");
        /* Test only */Serial.println (String(fPriceHour_n) +"\t - "+ String(fCostNowBuy) +"\t = "+ String(fPriceHour_n - (fCostNowBuy)));
        if ((fCostNowBuy < fPriceHour_n) && (ucCurrentHour < ucSortedHours[n])) { 
          Serial.println ("Hour with positive earnings found: " + String(ucSortedHours[n]));
          bPosEarnHourFound = true;
          for (uint8_t t = 0; t < ucMaxNoOfGridChargedHours; t++) {
            if (ucGridChargedForUseHours[t] == ucSortedHours[n]){ 
              Serial.println ("..but it's already taken");
              bPosEarnHourFound = false; // hour has already benn taken
            }
          }
          if (bPosEarnHourFound) {
            for (uint8_t t = 0; t < ucMaxNoOfGridChargedHours; t++) { // find an empty place
              if (ucGridChargedForUseHours[t] >= FREE_HOUR) {
                ucGridChargedForUseHours[t] = ucSortedHours[n];  // mark as taken
                bChargeFromGrid = true; // only allow charge if hour can be registred
                break;
              } 
            }
            ucHourToUseTHisCharge = ucSortedHours[n];
          }
        }
 /*240103 grab up to 4 relevant hours */      
        if (bPosEarnHourFound) ucNoOfHoursFound++;
        if (ucNoOfHoursFound >= 4) break; // out of while (n < 24 - ucCurrentHour)
        else n++;
      }
      if (web_Bed1Active() || web_Bed2Active()) bChargeFromGrid = false; //Make sure we never exceede max net tarrif
      if (bChargeFromGrid) {
        uint16_t uiChargeCurrent_A;
 /*240103 charge fully (3k3Wh)        if (iHourlyConsumption_kWh_x10[ucHourToUseTHisCharge] == 0) uiChargeCurrent_A = batt_MaxChargeRate_A; // consumption from battery is not included in iHourlyConsumption_kWh_x10. TODO add decharge from battery
        else uiChargeCurrent_A = (iHourlyConsumption_kWh_x10[ucHourToUseTHisCharge] * 100) / batt_V_Nominel;
        if (uiChargeCurrent_A > batt_MaxChargeRate_A)*/ uiChargeCurrent_A = batt_MaxChargeRate_A;
        setChargeFromGrid(inverter_ENABLE, uiChargeCurrent_A);
      }
      else setChargeFromGrid(inverter_DISABLE, 0);
    }



// Use from batteries
    bool bUseFromBatt = false;
    uint8_t ucNoOfGridChargedHours = 0;
    uint16_t uiChargeReserved_Wh = 0;

    // Have we grid-charged specific for this hour?
    Serial.print("Hours with charge for later use: ");
    for (uint8_t t = 0; t < ucMaxNoOfGridChargedHours; t++) {
      if (ucGridChargedForUseHours[t] < FREE_HOUR) {
        ucNoOfGridChargedHours++;
        uiChargeReserved_Wh += batt_MaxDechargeRate_A * batt_V_Nominel;  // Skal være Faktisk charge i den time i stedet for max charge 
        if (ucGridChargedForUseHours[t] == ucCurrentHour) bUseFromBatt = true;
        Serial.print(String(ucGridChargedForUseHours[t]) + " ");
      }
    }
    Serial.println("");

    if (!bUseFromBatt) {
      // Do we have anything left for free use?
      Serial.println ("This much charge is reserved (Wh): " + String(uiChargeReserved_Wh));
      uint16_t uiChargeFreeToUse_Wh;
      if (batt_CurrentCapacity_Wh > uiChargeReserved_Wh) uiChargeFreeToUse_Wh = batt_CurrentCapacity_Wh - uiChargeReserved_Wh;
      else uiChargeFreeToUse_Wh = 0;
      Serial.println ("This much is available (Wh): " + String(uiChargeFreeToUse_Wh));
      uint16_t uiExpedtedNessecaryForUseThisHour_kWh = batt_MaxDechargeRate_A * batt_V_Nominel;  // Skal være forventet forbrug i denne time i stedet for max decharge 
      Serial.println ("Expected consumption for the next hour is (Wh): " + String(uiExpedtedNessecaryForUseThisHour_kWh));
      if (uiChargeFreeToUse_Wh >= uiExpedtedNessecaryForUseThisHour_kWh) bUseFromBatt = true;
    }
    if (bUseFromBatt) inverter_setUseFromBattery(inverter_ENABLE);
    else inverter_setUseFromBattery(inverter_DISABLE);


/*
Når der skal sælges fra batt:
Sælg fra batt i "høj" timer så længe charge er størrer end forventet forbrug resten af PV natten
Start med at finde den allerhøjeste time og arbejd nedad så længe der er overskud.
Hvis der herefter stadig er overskud, sælges i dyreste time, hvis det kan betale sig i forhold til billigste.
*/


    // Sell from batteries (D):
    Serial.println("");
    // Find hours where prices are higher than batt. cost price:
    float hiPrice[24];
    uint8_t hiHour[24];
    // make a copy of price data
    for (int n= 0; n<24; n++) {
      hiPrice[n] = currentPrices[0]["spotExMoms"][n];
      hiHour[n] = n;
    }
    // remove already passed hours
    ucCurrentHour = timeDateClient.getHours();
    for (int a = 0; a < ucCurrentHour; a++) {
      hiPrice[a] = 0;
    }
    sort_prices(hiPrice, hiHour, 0, 23);
    // How many hours can we sell?
    bool batt_sellingCanBeEnabled = false;
    uint8_t ucNoOffSellingHours;
    uint8_t ucReserveForNight_pct = 60;
    uint8_t ucAmountToSellPerHour_pct = 20;
    if (batt_SoC_pct > ucReserveForNight_pct) {
      ucNoOffSellingHours = ((batt_SoC_pct - ucReserveForNight_pct) / ucAmountToSellPerHour_pct) + 1; 
      Serial.println("In these hours we may sell battery power: ");
      uint8_t ucFoundHours = 0;
      float priceTreshold_DKR = (float)batt_WearCost_DKR_kWh;
      int cnt = 24;
      while ((cnt > 0) && (ucFoundHours < ucNoOffSellingHours)){ 
        cnt --;
        if (hiPrice[cnt] > priceTreshold_DKR) {
          ucFoundHours++;
          Serial.println("\t" + String(hiHour[cnt]) + " (kr " + String(hiPrice[cnt]) + ")");
          if (hiHour[cnt] == ucCurrentHour) batt_sellingCanBeEnabled = true;
          for (uint8_t t = 0; t < ucMaxNoOfGridChargedHours; t++) {
            if (ucGridChargedForUseHours[t] == ucCurrentHour) {
              batt_sellingCanBeEnabled = false; // Can't sell anyway - charge ment for use
              Serial.println("Can't sell anyway - charge ment for use");
            }
          }
        }
      }
    }
    if (batt_sellingCanBeEnabled) inverter_setSellingFromBattery(inverter_ENABLE);
    else inverter_setSellingFromBattery(inverter_DISABLE);


    // Cleanup
    for (uint8_t t = 0; t < ucMaxNoOfGridChargedHours; t++) {
      if (ucGridChargedForUseHours[t] == ucCurrentHour) {
        ucGridChargedForUseHours[t] = FREE_HOUR; // no longer taken
      }
      String sNVSkey = "Ch4Use"+String(t);
      NVS.setInt(sNVSkey, ucGridChargedForUseHours[t]); 
    }


/*  print('-----------------------------------------------')
    print('\n ui - webserver (local) / MQTT - til user settings')
    print("starte op til 4 eksterne forbrugere når prisen er negativ (under limit 1..4)")
    print('-----------------------------------------------')
*/
  }
#endif
}