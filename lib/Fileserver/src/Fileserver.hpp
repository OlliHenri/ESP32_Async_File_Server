/*
    Fileserver LittleFS using ESP32 Asynchrone HTTP server with websockets and HTTP
*/

#ifndef _Fileserver_h
#define _Fileserver_h

#include <Arduino.h>

#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LITTLEFSEditor.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <AsyncUDP.h>

//AsyncWebSocket ws("/ws");
//String webpage;
//String upload_path;
//String remove_path;
//String wsUri = "";

//functions
void confirmRemove(AsyncWebSocketClient * client);
void parseJSONmsg(const char *msg, AsyncWebSocketClient * client);
void ws_handlePath(void);
void handlePath(AsyncWebServerRequest *request);
void ws_fileRemove(String removepath,AsyncWebSocketClient * client);
void fileRemove(AsyncWebServerRequest *request);
void ws_fileRename(String renamepath, String renamepathTo,AsyncWebSocketClient * client);
void confirmRename(AsyncWebSocketClient * client);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void printDirectory(const char * dirname, uint8_t levels);
void close_myJsondir(void);

void File_Upload(void);
void File_Header(void);
void File_Footer(void);

//void on_WiFiEvent (system_event_id_t event, system_event_info_t info);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <!-- <meta http-equiv="Content-type" content="text/html; charset=utf-12"> -->
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <!-- <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script> -->
    <script src="/myjQuery/jquery-3.5.1.min.js"></script>
    <title>WebSocketTester</title>
    <style type="text/css" media="screen">
    body {
      margin: 40px;
      padding:0;
      background-color: lightgray;
    }

    #dbg, #input_div, #input_el {
      font-family: monaco;
      font-size: 12px;
      line-height: 13px;
      color: #000;
    }

    #dbg, #input_div {
      margin:0;
      padding:0;
      padding-left:4px;
    }

    #input_el {
      width:98%;
      background-color: rgba(255,255,255,255); /*<!--(0,0,0,0);-->*/
      border: 0px;
    }
    #input_el:focus {
      outline-style: solid ; /*<!-- none; -->*/
    }
    </style>
    <script type="text/javascript">
    var ws = null;
    function ge(s){ return document.getElementById(s);}
    function ce(s){ return document.createElement(s);}
    function stb(){ window.scrollTo(0, document.body.scrollHeight || document.documentElement.scrollHeight); }
    function sendBlob(str){
      var buf = new Uint8Array(str.length);
      for (var i = 0; i < str.length; ++i) buf[i] = str.charCodeAt(i);
      ws.send(buf);
    }
    function addMessage(m){
      var msg = ce("div");
      msg.innerText = m;
      ge("dbg").appendChild(msg);
      stb();
    }
    function startSocket(){
      ws = new WebSocket('ws://'+document.location.host+'/ws',['arduino']);
      console.log("ws ist gleich:");
      console.log(ws);
      ws.binaryType = "arraybuffer";
      ws.onopen = function(e){
        addMessage("Connected");
      };
      ws.onclose = function(e){
        addMessage("Disconnected");
      };
      ws.onerror = function(e){
        console.log("ws error", e);
        addMessage("Error");
      };
      ws.onmessage = function(e){
        var msg = "";
        if(e.data instanceof ArrayBuffer){
          msg = "BIN:";
          var bytes = new Uint8Array(e.data);
          for (var i = 0; i < bytes.length; i++) {
            msg += String.fromCharCode(bytes[i]);
          }
        } else {
          msg = "TXT:"+e.data;
        }
        addMessage(msg);
      };
      ge("input_el").onkeydown = function(e){
        stb();
        if(e.keyCode == 13 && ge("input_el").value != ""){
          ws.send(ge("input_el").value);
          ge("input_el").value = "";
        }
      }
    }
    function disconnect() {
			ws.close();
		}
    function startEvents(){
      var es = new EventSource('/events');
      es.onopen = function(e) {
        addMessage("Events Opened");
      };
      es.onerror = function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          addMessage("Events Closed");
        }
      };
      es.onmessage = function(e) {
        addMessage("Event: " + e.data);
      };
      es.addEventListener('ota', function(e) {
        addMessage("Event[ota]: " + e.data);
      }, false);

	    //set websocket filename and upload file
      $("#myUpload").click(function(){
      var file = document.getElementById('filename').files[0];
			console.log("filename :" + file.name);
			// set the filename json object		
			var dataObj = {};
			dataObj.task = file.name; 
			dataObj.command = "setfilename";
			ws.send(JSON.stringify(dataObj));					// send the filename via JSON

            var reader = new FileReader();
            var rawData = new ArrayBuffer();            
            //alert(file.name);

            reader.loadend = function() {

            }
            reader.onload = function(e) {
                rawData = e.target.result;
                ws.send(rawData);                                       // send second the file
                //alert("the File has been transferred.")
                ws.send('end');                                         // send  last "end"
            }

            reader.readAsArrayBuffer(file);
			});

      // set LittleFS path on user request
      $('#set-path-btn').click(function () {
        //alert("set path clicked!");
        var mypath = $("#mypath").val();		// get path from <input> value
            // alert(mypath);
        // set the path value		
        var dataObj = {};
        dataObj.task = mypath; //$("#mypath").val();
        dataObj.command = "setpath";
        ws.send(JSON.stringify(dataObj));					// send the path via JSON
        localStorage.setItem("myKeypath", JSON.stringify(dataObj));	// store it for later
        var obj = JSON.parse(localStorage.getItem("myKeypath"));	// access it later
        // alert("I send upload path: " + obj);
      });

      // remove file
      $('#remove-btn').click(function () {
      console.log("remove btn clicked!");
      var mypath = $("#remove_path").val();		// get path from <input> value
      console.log("removepath :" + mypath);
      // set the path value		
      var dataObj = {};
      dataObj.task = mypath; 						//$("#remove_path").val();
      dataObj.command = "removefile";
      ws.send(JSON.stringify(dataObj));	// send the path via JSON
      console.log("I send remove path: ");
      });

      // rename file
      $('#rename-btn').click(function () {
      console.log("rename btn clicked!");
      var mypath = $("#rename_path").val();		// get path from <input> value
      console.log("rename path:" + mypath);
      // set the path value		
      var dataObj = {};
      dataObj.task = mypath; 					
      dataObj.command = "renamefile";
      ws.send(JSON.stringify(dataObj));	// send the path via JSON
      console.log("rename JSON: " + JSON.stringify(dataObj));
      mypath = $("#rename_pathTo").val();		// get pathTo from <input> value
      console.log("rename pathTo:" + mypath);
      // set the path value		
      var dataObj = {};
      dataObj.task = mypath; 						
      dataObj.command = "renamefileTo";
      ws.send(JSON.stringify(dataObj));	// send the pathTo via JSON
      console.log("rename JSON To: " + JSON.stringify(dataObj));
      });

      // add a click button with response
      let button = document.querySelector("#upload-btn");
      button.addEventListener("click", function() {  
          //alert("upload button clicked");  
          console.log("User has clicked on the upload button!");
          window.location.href="/my_upload";
      });
      // add a 2nd click button with response
      let button2 = document.querySelector("#register-btn");
      button2.addEventListener("click", function() {  
          //alert("register form button2 clicked");  
          console.log("User has clicked on the button2 !");
          window.location.href="/register";
      });
      // add a 3nd click button with response
      let button3 = document.querySelector("#directory-btn");
      button3.addEventListener("click", function() {  
          //alert("directory button3 clicked");  
          console.log("User has clicked on the directory button3 !");
          window.location.href="/list_directory";
      });
      // add a 4th click button with response
      let button4 = document.querySelector("#loadJson-btn");
      button4.addEventListener("click", function() {  
          //alert("loadJson button4 clicked");  
          console.log("User has clicked on the loadJson button4, will load JSON from server !");
          window.location.href="/loadJSON";
      });
      // add a 5th click button with response
      let button5 = document.querySelector("#websoc-btn");
      button5.addEventListener("click", function() {  
          //alert("loadJson button4 clicked");  
          console.log("User has clicked on the loadJson button5, will test websocket");
          window.location.href="/websoctest1.htm";
      });
    }
    function onBodyLoad(){
      startSocket();
      startEvents();
    }

    </script>
  </head>
  <body id="body" onload="onBodyLoad()">
    <h3>LittleFS FileServer</h3>
    

        <button id="upload-btn">Click here for upload</button><br>
    <br><button id="register-btn">Click here for form</button><br>
    <br><button id="directory-btn">Click here for LittleFS directory</button><br>
    <br><button id="loadJson-btn">Click here to load JSON</button><br>
    <br><button id="websoc-btn">Click here to test websocket</button><br>
    <br>

  <h3>Select File to Upload</h3>
	<div class="myupload">
		example: / = root   ;  /myfiles  = subdirectory 'myfiles'<br>
    example path:  /myfiles/file.txt<br>
    if the directory does not exist, it will be created</p>
		<label for="path">type LITTLEFS path here:</label>
		<input type="text" id="mypath" name="mypath" value="/"><br><br>
		<a><button class="set-path-btn" id="set-path-btn">Set the LittleFS path</button></a>
		<br>
    <form action=''>
      <input id='filename' type='file' name='myfile'>​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​​<br><br>
      <input id='myUpload' type='button' value='Upload'>press Upload to send file to ESP32
    </form>
	</div>
  <div class="myremove">
		<h3>Remove File</h3>
		<label for="path">type path to remove file here:<br>
    when the last file of a directory is removed<br>
    the directory itself will be also removed</label><br>
		<input type="text" id="remove_path" name="remove_path" value="/"><br><br>
		<a><button class="remove-btn" id="remove-btn">Remove File</button></a>			
	</div>
  <div class="myrename">
				<h3>Rename File</h3>
        Select path and file to be renamed, example: /subdirectory/file.txt<br>
        <input type="text" id="rename_path" name="rename_path" value="/"><br>
        insert path and renamed filename, example: /subdirectory/newfile.txt<br>
				<input type="text" id="rename_pathTo" name="rename_pathTo" value="/"><br><br>
				<a><button class="rename-btn" id="rename-btn">Rename File</button></a>	
	</div>
    <br>
    <div id="input_div">
      $<input type="text" value="" id="input_el">
    </div><br>
    Debug logging:
    <pre id="dbg"></pre>
  </body>
</html>
)rawliteral";


#endif