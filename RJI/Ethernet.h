#include "IPAddress.h"
#include <stdint.h>
#ifndef Ethernet_h
#define Ethernet_h


// This header acts as an interface for the NativeEthernet library
// Contating helper functions, classes and definitions for hosting the webpage/
// WebSockets server and connecting to the BlackMagic router


#include <NativeEthernet.h>
#include <ArduinoWebsockets.h>
using namespace websockets;


#include "Debug.h"
#include "VideoHub.h"

typedef void (*MessageHandle)(WebsocketsClient&, WebsocketsMessage);


// The following are raw literal strings that make up the webpage
// delivered by the web server which contains the instructions for
// a client to initalise a WebSocket connection


const char webpageA[] PROGMEM =R"rawLiteral(

<head>

    <script>

    var socket = new WebSocket("ws://" + window.location.hostname + ":80"); // This starts up the socket connection automagically.


    var lastMessageDate;                                                    // Used to keep track of last message sent to check if we have
                                                                            // disconnected from the interface.

    socket.addEventListener('message', webSocketMessage);                   // Listen for incoming mesages from the interface.


    // On webpage loaded
    document.addEventListener('DOMContentLoaded', function() {
            populateInputObjects();                         // Populate the Network tab input fields.
    });


    function webSocketMessage(_event){
        
        const json = JSON.parse(_event.data);

        switch(json[0]) {

            case "conn-stat":
                setConnectionStatus(json[1]);
                break;
            
            case "settings":
                readSettings(json);
                break;

            case "gpi":
                setGPI(json[1], json[2]);
                break;

        }
        lastMessageDate = new Date();

    }


    function checkConnection() {

        const currDate = new Date();
        if(currDate - lastMessageDate > 2000) {
            socket.close();
            setConnectionStatus(false);

        }

    }
    setInterval(checkConnection, 2000);


    function setConnectionStatus(_val) {
        const element = document.getElementById("ws-connection-status");

        if(_val) {
            element.innerHTML = "Connected!";
            element.style = "background-color: var(--green-accent); border-color: var(--green-accent);";

        }
        else {
            element.innerHTML = "Not Connected!";
            element.style = "";

        }        
    }


    function readSettings(json) {

        json.slice(1).forEach(element => {

            // Special case for "engineers"
            if(element[0] == "engineers") {
                populateEngineers(element);
                return;
            }

            // Special case for "buttons"
            if(element[0] == "buttons") {
                populateButtons(element);
                return;
            }

            // Get the input object
            var inputObject = document.getElementById(element[0]);
            // Here I am assuming there is only one class
            var inputType = inputObject.classList[0];
            
            switch (inputType) {

                case "ip":
                    inputObject.value = element[1] + "." + element[2] + "." + element[3] + "." + element[4]; 
                    break;

                case "port":
                    inputObject.value = element[1];
                    break;
                
                case "bool":
                    inputObject.checked = element[1];
                    break;

            }

        });
        updateButtonOptions(false);

    }


	function setGPI(id, state) {

	    var button = document.getElementById(id);
	    if(state) {

		button.style = "background: var(--green-accent);";

	    }
	    else {
		button.style = "background: inherit";
	    }

	}


    function populateEngineers(engineers) {

        const table = document.getElementById("engineers");
        const rows = table.getElementsByTagName("tr");

        for(let i=0; i<6; i++) {


            const mask = document.getElementById(rows[i+1].id + "_mask");
            const dest = document.getElementById(rows[i+1].id + "_dest");
            const type = document.getElementById(rows[i+1].id + "_type");
            const name = document.getElementById(rows[i+1].id + "_name");

            mask.value = parseInt(engineers[i+1][0]).toString(2);
            dest.value = engineers[i+1][1];
            type.checked = engineers[i+1][2];
            name.value = engineers[i+1][3];

        }

    }


    function populateButtons(buttons) {

        console.log(buttons);

        const table = document.getElementById("buttons");
        const rows = table.getElementsByTagName("tr");

        for(let i=0; i<12; i++) {

            const source = document.getElementById(rows[i+1].id + "_source");
            source.value = buttons[i+1];

        }   

    }


    var inputObjects = [
        { id:"interface-ip", input_type:"ip",  label:"Interface IP ", error_message:" Not a valid IP!"},
        { id:"interface-web-port", input_type:"port",  label:"Interface Web Port ", error_message:" Not a valid port number!"},
        { id:"interface-dhcp", input_type:"bool", label:"Toggle DHCP ", error_message:" ???Thats not good???" },
        { id:"videohub-ip", input_type:"ip", label:"VideoHub IP ", error_message:" Not a valid IP!"},
        { id:"videohub-port", input_type:"port", label:"VideoHub Port ", error_message:" Not a valid port number!"}
    ];


    function populateInputObjects() {
        var tokyo = document.getElementById("Tokyo")
        inputObjects.forEach(obj => {
            
            console.log(obj)

            // Add a span object and populate
            var inputObject = document.createElement("div");

            // Extra objects
            var inputObjectLabel = document.createElement("label");

            // Input type
            var inputObjectInput = document.createElement("input");
            
            switch (obj.input_type) {

                case "bool":
                    inputObjectInput.type = "checkbox";
                    break;

            }
            
            var inputObjectError = document.createElement("error");

            inputObjectLabel.innerHTML = obj.label;
            inputObjectInput.classList.add(obj.input_type);
            inputObjectInput.id = obj.id
            inputObjectError.id = obj.id + "-error";
            //inputObjectError.innerHTML = obj.error_message;

            // Add objects
            inputObject.appendChild(inputObjectLabel);
            inputObject.appendChild(inputObjectInput);
            inputObject.appendChild(inputObjectError);

            tokyo.appendChild(inputObject);
            tokyo.appendChild(document.createElement("br"));

        });

        var button = document.createElement("button");
        button.innerHTML = "submit";
        button.onclick = function(){submitSettings()};
        document.body.appendChild(button);

        button = document.createElement("button");
        button.innerHTML = "reboot";
        button.onclick = function(){sendReset()};
        button.id = "reboot-button";
        document.body.appendChild(button);

    }


	function sendReset() {
	    socket.send("[\"reset\"]");

	}


    function submitSettings() {

        // Loop through the list of input objects, check the input type,
        // check the validity of this input object

        // Gotta update the masks first based on values given in 'Buttons':
        updateMaskOnButtons();

        inputObjects.forEach(obj => {
            
            var falsifiable = false;
            var message = "";

            // Get input object
            var inputObj = document.getElementById(obj.id);

            // Check validity
            switch(obj.input_type) {

                case "ip":
                    falsifiable = !isIPValid(inputObj.value);
                    var splitIp = inputObj.value.split(".");
                    message = "[\"" + obj.id + "\"," + 
                    splitIp[0] + "," +
                    splitIp[1] + "," +
                    splitIp[2] + "," +
                    splitIp[3] + "]";
                    break;

                case "port":
                    falsifiable = !isPortValid(inputObj.value);
                    message = "[\"" + obj.id + "\"," + inputObj.value + "]"
                    break;

                case "bool":
                    message = "[\"" + obj.id + "\"," + inputObj.checked + "]"
                    break;

            }

            var error = document.getElementById(obj.id + "-error");

            if(falsifiable) {
                // Get error
                error.innerHTML = obj.error_message; 
                error.style = "color: var(--dark-orange-accent)";

            }
            else {
                if(message != "") {
                    socket.send(message);
                    error.innerHTML = " Sent!";
                    error.style = "color: var(--dark-green-accent)";
                }

            }

        });

        // Loop through engineers
        const tableEng = document.getElementById("engineers");
        const rowsEng = tableEng.getElementsByTagName("tr");


        for(let i=0; i<6; i++) {

            message = "[\"eng_" + i + "\",";

            const mask = document.getElementById(rowsEng[i+1].id + "_mask");
            const dest = document.getElementById(rowsEng[i+1].id + "_dest");
            const type = document.getElementById(rowsEng[i+1].id + "_type");
            const name = document.getElementById(rowsEng[i+1].id + "_name");
            const error = document.getElementById(rowsEng[i+1].id + "-error");

            falsifiable = false;
            falsifiable = falsifiable || !isValid16Bit(parseInt(mask.value, 2));
            falsifiable = falsifiable || !isValid16Bit(parseInt(dest.value));
            falsifiable = falsifiable || !isValidName(name.value);

            message += parseInt(mask.value, 2) + ",";
            message += parseInt(dest.value) + ",";
            message += type.checked + ",";
            message += "\"" + name.value + "\"]";

            if(falsifiable) {
                // Get error
                error.innerHTML = "Invalid Engineer!"; 
                error.style = "color: var(--dark-orange-accent)";

            }
            else {
                if(message != "") {
                    socket.send(message);
                    error.innerHTML = " Sent!";
                    error.style = "color: var(--dark-green-accent)";
                }

            }

        }

        // Again for the buttons
        const tableButt = document.getElementById("buttons");
        const rowsButt = tableButt.getElementsByTagName("tr");

        for(let i=0; i<12; i++) {

            message = "[\"button_" + i + "\",";
            const source = document.getElementById(rowsButt[i+1].id + "_source");
            const error = document.getElementById(rowsButt[i+1].id + "-error");


            message += "\"" + source.value + "\"]";

            if(!isPortValid(source.value)) {
                // Get error
                error.innerHTML = "Invalid Button! (0-256)"; 
                error.style = "color: var(--dark-orange-accent)";

            }
            else {
                if(message != "") {
                    socket.send(message);
                    error.innerHTML = " Sent!";
                    error.style = "color: var(--dark-green-accent)";
                }

            }

        }

    }


    function isValid16Bit(mask) {
        // Check if the number is an integer
        if (!Number.isInteger(mask)) {
            return false;
        }

        // Check if the number is within the 16-bit range
        return mask >= -32768 && mask <= 32767;
    }


    function isValidName(name) {
        // Regular expression to match only letters, numbers, underscores and spaces
        const regex = /^[a-zA-Z0-9_ ]+$/;
        
        return regex.test(name);
    }


    function isIPValid(ip) {
        console.log(ip);
        // Regular expression to match a valid IPv4 address
        const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;

        return ipRegex.test(ip);
    }


    function isPortValid(port) {
        // Check if the port is a non-empty string and is a valid positive integer
        return /^\d+$/.test(port) && parseInt(port, 10) >= 0 && parseInt(port, 10) <= 65535;
    }     


    function changeTab(evt, tabid) {

        // Everytime a user changes tab I will also update the buttons engineer dropdown list
        updateButtonOptions(true)

        var i, tabcontent, tablinks;
        tabcontent = document.getElementsByClassName("tabcontent");
        for (i = 0; i < tabcontent.length; i++) {
            tabcontent[i].style.display = "none";
        }
        tablinks = document.getElementsByClassName("tablinks");
        for (i = 0; i < tablinks.length; i++) {
            tablinks[i].className = tablinks[i].className.replace(" active", "");
        }
        document.getElementById(tabid).style.display = "block";
        evt.currentTarget.className += " active";
    }

    
    function updateMaskOnButtons() {

        const tableEng = document.getElementById("engineers");
        const rowsEng = tableEng.getElementsByTagName("tr");


        const tableButt = document.getElementById("buttons");
        const rowsButt = tableButt.getElementsByTagName("tr");

        var eng0Mask = 0;
        var eng1Mask = 0;
        var eng2Mask = 0;
        var eng3Mask = 0;
        var eng4Mask = 0;
        var eng5Mask = 0;


        // loop through engineers
        for(let i=0; i<12; i++) {
            const eng = document.getElementById(rowsButt[i+1].id + "_eng");

            console.log();
            switch (eng.selectedIndex) {
                case 1:
                    eng0Mask |= 1 << i;
                    break;
                case 2:
                    eng1Mask |= 1 << i;
                    break;
                case 3:
                    eng2Mask |= 1 << i;
                    break;
                case 4:
                    eng3Mask |= 1 << i;
                    break;
                case 5:
                    eng4Mask |= 1 << i;
                    break;
                case 6:
                    eng5Mask |= 1 << i;
                    break;
            }

        }

        // set masks
        document.getElementById(rowsEng[1].id + "_mask").value = eng0Mask.toString(2);
        document.getElementById(rowsEng[2].id + "_mask").value = eng1Mask.toString(2);
        document.getElementById(rowsEng[3].id + "_mask").value = eng2Mask.toString(2);
        document.getElementById(rowsEng[4].id + "_mask").value = eng3Mask.toString(2);
        document.getElementById(rowsEng[5].id + "_mask").value = eng4Mask.toString(2);
        document.getElementById(rowsEng[6].id + "_mask").value = eng5Mask.toString(2);

    }


    function updateButtonOptions(updateMask) {

        const tableEng = document.getElementById("engineers");
        const rowsEng = tableEng.getElementsByTagName("tr");


        const tableButt = document.getElementById("buttons");
        const rowsButt = tableButt.getElementsByTagName("tr");


        if(updateMask){ 
            updateMaskOnButtons();
        }


        // Loop through the engineer list of names and add them to a new list:

        var names = ["None"];

        for(let i=0; i<6; i++) {
            const name = document.getElementById(rowsEng[i+1].id + "_name");
            names.push(name.value);

        }

        // Then for each button row, update the list of engineers



        for(let i=0; i<12; i++) {
            const eng = document.getElementById(rowsButt[i+1].id + "_eng");

            // First clear the options
            eng.innerHTML = "";

            names.forEach(name => {
                var option = document.createElement("option");
                option.innerHTML = name;
                eng.appendChild(option);
            });


        }

        // Again loop through engineers to look at the mask and set the default option based on that

        for(let i=0; i<6; i++) {
            const mask = document.getElementById(rowsEng[i+1].id + "_mask");
            var maskVal = parseInt(mask.value, 2);

            if(isValid16Bit(maskVal)) {

                for(let j=0; j<12; j++) {

                    if(maskVal & (1 << j)) {
                        // j is the index of the button we want to have the default option of
                        // engineer[i]
                        const eng = document.getElementById(rowsButt[j+1].id + "_eng");
                        eng.selectedIndex = i+1;

                    }

                }
         

            }

        }

    }


    </script>

    <style>


    /* Color Scheme */
    :root {
        --bg-col: #FFFFFA;
        --type-col: #2F2F2F;
        --green-accent: #D1EFB5;
        --dark-green-accent: #587c36;
        --blue-accent: #E3F9FF;
        --dark-blue-accent: #BAD5DC;
        --orange-accent: #F59D46;
        --dark-orange-accent: #ff8000;

    }


    /* body {
        -webkit-filter: grayscale(100%);
        -moz-filter: grayscale(100%);
        filter: grayscale(100%);
    } */


    * {
        font-family: 'Courier New', Courier, monospace;
        font-family: Arial, Helvetica, sans-serif;
        background-color: var(--bg-col);
        color: var(--type-col);
    }


    #ws-connection-status {
        display: block;
        text-align: center;
        background-color: red;
        border: red 5px solid;
        border-radius: 5px;
        margin-bottom: 10px;

    }


    .input {
        background-color: rgb(224, 224, 224);
        border: rgb(224, 224, 224) 5px solid;
        border-radius: 5px;

    }


    error {
        color: var(--dark-orange-accent);

    }


    /* Style for GPI button grid at top of screen */
    .grid-cont {
        display: grid;
        grid-template-columns: auto auto auto auto;
        border-top: 1px solid #ccc;
        border-left: 1px solid #ccc;
        border-right: 1px solid #ccc;

    }


    .square {
        background-color: inherit;
        padding: 20px;
        text-align: center;

    }


    /* Style the tab */
    .tab {
        overflow: hidden;
        border: 1px solid #ccc;
        background-color: #f1f1f1;

    }


    #reboot-button {
        background-color: var(--orange-accent);

    }

    
    #reboot-button:hover {
        background-color: var(--dark-orange-accent);

    }


    input {
        border: none;
        outline: none;
        border-bottom: solid 1px #ccc;
    }


    button {
        background-color: var(--blue-accent);
        float: left;
        border: none;
        outline: none;
        cursor: pointer;
        padding: 14px 16px;
        transition: 0.3s;
        font-size: 17px;

    }


    /* Change background color of buttons on hover */
    button:hover {
        background-color: var(--dark-blue-accent);

    }


    /* Create an active/current tablink class */
    button.active {
        background-color: #83CBDD;

    }


    /* Style the tab content */
    .tabcontent {
        display: none;
        padding: 6px 12px;
        border: 1px solid #ccc;
        border-top: none;

    }


    #buttons td,
    #buttons th {
        padding-top: 10px;
        padding-left: 20px;
        text-align: center;
    }


    </style>

</head>


<body>


    <span id="ws-connection-status">Not Connected!</span>

    <div class="grid-cont">
        <div id="gpi-1" class="square">GPI 1</div>
        <div id="gpi-2" class="square">GPI 2</div>    
        <div id="gpi-3" class="square">GPI 3</div>
        <div id="gpi-4" class="square">GPI 4</div>  
        <div id="gpi-5" class="square">GPI 5</div>
        <div id="gpi-6" class="square">GPI 6</div>  
        <div id="gpi-7" class="square">GPI 7</div>
        <div id="gpi-8" class="square">GPI 8</div>  
        <div id="gpi-9" class="square">GPI 9</div>
        <div id="gpi-10" class="square">GPI 10</div>  
        <div id="gpi-11" class="square">GPI 11</div>
        <div id="gpi-12" class="square">GPI 12</div>  
    </div>


    <div class="tab">
        <button id="default-tab" class="tablinks" onclick="changeTab(event, 'London')">Engineers</button>
        <button class="tablinks" onclick="changeTab(event, 'Paris')">Buttons</button>
        <button class="tablinks" onclick="changeTab(event, 'Tokyo')">Network</button>
    </div>
    

    <div id="London" class="tabcontent" style="display: block">
        <table id="engineers">

            <tr>
                <th  style="display:none" >Mask</th><th>Destination</th><th>Enable Cool Logic</th><th>Name</th>
            </tr>
    
            <tr id="eng_0">
                <td  style="display:none" ><input id="eng_0_mask" class="mask"></td><td><input id="eng_0_dest" class="route"></td><td><input id="eng_0_type" type = "checkbox" class="type"></td><td><input id="eng_0_name" class="name"></td><td><error id="eng_0-error"></error></td>
            </tr>
    
            <tr id="eng_1">
                <td  style="display:none" ><input id="eng_1_mask" class="mask"></td><td><input id="eng_1_dest" class="route"></td><td><input id="eng_1_type" type = "checkbox" class="type"></td><td><input id="eng_1_name" class="name"></td><td><error id="eng_1-error"></error></td>
            </tr>
    
            <tr id="eng_2">
                <td  style="display:none" ><input id="eng_2_mask" class="mask"></td><td><input id="eng_2_dest" class="route"></td><td><input id="eng_2_type" type = "checkbox" class="type"></td><td><input id="eng_2_name" class="name"></td><td><error id="eng_2-error"></error></td>
            </tr>
    
            <tr id="eng_3">
                <td  style="display:none" ><input id="eng_3_mask" class="mask"></td><td><input id="eng_3_dest" class="route"></td><td><input id="eng_3_type" type = "checkbox" class="type"></td><td><input id="eng_3_name" class="name"></td><td><error id="eng_3-error"></error></td>
            </tr>
    
            <tr id="eng_4">
                <td  style="display:none" ><input id="eng_4_mask" class="mask"></td><td><input id="eng_4_dest" class="route"></td><td><input id="eng_4_type" type = "checkbox" class="type"></td><td><input id="eng_4_name" class="name"></td><td><error id="eng_4-error"></error></td>
            </tr>
    
            <tr id="eng_5">
                <td  style="display:none" ><input id="eng_5_mask" class="mask"></td><td><input id="eng_5_dest" class="route"></td><td><input id="eng_5_type" type = "checkbox" class="type"></td><td><input id="eng_5_name" class="name"></td><td><error id="eng_5-error"></error></td>
            </tr>
    
        </table>
    
    </div>
        

    <div id="Paris" class="tabcontent">
        <table id="buttons">

            <tr>
                <th>Button</th><th>Source</th><th>Engineer</th>
            </tr>

            <tr id="button_0">
                <td>1</td><td><input id="button_0_source"></td> <td><select id="button_0_eng"></select></td><td> <error id="button_0-error"></error></td>
            </tr>
            
            <tr id="button_1">
                <td>2</td><td><input id="button_1_source"></td> <td><select id="button_1_eng"></select></td><td> <td><error id="button_1-error"></error></td>
            </tr>
            
            <tr id="button_2">
                <td>3</td><td><input id="button_2_source"></td> <td><select id="button_2_eng"></select></td><td> <td><error id="button_2-error"></error></td>
            </tr>
            
            <tr id="button_3">
                <td>4</td><td><input id="button_3_source"></td> <td><select id="button_3_eng"></select></td><td> <td><error id="button_3-error"></error></td>
            </tr>
            
            <tr id="button_4">
                <td>5</td><td><input id="button_4_source"></td> <td><select id="button_4_eng"></select></td><td> <td><error id="button_4-error"></error></td>
            </tr>
            
            <tr id="button_5">
                <td>6</td><td><input id="button_5_source"></td> <td><select id="button_5_eng"></select></td><td> <td><error id="button_5-error"></error></td>
            </tr>
            
            <tr id="button_6">
                <td>7</td><td><input id="button_6_source"></td> <td><select id="button_6_eng"></select></td><td> <td><error id="button_6-error"></error></td>
            </tr>
            
            <tr id="button_7">
                <td>8</td><td><input id="button_7_source"></td> <td><select id="button_7_eng"></select></td><td> <td><error id="button_7-error"></error></td>
            </tr>
            
            <tr id="button_8">
                <td>9</td><td><input id="button_8_source"></td> <td><select id="button_8_eng"></select></td><td> <td><error id="button_8-error"></error></td>
            </tr>
            
            <tr id="button_9">
                <td>10</td><td><input id="button_9_source"></td> <td><select id="button_9_eng"></select></td><td> <td><error id="button_9-error"></error></td>
            </tr>
            
            <tr id="button_10">
                <td>11</td><td><input id="button_10_source"></td> <td><select id="button_10_eng"></select></td><td> <td><error id="button_10-error"></error></td>
            </tr>
            
            <tr id="button_11">
                <td>12</td><td><input id="button_11_source"></td> <td><select id="button_11_eng"></select></td><td> <td><error id="button_11-error"></error></td>
            </tr>

        </table> 
    </div>
        

    <div id="Tokyo" class="tabcontent">
    </div>


</body>


)rawLiteral";


// Singleton for easy use of NativeEthernet library
class Network {


public:
  IPAddress ip;
  WebsocketsClient* webSocketClient = nullptr;


  Network() { 
    // Get mac address
    teensyMAC(mac);

  }


  // Startup ethernet
  // @param (IPAddress) ip address to start ethernet on  
  void startEthernet(IPAddress _ip) {
    Ethernet.begin(mac, _ip);
    info("Ethernet has local IP: ", Ethernet.localIP());
    ip = _ip;

  }


  // Startup ethernet with DHCP
  void startEthernet() {
    Ethernet.begin(mac);
    info("Ethernet has local IP: ", Ethernet.localIP());
    ip = Ethernet.localIP();

  }


  // Start web server
  // @param (uint16_t) port number to listen from
  void startWebServer(uint16_t _port) {
    webServer = new EthernetServer(_port);
    webServer->begin();
    info("Web Server is on Port: ", _port);

  }


  // Start web socket server
  // @param(uint16_t) port number to send messages on
  void startWebSocketServer(uint16_t _port, MessageHandle _messageHandle) {
    messageHandle = _messageHandle;

    webSocketServer = new WebsocketsServer();
    webSocketServer->listen(_port);
    if(!webSocketServer->available()) {
      err("WebSocket server could not start!");
    }
    else {
      info("WebSocket Server is on Port: ", _port);
    }

  }


  void connectToVideoHub(IPAddress _ip, uint16_t _port) {
    videoHubRouter = new EthernetClient();
    if(videoHubRouter->connect(_ip, _port)) {
      info("Connected to video hub!");

    }
    else {
      err("Could NOT connect to video hub!");

    }

  }


  void pollVideoHub() {
    if(videoHubRouter->available()) {
      char c = videoHubRouter->read();
        VideoHub.parse(c);
        //Serial.print(c); <- !!!uncomment for videohub messages!!!
    }

  }


  void sendMessageToVideoHub(const char* _message) {
    info("Sending message to VideoHub: ", _message);
    videoHubRouter->write(_message);

  }


  // Poll web server for incoming messages
  void pollWebServer() {
    EthernetClient client = webServer->accept();
    if (client) {
      info("Sending a new webpage...");
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");  // the connection will be closed after completion of the response
      client.println();

      // Send the client the webpage
      // client with too large of a buffer
      char buffer[513];
      for(int i=0; i<sizeof(webpageA); i += 512) {
        for(int j=0; j<512 && (i+j) < sizeof(webpageA) ;j++) {
          buffer[j] = webpageA[i+j];
        }
        buffer[512] = '\0';
        client.print(buffer);

      }

      client.stop();
    }

  }


  // Small clock to send a message to keep alive the webclient
  int clock = 0;


  void pollWebSocketServer() {
    if(webSocketServer->poll()) {
      // First check if there is already a client
      if(webSocketClient != nullptr && webSocketClient->available()) {
        // Send this client a disconnection message
        sendMessage(webSocketClient, "[\"conn-stat\", false]");
      }
      // Set this client to the current client
      webSocketClient = new WebsocketsClient(webSocketServer->accept());
      webSocketClient->onMessage(messageHandle);
      sendMessage(webSocketClient, "[\"conn-stat\", true]");
      // Send the current settings to the client
      char buffer[1024];
      serializeJson(Settings.getJson(), buffer);
      sendMessage(webSocketClient, buffer);

    }

    // Poll the current client
    if(webSocketClient != nullptr && webSocketClient->available()) {
      webSocketClient->poll();
      // Keep up the current connection status to keep client alive,
      // client will look for a LOS and hault entierly.
      if (clock%100000 == 0) webSocketClient->send("[\"conn-stat\", true]");
      clock ++;
    }

  }


  // Helper to send messages along with a nice debug output
  void sendMessage(WebsocketsClient* _client, const char* _message) {
    info("Sending message: ", _message);
    _client->send(_message);

  }


private:
  byte mac[6];

  EthernetServer* webServer;
  EthernetClient* videoHubRouter;

  WebsocketsServer* webSocketServer;
  MessageHandle messageHandle;


  // Helper function gets the Teensy's preprogrammed mac address,
  // writen by user 'vjmuzik' on https://forum.pjrc.com/index.php?threads/teensy-4-1-mac-address.62932/
  void teensyMAC(uint8_t *mac) {
    for(uint8_t by=0; by<2; by++) mac[by]=(HW_OCOTP_MAC1 >> ((1-by)*8)) & 0xFF;
    for(uint8_t by=0; by<4; by++) mac[by+2]=(HW_OCOTP_MAC0 >> ((3-by)*8)) & 0xFF;
    info("Mac: ", mac[0], ":", mac[1], ":", mac[2], ":", mac[3], ":", mac[4], ":", mac[5]);
  }


};


// Global definition for Network
Network Network;


#endif
