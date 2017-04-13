#include "spiffs_functions.h"
#include "things_functions.h"
#include "time_ntp.h"

#define CHIP_ID system_get_chip_id()

TM1637Display display(CLK, DIO);


// ------- Curent variables ----------
long  range = 2; // este es el rango por el que se disparará la salida 2 y pasa a estado lógico 1
long  lastValue; // contiene el valor de la última medición que disparó a lógico 1, la salida 2
int   cycle = 0; // 1=alto 0=bajo
int   cycleChange = 0;
int   maxVoltage;
int   minVoltage = 1023;
long  contadorvisualizacion;
long  cycleCounter;
bool  continuar;
const unsigned long sampleTime = 1000000UL;                           // sample over 100ms, it is an exact number of cycles for both 50Hz and 60Hz mains
const unsigned long numSamples = 250UL;                               // choose the number of samples to divide sampleTime exactly, but low enough for the ADC to keep up
const unsigned long sampleInterval = sampleTime / numSamples;
long adc_zero;
unsigned long currentAcc;
unsigned int count;
unsigned long prevMicros;
long startMicros;
float dwh = 0;

char sNodes[NODE_JSON_SIZE*NODES_LEN];
char sThings[THING_JSON_SIZE*THINGS_LEN];
char sRecipes[RECIPE_JSON_SIZE*RECIPES_LEN];
// ---------------------------------------

// Auxiliary functions for measuring voltage and current zero point
int determineVQ(int PIN) {
  DBG_OUTPUT_PORT.print(F("Estemating voltage zero point: "));
  long VQ = 0;
  for (int i = 0; i < 5000; i++) {
    VQ += analogRead(PIN);
    delay(1);//depends on sampling (on filter capacitor), can be 1/80000 (80kHz) max.
  }
  VQ /= 5000;
  DBG_OUTPUT_PORT.print(map(VQ, 0, 1023, 0, 5000)); Serial.println(F(" mV"));
  return int(VQ);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void init(char nodeName[128]){
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect();

  String hostname(nodeName);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
  }


  // unsigned long ntp_timer = getNTPTimestamp();
  // ntp_timer -= millis()/1000;

  pinMode(sensorIn, INPUT);
  pinMode(switchOut, OUTPUT);
  adc_zero = determineVQ(sensorIn); //Quiscent output voltage - the average voltage ACS712 shows with no load (0 A)
  startMicros = micros();
  currentAcc = 0;
  count = 0;
  prevMicros = micros() - sampleInterval ;
}

int initNodes(Node (*ptrNodes)[NODES_LEN],unsigned long ntp_timer){
  IPAddress localIP = WiFi.localIP();
  sNodes[0] = 0;
  int lastNodeIdx = 0;
  display.setBrightness(0x0f);
  if(!loadFromFileNew("/nodes.json",sNodes,NODE_JSON_SIZE*NODES_LEN) || sNodes[0] == '\0') {
    (*ptrNodes)[0].id = CHIP_ID;
    String name = "SmartPlugEsp";
    name.toCharArray((*ptrNodes)[0].name,NAME_SIZE);
    //strcpy((*ptrNodes)[0].name, "SmartPlugEsp");
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
  DBG_OUTPUT_PORT.println(sNodes);
  display.showNumberDec(localIP[3], false);
  return lastNodeIdx;
}

void initThings(Thing (*ptrThings)[THINGS_LEN],unsigned long ntp_timer){
  sThings[0] = 0;
  delay(500);
  if(!loadFromFileNew("/things.json",sThings,THING_JSON_SIZE*THINGS_LEN) || (sThings[0] == '\0')){
    (*ptrThings)[0].id = 1;
    String("Plug").toCharArray((*ptrThings)[0].name, NAME_SIZE);
    (*ptrThings)[0].type = SWITCH;
    strcpy((*ptrThings)[0].value, "0");
    (*ptrThings)[0].override = false;
    (*ptrThings)[0].last_updated = millis()/1000+ntp_timer;
    (*ptrThings)[1].id = 2;
    String("Current Wats").toCharArray((*ptrThings)[1].name, NAME_SIZE);
    (*ptrThings)[1].type = GENERIC;
    strcpy((*ptrThings)[1].value, "0");
    (*ptrThings)[1].override = false;
    (*ptrThings)[1].last_updated = millis()/1000+ntp_timer;
    (*ptrThings)[2].id = 3;
    String("Daily Wats per Hour").toCharArray((*ptrThings)[2].name, NAME_SIZE);
    (*ptrThings)[2].type = GENERIC;
    strcpy((*ptrThings)[2].value, "1");
    (*ptrThings)[2].override = false;
    (*ptrThings)[2].last_updated = millis()/1000+ntp_timer;
    (*ptrThings)[3].id = 4;
    String("Time of day").toCharArray((*ptrThings)[3].name, NAME_SIZE);
    (*ptrThings)[3].type = CLOCK;
    strcpy((*ptrThings)[3].value, String(seconds_since_midnight(millis()/1000+ntp_timer)).c_str());
    (*ptrThings)[3].override = false;
    (*ptrThings)[3].last_updated = millis()/1000+ntp_timer;
    saveThingsToFile(ptrThings);
    serializeThings(ptrThings, sThings, THING_JSON_SIZE*THINGS_LEN);
  } else {
    deserializeThings(ptrThings,sThings);
  }
  DBG_OUTPUT_PORT.print("sThings ");
  DBG_OUTPUT_PORT.println(sThings);
  delay(100);
  strcpy((*ptrThings)[0].value, "0");
}

void initRecipes(Recipe (*ptrRecipes)[RECIPES_LEN], unsigned long ntp_timer){
  sRecipes[0] = 0;
  if(!loadFromFileNew("/recipes.json",sRecipes,RECIPE_JSON_SIZE*RECIPES_LEN) || sRecipes[0] == '\0'){
    (*ptrRecipes)[0].id = 1;
    String name = "Max Watts/Hour";
    name.toCharArray((*ptrRecipes)[0].name,NAME_SIZE);
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

void processThings(Thing (*ptrThings)[THINGS_LEN], Recipe (*ptrRecipes)[RECIPES_LEN], long nodeId, unsigned long ntp_timer){
  long sensorValue = 0;
  unsigned long curr_time = millis()/1000+ntp_timer;
  digitalWrite(switchOut, String((*ptrThings)[0].value).toFloat());
  strcpy((*ptrThings)[3].value, String(seconds_since_midnight(curr_time)).c_str());
  (*ptrThings)[3].last_updated = curr_time;

  if (micros() - startMicros > sampleTime) {// Displays to the serial port the results, after one second
    //sensorValue = analogRead(sensorIn);
    range = (2 + ((maxVoltage - minVoltage) / 5));
    maxVoltage = 0; //sensorValue;
    minVoltage = 1024; //sensorValue;
    float rms = sqrt((float)currentAcc / (float)count) * (83.3333 / 1024.0); //75.7576
    float vrms = rms * 240;
    if (cycleCounter < 48 || cycleCounter > 52 || vrms < 300) vrms = 0;
    display.showNumberDec(round(vrms), false);
    strcpy((*ptrThings)[1].value, String(vrms).c_str());
    (*ptrThings)[1].last_updated = curr_time;
    updateRecipes(ptrRecipes, nodeId, 2, &(*ptrThings)[1].value, curr_time);
    dwh = dwh + (vrms / 3600);
    strcpy((*ptrThings)[2].value, String(dwh).c_str());
    (*ptrThings)[2].last_updated = curr_time;
    updateRecipes(ptrRecipes, nodeId, 3, &(*ptrThings)[2].value, curr_time);
    cycleCounter = 0;
    startMicros = micros();
    currentAcc = 0;
    count = 0;
    prevMicros = micros() - sampleInterval;
    updateRecipes(ptrRecipes, nodeId, 1, &(*ptrThings)[0].value, curr_time);
    updateRecipes(ptrRecipes, nodeId, 4, &(*ptrThings)[3].value, curr_time);
  }
  if (micros() - prevMicros >= sampleInterval){
    sensorValue = analogRead(sensorIn);
    long adc_raw = sensorValue - adc_zero;
    currentAcc += (unsigned long)(adc_raw * adc_raw);
    ++count;
    prevMicros += sampleInterval;
    if (sensorValue >= ( lastValue + range) ) {
      lastValue = sensorValue;
      cycle = 1;
      if (sensorValue > maxVoltage) {
        maxVoltage = sensorValue;
      }
    }
    if (sensorValue <= ( lastValue - range)) {
      lastValue = sensorValue;
      cycle = 0;
      if (sensorValue < minVoltage) {
        minVoltage = sensorValue;
      }
    }
    if (cycle == 1 && cycleChange == 0) {
      cycleChange = 1;
      cycleCounter++;
    }
    if (cycle == 0 && cycleChange == 1) {
      cycleChange = 0;
    }
  } else {
    delay(10);
  }
  //delay(1);
}

void customRequest(const char* command, char* json, size_t maxSize){
  loadFromFileNew(String("/"+String(command)+".json").c_str(),json,maxSize);
}
