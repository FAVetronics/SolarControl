#include "LED.h"
#include "FastLED.h"
#include "Inverter.h"
#include "userData.h"
#include "web.h"



#define UPDATE_LED_INTERVAL_ms          (1000) // 1 sec

#if defined(ESP32S3)
#define LED_COLOUMNS  4
#define LEDs_PER_COLOUMN  12
#define DATA_PIN 9
#elif defined(ESP32)
#define LED_COLOUMNS  1
#define LEDs_PER_COLOUMN  32
#define DATA_PIN 12
#endif
#define NUM_LEDS (LED_COLOUMNS*LEDs_PER_COLOUMN)

CRGB leds[NUM_LEDS];


// Returnér fysisk pixelnummer ud fra logiske (x(coloumn), y(Row))
int xyToIndex(int x, int y) {
#if defined(ESP32)
  return y;
#endif
#ifdef StartLowerLeftCorner
  // startende i nederste venstre hjørne
  // y=0 er nederst
  // Hver række har LED_COLOUMNS pixels
  int row = y;
  int base = row * LED_COLOUMNS;

  if (row % 2 == 0) {
    // Lige række: venstre -> højre
    return base + x;
  } else {
    // Ulige række: højre -> venstre
    return base + (LED_COLOUMNS - 1 - x);
  }
 #else
  // startende i øverste højre hjørne
  // Vend y så 0 er øverst i stedet for nederst
  int row = LEDs_PER_COLOUMN - 1 - y;
  // Beregn basis for rækken
  int base = row * LED_COLOUMNS;

  // Zigzag-mønster
  if (row % 2 == 0) {
    // Lige række: højre -> venstre
    return base + (LED_COLOUMNS - 1 - x);
  } else {
    // Ulige række: venstre -> højre
    return base + x;
  }
#endif
}


unsigned long lastLEDUpdateTime = UPDATE_LED_INTERVAL_ms;


void LED_coloumn(int iCol, int iVal, int iMax, struct CRGB sColor) {
  assert(iMax);
  long NoLEDOn = (iVal*LEDs_PER_COLOUMN) / iMax;
  for (int n = 0; n < LEDs_PER_COLOUMN; n++) {
    if (n < NoLEDOn) {
      leds[xyToIndex(iCol,n)] = sColor;
    }
    else {
      leds[xyToIndex(iCol,n)] = CRGB::Black;
    }
  }
  FastLED.show();
}



void LED_Stacked3p1(int iCol, int iValBot, int iValMid, int iValTop, float fValPerLED, struct CRGB sBotColor, struct CRGB sMidColor, struct CRGB sTopColor, struct CRGB sTopLEDColor) {
  int iAvailableLEDs = LEDs_PER_COLOUMN;
  if (sTopLEDColor != CRGB::Black) iAvailableLEDs -= 1;
  int iMax = (int)(fValPerLED * iAvailableLEDs);
  assert(iMax);
  int iNoOfBotLEDOn = (long)(iValBot * iAvailableLEDs) / iMax;
  int iNoOfMidLEDOn = (long)(iValMid * iAvailableLEDs) / iMax + iNoOfBotLEDOn;
  int iNoOfTopLEDOn = (long)(iValTop * iAvailableLEDs) / iMax + iNoOfMidLEDOn;
  for (int n = 0; n < iAvailableLEDs; n++) {
    if (n < iNoOfBotLEDOn) {
      leds[xyToIndex(iCol,n)] = sBotColor;
      //Serial.println("White");
    }
    else if (n < iNoOfMidLEDOn) {
      leds[xyToIndex(iCol,n)] = sMidColor;
      //Serial.println("yellow");
    }
    else if (n < iNoOfTopLEDOn) {
      leds[xyToIndex(iCol,n)] = sTopColor;
      //Serial.println("red");
    }
    else {
      leds[xyToIndex(iCol,n)] = CRGB::Black;
    }
  }
  if (sTopLEDColor != CRGB::Black) {
    leds[xyToIndex(iCol,LEDs_PER_COLOUMN - 1)] = sTopLEDColor;
    leds[xyToIndex(iCol,LEDs_PER_COLOUMN - 1)].subtractFromRGB(245);
  }
  FastLED.show();
}


void LED_init(void){
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  if (LED_COLOUMNS > 1) 
  {
    FastLED.setBrightness(3);
  }
  pinMode(LED_BUILTIN, OUTPUT);
}


void LED_showConsumption(int iCol) {
  int iFullScale_W = 24000; // 3 * 230V * 35A
  if (LED_COLOUMNS > 1) iFullScale_W = lInverterMaxProd_w * 1,2;
  float fValPerLED_W = (float)iFullScale_W / (LEDs_PER_COLOUMN-1); //200;
  int iConsumption = inverter_get_iCurrentTotalConsumption_W();
  //Serial.println(iConsumption);
  int iPVProduction = inverter_get_iCurrentProduction_W();
  //Serial.println(iPVProduction);
  int iBattContrib = (int)inverter_get_siBatteryPower_W();
  if ((iBattContrib < 0) || (iBattContrib > 0x7FFF)) iBattContrib = 0;
  //Serial.println(iBattContrib);
  // round up
  iConsumption += (fValPerLED_W / 2);
  iPVProduction += (fValPerLED_W / 2);
  iBattContrib += (fValPerLED_W / 2);
  int iBotVal;
  int iMidVal;
  int iTopVal;
  struct CRGB sTopLED = CRGB::Black;
  if (!inverter_getPVSellingState()) sTopLED = CRGB::Blue; // Price is negative - we're charging the battery and "ExtraLoad" relay is activated
  if (iConsumption < iPVProduction){
    iBotVal = iConsumption;
    iMidVal = 0;
    iTopVal = 0;
    //Serial.println("first");
  }
  else if (iConsumption < (iPVProduction + iBattContrib)){
    iBotVal = iPVProduction;
    iMidVal = iConsumption - iPVProduction;
    iTopVal = 0;
    //Serial.println("mid");
  }
  else {
    iBotVal = iPVProduction;
    iMidVal = iBattContrib;
    iTopVal = iConsumption - iPVProduction - iBattContrib;
    //Serial.println("last");
//    sTopLED = CRGB::Red; // we're bying from the grid and "TurnOffUnnecessaryLoad" relay is activated
  }
  LED_Stacked3p1(iCol, iBotVal, iMidVal, iTopVal, fValPerLED_W, CRGB::Wheat, CRGB::Yellow, CRGB::Red, sTopLED);
}



void LED_showProduction(int iCol) {
  int iFullScale_W = lInverterMaxProd_w;
  if (LED_COLOUMNS > 1) iFullScale_W = lInverterMaxProd_w * 1,2;
  float fValPerLED_W = (float)iFullScale_W / (LEDs_PER_COLOUMN);
  int iPVProduction = inverter_get_iCurrentProduction_W();
  int iPossibleProduction = web_get_iPossibleProduction_W();
  if (iPVProduction > iPossibleProduction) iPossibleProduction = iPVProduction;
  // round up
  iPVProduction += (fValPerLED_W / 2);
  iPossibleProduction += (fValPerLED_W / 2);
  int iBotVal;
  int iMidVal;
  int iTopVal;
  struct CRGB sTopLEDColor = CRGB::Black;
  iBotVal = iPVProduction;
  iMidVal = iPossibleProduction - iPVProduction;
  iTopVal = 0;
  LED_Stacked3p1(iCol, iBotVal, iMidVal, iTopVal, fValPerLED_W, CRGB::Green, CRGB::GreenYellow, CRGB::Black, sTopLEDColor);
}




float get_fElectricityPrice_kr(int iOffsetFromNow_hour);
float get_fElectricityPriceMin_kr(void);
float get_fElectricityPriceMax_kr(void);

void ShowElectricityCost(int iCol) {
  int iMaxPriceToday_Ore = (int)(get_fElectricityPriceMax_kr() * 100);
  int iMinPriceToday_Ore = (int)(get_fElectricityPriceMin_kr() * 100);
  int iFullScale_Ore = iMaxPriceToday_Ore - iMinPriceToday_Ore;
  if (iFullScale_Ore == 0) iFullScale_Ore = 10000;
  float fValPerLED_Ore = (float)iFullScale_Ore / LEDs_PER_COLOUMN;
  int iPriceNow_Ore = (int)(get_fElectricityPrice_kr(0) * 100) - iMinPriceToday_Ore;
  int iPriceNextHour_Ore = (int)(get_fElectricityPrice_kr(1) * 100) - iMinPriceToday_Ore;
  int iBotVal;
  int iMidVal;
  int iTopVal;
  struct CRGB sTopLEDColor = CRGB::Black; // not in use
  if (iPriceNextHour_Ore > iPriceNow_Ore) {
    iBotVal = 0;
    iMidVal = iPriceNow_Ore + (int)fValPerLED_Ore; // bottom LED always on
    iTopVal = iPriceNextHour_Ore - iPriceNow_Ore;
  }
  else if (iPriceNow_Ore > (iPriceNextHour_Ore + fValPerLED_Ore)) { // difference must be visible on LEDs
    iBotVal = iPriceNextHour_Ore + (int)fValPerLED_Ore; // bottom LED always on
    iMidVal = iPriceNow_Ore - iPriceNextHour_Ore;
    iTopVal = 0;
  }
  else {
    iBotVal = 0;
    iMidVal = iPriceNow_Ore + (int)fValPerLED_Ore; // bottom LED always on
    iTopVal = 0;
  }
  LED_Stacked3p1(iCol, iBotVal, iMidVal, iTopVal, fValPerLED_Ore, 0x4F004F, 0xFF4FFF, 0x4F004F, sTopLEDColor);
}



#define LED_SHOW_CONSUMPTION 0
#define LED_SHOW_PRODUCTION 1
#define LED_SHOW_SOC 2

void LED_showChangingColoumns(void) {
  static int iShow = LED_SHOW_CONSUMPTION;
  int iCol = 0;
  switch (iShow) {
    case LED_SHOW_CONSUMPTION:{
      static uint8_t ucNoOfShowings = 0;
      if (LED_COLOUMNS > 1) iCol = LED_SHOW_CONSUMPTION;
      LED_showConsumption(iCol);
      if (++ucNoOfShowings >= 8){
        ucNoOfShowings = 0;
        iShow = LED_SHOW_PRODUCTION;
      }
      break;}
    case LED_SHOW_PRODUCTION:{
      static uint8_t ucNoOfShowings = 0;
      if (LED_COLOUMNS > 1) iCol = LED_SHOW_PRODUCTION;
      LED_showProduction(iCol);
      if (++ucNoOfShowings >= 4){
        ucNoOfShowings = 0;
        iShow = LED_SHOW_SOC;
      }
      break;}
    case LED_SHOW_SOC:
    default:{
      static uint8_t ucNoOfShowings = 0;
      if (LED_COLOUMNS > 1) iCol = LED_SHOW_SOC;
      LED_coloumn(iCol, inverter_get_iBatt_SoC_pct(), 100, CRGB::Blue);
      if (++ucNoOfShowings >= 2){
        ucNoOfShowings = 0;
        iShow = LED_SHOW_CONSUMPTION;
      }
      break;}
  }
}


void ShowAllColoumns(void) {
  LED_showConsumption(LED_SHOW_CONSUMPTION);
  LED_showProduction(LED_SHOW_PRODUCTION);
  LED_coloumn(LED_SHOW_SOC, inverter_get_iBatt_SoC_pct(), 100, CRGB::Blue);
  ShowElectricityCost(3);
}


#ifdef NO_INVERTER
void web_UpdateLocalnetValues(void);
#endif   
 
void LED_update(void) {
  if ((millis() - lastLEDUpdateTime) > UPDATE_LED_INTERVAL_ms) {
#ifdef NO_INVERTER
   static int n = 0;
   if (!n--) {
      web_UpdateLocalnetValues();// temporarely get localnet values from another running module
      n = 10;
   }
#endif   
   if (LED_COLOUMNS > 1) ShowAllColoumns();
    else LED_showChangingColoumns();
    lastLEDUpdateTime = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Builtin Led toggle
  }
}