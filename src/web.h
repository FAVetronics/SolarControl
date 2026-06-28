#ifndef __COMPILE_WEB_H__
#define __COMPILE_WEB_H__

#include <ArduinoJson.h>


const String HOST_NAME = "opendataapi.dmi.dk";
const int HTTP_PORT = 443;
const String PATH_NAME = "/v2/metObs/collections/observation/items";
const String queryString = "?period=latest&stationId=06068&parameterId=";
const String PAR_TEMP_DRY = "temp_dry"; 
const String PAR_TEMP_DEW = "temp_dew"; 


void web_init();
void UpdateNordPoolPrices(DynamicJsonDocument* jsonPrices, int uiYear, int uiMonth, int uiDate);
bool web_getbPricesValid(void);
int web_get_iPossibleProduction_W(void);
float get_fDMI_temp(String sParameter);
void web_SetDefrostRelay(bool bNewState);

bool web_Bed1Active(void);
bool web_Bed2Active(void);

#endif