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
void ws_handleUpload( AwsFrameInfo * info, uint8_t *data);

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
	<link href="/loadJson2Style.css" rel="stylesheet">
    <title>WebSocketTester</title>

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

      // file rename_pathTo automaticaly
      var $my_rename = $("#rename_pathTo");
      $("#rename_path").keyup(function() {
      $my_rename.val( this.value );
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

      function refreshDir() {
				var dataObj = {};
				dataObj.task = "refresh"; 
				dataObj.command = "loaddir";
				ws.send(JSON.stringify(dataObj));			// send the command to refresh directory via JSON
			};

 /*     // add a click button with response
      let button = document.querySelector("#upload-btn");
      button.addEventListener("click", function() {   
          console.log("User has clicked on the upload button!");
          window.location.href="/my_upload";
      });  */

	  // list dirctory on user request
	  $('#get-dir-btn').click(function () {
		//alert("get directory clicked!");		
		refreshDir();
		$.getJSON('directory.json', function (data) {
		//console.log(data);
			$.each(data, function (key, value) {
				if (key === 'directory') {
					$('#directory').empty();
					for (var i = 0; i < value.length; i++) {
						var option = ('<li>' + value[i] + '</li>');
						$('#directory').append(option);
					}
				}
			});
		//alert("show data JSON!");
		});
	  });

	  // document ready function
		$(document).ready(function () {	
			// refresh dirctory when page is ready 
			function refreshDir() {
				var dataObj = {};
				dataObj.task = "refresh"; 
				dataObj.command = "loaddir";
				ws.send(JSON.stringify(dataObj));			// send the command to refresh directory via JSON
			};
			
			
			// get JSON file with directory from LittleFS flash into flexbox
			$.getJSON('directory.json', function (data) {
			  console.log(data);
				$.each(data, function (key, value) {
					if (key === 'directory') {
						$('#directory').empty();
						for (var i = 0; i < value.length; i++) {
							var option = ('<li>' + value[i] + '</li>');
							$('#directory').append(option);
						}
					}
				});
			});
				
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
    
	   <div>
		<div class="flexbox-container">
		  <div class="flexbox-item flexbox-item-1">flexbox-item-1
			<div class="message">
			  <h3>File Directory of ESP32 LITTLEFS</h3>
			  <div class="list_dir">
				<div><ul id="directory"></ul></div><br><br>
			  </div>
			  <div class="refreshtext">
				refresh the directory here<br><br><br>
			  </div>          
			   <div class="mybutton"> 
				<a><button class="get-dir-btn" id="get-dir-btn">refresh dir here</button></a>
			  </div>
			</div>
			<br>
		 </div>
		 <div class="flexbox-item flexbox-item-2">flexbox-item-2<br>
		    
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
				Insert path and renamed filename, example: /subdirectory/newfile.txt<br>
						<input type="text" id="rename_pathTo" name="rename_pathTo" value="/"><br><br>
						<a><button class="rename-btn" id="rename-btn">Rename File</button></a>	
			</div>
			<br>
			<div id="input_div">
			  $<input type="text" value="" id="input_el">
			</div><br>
			Debug logging:
			<pre id="dbg"></pre>
	  </div>
     </div>
	</div>
  </body>
</html>
)rawliteral";


#endif