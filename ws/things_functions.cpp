#include "spiffs_functions.h"
#include "things_functions.h"
#include "time_ntp.h"
#include <RingBufCPP.h>

#define CHIP_ID system_get_chip_id()
#define DBG_OUTPUT_PORT Serial
#define I2C_DISPLAY_ADDRESS 0x3c
#define SDA_PIN D3
#define SDC_PIN D4
#define DHTTYPE DHT11
#define DHTPIN  D6
#define MAX_NUM_ELEMENTS 30

const int UPDATE_INTERVAL_SECS = 5 * 60; // Update every 10 minutes
const float UTC_OFFSET = 2;
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "ae179b03370d603b";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "ZA";
const String WUNDERGROUND_CITY = "Johannesburg";

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );

//DHT dht(DHTPIN, DHTTYPE, 15); // 11 works fine for ESP8266
SimpleDHT11 dht;

float vdd;  // Values read from sensor
int humidity, temp_f;  // Values read from sensor
char sNodes[MAX_JSON_SIZE];
char sThings[MAX_JSON_SIZE];
char sRecipes[MAX_JSON_SIZE];

unsigned char curr_day;

TimeClientAsync timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClientAsync wunderground(IS_METRIC);

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;
bool sameSecond = false;
bool sameMinute = false;

String lastUpdate = "--";

Ticker ticker;

FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawThingspeak };
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

char static_ip[16] = "10.0.0.56";
char static_gw[16] = "10.0.0.3";
char static_sn[16] = "255.255.255.0";

unsigned long current_time;
unsigned long temp_timer=millis();

struct Temp_humid
{
  int temp;
  int humid;
  unsigned long timestamp;
};

RingBufCPP<struct Temp_humid, MAX_NUM_ELEMENTS> buf;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Wifi Manager");
  display.drawString(64, 20, "Please connect to AP");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "To setup Wifi Configuration");
  display.display();
}

void init(char nodeName[128]){
  // initialize temperature sensor
  //dht.begin();
  // initialize dispaly
  deleteAll();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
  wifiManager.setMinimumSignalQuality();

  //or use this for auto generated name ESP + ChipID

  wifiManager.autoConnect();

  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(nodeName);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  display.init();
  display.clear();
  display.display();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);
  display.drawString(64, 10, "Connecting to WiFi");

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    display.display();

    counter++;
  }
  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);
  ui.init();

  unsigned long ntp_timer = getNTPTimestamp();
  ntp_timer -= millis()/1000;

  char sHistory[MAX_JSON_SIZE];
  loadFromFileNew("/temp_humid.json",sHistory,MAX_JSON_SIZE);
  const size_t bufferSize = JSON_ARRAY_SIZE(MAX_NUM_ELEMENTS) + MAX_NUM_ELEMENTS*JSON_OBJECT_SIZE(3) + 1240;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonArray& root = jsonBuffer.parseArray(sHistory);
  struct Temp_humid th1;
  for(JsonArray::iterator it=root.begin(); it!=root.end(); ++it){
      JsonObject& jNode = it->as<JsonObject&>();
      th1.temp = jNode["temp"].as<int>();
      th1.humid = jNode["humid"].as<int>();
      th1.timestamp = jNode["timestamp"].as<unsigned long>();
      buf.add(th1);
  }
  gettemperature(ntp_timer);
  updateDataSync(&display, ntp_timer);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
}

int initNodes(Node (*ptrNodes)[NODES_LEN],unsigned long ntp_timer){
  IPAddress localIP = WiFi.localIP();
  sNodes[0] = 0;
  int lastNodeIdx = 0;
  if(!loadFromFileNew("/nodes.json",sNodes,MAX_JSON_SIZE) || sNodes[0] == '\0' || sNodes == "[]") {
    (*ptrNodes)[0].id = CHIP_ID;
    String name = "WeatherStationEsp";
    name.toCharArray((*ptrNodes)[0].name,NAME_SIZE);
    sprintf((*ptrNodes)[0].ip,"%u.%u.%u.%u",localIP[0], localIP[1], localIP[2], localIP[3]);
    saveNodesToFile(ptrNodes);
    serializeNodes(ptrNodes, sNodes, NODE_JSON_SIZE*NODES_LEN);
    lastNodeIdx = 1;
    delay(100);
  } else {
    lastNodeIdx = deserializeNodes(ptrNodes,sNodes);
    sprintf((*ptrNodes)[0].ip,"%u.%u.%u.%u",localIP[0], localIP[1], localIP[2], localIP[3]);
    saveNodesToFile(ptrNodes);
  }
  DBG_OUTPUT_PORT.print("sNodes ");
  DBG_OUTPUT_PORT.write(sNodes);
  return lastNodeIdx;
}

void initThings(Thing (*ptrThings)[THINGS_LEN],unsigned long ntp_timer) {
  sThings[0] = 0;
  //gettemperature(ntp_timer);
  current_time = millis()/1000+ntp_timer;
  date_time_t curr_date_time;
  epoch_to_date_time(&curr_date_time, current_time);
  curr_day = curr_date_time.day;
  delay(50);
  if(!loadFromFileNew("/things.json",sThings,MAX_JSON_SIZE) || (sThings[0] == '\0') || sThings == "[]"){
    (*ptrThings)[0].id = 1;
    String("Temperature").toCharArray((*ptrThings)[0].name, NAME_SIZE);
    (*ptrThings)[0].type = TEMP;
    strcpy((*ptrThings)[0].value, String(temp_f).c_str());
    (*ptrThings)[0].override = false;
    (*ptrThings)[0].last_updated = millis()/1000+ntp_timer;
    (*ptrThings)[1].id = 2;
    String("Humidity").toCharArray((*ptrThings)[1].name, NAME_SIZE);
    (*ptrThings)[1].type = HUMID;
    strcpy((*ptrThings)[1].value, String(humidity).c_str());
    (*ptrThings)[1].override = false;
    (*ptrThings)[1].last_updated = millis()/1000+ntp_timer;
    saveThingsToFile(ptrThings);
    serializeThings(ptrThings, sThings, THING_JSON_SIZE*THINGS_LEN);
  } else {
    deserializeThings(ptrThings,sThings);
  }
  DBG_OUTPUT_PORT.print("sThings ");
  DBG_OUTPUT_PORT.println(sThings);
}

void initRecipes(Recipe (*ptrRecipes)[RECIPES_LEN], unsigned long ntp_timer){
  sRecipes[0] = 0;
  if(!loadFromFileNew("/recipes.json",sRecipes,MAX_JSON_SIZE) || sRecipes[0] == '\0' || sRecipes == "[]"){
    (*ptrRecipes)[0].id = 1;
    String name = "Max Watts/Hour";
    name.toCharArray((*ptrRecipes)[0].name,50);
    (*ptrRecipes)[0].localThingId = 1;
    strcpy((*ptrRecipes)[0].localValue, "0");
    (*ptrRecipes)[0].sourceNodeId = CHIP_ID;
    (*ptrRecipes)[0].sourceThingId = 3;
    strcpy((*ptrRecipes)[0].sourceValue, "0");
    (*ptrRecipes)[0].relation = BIGGER_THAN;
    strcpy((*ptrRecipes)[0].targetValue, "20");
    saveRecipesToFile(ptrRecipes);
    delay(100);
    serializeRecipes(ptrRecipes, sRecipes, RECIPE_JSON_SIZE*RECIPES_LEN);
    delay(100);
  } else {
    delay(100);
    deserializeRecipes(ptrRecipes,sRecipes);
  }

  DBG_OUTPUT_PORT.print("sRecipes ");
  DBG_OUTPUT_PORT.println(sRecipes);
}

void processThings(Thing (*ptrThings)[THINGS_LEN], Recipe (*ptrRecipes)[RECIPES_LEN], long nodeId, unsigned long ntp_timer) {
  float currTemp, currHumid;
  unsigned long curr_time = millis()/1000+ntp_timer;
  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display, ntp_timer);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    gettemperature(ntp_timer);
    strcpy((*ptrThings)[0].value, String(temp_f).c_str());
    (*ptrThings)[0].last_updated = curr_time;
    strcpy((*ptrThings)[1].value, String(humidity).c_str());
    (*ptrThings)[1].last_updated = curr_time;
    //delay(remainingTimeBudget);
    yield();
  }
}

void customRequest(const char* command, char* json, size_t maxSize){
  const size_t bufferSize = JSON_ARRAY_SIZE(MAX_NUM_ELEMENTS) + MAX_NUM_ELEMENTS*JSON_OBJECT_SIZE(3) + 1240;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  //StaticJsonBuffer<NODE_JSON_SIZE*NODES_LEN> jsonBuffer;
  JsonArray& root = jsonBuffer.createArray();
  struct Temp_humid * th1;
  for( int idx = 0 ; idx < buf.numElements() ; idx++ ){
    th1 = buf.peek(idx);
    JsonObject& tmpJNode = root.createNestedObject();
    tmpJNode["temp"] = String(th1->temp);
    tmpJNode["humid"] = String(th1->humid);
    tmpJNode["timestamp"] = String(th1->timestamp);
  }
  root.printTo(json, maxSize);
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display, unsigned long ntp_timer) {
  //drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  //drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  //drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  //drawProgress(display, 80, "Updating indoor...");
  //gettemperature(ntp_timer);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  //drawProgress(display, 100, "Done...");
  //delay(10);
}

void updateDataSync(OLEDDisplay *display, unsigned long ntp_timer) {
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 80, "Updating indoor...");
  //gettemperature(ntp_timer);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  //delay(10);
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(55 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(60 + x, 0 + y, "Room");
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 15 + y, String((int)temp_f) + "°C  " + String((int)humidity) + "%");
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  String time = timeClient.getFormattedTime().substring(0, 5);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, time);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void gettemperature(unsigned long ntp_timer) {
  if(temp_timer >= millis()) return;
  Serial.println(F("Updating temp and humid..."));
  struct Temp_humid th;
  struct Temp_humid full;
  current_time = millis()/1000+ntp_timer;
  byte b_temperature = 0;
  byte b_humidity = 0;
  if (dht.read(DHTPIN, &b_temperature, &b_humidity, NULL)) {
    Serial.println(F("Read DHT11 failed."));
    temp_timer = millis()+1000;
    return;
  }
  humidity = (int)b_humidity;
  temp_f = (int)b_temperature;
  th.temp = temp_f;
  th.humid = humidity;
  th.timestamp = current_time;
  if(buf.isFull()) buf.pull(&full);
  buf.add(th);
  const size_t bufferSize = JSON_ARRAY_SIZE(MAX_NUM_ELEMENTS) + MAX_NUM_ELEMENTS*JSON_OBJECT_SIZE(3) + 1240;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonArray& root = jsonBuffer.createArray();
  struct Temp_humid * th1;
  for( int idx = 0 ; idx < buf.numElements() ; idx++ ){
    th1 = buf.peek(idx);
    JsonObject& tmpJNode = root.createNestedObject();
    tmpJNode["temp"] = String(th1->temp);
    tmpJNode["humid"] = String(th1->humid);
    tmpJNode["timestamp"] = String(th1->timestamp);
  }
  saveToFile(root, "/temp_humid.json");
  temp_timer = millis()+60000;
  Serial.println(ESP.getFreeHeap());
  // float tmpTemp, tmpHumid;
  // tmpTemp = dht.readTemperature(false);
  // tmpHumid = dht.readHumidity();
  // if(!isnan(tmpTemp)) temp_f = tmpTemp;
  // if(!isnan(tmpHumid)) humidity = tmpHumid;
  // humidity = dht.readHumidity();          // Read humidity (percent)
  // temp_f = dht.readTemperature(false);     // Read temperature as Fahrenheit
}
