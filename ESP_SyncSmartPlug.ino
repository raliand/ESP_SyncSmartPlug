#include <Arduino.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include "ESPAsyncUDP.h"
#include <Hash.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "time_ntp.h"
#include "spiffs_functions.h"
#include "ws\things_functions.h"
//#include "plug\things_functions.h"

#define CHIP_ID system_get_chip_id()
#define DBG_OUTPUT_PORT Serial

extern "C"
{
  #include "user_interface.h"
}

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

char nodeName[128] = "SmartPlugEsp";
unsigned int localPort = 2390;    // local port to listen for UDP packets
static bool hasTime = false;

AsyncUDP aUdp;  // A UDP instance to let us send and receive packets over UDP

unsigned long multicast_timer=millis()+1000*5;
unsigned long web_socket_timer=millis()+1000*60*2;
// ntp timestamp
unsigned long ntp_timer=0;
int lastNodeIdx;
int udpCount = 0;

//format bytes
// String formatBytes(size_t bytes){
//   if (bytes < 1024){
//     return String(bytes)+"B";
//   } else if(bytes < (1024 * 1024)){
//     return String(bytes/1024.0)+"KB";
//   } else if(bytes < (1024 * 1024 * 1024)){
//     return String(bytes/1024.0/1024.0)+"MB";
//   } else {
//     return String(bytes/1024.0/1024.0/1024.0)+"GB";
//   }
// }

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".json")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
    path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void multicast_status()
{
  //DBG_OUTPUT_PORT.println("Multicast Status");
  if(multicast_timer < millis()){
    IPAddress ip = WiFi.localIP();
    ip[3] = 255;
    String message = String(CHIP_ID) + ":" + String(nodeName);

    for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
      message += "&";
      message += arrThings[idx].id;
      message += ":" ;
      message += String(arrThings[idx].value);
    }
    aUdp.broadcastTo(message.c_str(),1234);
    multicast_timer = millis()+1000*5;
  }
}

void websocket_status(){
  if(web_socket_timer < millis()){
    for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
      const Thing tmpThing = arrThings[idx];
      char buffer[JSON_OBJECT_SIZE(12)];
      StaticJsonBuffer<JSON_OBJECT_SIZE(12)> jsonBuffer;
      JsonObject &msg = jsonBuffer.createObject();
      msg["command"] = "thing_update";
      msg["nodeId"] = String(CHIP_ID);
      msg["nodeName"] = nodeName;
      msg["thingId"] = tmpThing.id;
      msg["thingName"] = tmpThing.name;
      msg["thingType"] = tmpThing.type;
      msg["override"] = tmpThing.override;
      msg["lastUpdate"] = tmpThing.last_updated;
      msg["value"] = tmpThing.value;
      msg.printTo(buffer, sizeof(buffer));
      webSocket.broadcastTXT(buffer);
    }
    web_socket_timer = millis()+1000*60*2;
  }
}

void sendJson(uint8_t num, String command, String resKey, String response){
  String sJson = "{\"command\":\""+ command +"\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\""+ resKey +"\":"+response+"}";
  DBG_OUTPUT_PORT.println(sJson);
  DBG_OUTPUT_PORT.println(system_get_free_heap_size());
  webSocket.sendTXT(num, sJson);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  DBG_OUTPUT_PORT.println(system_get_free_heap_size());
  switch(type) {
    case WStype_DISCONNECTED:
      DBG_OUTPUT_PORT.printf_P(PSTR("[%u] Disconnected!\n"), num);
    break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      DBG_OUTPUT_PORT.printf_P(PSTR("[%u] Connected from %d.%d.%d.%d url: %s\n"), num, ip[0], ip[1], ip[2], ip[3], payload);
    }
    break;
    case WStype_TEXT: {
      DBG_OUTPUT_PORT.printf_P(PSTR("[%u] get Text: %s\n"), num, payload);
      StaticJsonBuffer<RECIPE_JSON_SIZE*RECIPES_LEN> jsonCommandBuffer;
      JsonObject &response = jsonCommandBuffer.parseObject((char*)payload);
      if (response.success()) {
        if (response["command"].as<String>().compareTo("request_custom") == 0){
          char jResponse[MAX_JSON_SIZE];
          customRequest(response["param"].as<String>().c_str(), jResponse, MAX_JSON_SIZE);
          sendJson(num, "response_custom", "param\":\""+response["param"].as<String>()+"\",\"response", jResponse);
        } else if (response["command"].as<String>().compareTo("request_nodes") == 0){
          char jNodes[MAX_JSON_SIZE];
          serializeNodes(&arrNodes, jNodes, MAX_JSON_SIZE);
          sendJson(num, "response_nodes", "nodes", jNodes);
        } else if (response["command"].as<String>().compareTo("request_things") == 0){
          char jThings[MAX_JSON_SIZE];
          serializeThings(&arrThings, jThings, MAX_JSON_SIZE);
          sendJson(num, "response_things", "things", jThings);
        } else if (response["command"].as<String>().compareTo("request_recipes") == 0){
          char jRecipes[MAX_JSON_SIZE];
          serializeRecipes(&arrRecipes, jRecipes, MAX_JSON_SIZE);
          sendJson(num, "response_recipes", "recipes", jRecipes);
        } else if (response["command"].as<String>().compareTo("save_thing") == 0){
          bool saved = saveThing(&arrThings,response["thing"],ntp_timer);
          sendJson(num, "response_save_thing", "success", String((saved ? "true" : "false")));
        } else if (response["command"].as<String>().compareTo("save_recipe") == 0){
          bool saved = saveRecipe(&arrRecipes,response["recipe"],ntp_timer);
          sendJson(num, "response_save_recipe", "success", String((saved ? "true" : "false")));
        } else if (response["command"].as<String>().compareTo("save_recipes") == 0){
          bool saved = saveRecipes(&arrRecipes,response["recipes"]);
          sendJson(num, "response_save_recipes", "success", String((saved ? "true" : "false")));
        } else if (response["command"].as<String>().compareTo("temp_override") == 0){
          setSkipRecipeId(getFiredRecipeId());
          DBG_OUTPUT_PORT.println("Temp override.");
        } else {
          String sJson = "{\"command\":\"response_default\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\"}";
          webSocket.sendTXT(num, sJson);
        }
      }
    }
    break;
    case WStype_BIN: {
      DBG_OUTPUT_PORT.printf_P(PSTR("[%u] get binary lenght: %u\n"), num, lenght);
      hexdump(payload, lenght);
      webSocket.sendBIN(num, payload, lenght);
    }
    break;
  }
}

void setup() {
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  // Dir dir = SPIFFS.openDir("/");
  // while (dir.next()) {
  //   String fileName = dir.fileName();
  //   size_t fileSize = dir.fileSize();
  //   DBG_OUTPUT_PORT.printf_P(PSTR("FS File: %s, size: %s\n"), fileName.c_str(), formatBytes(fileSize).c_str());
  // }
  // DBG_OUTPUT_PORT.printf("\n");

  init(nodeName);

  Serial.println(F("connected...yeey :)"));
  ntp_timer = getNTPTimestamp();
  ntp_timer -= millis()/1000;  // keep distance to millis() counter

  lastNodeIdx = initNodes(&arrNodes, ntp_timer);
  delay(100);
  initThings(&arrThings, ntp_timer);
  delay(100);
  initRecipes(&arrRecipes, ntp_timer);

  sprintf(nodeName,"%s",arrNodes[0].name);

  DBG_OUTPUT_PORT.print(F("Open http://"));
  DBG_OUTPUT_PORT.print(WiFi.localIP());
  DBG_OUTPUT_PORT.println(F("/ to see the ESP"));
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
    server.send(404, "text/plain", "FileNotFound");
  });

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.begin();
  //server.serveStatic("/", SPIFFS, "/", "no-cache");
  Serial.println(F("HTTP server started"));

  if(aUdp.listenMulticast(IPAddress(239,1,2,3), 1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    aUdp.onPacket([](AsyncUDPPacket packet) {
      if(packet.length() > 1){
        // Serial.print("UDP Packet Type: ");
        // Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
        // Serial.print(", From: ");
        // Serial.print(packet.remoteIP());
        // Serial.print(":");
        // Serial.print(packet.remotePort());
        // Serial.print(", To: ");
        // Serial.print(packet.localIP());
        // Serial.print(":");
        // Serial.print(packet.localPort());
        // Serial.print(", Length: ");
        // Serial.print(packet.length());
        // Serial.print(", Data: ");
        // Serial.write(packet.data(), packet.length());
        // Serial.println();

        char* pPacket = new char[packet.length()+1];
        pPacket[packet.length()] = '\0';
        strncpy(pPacket, (char*)packet.data(), packet.length());
        char* command = strtok(pPacket, "&");
        if(command != 0){
          char* separator = strchr(command, ':');
          if (separator != 0){
            *separator = 0;
            String rNodeId = String(command);
            ++separator;
            String rNodeName = String(separator);
            IPAddress remoteIP = packet.remoteIP();
            int oldIdx = lastNodeIdx;
            lastNodeIdx = addNode(&arrNodes, rNodeId.toInt(), rNodeName.c_str(), packet.remoteIP().toString().c_str());
            if(oldIdx != lastNodeIdx) {
              DBG_OUTPUT_PORT.println(F("sending response_nodes"));
              char jNodes[MAX_JSON_SIZE];
              serializeNodes(&arrNodes, jNodes, MAX_JSON_SIZE);
              String sJson = "{\"command\":\"response_nodes\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\"nodes\":"+jNodes+"}";
              webSocket.broadcastTXT(sJson);
            }
            command = strtok(0, "&");
            while (command != 0){
              char* separator1 = strchr(command, ':');
              if (separator != 0){
                *separator1 = 0;
                int rThingId = atoi(command);
                ++separator1;
                char rThingValue[VALUE_SIZE];
                strcpy(rThingValue, String(separator1).c_str());
                updateRecipes(&arrRecipes, atol(rNodeId.c_str()),rThingId,&rThingValue, ntp_timer);
              }
              command = strtok(0, "&");
            }
          }
        }
      }
    });
  }
}

void loop() {
  processRecipes(&arrThings,&arrRecipes);
  processThings(&arrThings,&arrRecipes,CHIP_ID, ntp_timer);
  multicast_status();
  websocket_status();
  server.handleClient();
  yield();
}
