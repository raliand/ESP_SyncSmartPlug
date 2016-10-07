#include <Arduino.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUDP.h>
#include <Hash.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <TM1637Display.h>
#include <ESP8266SSDP.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include "time_ntp.h"
#include "spiffs_functions.h"
#include "things_functions.h"

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

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP

unsigned long multicast_timer=millis()+5000;
unsigned long web_socket_timer=millis()+1000;
// ntp timestamp
unsigned long ntp_timer=0;
int lastNodeIdx;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

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
  if(path.endsWith("/")) path += "index.htm";
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
  String message = String(CHIP_ID) + ":" + String(nodeName);
  IPAddress ip = WiFi.localIP();
  ip[3] = 255;
  for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
    const Thing tmpThing = arrThings[idx];
    message += "&";
    message += tmpThing.id;
    message += ":" ;
    message += String(tmpThing.value);
  }
  Udp.beginPacket(ip, localPort);
  Udp.print(message);
  Udp.endPacket();
  //  udp.print(message);
  //  DBG_OUTPUT_PORT.println(system_get_free_heap_size());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  DBG_OUTPUT_PORT.println(system_get_free_heap_size());
  switch(type) {
    case WStype_DISCONNECTED:
    DBG_OUTPUT_PORT.printf("[%u] Disconnected!\n", num);
    break;
    case WStype_CONNECTED:
    {
      IPAddress ip = webSocket.remoteIP(num);
      DBG_OUTPUT_PORT.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    }
    break;
    case WStype_TEXT: {
      DBG_OUTPUT_PORT.printf("[%u] get Text: %s\n", num, payload);
      StaticJsonBuffer<500> jsonCommandBuffer;
      JsonObject &response = jsonCommandBuffer.parseObject((char*)payload);
      if (response.success()) {
        if (response["command"].as<String>().compareTo("request_nodes") == 0){
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
          char jNodes[NODE_JSON_SIZE*NODES_LEN];
          serializeNodes(&arrNodes, jNodes, NODE_JSON_SIZE*NODES_LEN);
          String sJson = "{\"command\":\"response_nodes\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\"nodes\":"+jNodes+"}";
          webSocket.sendTXT(num, sJson);
          //DBG_OUTPUT_PORT.println(sJson);
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else if (response["command"].as<String>().compareTo("request_things") == 0){
          char jThings[THING_JSON_SIZE*THINGS_LEN];
          serializeThings(&arrThings, jThings, THING_JSON_SIZE*THINGS_LEN);
          String sJson = "{\"command\":\"response_things\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\"things\":"+jThings+"}";
          webSocket.sendTXT(num, sJson);
          DBG_OUTPUT_PORT.println(sJson);
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else if (response["command"].as<String>().compareTo("request_recipes") == 0){
          char jRecipes[RECIPE_JSON_SIZE*RECIPES_LEN];
          serializeRecipes(&arrRecipes, jRecipes, RECIPE_JSON_SIZE*RECIPES_LEN);
          String sJson = "{\"command\":\"response_recipes\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\"recipes\":"+jRecipes+"}";
          webSocket.sendTXT(num, sJson);
          DBG_OUTPUT_PORT.println(sJson);
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else if (response["command"].as<String>().compareTo("save_thing") == 0){
          StaticJsonBuffer<200> jsonBuffer;
          JsonObject &thing = jsonBuffer.parseObject(response["thing"].as<String>());
          int index = thing["id"].as<int>() - 1;
          //aThings[index].name = thing["name"].as<String>().c_str();
          arrThings[index].type = thing["type"].as<int>();
          strcpy(arrThings[index].value, thing["value"]);
          arrThings[index].override = thing["override"].as<bool>();
          arrThings[index].last_updated = millis()/1000+ntp_timer;
          bool saved = saveThingsToFile(&arrThings);
          DBG_OUTPUT_PORT.println("Updated thing value.");
          String sJson = "{\"command\":\"response_save_thing\",\"nodeId\":"+String(CHIP_ID)+",\"thingId\":\""+thing["id"].as<int>()+"\",\"success\":"+(saved ? "true" : "false")+"}";
          webSocket.sendTXT(num, sJson);
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else if (response["command"].as<String>().compareTo("save_recipe") == 0){
          bool saved = saveRecipe(&arrRecipes, response["recipe"],ntp_timer);
          DBG_OUTPUT_PORT.println("Updated recipe value.");
          String sJson = "{\"command\":\"response_save_recipe\",\"nodeId\":"+String(CHIP_ID)+",\"nodeName\":\""+nodeName+"\",\"success\":"+(saved ? "true" : "false")+"}";
          webSocket.sendTXT(num, sJson);
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else if (response["command"].as<String>().compareTo("temp_override") == 0){
          setSkipRecipeId(getFiredRecipeId());
          DBG_OUTPUT_PORT.println("Temp override.");
          DBG_OUTPUT_PORT.println(system_get_free_heap_size());
        } else {
          StaticJsonBuffer<200> jsonBuffer;
          JsonObject &msg = jsonBuffer.createObject();
          char buffer[200];
          msg["command"] = "response_default";
          msg["nodeId"] = CHIP_ID;
          msg["nodeName"] = nodeName;
          msg.printTo(buffer, sizeof(buffer));
          webSocket.sendTXT(num, buffer);
        }
        Udp.begin(localPort);
        //break;
      }
    }
    break;
    case WStype_BIN:
    DBG_OUTPUT_PORT.printf("[%u] get binary lenght: %u\n", num, lenght);
    hexdump(payload, lenght);

    // echo data back to browser
    webSocket.sendBIN(num, payload, lenght);
    break;
  }
}

void setup() {
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  DBG_OUTPUT_PORT.printf("\n");

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi

  Serial.println("connected...yeey :)");
  ntp_timer = getNTPTimestamp();
  ntp_timer -= millis()/1000;  // keep distance to millis() counter

  lastNodeIdx = initNodes(&arrNodes, ntp_timer);
  delay(100);
  initThings(&arrThings, ntp_timer);
  delay(100);
  initRecipes(&arrRecipes, ntp_timer);

  //nodeName = arrNodes[0].name;
  sprintf(nodeName,"%s",arrNodes[0].name);
  MDNS.begin(nodeName);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(nodeName);
  DBG_OUTPUT_PORT.println(".local to see the file browser");
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
    server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Udp.begin(localPort);
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Starting SSDP...\n");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("Philips hue clone");
  SSDP.setSerialNumber("001788102201");
  SSDP.setURL("index.htm");
  SSDP.setModelName("Philips hue bridge 2012");
  SSDP.setModelNumber("929000226503");
  SSDP.setModelURL("http://www.meethue.com");
  SSDP.setManufacturer("Royal Philips Electronics");
  SSDP.setManufacturerURL("http://www.philips.com");
  SSDP.begin();


  DBG_OUTPUT_PORT.println("HTTP server started");
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
  int noBytes = Udp.parsePacket();
  if ( noBytes ) {
    DBG_OUTPUT_PORT.print(epoch_to_string(millis()/1000+ntp_timer));
    DBG_OUTPUT_PORT.print(":Packet of ");
    DBG_OUTPUT_PORT.print(noBytes);
    DBG_OUTPUT_PORT.print(" received from ");
    DBG_OUTPUT_PORT.print(Udp.remoteIP());
    DBG_OUTPUT_PORT.print(":");
    DBG_OUTPUT_PORT.println(String(Udp.remotePort()));
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,noBytes); // read the packet into the buffer
    char* command = strtok(packetBuffer, "&");
    if(command != 0){
      char* separator = strchr(command, ':');
      if (separator != 0){
        // Actually split the string in 2: replace ':' with 0
        *separator = 0;
        String rNodeId = String(command);
        ++separator;
        String rNodeName = String(separator);
        arrNodes[lastNodeIdx].id = rNodeId.toInt();
        arrNodes[lastNodeIdx].name = rNodeName.c_str();
        //arrNodes[lastNodeIdx].ip = Udp.remoteIP().toString().c_str();
        IPAddress remoteIP = Udp.remoteIP();
        sprintf(arrNodes[lastNodeIdx].ip,"%u.%u.%u.%u",remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
        //saveNodesToFile(&arrNodes);
        lastNodeIdx++;
        DBG_OUTPUT_PORT.println("");
        command = strtok(0, "&");
        while (command != 0){
          char* separator1 = strchr(command, ':');
          if (separator != 0)
          {
            // Actually split the string in 2: replace ':' with 0
            *separator1 = 0;
            int rThingId = atoi(command);
            ++separator1;
            char rThingValue[VALUE_SIZE];// = String(separator1);
            strcpy(rThingValue, String(separator1).c_str());
            updateRecipes(&arrRecipes, atol(rNodeId.c_str()),rThingId,&rThingValue, ntp_timer);
            saveRecipesToFile(&arrRecipes);
            // Do something with servoId and position  vb
          }
          // Find the next command in input string
          command = strtok(0, "&");
        }
      }
    }
  } // end if packet received
  processRecipes(&arrThings,&arrRecipes);
  processThings(&arrThings,&arrRecipes,CHIP_ID, ntp_timer);
  if(multicast_timer < millis()){
    multicast_status();
    multicast_timer = millis()+5000;
  }
  if(web_socket_timer < millis()){
    for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
      const Thing tmpThing = arrThings[idx];
      //if (tmpThing.id == 2 || tmpThing.id == 3){
        char buffer[JSON_OBJECT_SIZE(10)];
        StaticJsonBuffer<JSON_OBJECT_SIZE(10)> jsonBuffer;
        JsonObject &msg = jsonBuffer.createObject();
        msg["command"] = "thing_update";
        msg["nodeId"] = String(CHIP_ID);
        msg["nodeName"] = nodeName;
        msg["thingId"] = tmpThing.id;
        msg["thingName"] = tmpThing.name;
        msg["lastUpdate"] = millis()/1000+ntp_timer;
        msg["value"] = tmpThing.value;
        msg.printTo(buffer, sizeof(buffer));
        //msg.printTo(DBG_OUTPUT_PORT);
        //DBG_OUTPUT_PORT.print("\n");
        webSocket.broadcastTXT(buffer);
      //}
    }
    web_socket_timer = millis()+1000;
  }
  webSocket.loop();
}
