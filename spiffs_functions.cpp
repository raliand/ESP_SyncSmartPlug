#include "spiffs_functions.h"

bool loadFromFileNew(const char* cFileName, char* json, size_t maxSize) {
  File spifsFile = SPIFFS.open(cFileName, "r");
  if (!spifsFile) {
    DBG_OUTPUT_PORT.println("Failed to open "+String(cFileName)+" file for reading");
    spifsFile.close();
    delay(500);
    return false;
  }
  size_t size = spifsFile.size();
  if (size > maxSize) {
    DBG_OUTPUT_PORT.println(String(cFileName)+" file size is too large");
    spifsFile.close();
    delay(500);
    return false;
  }
  spifsFile.readBytes(json, size);
  delay(500);
  return true;
//  DBG_OUTPUT_PORT.print("Load fromfile ");
//  DBG_OUTPUT_PORT.println(String(buf.get()));
}

bool saveToFile(JsonArray& jArr, const char* cFileName) {
  File spifsFile = SPIFFS.open(cFileName, "w");
  if (!spifsFile) {
    DBG_OUTPUT_PORT.println("Failed to open things file for writing");
    return false;
  }
  jArr.printTo(spifsFile);
  //jArr.printTo(DBG_OUTPUT_PORT);
  spifsFile.close();
  return true;;
}

bool saveNodesToFile(const Node (*ptrNodes)[NODES_LEN]) {
  File spifsFile = SPIFFS.open("/nodes.json", "w");
  if (!spifsFile) {
    DBG_OUTPUT_PORT.println("Failed to open things file for writing");
    return false;
  }

  StaticJsonBuffer<NODE_JSON_SIZE*NODES_LEN> jsonBuffer;
  JsonArray& root = jsonBuffer.createArray();
  for( int idx = 0 ; idx < NODES_LEN ; idx++ ){
    Node tmpNode = (*ptrNodes)[idx];
    if(tmpNode.id != 0 ){
      JsonObject& tmpJNode = root.createNestedObject();
      tmpJNode["id"] = tmpNode.id;
      tmpJNode["name"] = tmpNode.name;
      tmpJNode["ip"] = String(tmpNode.ip);
    }
  }
  root.printTo(spifsFile);
  spifsFile.close();
  return true;
}

bool saveThingsToFile(const Thing (*ptrThings)[THINGS_LEN]) {
  File spifsFile = SPIFFS.open("/things.json", "w");
  if (!spifsFile) {
    DBG_OUTPUT_PORT.println("Failed to open things file for writing");
    return false;
  }

  StaticJsonBuffer<THING_JSON_SIZE*THINGS_LEN> jsonBuffer;
  JsonArray& root = jsonBuffer.createArray();
  for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
    const Thing tmpThing = (*ptrThings)[idx];
    if(tmpThing.id != 0){
      JsonObject& tmpJThing = root.createNestedObject();
      tmpJThing["id"] = tmpThing.id;
      tmpJThing["name"] = tmpThing.name;
      tmpJThing["type"] = tmpThing.type;
      tmpJThing["value"] = String(tmpThing.value).toFloat();
      tmpJThing["override"] = tmpThing.override;
      tmpJThing["last_updated"] = tmpThing.last_updated;
    }
  }
  root.printTo(spifsFile);
  spifsFile.close();
  return true;
}

bool saveRecipesToFile(const Recipe (*ptrRecipes)[RECIPES_LEN]) {
  File spifsFile = SPIFFS.open("/recipes.json", "w");
  if (!spifsFile) {
    DBG_OUTPUT_PORT.println("Failed to open things file for writing");
    return false;
  }

  StaticJsonBuffer<RECIPE_JSON_SIZE*RECIPES_LEN> jsonBuffer;
  JsonArray& root = jsonBuffer.createArray();
  for( int idx = 0 ; idx < RECIPES_LEN ; idx++ ){
    Recipe tmpRecipe = (*ptrRecipes)[idx];
    if(tmpRecipe.id != 0){
      JsonObject& tmpJRecipe = root.createNestedObject();
      tmpJRecipe["id"] = tmpRecipe.id;
      tmpJRecipe["name"] = String(tmpRecipe.name);
      tmpJRecipe["sourceNodeId"] = tmpRecipe.sourceNodeId;
      tmpJRecipe["sourceThingId"] = tmpRecipe.sourceThingId;
      tmpJRecipe["sourceValue"] = tmpRecipe.sourceValue;
      tmpJRecipe["relation"] = tmpRecipe.relation;
      tmpJRecipe["localThingId"] = tmpRecipe.localThingId;
      tmpJRecipe["targetValue"] = tmpRecipe.targetValue;
      tmpJRecipe["localValue"] = tmpRecipe.localValue;
      tmpJRecipe["last_updated"] = tmpRecipe.last_updated;
    }
  }
  root.printTo(spifsFile);
  spifsFile.close();
  root.printTo(DBG_OUTPUT_PORT);
  return true;
}

bool saveRecipe(Recipe (*ptrRecipes)[RECIPES_LEN], JsonObject& recipe, unsigned long ntp_timer){
  //StaticJsonBuffer<RECIPE_JSON_SIZE> jsonBuffer;
  //JsonObject &recipe = jsonBuffer.parseObject(json);
  recipe.printTo(DBG_OUTPUT_PORT);
  int index = recipe["id"].as<int>() - 1;
  (*ptrRecipes)[index].id = recipe["id"].as<int>();
  recipe["name"].as<String>().toCharArray((*ptrRecipes)[index].name,50);
  (*ptrRecipes)[index].sourceNodeId = recipe["sourceNodeId"].as<long>();
  (*ptrRecipes)[index].sourceThingId = recipe["sourceThingId"].as<int>();
  (*ptrRecipes)[index].sourceValue = recipe["sourceValue"].as<float>();
  (*ptrRecipes)[index].relation = recipe["relation"].as<int>();
  (*ptrRecipes)[index].localThingId = recipe["localThingId"].as<int>();
  (*ptrRecipes)[index].targetValue = recipe["targetValue"].as<float>();
  (*ptrRecipes)[index].localValue = recipe["localValue"].as<float>();
  (*ptrRecipes)[index].last_updated = millis()/1000+ntp_timer;
  return saveRecipesToFile(ptrRecipes);
}

void serializeNodes(const Node (*ptrNodes)[NODES_LEN], char* json, size_t maxSize){
    StaticJsonBuffer<NODE_JSON_SIZE*NODES_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.createArray();
    for( int idx = 0 ; idx < NODES_LEN ; idx++ ){
      Node tmpNode = (*ptrNodes)[idx];
      if(tmpNode.id != 0 ){
        JsonObject& tmpJNode = root.createNestedObject();
        tmpJNode["id"] = tmpNode.id;
        tmpJNode["name"] = tmpNode.name;
        tmpJNode["ip"] = String(tmpNode.ip);
      }
    }
    root.printTo(json, maxSize);
}

void serializeThings(const Thing (*ptrThings)[THINGS_LEN], char* json, size_t maxSize){
    StaticJsonBuffer<THING_JSON_SIZE*THINGS_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.createArray();
    for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
      const Thing tmpThing = (*ptrThings)[idx];
      if(tmpThing.id != 0){
        JsonObject& tmpJThing = root.createNestedObject();
        tmpJThing["id"] = tmpThing.id;
        tmpJThing["name"] = tmpThing.name;
        tmpJThing["type"] = tmpThing.type;
        tmpJThing["value"] = String(tmpThing.value).toFloat();
        tmpJThing["override"] = tmpThing.override;
        tmpJThing["last_updated"] = tmpThing.last_updated;
      }
    }
    root.printTo(json, maxSize);
}

void serializeRecipes(const Recipe (*ptrRecipes)[RECIPES_LEN], char* json, size_t maxSize){
    StaticJsonBuffer<RECIPE_JSON_SIZE*RECIPES_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.createArray();
    for( int idx = 0 ; idx < RECIPES_LEN ; idx++ ){
      Recipe tmpRecipe = (*ptrRecipes)[idx];
      if(tmpRecipe.id != 0){
        JsonObject& tmpJRecipe = root.createNestedObject();
        tmpJRecipe["id"] = tmpRecipe.id;
        tmpJRecipe["name"] = String(tmpRecipe.name);
        tmpJRecipe["sourceNodeId"] = tmpRecipe.sourceNodeId;
        tmpJRecipe["sourceThingId"] = tmpRecipe.sourceThingId;
        tmpJRecipe["sourceValue"] = tmpRecipe.sourceValue;
        tmpJRecipe["relation"] = tmpRecipe.relation;
        tmpJRecipe["localThingId"] = tmpRecipe.localThingId;
        tmpJRecipe["targetValue"] = tmpRecipe.targetValue;
        tmpJRecipe["localValue"] = tmpRecipe.localValue;
        tmpJRecipe["last_updated"] = tmpRecipe.last_updated;
      }
    }
    root.printTo(json, maxSize);
}

int deserializeNodes(Node (*ptrNodes)[NODES_LEN], char* json){
    StaticJsonBuffer<NODE_JSON_SIZE*NODES_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.parseArray(json);
    int cuntr = 0;
    int lastINDEX = 0;
    for(JsonArray::iterator it=root.begin(); it!=root.end(); ++it){
        JsonObject& jNode = it->as<JsonObject&>();
        if (jNode["id"].as<int>() != 0) {
          (*ptrNodes)[cuntr].id = jNode["id"];
          (*ptrNodes)[cuntr].name = jNode["name"];
          strcpy((*ptrNodes)[cuntr].ip,jNode["ip"]);
          lastINDEX++;
          cuntr++;
        }
    }
    if (root.success()){
      return lastINDEX;
    }
    return -1;
}

bool deserializeThings(Thing (*ptrThings)[THINGS_LEN], char* json){
    StaticJsonBuffer<THING_JSON_SIZE*THINGS_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.parseArray(json);
    int cuntr = 0;
    for(JsonArray::iterator it=root.begin(); it!=root.end(); ++it){
        JsonObject& jThing = it->as<JsonObject&>();
        if(jThing["id"].as<int>() != 0){
          (*ptrThings)[cuntr].id = jThing["id"];
          (*ptrThings)[cuntr].name = jThing["name"];
          (*ptrThings)[cuntr].type = jThing["type"];
          (*ptrThings)[cuntr].value = jThing["value"];
          (*ptrThings)[cuntr].override = jThing["override"];
          (*ptrThings)[cuntr].last_updated = jThing["last_updated"];
          cuntr++;
        }
    }
    return root.success();
}

bool deserializeRecipes(Recipe (*ptrRecipes)[RECIPES_LEN], char* json){
    StaticJsonBuffer<RECIPE_JSON_SIZE*RECIPES_LEN> jsonBuffer;
    JsonArray& root = jsonBuffer.parseArray(json);
    int cuntr = 0;
    int lastRINDEX = 0;
    for(JsonArray::iterator it=root.begin(); it!=root.end(); ++it){
        JsonObject& jRecipe = it->as<JsonObject&>();
        if(jRecipe["id"].as<int>() != 0){
          (*ptrRecipes)[cuntr].id = jRecipe["id"];
          jRecipe["name"].as<String>().toCharArray((*ptrRecipes)[cuntr].name,50);
          (*ptrRecipes)[cuntr].sourceNodeId = jRecipe["sourceNodeId"];
          (*ptrRecipes)[cuntr].sourceThingId = jRecipe["sourceThingId"];
          (*ptrRecipes)[cuntr].sourceValue = jRecipe["sourceValue"];
          (*ptrRecipes)[cuntr].relation = jRecipe["relation"];
          (*ptrRecipes)[cuntr].localThingId = jRecipe["localThingId"];
          (*ptrRecipes)[cuntr].targetValue = jRecipe["targetValue"];
          (*ptrRecipes)[cuntr].localValue = jRecipe["localValue"];
          (*ptrRecipes)[cuntr].last_updated = jRecipe["last_updated"];
          cuntr++;
          lastRINDEX++;
        }
    }
    if (root.success()){
      return lastRINDEX;
    }
    return -1;
}

void processRecipes(Thing (*ptrThings)[THINGS_LEN], Recipe (*ptrRecipes)[RECIPES_LEN]){
  //DBG_OUTPUT_PORT.print("Processing Recipes... ");

  for( int idx = 0 ; idx < THINGS_LEN ; idx++ ){
    Thing aThing = (*ptrThings)[idx];
    //aThing.value = 0;
    for( int ridx = 0 ; ridx < RECIPES_LEN ; ridx++ ){
      Recipe aRecipe = (*ptrRecipes)[ridx];
      if (aRecipe.localThingId == aThing.id && !aThing.override) {
        //DBG_OUTPUT_PORT.println("Processing Recipe For... " + String(aThing.name));
        switch (aRecipe.relation) {
          case EQUALS: {
              if(aRecipe.sourceValue == aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          case NOT_EQUALS: {
              if(aRecipe.sourceValue != aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          case BIGGER_THAN: {
              if(aRecipe.sourceValue > aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          case SMALLER_THAN: {
              if(aRecipe.sourceValue < aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          case NOT_BIGGER_THAN: {
              if(aRecipe.sourceValue <= aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          case NOT_SMALLER_THAN: {
              if(aRecipe.sourceValue >= aRecipe.targetValue) (*ptrThings)[idx].value = aRecipe.localValue;
          }
          break;
          default :
            break;
        }
      }
    }
  }
}

void updateRecipes(Recipe (*ptrRecipes)[RECIPES_LEN], long rNodeId, int rThingId, float rThingValue, unsigned long ntp_timer){
  for( int ridx = 0 ; ridx < RECIPES_LEN ; ridx++ ){
    if ((*ptrRecipes)[ridx].sourceNodeId == rNodeId && (*ptrRecipes)[ridx].sourceThingId == rThingId) {
      (*ptrRecipes)[ridx].sourceValue = rThingValue;
      (*ptrRecipes)[ridx].last_updated = millis()/1000+ntp_timer;
      break;
    }
  }
}

bool deleteNodes() {
  return SPIFFS.remove("/nodes.json");
}

bool deleteThings() {
  return SPIFFS.remove("/things.json");
}

bool deleteRecipes() {
  return SPIFFS.remove("/recipes.json");
}

void deleteAll() {
  deleteNodes();
  deleteThings();
  deleteRecipes();
}
