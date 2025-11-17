#ifndef __COMPILE_USERDATA_H__
#define __COMPILE_USERDATA_H__


//#define NO_INVERTER 1
#undef NO_INVERTER

#define METHOD_GET  1 // Shelly Plus 1
#define METHOD_POST 0 // Shelly Plug S
const String defrostRelayIP = "192.168.2.123";
const bool bDefrostRelayMethod = METHOD_GET;

#define use_billigkwh       true
#define use_elprisenligenu false


// find latitude & longitude here: https://www.latlong.net
const double latitude = 56.096680;
const double longitude = 9.204950;
const double PVDelayFromSunrise_min = (double)2.30;
const double PVDelayFromSunset_min = (double)0.30;

const long lInverterMaxProd_w = 12000;

const uint8_t ucBatt_MinSOC = 7;
const uint8_t ucBatt_MaxSOC = 99;
const int batt_MaxChargeRate_A = 68; // users setting could be read from inverter at "start" so that user doesn't need to set up two places...
const int batt_V_Nominel = 48;
const int batt_MaxDechargeRate_A = 68;
const int batt_100pct_Wh = 2*8800;
const double batt_price_DKR = 30960; // ex moms
const double batt_life_cycles = 8000; // 4000 to 80% capacity
const double batt_WearCost_DKR_kWh = batt_price_DKR / batt_life_cycles / (double)batt_100pct_Wh * 1000; //(0,22 with 8000h)
const double batt_ChargeCostIncreasePerHour_DKR_kWh = batt_WearCost_DKR_kWh / 10; // The cost increases 10% per hour into the furture

const float elafgift = 0.72;
const float indfodningsTarrifNetselskab_DKK = 0.0;  // 250513 kl 12:00 excl moms
const float indfodningsTarrifEnerginet_DKK = 0.061; //
const float balanceTarrifEnerginet_DKK = 0.074;     //
const float balanceTarrifLeverandoer_DKK = 0.2275;  //

const bool bCommercialPlant = true;

#define POWER_ZERO_LIMIT_W 50 // Values below this is treated as 0W

// 36 paneler á 1,9m2 = 68,4m2
// Panel effektivitet, opgivet: 20,3%
// Systemeffktivitet, estimeret: 16%
// convertion J/m2 -> w/m2: /3600
// solar irradiance flux factor
// (68,4 * 0,16) / 3600 = 10,94 / 3600 = 0,00304
// multiply the increase in flux with this factor to get estimated hourly production in W
const float fSolarIrradianceFluxFactor = 0.00304;
// optimering:
// start med at sammenligne den faktisk mulige prod. lige nu med observationen for at få den aktuelle virkningsgrad - uafhængigt af vinkler, støv og skidt, ældning, temperatur, etc.
// kræver at anlæget sættes til fuld prod. et kort øjeblik (fx 30 sek)


#endif