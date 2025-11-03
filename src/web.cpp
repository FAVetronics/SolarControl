#include <WiFi.h>
#include <HTTPClient.h>
#include "web.h"
#include "userData.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "string.h"
#include <WiFiClientSecure.h>
#include <NTPClient.h>

#include "Inverter.h"

float getPayoutNowSell();


// user data
const char* host = "billigkwh.dk";
const int httpsPort = 443;
const char* url = "/api/Priser/HentPriser?sted=DK1&netselskab=ikast_el_net_as_c&produkt=vindstoed_elforbundet_2025";

const char* ssid = "FAVetronics2G4";
//  const char* ssid = "FAVetronicsMobileAP";
//  const char* ssid = "Mads13";
const char* password = "AAAAAAAA";

// Set Static IP address
#if defined(ESP32S3)
IPAddress local_IP(192, 168, 1, 102);
#elif defined(ESP32)
IPAddress local_IP(192, 168, 1, 101);
#endif
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(194,239,134,83); // TDC - optional
IPAddress secondaryDNS(193,162,153,164); // TDC - optional



AsyncWebServer server(80);


bool bPricesValid = false;

bool web_getbPricesValid(void) {
  return bPricesValid;
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}



void web_init() {
   // Configure static IP address begin
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  // Configure static IP address end
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("http request");
    request->send(200, "text/html", "<!DOCTYPE html><html>\
 <head>\
  <title> Inverter controller </title>\
  <meta http-equiv=\"refresh\" content=\"10\">\
  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\
  <style type=\"text/css\">\
   html, body {\
             outline: 0px none;\
              margin: 0 0 0 0;\
              border: 0px none;\
             padding: 0 0 0 0;\
          background: #f7f7f7;\
               color: #000000;\
         font-family: Times New Roman, serif;\
         font-weight: normal;\
           font-size: 100%;\
   }\
   table {\
             outline: 0px none;\
              margin: 0 0 0 0;\
              border: 1px transparent;\
             padding: 0.25em 1em 0.25em 1em;\
     border-collapse: collapse;\
   }\
   .name {\
          text-align: left;\
      vertical-align: text-top;\
               color: #666666;\
   }\
   .value {\
          text-align: left;\
      vertical-align: text-top;\
    }\
   .box {\
               float: center;\
               outline: 0.25em transparent;\
              margin: 0.25em;\
              border: 0.05em solid #000000;\
             padding: 0.5em 1em 0.5em 1em;\
          background: #ffffff;\
    }\
  </style>\
 </head>\
 <body>\
  <div class=\"box\">\
   <table>\
    <tr> <td class=\"name\"> Current production (W): </td> <td class=\"value\">          "+(String)inverter_get_iCurrentProduction_W()+"           </td></tr>\
    <tr> <td class=\"name\"> Current consumption (W): </td> <td class=\"value\">         "+(String)inverter_get_iCurrentTotalConsumption_W()+"          </td></tr>\
    <tr> <td class=\"name\"> Battery SoC %: </td> <td class=\"value\">                 "+(String)inverter_get_iBatt_SoC_pct()+"                  </td></tr>\
    <tr> <td class=\"name\"> Batt_ChargeToday_kWh_x10: </td> <td class=\"value\">     "+(String)inverter_get_iBatt_ChargeToday_Wh_x100()+"      </td></tr>\
    <tr> <td class=\"name\"> iBatteryPower_W: </td> <td class=\"value\">              "+(String)inverter_get_siBatteryPower_W()+"                </td></tr>\
    <tr> <td class=\"name\"> Tdry: </td> <td class=\"value\">                         "+(String)get_fDMI_temp(PAR_TEMP_DRY)+"                   </td></tr>\
    <tr> <td class=\"name\"> Payout now: </td> <td class=\"value\">                   "+(String)getPayoutNowSell()+"                            </td></tr>\
   </table>\
  </div>\
  <div class=\"box\">\
        <label for=\"WearCost_DKR_kWh\" class=\"formbuilder-number-label\">WearCost DKR/kWh</label>\
        <input type=\"number\" class=\"form-control\" name=\"WearCost_DKR_kWh\" access=\"false\" value=\""+(String)batt_WearCost_DKR_kWh+"\" id=\"WearCost_DKR_kWh\">\
  </div>\
 </body>\
</html>"
/*    "\nCurrentProduction_W: \t\t"+(String)inverter_get_iCurrentProduction_W()+
    "\nCurrentConsumption_W: \t\t"+(String)inverter_get_iCurrentConsumption_W()+
    "\nCurrentAvailableProduction_W: \t"+(String)inverter_get_iCurrentAvailableProduction_W()+
    "\nBatt_SoC_pct: \t\t\t"+(String)inverter_get_iBatt_SoC_pct()+
    "\nBatt_ChargeToday_kWh_x10: \t"+(String)inverter_get_iBatt_ChargeToday_kWh_x10()+
    "\nBatteryPower_W: \t\t"+(String)inverter_get_iBatteryPower_W()+
    \"*/);
  });

  server.on("/values", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("values request");
    request->send(200, "text/html", "<html><head><meta name=\"color-scheme\" content=\"light dark\"><meta charset=\"utf-8\"></head><body><pre>[\n\
  {\n\
    \"CurrentProduction_W\" : "+(String)inverter_get_iCurrentProduction_W() + ",\n\
    \"CurrentConsumptionTotal_W\" : "+(String)inverter_get_iCurrentTotalConsumption_W() + ",\n\
    \"CurrentGridPower_W\" : "+({int tmp=inverter_get_iCurrentGridPower_W(); (tmp > 0x7FFF) ? "-"+(String)(0xFFFF-tmp+1) : (String)tmp; })+ ",\n\
    \"CurrentBatteryPower_W\" : "+({int tmp=inverter_get_siBatteryPower_W(); (tmp > 0x7FFF) ? "-"+(String)(0xFFFF-tmp+1) : (String)tmp; })+ ",\n\
    \"Batt_SoC_pct\" : "+(String)inverter_get_iBatt_SoC_pct() + ",\n\
    \"Batt_ChargeToday_Wh_x100\" : "+(String)inverter_get_iBatt_ChargeToday_Wh_x100() + ",\n\
    \"SellFromPVStatus\" : "+(String)inverter_getPVSellingState() + ",\n\
    \"TotalInverterProduction_Wh_x100\" : "+(String)inverter_get_lTotalProduction_Wh_x100() + ",\n\
    \"TotalInverterConsumption_Wh_x100\" : "+(String)inverter_get_lTotalConsumption_Wh_x100() + "\n\
  }\n\
]\n</pre><div class=\"json-formatter-container\"></div></body></html>");
});

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("settings request");
    request->send(200, "text/html", "<html><head><meta name=\"color-scheme\" content=\"light dark\"><meta charset=\"utf-8\"></head><body><pre>[\n\
  {\n\
    \"BatteryVNom_V_x100\" : "+(String)inverter_get_iBatt_V_Nominel_V_x100() + ",\n\
    \"BatteryCapacity_Ah\" : "+(String)inverter_get_iBatteryCapacity_Ah() + ",\n\
    \"BatteryMaxCharge_Ah\" : "+(String)inverter_get_iBatteryMaxCharge_A() + ",\n\
    \"BatteryMaxDischarge_Ah\" : "+(String)inverter_get_iBatteryMaxDischarge_A() + ",\n\
    \"BatteryMinSOC_pct\" : "+(String)inverter_get_iBatteryMinSOC_pct() + "\n\
  }\n\
]\n</pre><div class=\"json-formatter-container\"></div></body></html>");
});

  server.onNotFound(notFound);

  server.begin();
 }


 #ifdef NO_INVERTER
// get inverter values from external source - test only
void inverter_set_extCurrentProduction_W(int iVal);
void inverter_set_extCurrentConsumption_W(int iVal);
void inverter_set_extGridPower_W(int iVal);
void inverter_set_extBattPower_W(int iVal);
void inverter_set_extSOC_pct(int iVal);

 void web_UpdateLocalnetValues(void)
 {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED) {
      HTTPClient http;
      http.begin("http://192.168.1.101/values");
/*      http.addHeader("Accept", "application/json");
      http.addHeader("Content-Type", "application/json");
      http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36");
*/      // Send HTTP GET request
      int httpResponseCode = http.GET();
      if (httpResponseCode == HTTP_CODE_OK) {
//        Serial.print("HTTP Response code: ");
//        Serial.println(httpResponseCode);
        String payload = http.getString();
//        Serial.println(payload);
        StaticJsonDocument<1024> doc;
        int start = payload.indexOf('[');
        int end   = payload.lastIndexOf(']');
        String jsonPart = payload.substring(start, end + 1);
//        Serial.println(jsonPart);
        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, jsonPart);
        // Test if parsing succeeds.
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        JsonArray arr = doc.as<JsonArray>();
        JsonObject obj = arr[0];      
        inverter_set_extCurrentProduction_W(obj["CurrentProduction_W"]);
        inverter_set_extCurrentConsumption_W(obj["CurrentConsumptionTotal_W"]);
        inverter_set_extGridPower_W(obj["CurrentGridPower_W"]);
        inverter_set_extBattPower_W(obj["CurrentBatteryPower_W"]);
        inverter_set_extSOC_pct(obj["Batt_SoC_pct"]);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
 }
#endif



 #if use_billigkwh
 void UpdateNordPoolPrices(DynamicJsonDocument* jsonPrices, int uiYear, int uiMonth, int uiDate)
 {
  if (WiFi.status()== WL_CONNECTED) {
    bPricesValid = false;
  ///////////////////////// Bygget over forslag fra claus Elmann (WiFiClientSecure) ///////////////////////////
    WiFiClientSecure client;
  //Warning: Might expire Mar 12, 2027
  const char* root_ca =  "-----BEGIN CERTIFICATE-----\n\
MIIFBTCCAu2gAwIBAgIQS6hSk/eaL6JzBkuoBI110DANBgkqhkiG9w0BAQsFADBP\n\
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy\n\
Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa\n\
Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF\n\
bmNyeXB0MQwwCgYDVQQDEwNSMTAwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n\
AoIBAQDPV+XmxFQS7bRH/sknWHZGUCiMHT6I3wWd1bUYKb3dtVq/+vbOo76vACFL\n\
YlpaPAEvxVgD9on/jhFD68G14BQHlo9vH9fnuoE5CXVlt8KvGFs3Jijno/QHK20a\n\
/6tYvJWuQP/py1fEtVt/eA0YYbwX51TGu0mRzW4Y0YCF7qZlNrx06rxQTOr8IfM4\n\
FpOUurDTazgGzRYSespSdcitdrLCnF2YRVxvYXvGLe48E1KGAdlX5jgc3421H5KR\n\
mudKHMxFqHJV8LDmowfs/acbZp4/SItxhHFYyTr6717yW0QrPHTnj7JHwQdqzZq3\n\
DZb3EoEmUVQK7GH29/Xi8orIlQ2NAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG\n\
MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/\n\
AgEAMB0GA1UdDgQWBBS7vMNHpeS8qcbDpHIMEI2iNeHI6DAfBgNVHSMEGDAWgBR5\n\
tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG\n\
Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD\n\
VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B\n\
AQsFAAOCAgEAkrHnQTfreZ2B5s3iJeE6IOmQRJWjgVzPw139vaBw1bGWKCIL0vIo\n\
zwzn1OZDjCQiHcFCktEJr59L9MhwTyAWsVrdAfYf+B9haxQnsHKNY67u4s5Lzzfd\n\
u6PUzeetUK29v+PsPmI2cJkxp+iN3epi4hKu9ZzUPSwMqtCceb7qPVxEbpYxY1p9\n\
1n5PJKBLBX9eb9LU6l8zSxPWV7bK3lG4XaMJgnT9x3ies7msFtpKK5bDtotij/l0\n\
GaKeA97pb5uwD9KgWvaFXMIEt8jVTjLEvwRdvCn294GPDF08U8lAkIv7tghluaQh\n\
1QnlE4SEN4LOECj8dsIGJXpGUk3aU3KkJz9icKy+aUgA+2cP21uh6NcDIS3XyfaZ\n\
QjmDQ993ChII8SXWupQZVBiIpcWO4RqZk3lr7Bz5MUCwzDIA359e57SSq5CCkY0N\n\
4B6Vulk7LktfwrdGNVI5BsC9qqxSwSKgRJeZ9wygIaehbHFHFhcBaMDKpiZlBHyz\n\
rsnnlFXCb5s8HKn5LsUgGvB24L7sGNZP2CX7dhHov+YhD+jozLW2p9W4959Bz2Ei\n\
RmqDtmiXLnzqTpXbI+suyCsohKRg6Un0RC47+cpiVwHiXZAW+cn8eiNIjqbVgXLx\n\
KPpdzvvtTnOPlC7SQZSYmdunr3Bf9b77AiC/ZidstK36dRILKz7OA54=\n\
-----END CERTIFICATE-----\n";
    client.setCACert(root_ca);
    client.setInsecure(); // midlertidigt
  
    Serial.println("\nConnecting...");
    if (!client.connect(host, httpsPort)) {
      Serial.println("Connection failed");
      return;
    }
  
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Connection: close\r\n\r\n");
  
    while (client.connected()) {
    if (client.available())
    {
        char c = client.read();
        if (c == '\n') break;
    }
//      String line = client.readStringUntil('\n');
//      if (line == "\r") break;
    }
    String body = client.readString();
  ////////////////////////////////////////////////////

    // Find the index of the first [ character
    int index = body.indexOf('[');
    if (index != -1) {
      // Remove anything before the first [
      body.remove(0, index);
    }
    //Serial.println("Data fetched:");
    //Serial.println(body);
    DeserializationError error = deserializeJson(*jsonPrices,  body);
    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    Serial.println("Prices fetched OK.");
    bPricesValid = true;
  }
  else {
    Serial.println("WiFi Disconnected - retrying...");
    web_init();
  }
}

#elif use_elprisenligenu

void UpdateNordPoolPrices(DynamicJsonDocument* jsonPrices, int uiYear, int uiMonth, int uiDate)
 {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED) {
      HTTPClient http;
      http.begin("https://www.elprisenligenu.dk/api/v1/prices/2025/05-13_DK1.json");
      http.addHeader("Accept", "application/json");
      http.addHeader("Content-Type", "application/json");
      http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36");
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        // Deserialize the JSON document
        DeserializationError error = deserializeJson(*jsonPrices, payload);
        // Test if parsing succeeds.
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
 }

#endif



//returns irradiance for the last 10 min
//only updated every 10 min
char metobs_url[400];
bool fetchPreviousSolarActual(int* pSolarLastHour_W_m2)
{
  HTTPClient http;
  snprintf(metobs_url, sizeof(metobs_url),
  "https://dmigw.govcloud.dk/v2/metObs/collections/observation/items?period=latest&stationId=06068&parameterId=radia_glob&api-key=40688928-55b1-462b-856d-a92bda386bb0");
// to get average for the last hour:  "https://dmigw.govcloud.dk/v2/metObs/collections/observation/items?period=latest&stationId=06068&parameterId=radia_glob_last1h&api-key=40688928-55b1-462b-856d-a92bda386bb0");
  //Serial.println("constructed URL:");
//  Serial.println(metobs_url);
  http.setTimeout(15000);  //API is sometimes a little bit slow.
  http.begin(metobs_url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
//    Serial.println("Response:");
//    Serial.println(payload);
 
    DynamicJsonDocument doc(4000); //UTF-16 representation
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print("metobs JSON deserialization failed: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }
    int value = doc["features"][0]["properties"]["value"];
    //Serial.print("pSolarLast10Min_W_m2: ");
    //Serial.println(value);
    *pSolarLastHour_W_m2 = value;
    http.end();
    return true;
  }
  //Else, basically
  Serial.printf("HTTP request failed, code: %d\n", httpCode);
  http.end();
  return false;
}



WiFiUDP thisNTPUDP;
const char* thisNTPServer = "europe.pool.ntp.org";
NTPClient thistimeDateClient(thisNTPUDP, thisNTPServer);

int web_get_iPossibleProduction_W(void) {
  static int iMetObsIrradiance = -1;
  static int iPossibleProduction;
  static int iLastMin = -1;
  int iCurrentMin = thistimeDateClient.getMinutes();
  if ((iMetObsIrradiance == -1) || ((iLastMin / 10) != (iCurrentMin / 10))) { // Update every 10 min
    fetchPreviousSolarActual(&iMetObsIrradiance);
    iPossibleProduction = int((float)iMetObsIrradiance * 10.94f);
    Serial.print("Possible production the last 10 Min (W): ");
    Serial.println(iPossibleProduction);
  }
  iLastMin = iCurrentMin;
  return iPossibleProduction;
}


float get_fDMI_temp(String sParameter)
{
  if (WiFi.status()== WL_CONNECTED) {
    //Serial.print("Fetching data from DMI: ");
    String url = "http://" + HOST_NAME + PATH_NAME + queryString + sParameter;
    WiFiClient client;
    HTTPClient http;
    http.useHTTP10(true);
    http.begin(client, url);
//    http.begin(url.c_str());
    http.addHeader("X-Gravitee-Api-Key", API_KEY);
//    http.useHTTP10(true);
    int httpCode = http.GET();
    if (httpCode <= 0)
    {
      /// TODO: ERROR
      Serial.println(F("Getting temp data from DMI failed!"));
      http.end();
      return -99.8;
    }
    String payload = http.getString(); // [  9883][E][WiFiClient.cpp:517] flush(): fail on fd 49, errno: 128, "Socket is not connected"
    http.end();

    /// https://arduinojson.org/v6/assistant/
    DynamicJsonDocument doc(1536);
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return -99.9;
    }
    JsonObject features_0 = doc["features"][0];
    JsonObject features_0_properties = features_0["properties"];
    float features_0_properties_value = features_0_properties["value"];
    return features_0_properties_value;
    }
  else {
    Serial.println("WiFi Disconnected - retrying...");
    web_init();
  }
  return -99.0;
}


const String defrostRelayURL = "http://" + defrostRelayIP + "/relay/0";
const String DefrostRelayTurnOnRequest = "turn=on";
const String DefrostRelayTurnOffRequest = "turn=off";

void web_SetDefrostRelayPOST(bool bNewState) { // works with shelly plug
  if (WiFi.status()== WL_CONNECTED) {
    Serial.print("Updating Defrost Relay: ");
    String sRequest;
    if (bNewState) sRequest = DefrostRelayTurnOnRequest;
    else sRequest = DefrostRelayTurnOffRequest;
    WiFiClient client;
    HTTPClient http;
    http.begin(client, defrostRelayURL);
    int httpCode = http.POST(sRequest);
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    if (httpCode <= 0){
      /// TODO: ERROR
      Serial.println(F("Updating Defrost Relay failed!"));
    }
    else {
      String payload = http.getString();
      Serial.println(payload);
    }
    http.end();
    }
  else {
    Serial.println("WiFi Disconnected - retrying...");
    web_init();
  }
}

void web_SetDefrostRelayGET(bool bNewState) {
  if (WiFi.status()== WL_CONNECTED) {
    Serial.print("Updating Defrost Relay: ");
    String sRequest;
    if (bNewState) sRequest = DefrostRelayTurnOnRequest;
    else sRequest = DefrostRelayTurnOffRequest;
    WiFiClient client;
    HTTPClient http;
    http.begin(client, defrostRelayURL+"?"+sRequest);
    int httpCode = http.GET();
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    if (httpCode <= 0){
      /// TODO: ERROR
      Serial.println(F("Updating Defrost Relay failed!"));
    }
    else {
      String payload = http.getString();
      Serial.print(payload);
    }
    http.end();
    }
  else {
    Serial.println("WiFi Disconnected - retrying...");
    web_init();
  }
}


void web_SetDefrostRelay(bool bNewState) {
#ifdef NO_INVERTER
  return;
#endif
  if (bDefrostRelayMethod == METHOD_GET) web_SetDefrostRelayGET(bNewState);
  else web_SetDefrostRelayPOST(bNewState);
}



bool web_Bed1Active(void) {
  return false;
}

bool web_Bed2Active(void) {
  return false;
}