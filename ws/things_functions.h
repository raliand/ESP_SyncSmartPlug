#ifndef _DEF_THINGS_FUNC_
#define _DEF_THINGS_FUNC_

#include <Ticker.h>
#include <JsonListener.h>
#include <SimpleDHT.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WundergroundClientAsync.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClientAsync.h"
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager

void init(char nodeName[128]);
int initNodes(Node (*ptrNodes)[NODES_LEN],unsigned long ntp_timer);
void initThings(Thing (*ptrThings)[THINGS_LEN], unsigned long ntp_timer);
void initRecipes(Recipe (*ptrRecipes)[RECIPES_LEN], unsigned long ntp_timer);
void processThings(Thing (*ptrThings)[THINGS_LEN], Recipe (*ptrRecipes)[RECIPES_LEN], long nodeId, unsigned long ntp_timer);
void customRequest(const char* command, char* json, size_t maxSize);
//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display, unsigned long ntp_timer);
void updateDataSync(OLEDDisplay *display, unsigned long ntp_timer);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();
void gettemperature(unsigned long ntp_timer);
#endif
