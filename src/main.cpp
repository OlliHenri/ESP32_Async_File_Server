/*
  Asynch Fileserver ESP32 20210213

  Formats Flash Memory and mounts LittleFS File System
  IMPORTANT! reset ESP after first installation and if the flash memeory was not formatted !!!

  - shows directory
  - create subdirectory like this: /subdirectory     (no slash at the end)
  - set path to copy file into subdirectory like this:  /subdirectory_path/       (with slash at the end)
  - Remove file
  - File Upload
*/

#include <Arduino.h>

#include <defines.h>

#include <ArduinoOTA.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LITTLEFSEditor.h>
#include <TimeLib.h>
#include <NtpClient.h>

//-- activate the following if needed for BMS
#include <AsyncUDP.h>
//#include <Ticker.h>
//#include <PacketSerial.h>
//#include <cppQueue.h>
//#include <pcf8574_esp.h>
//#include <Wire.h>

#include <Fileserver.hpp>

#define FORMAT_LITTLEFS_IF_FAILED true

extern String webpage;
extern String upload_path;
extern String remove_path;
extern String up_filename;
extern String wsUri; 

//functions declarations
void on_WiFiEvent (system_event_id_t event, system_event_info_t info);
//void processSyncEvent (NTPSyncEvent_t ntpEvent);

// NTP timeserver
#define ONBOARDLED 2 // Built in LED on ESP-12/ESP-07
#define SHOW_TIME_PERIOD 5000
#define NTP_TIMEOUT 1500

int8_t timeZone = 1;
int8_t minutesTimeZone = 0;
const PROGMEM char *ntpServer = "europe.pool.ntp.org";
bool wifiFirstConnected = false;

WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// controll WiFi SYSTEM events
void on_WiFiEvent (system_event_id_t event, system_event_info_t info) {
    Serial.printf ("[WiFi-event] event: %d\n", event);

    switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
        Serial.printf ("Connected to %s. Asking for IP address.\r\n", info.connected.ssid);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        wsUri = "ws://" + IPAddress (info.got_ip.ip_info.ip.addr).toString () + "/ws";
        Serial.printf ("Got IP: %s\r\n", IPAddress (info.got_ip.ip_info.ip.addr).toString ().c_str ());
        Serial.printf ("Connected: %s\r\n", WiFi.status () == WL_CONNECTED ? "yes" : "no");
        digitalWrite (ONBOARDLED, LOW); // Turn on LED
        wifiFirstConnected = true;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.printf ("Disconnected from SSID: %s\n", info.disconnected.ssid);
        Serial.printf ("Reason: %d\n", info.disconnected.reason);
        digitalWrite (ONBOARDLED, HIGH); // Turn off LED
        //NTP.stop(); // NTP sync can be disabled to avoid sync errors
		WiFi.reconnect ();
        break;
    default:
        break;
    }
}

// #######    SKETCH BEGIN    #######
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){  //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_BINARY){
          Serial.println("Binary Data received: ");
 
          for(int i=0; i < info->len; i++) {        // print binary data file
            Serial.printf("%X |", data[i]);         
          }
          Serial.println();

          for(int i=0; i < info->len; i++) {        // print binary data as ASCII
            Serial.printf("%c, ", data[i]);         // somehow the first "{" = 0x7B is not printed but its in binary file ????
          }
          Serial.println();
          client->binary("I got your binary file");   // send message to client
          // ToDo save file to LittleFS, this needs to be in subroutine in Fileserver.cpp and with posibility to edit the path
          String filepath = upload_path;
          if(!(filepath == "/"))  {
            filepath += "/";
          }
          filepath += up_filename;                  // ToDo create union in Fileserver.hpp and get rid of global arduino Strings maybe change to Std::Sting                 
          File myFile = LITTLEFS.open(filepath, "w");
          myFile.write(data,info->len);
          myFile.close();
      }

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
        Serial.printf("%s\n",msg.c_str());
        parseJSONmsg(msg.c_str(), client);                          // receive the websocket JSON string and parse it
        client->text("I got your text message");    // send message to client
      }
      
    } else {        //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < len; i++) {
          msg += (char) data[i];
        }
        Serial.printf("%s\n",msg.c_str());
      } 
      
      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");              // ToDo we need to add what we do with this message
          if(info->message_opcode == WS_BINARY)
            client->binary("I got your binary message");          // ToDo we need to add what we do with this file
        }
      } 
    }  
  }
}


void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
    // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
  //wsUri = "ws://" + WiFi.localIP().toString().c_str() + "/ws";
  wsUri = "ws://" + IPAddress (WiFi.localIP()).toString () + "/ws";
  /*
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(hostName);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(ssid, password);
  }*/
 

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();

  MDNS.addService("http","tcp",80);

  //LITTLEFS.begin();

   if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LITTLEFS Mount Failed");
        return;
    } else {
         Serial.println("LITTLEFS Mounted please send files");
    }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);


  server.addHandler(new LITTLEFSEditor(LITTLEFS, http_username, http_password));

  
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // simple upload page with HTML in program code
  server.on("/my_upload", HTTP_GET, [](AsyncWebServerRequest *request){
    File_Upload();
    request->send(200, "text/html", webpage);
  });

    // upload a file to /upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  // this /loadJSON should be a serveStatic response, pls change it
  server.on("/loadJSON", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LITTLEFS, "/loadJSON2.html", "text/html");    //Send file with html website from filesystem
  });  

  // respond to getJSON() request with websocket URI in JSON
  server.on("/wsUri.json", HTTP_GET, [](AsyncWebServerRequest *request){

      StaticJsonDocument<128> doc;
    
      JsonObject object = doc.to<JsonObject>();
      object["wsUri"] = wsUri;
      // serialize the object and send the result to Serial
      Serial.print("serialized object:");
      serializeJson(doc, Serial);
      Serial.println("");
      char   buffer[200]; // create temp buffer
      size_t len = serializeJson(doc, buffer);  // serialize to buffer

      AsyncResponseStream *response = request->beginResponseStream("application/json");
      //AsyncResponseStream *response = request->beginResponseStream("text/html");
      
      response->print(buffer);    // = (AsyncResponseStream) buffer.c_str();
      request->send(response);
  });

 
  server.serveStatic("/directory.json", LITTLEFS, "/directory.json");

  server.serveStatic("/loadJson2Style.css", LITTLEFS, "/loadJson2Style.css");

  server.serveStatic("/myjQuery/jquery-3.5.1.min.js", LITTLEFS, "/myjQuery/jquery-3.5.1.min.js");

  server.serveStatic("/websoctest1.htm", LITTLEFS, "/websoctest1.htm");

  server.serveStatic("/websoctest2.htm", LITTLEFS, "/websoctest2.htm");
  

  server.on("/mypath", HTTP_POST, [](AsyncWebServerRequest *request){
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    handlePath(request);
    File_Upload();
    //printDirectory("/", 5);
    request->send(200, "text/html", webpage);
  });

  server.on("/remove", HTTP_POST, [](AsyncWebServerRequest *request){
    fileRemove(request);
    webpage = "";
    printDirectory("/", 5);
    close_myJsondir();
    request->send(200, "text/html", webpage);
  });

  //server.serveStatic("/", LITTLEFS, "/").setDefaultFile("index.htm"); // wenn file nicht in LittleFS vorhanden ist dann aus PROGMEMM laden

   // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/register", HTTP_GET, [](AsyncWebServerRequest *request){              
    request->send(LITTLEFS, "/form.html", "text/html");
  });

  server.on("/list_directory", HTTP_GET, [](AsyncWebServerRequest *request){              
    File_Header();
    printDirectory( "/", 5);
    File_Footer();
    request->send(200, "text/html", webpage);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });


  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("onFileUpload says: UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    //handleUpload(request, filename, index, data, len, final);     // write the file to LittleFS 
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  server.begin();

  WiFi.onEvent (on_WiFiEvent);     // controll WiFi System events

  timeClient.begin();
} // end Setup()

void loop(){
  static int i = 0;
  static int last = 0;

  ArduinoOTA.handle();
  ws.cleanupClients();

  if (wifiFirstConnected) {
      wifiFirstConnected = false;
      timeClient.update();
  }

  if ((millis () - last) > SHOW_TIME_PERIOD) {
      //Serial.println(millis() - last);
      last = millis ();
      Serial.print (i); Serial.print (" ");
      Serial.println(timeClient.getFormattedTime());      
      Serial.print ("WiFi is ");
      Serial.print (WiFi.isConnected () ? "connected" : "not connected"); Serial.print (". ");
      Serial.printf ("Free heap: %u\n", ESP.getFreeHeap ());
      i++;
  }
} // end loop()

