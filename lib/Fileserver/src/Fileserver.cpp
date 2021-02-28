/*
    Fileserver LittleFS using ESP32 Asynchrone HTTP server with websockets and HTTP
*/

#include <Fileserver.hpp>

// variables for printdirectory()
String myJsondir = "{\"directory\":[";
// String wsUri = "";
extern AsyncWebSocket ws;
String webpage;
String upload_path = "/";
String remove_path = "/";
String wsUri = "";


void confirmRemove(void)  {
  // Create an object and serialize it
  // allocate the memory for the document
  //const size_t CAPACITY = JSON_OBJECT_SIZE(1);
  StaticJsonDocument<128> doc;
  
  JsonObject object = doc.to<JsonObject>();
  object["command"] = "removefile";
  object["task"] = "removed";

  // serialize the object and send the result to Serial
  Serial.print("serialized object:");
  serializeJson(doc, Serial);
  Serial.println("");
  char   buffer[200]; // create temp buffer
  size_t len = serializeJson(doc, buffer);  // serialize to buffer

  ws.textAll(buffer, len); // send buffer to web socket
}

// handle JSON msg coming from client
void parseJSONmsg(String msg)  {

  // Create an object and serialize it
  // allocate the memory for the document
  //const size_t CAPACITY = JSON_OBJECT_SIZE(1);
  StaticJsonDocument<1024> doc;

  DeserializationError error = deserializeJson(doc, msg);
      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

  // extract the data
  JsonObject jobject = doc.as<JsonObject>();
  // parse Fileserver LITTLEFS upload path
  const char* task = jobject["task"];         //deserialize json object, task defines the details
  const char* command = jobject["command"];   // command contains the option what to do
  Serial.print("deserialized json object is:");
  Serial.println(task);
  Serial.print("deserialized json object is:");
  Serial.println(command);
  if (strcmp(command, "setpath") == 0) {      // compare command to find what JSON we receive
    upload_path = String(task);               // set the Filesystem UPLOAD path
    Serial.println("upload path is set!");
  }
  // remove file command
  if (strcmp(command, "removefile") == 0) {       // compare command to find what JSON we receive       
    Serial.println("remove path is set!");
    ws_fileRemove(String(task));                  // set the Filesystem remove path
  }
  // refresh directory file command
  if (strcmp(command, "loaddir") == 0) {       // compare command to find what JSON we receive 
    webpage = "";
    printDirectory("/",5);                     // refresh the directory.json file
    close_myJsondir();
    Serial.println("refresh directory.json is called!");
  }  
}

void handlePath(AsyncWebServerRequest *request)
{
  //List all parameters (Compatibility)
  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
  }

   Serial.println("handle Path function");
  if (request->hasParam("mypath", true))
  {
    AsyncWebParameter *myway = request->getParam("mypath", true);

    upload_path = myway->value();
    Serial.print("handle Path is: ");
    Serial.println(upload_path);
    if(!LITTLEFS.exists(upload_path))  {      // if path does not exist -> create it
        Serial.println("mkdir bcs does not exists");
        if(LITTLEFS.mkdir(upload_path)) {
            Serial.println("mkdir success");
        }else {
            Serial.println("mkdir No success");
        }
    }
  }
  else
  {
    request->send(500, "text/plain", "Missing parameters");
  }
}

void ws_fileRemove(String removepath)  {
    if(!LITTLEFS.exists(removepath))  {            // if path does not exist -> return
        Serial.println("path/file does not exist!");
        return;
    }
    File file = LITTLEFS.open(removepath, FILE_WRITE);
    
      if(file.isDirectory()){
        Serial.println("remove directory");
        file.close();
        LITTLEFS.rmdir(removepath);     // remove directory
      }
      else
      {
        file.close();
        LITTLEFS.remove(removepath);    // remove file
        Serial.println("file deleted");
      }
      file.close();
      confirmRemove();
}

void fileRemove(AsyncWebServerRequest *request)  {
  webpage = "";

  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
  }

  Serial.println("remove file function");
  if (request->hasParam("remove_path", true))
  {
    AsyncWebParameter *myway = request->getParam("remove_path", true);

    remove_path = myway->value();
    Serial.print("remove Path is: ");
    Serial.println(remove_path);
    if(!LITTLEFS.exists(remove_path))  {      // if path does not exist -> return
        Serial.println("path/file does not exist!");
        return;
    }
    File file = LITTLEFS.open(remove_path, "w");
    Serial.println(" opened remove_path ");
/*
      if(!file){
        Serial.println("is not root");
        return;
      }  */

      if(file.isDirectory()){
        Serial.println(String(file.isDirectory()?"Dir ":"File ")+String(file.name()));
        webpage += "<tr><td>"+String(file.isDirectory()?"Dir":"File")+"</td><td>"+String(file.name())+"</td><td></td></tr><br>";
        Serial.println("remove directory");
        file.close();
        LITTLEFS.rmdir(remove_path);     // remove directory
      }
      else
      {
        webpage += "<tr><td>"+String(file.name())+"</td>";
        Serial.print(String(file.isDirectory()?"Dir ":"File ")+String(file.name())+"\t");
        webpage += "<td>"+String(file.isDirectory()?"Dir":"File")+"</td>";
        int bytes = file.size();
        String fsize = "";
        if (bytes < 1024)                     fsize = String(bytes)+" B";
        else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
        else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
        else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
        webpage += "<td>"+fsize+"</td></tr><br>";
        Serial.println(String(fsize));
        file.close();
        LITTLEFS.remove(remove_path);    // remove file
        Serial.println("file deleted");
        webpage += "<br>File deleted<br>";
      }
      file.close();   
  }
  else
  {
    request->send(500, "text/plain", "Missing parameters");
  }   
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String filepath;

  Serial.println((String)"UploadStart: " + filename);
  // open the file on first call and store the file handle in the request object
  filepath = upload_path+filename;      // + "/"
  Serial.println((String)"Uploadpath: " + filepath);
  
  if(!index){
    request->_tempFile = LITTLEFS.open(filepath, "w");
  }
  if(len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data,len);
  }
  if(final){
    Serial.println((String)"UploadEnd: " + filepath + "," + index+len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    request->send(200, "text/plain", "File Uploaded !");
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);
    webpage += " start list directory ";

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            webpage += "<a> DIR : </a>";
#ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.name());
#else
            Serial.print(file.name());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
#endif

            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            webpage += "<a> FILE: </a>";
            webpage += file.name();
            webpage += "<a> SIZE: </a>";

#ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.size());
#else
            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
#endif
        }
        file = root.openNextFile();
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


void printDirectory(const char * dirname, uint8_t levels){
  
  File root = LITTLEFS.open(dirname);
  
  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }

  File file = root.openNextFile();
  while(file){
    if (webpage.length() > 3000) {
      //SendHTML_Content();
      Serial.println("SendHTML_Content() failed!");     // fix this later !!!!!!!
    }
      if(file.isDirectory()){
        Serial.println(String(file.isDirectory()?"Dir ":"File ")+String(file.name()));
        myJsondir += "\"" + String(file.isDirectory()?"Dir ":"File ")+String(file.name()) + "\",";
        webpage += "<tr><td>"+String(file.isDirectory()?"Dir ":"File ")+"</td><td>"+String(file.name())+"</td><td></td></tr><br>";
        printDirectory(file.name(), levels-1);
      }
      else
      {
        //Serial.print(String(file.name())+"\t");
        webpage += "<tr><td>"+String(file.name())+"</td>";
        Serial.print(String(file.isDirectory()?"Dir ":"File ")+String(file.name())+"\t");
        myJsondir += "\"" + String(file.isDirectory()?"Dir ":"File ")+String(file.name());
        webpage += "<td>"+String(file.isDirectory()?"Dir ":" File ")+"</td>";
        int bytes = file.size();
        String fsize = "";
        if (bytes < 1024)                     fsize = String(bytes)+" B";
        else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
        else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
        else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
        webpage += "<td>"+fsize+"</td></tr><br>";
        Serial.println(String(fsize));
        myJsondir += " Filesize " + String(fsize) + "\",";
      }
    file = root.openNextFile();
  }
}

// close JSON file what is unfinalized when printDirectory is left
void close_myJsondir(void)  {

      // Print Filesystem info
      Serial.println("File system info.");
 
      Serial.print("Total space: ");
      myJsondir += "\"Total space= ";
      Serial.print(LITTLEFS.totalBytes());
      myJsondir += LITTLEFS.totalBytes();
      Serial.println(" byte");
      myJsondir += " byte";
      myJsondir += "\",";
  
      Serial.print("Total space used: ");
      myJsondir += "\"Total space used= ";
      Serial.print(LITTLEFS.usedBytes());
      myJsondir +=LITTLEFS.usedBytes();
      Serial.println(" byte");
      myJsondir += " byte";
      myJsondir += "\",";

      Serial.print("Total free space: ");
      myJsondir += "\"Total free space= ";
      Serial.print(LITTLEFS.totalBytes() - LITTLEFS.usedBytes());   // Berechnet den freien Speicherplatz
      myJsondir += (LITTLEFS.totalBytes() - LITTLEFS.usedBytes());
      Serial.println(" byte");
      myJsondir += " byte";
      myJsondir += "\",";

      // close DIR Json string
      myJsondir.remove(myJsondir.length() - 1);      // remove the last ,{
      myJsondir += "]}";                    // close the JSON string
      Serial.println(myJsondir);            // print the JSON string
      // save the string "myjsondir" to directory.json in LITTLEFS
      LITTLEFS.remove("/directory.json");                           // remove old directory file
      File myFile = LITTLEFS.open("/directory.json", FILE_WRITE);   // open file for writing
      myFile.print(myJsondir);                                      // write new file
      myFile.close();                                               // close file
      myJsondir = "{\"directory\":[";       // open new JSON string for the next JSON request 
  }




// ***** File Upload SDcard page *****
void File_Upload(){
  Serial.println("File upload stage-1");
  webpage  = "";
  webpage += "<!DOCTYPE html><html><head>";
  webpage += "</head>";
  webpage += "<body>";
  webpage += "<div class='navbar'>";
  webpage += "<a href='/homepage'>Home</a>";
  webpage += "</div>";
  webpage += "<title>'LittleFS File Upload Page'</title>";
  webpage += "<div class='main'>";
  webpage += "<h3>Select File to Upload</h3>"; 
  webpage += "<div class='mypath'><p> example: / = root   ;  /myfiles  = subdirectory 'myfiles' </p>"; 
  webpage += "<p>if the directory does not exist, it will be created</p>";
  webpage += "<form action='/mypath' method='POST'><label for='path'>type path here:</label>";
  webpage += "<input type='text' id='mypath' name='mypath' value='";
  webpage += upload_path;
  webpage += "'><br><br>";
  webpage += "<input type='submit' value='set path'></form></div><br><br>";    
  webpage += "<FORM action='/upload' method='post' enctype='multipart/form-data'>";
  webpage += "<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>";
  webpage += "<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>";
  webpage += "<br><br></FORM>";
  webpage += "<br><form action='/remove' method='POST'><label for='path'>type path to delete here:</label><br><br>";
  webpage += "<input type='text' id='remove_path' name='remove_path' value='";
  webpage += remove_path;
  webpage += "'><br><br>";
  webpage += "<input type='submit' value='Remove File'></form></div><br><br></form>";
  webpage += "<form action='/download' method='POST'><br><br>";
  webpage += "<br><button class='buttons' style='width:10%' type='submit'>Download File</button><br><br></form>";
  webpage += "<form action='/dir' method='POST'><br><br>";
  webpage += "<br><button class='buttons' style='width:10%' type='submit'>List Files</button><br><br>";
  webpage += "<a href='/'>[Back]</a><br><br></form>";
  webpage += "<footer><p>'myFootnote_here'<br>";
  webpage += "</p></footer>";
  webpage += "</div></body></html>";
  Serial.println("File upload stage-2");
}

void File_Header()  {
  webpage  = "";
  webpage += "<!DOCTYPE html><html><head>";
  webpage += "<h1> LITTLEFS directory </h1>";
  webpage += "</head>";
  webpage += "<body>";
  webpage += "<h2> LITTLEFS directory body </h2>";
}

void File_Footer()  {
  webpage += "<div><footer><p>'myFootnote_here'<br>";
  webpage += "</p></footer>";
  webpage += "</div></body></html>";
}
