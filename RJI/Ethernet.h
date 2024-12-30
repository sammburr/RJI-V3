#include "core_pins.h"
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

    var socket = new WebSocket("ws://" + window.location.hostname + ":8080"); // This starts up the socket connection automagically.
    var lastMessageDate;                                // Used to keep track of last message sent to check if we have
                                                        // disconnected from the interface.

    socket.addEventListener('message', webSocketMessage);   // Listen for incoming mesages from the interface.


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
            
            case "vh-stat":
                setVideoHubConnectionStatus(json[1]);
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
            setVideoHubConnectionStatus(_val);
        }        
    }

    function setVideoHubConnectionStatus(_val) {
        const element = document.getElementById("vh-connection-status");

        if(_val) {
            element.innerHTML = "Connected to Carbonite!";
            element.style = "background-color: var(--green-accent); border-color: var(--green-accent);";

        }
        else {
            element.innerHTML = "Not Connected to Carbonite!";
            element.style = "";

        }     

    }

    function readSettings(json) {

        console.log(json);
        json.slice(1).forEach(element => {

            // Special case for "engineers"
            if(element[0] == "engineers") {
                populateEngineers(element);
                return;
            }

            // Special case for "return to sources"
            if(element[0] == "return-to-sources") {
              populateReturnSources(element);
              return;
            }

            // Special case for "buttons"
            if(element[0] == "buttons") {
                populateButtons(element);
                return;
            }

            // Get the input object
            var inputObject = document.getElementById(element[0]);
            if (inputObject != null) {
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
            }

        });
        updateButtonOptions(false);
        updateSwitchLogic();

    }


	function setGPI(id, state) {

	    var button = document.getElementById(id);
	    if(state) {

		button.style = "background: var(--green-accent); cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none;";

	    }
	    else {
		button.style = "background: inherit; cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none;";
	    }

	}


  function populateReturnSources(sources) {

    const table = document.getElementById("engineers");
    const rows = table.getElementsByTagName("tr");

    for(let i=0; i<6; i++) {
      const source = document.getElementById(rows[i+1].id + "_returnsource");

      source.value = sources[i+1];
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

        updateSwitchLogic();

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
        { id:"interface-gw", input_type:"ip",  label:"Interface Gateway IP ", error_message:" Not a valid IP!"},
        { id:"interface-sub", input_type:"ip",  label:"Interface Subnet Mask ", error_message:" Not a valid Subnet Mask!"},
        { id:"interface-dhcp", input_type:"bool", label:"Toggle DHCP ", error_message:" ???Thats not good???" },
        { id:"videohub-ip", input_type:"ip", label:"Carbonite IP ", error_message:" Not a valid IP!"},
        { id:"videohub-port", input_type:"port", label:"Carbonite Port ", error_message:" Not a valid port number!"}
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

        var div = document.createElement("div");

        button = document.createElement("button");
        button.innerHTML = "connect to carbonite";
        button.onclick = function(){sendConnectToVideohub()};
        button.id = "retry-video-button";

        div.appendChild(button);
        div.appendChild(document.createElement("br"));
        div.appendChild(document.createElement("br"));
        div.appendChild(document.createElement("br"));

        var span = document.createElement("span");
        span.style.display = "none";
        span.id = "auto-connect-span";

        var label = document.createElement("label");
        label.innerHTML = "auto connect";


        button = document.createElement("input");
        button.type = "checkbox";
        button.id = "auto-connect";
        button.checked = true;

        span.appendChild(label);
        span.appendChild(button);

        div.appendChild(span);

        document.body.appendChild(div);
        

    }

    function sendConnectToVideohub() {

        socket.send("[\"video_hub_retry\", " + false + "]");
        
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

        // Again for return to sources
        for(let i=0; i<6; i++) {

          message = "[\"ret_to_source_" + i + "\",";
          const name = document.getElementById(rowsEng[i+1].id + "_returnsource");

          message += "\"" + name.value + "\"]";

          if(message != "") {
            socket.send(message);
          }
        }

        // Again for the buttons
        const tableButt = document.getElementById("buttons");
        const rowsButt = tableButt.getElementsByTagName("tr");

        for(let i=0; i<12; i++) {

          message = "[\"button_" + i + "\",";
          const source = document.getElementById(rowsButt[i+1].id + "_source");
          const error = document.getElementById(rowsButt[i+1].id + "-error");


          message += "\"" + (source.value) + "\"]";

          if(message != "") {
              socket.send(message);
              error.innerHTML = " Sent!";
              error.style = "color: var(--dark-green-accent)";
              

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

    function updateSwitchLogic() {
        const eng0 = document.getElementById("eng_0_logic");
        const eng1 = document.getElementById("eng_1_logic");
        const eng2 = document.getElementById("eng_2_logic");
        const eng3 = document.getElementById("eng_3_logic");
        const eng4 = document.getElementById("eng_4_logic");
        const eng5 = document.getElementById("eng_5_logic");

        const eng0t = document.getElementById("eng_0_type");
        const eng1t = document.getElementById("eng_1_type");
        const eng2t = document.getElementById("eng_2_type");
        const eng3t = document.getElementById("eng_3_type");
        const eng4t = document.getElementById("eng_4_type");
        const eng5t = document.getElementById("eng_5_type");

        if(eng0t.checked) eng0.innerHTML = "Toggle";
        else eng0.innerHTML = "Latch";
        if(eng1t.checked) eng1.innerHTML = "Toggle";
        else eng1.innerHTML = "Latch";
        if(eng2t.checked) eng2.innerHTML = "Toggle";
        else eng2.innerHTML = "Latch";
        if(eng3t.checked) eng3.innerHTML = "Toggle";
        else eng3.innerHTML = "Latch";
        if(eng4t.checked) eng4.innerHTML = "Toggle";
        else eng4.innerHTML = "Latch";
        if(eng5t.checked) eng5.innerHTML = "Toggle";
        else eng5.innerHTML = "Latch";


    }

    function switchLogic(evt, checkbox) {
        if(evt.currentTarget.innerHTML == "Latch") {
            evt.currentTarget.innerHTML = "Toggle";
            document.getElementById(checkbox).checked = true;
        }
        else {
            document.getElementById(checkbox).checked = false;
            evt.currentTarget.innerHTML = "Latch";
        }

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

    var last_gpi_id;

    function gpiDown(id) {

      var message = "[\"gpi_down\",";
      message += id;
      message += "]";

      last_gpi_id = id;

      socket.send(message);
    }


    function gpiUp(id) {

      var message = "[\"gpi_up\",";
      message += last_gpi_id;
      message += "]";

      socket.send(message);
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

    #vh-connection-status {
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

    td button {
        padding: 2px 50px;


    }

    select {
        padding: 0px 15px;
        border: none;
        border-bottom: 1px solid #ccc;
    }

    #retry-video-button {
        display: block;
        float: right;
    }

    #auto-connect-span {
        float: right;
    }

    img {
      padding: 10px;
    }

    page-title {
      font-size: 50px;
      font-weight: bold;
    }

    </style>

</head>


<body>

            <table>
            <thead>
              <tr>
                <td>
                  <img
                    src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAMAAAAp4XiDAAAC91BMVEVHcEyge1uQl6/EakvjUTO2io/6XyhhYWUtMDrvWij3ViOjn5/vXy4sNTf7Th3RNSxubmn7NhL4Viz5eZn5XSR8dHTXMyTzVi/7OxNATEzbLST7UyD/PBX4YiPyNxzZPCjzUSz/UhpybGzuNB1jPCj6UyDzORT+URn/NhL4YSbvRRpya2f5PBiSkI79RxzDurj3MBiTlZX/URu3s7HBu7nQKxr4ZyL8ORaOKCh+KSj+VBT/RRj/LxH+PxX9Nhf/JhD/Uxj/PBFxaGViPi78YBz8QBdxbmryUBWwqarKHiW/ubb8Xxf/SBZER0f6OBb+RRRzbGfMHx//Wxr/OhK9JR8lLjGIiYj+YhYeKi6sqacqMDL+QRQ0KiuYmZX/URbMRxpBKChVJyY7Pz9vJyZ0JydkKCb/YhW7KyAmMDL/PBCHIiK4tbGFICAlMzf9YhTXNiCsJyOVlpSuQB77XhScMSW7IR7lXhfTUxUqPD2INyhwKCqZQh77XhS8OCVCTlN+NylgJy09MCr///////7+/////v/+/v7+/vz6+vn49/b29vXv7enj5OTg4N3d29nb2NbX09DPzs3RysXJycjIv7m+vr26vLrBuK+7sKe1qZ+pqaixo5qvopWio6Kvn5Ckn5msmYqbnJqnmIq+jn2klIWVlZWhjoGdi36YhHaWgnWXgnKWgnSVgnSUgXOTfG/wWxTmWxXcVxSJbGHVTxXDThVrZWCxSBlgYF1ZW1lWVlNUVVxSUlBaTFSZNSJLUlZFUlaDPBuoJyNMTUtITlBXRkxTR0BKSkddQUJpODtHR0ZASUxyOBiTJSOELCZ1MyVEREJCREY8RkqBKSdAQ0REQUBqMihrNBg5REhYNzg3QkU9Pz9BPTpgMRk5PT08PDk5OjlENC42Nzc3NDFNLRlHLSAyNDMtMTEtLy8tLi0qLi4nLS05JhooKysxJiImKiomKSgqJyMjKCghJykiJiUrIBsZJiggIiEdIiMdIB8XISUhHRoaHx8VHB3O76tNAAAAgnRSTlMAAQIEBgoLDQ4QFBoaGh4fISUmKCsrLjMzNjk6PEBISU5QVVVaW1xjZGVlam5ydXd7fX1/h4iJi4yOj5GSlpycnZ+foaWmqaqrrK6ur7Czur2/wMTFxcvOztPV1tbX19fX2NnZ2trb3d7g4eLk5efu7vDy9PT19vn5+vz9/v7+/v7+dawK8gAABKlJREFUeNq9lcWW68wVRvepKpHV9m3uvsyMYWZmzjTTPEZeIpzMQpPgMPQzMzOjm9soWQUhLy33ujjKHok+7eLDTqLPc100O0lHBYzJK26E5LMJYxY+z40xfRYARH8y5wY5O9YcWeDKKNiJvPEFAxBlqzfYfSj0/HpizIdfGnBl5AriL6UaGr8puDLmMm28993A93jlEY/13np7VYuEqZmpZqsVHfwgBOAmKNimKle7vc2Bv8ySLZ09lb1LRIm6YzrPY+D8qE//bx/yt30whAc6Tz7zptthyb5w5oOHmilZrIBg6x8K+NGQ8q32TU//czhhyb7z3Q/sChZbFc4zgYBoFcetBU49sXnnROTAdz6sO9YOuwAaHBqApCkhMCoJomY/dufTG9SRQx9Uw/DgM30UO7DJ9HuXQ/DDKoqZ/8I9t4R69pdTGx56rAP+P1gL1la+8qFY+/sr4Drdjb53x06o2iJzzku7wgHaRY1GNhoMS4dDwuD5o44umOliqqnd2ELECHoA4KS51IryhdkYIDizDTr7N4rw8ZhxhDSzKOtAKUU2lxBCPNVQgMIHoCzLEaHUad0w7UGhnQIkdpZ0VJEoPChBicotMRIaaW1JJpa0xHZ11XTbw0QBICLQL0uAVlRbTAwKMR4w6bDgeU9nOu2BV4ColiM3kJjaAiMFIQDSwIkWJcFPG8WYWGswjvEg1zhQRPOSbDXxA1JiCwDKtBwJgJ6woDUAxIlOVBynumFMFsCDwWSr0bRoYCJS8j984iVoJSIKmgYAhJdefLKHWGwdsaMS0CiUAZSA0qJTAwRQyKIbCJRuoi8dNbbkCAYJiASSIiBo9N7R2kyFp6ojJRZwQByHACGgDLh4PCh+trsrdoQwoo5UFgV4oyUIChnPFypoEHWH0xc0bmtQ92VoxQcFOBDQFjTA/4RAWAmPINxc1ZEwvKkkAZQdOVAGFKDLINolyrDrYz6HslNHYP2bFQdSEe87BBSgBan64ly6D+TE22fOyGb3JVvPJ7k/OHeo2A5RSkvJf/FKNp34Xec+hlg1n4p7ovmjzbr7vNLvvrbw8Q89dmQXRbcEgKTRoGsXwalGoBq+tecHL02cY3LgG99ftbNTWZao2u200zh81a+K7VLP/vLXb05EYM/79ra+GlzhDVGSECvwo7KksqBy4S9bL925zY6I6Pz0z0Z9IJcIACoiqtAHyP9w05N9e/kxHn91Od/qGTF4BAgoLMH61jSv3vTWFetLevrjv+txBb6Q/KkXrlhfCv1223M58tRH+4ErRqievVICeXF/4P+PisHoyTZKzPhWK8ZgFABaUKRfbqj3XmwRf7w+Or6czc+bC8Ce3YyRo7PEAkcyDIO394+KVT87t8fkrpvm/aF9+/Cpt9eJmjZlStnCGNUL8z3e/XTR2ttGw9r5U/dfMO8ezNn5veVZjr7G5sfL9NJd72mdl/75jfOrJ07HbQ5tdY8NT4XlV0sFw2JlK8urDXbvm0vVWgy97v0vr2yfagctnfbMbPbGcUWYSqu1tN0BDfhXhtEbfvGpN9PVF9WxN9dhuz3c2Hz9UKc9qDajva/F5ZshWuq5tfJkZ8X+C5rlAVjEmfFsAAAAAElFTkSuQmCC"
                  />
                </td>
                 <td><page-title>GPI 12</page-title> Joystick Interface</td>
              </tr>
            </thead>
            </table>
    <span id="ws-connection-status">Not Connected!</span>
    <span id="vh-connection-status">Not Connected to VideoHub!</span>

    <div class="grid-cont">
        <div id="gpi-1" class="square" onmousedown="gpiDown(1)" onmouseup="gpiUp(1)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 1</div>
        <div id="gpi-2" class="square" onmousedown="gpiDown(2)" onmouseup="gpiUp(2)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 2</div>    
        <div id="gpi-3" class="square" onmousedown="gpiDown(3)" onmouseup="gpiUp(3)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 3</div>
        <div id="gpi-4" class="square" onmousedown="gpiDown(4)" onmouseup="gpiUp(4)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 4</div>  
        <div id="gpi-5" class="square" onmousedown="gpiDown(5)" onmouseup="gpiUp(5)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 5</div>
        <div id="gpi-6" class="square" onmousedown="gpiDown(6)" onmouseup="gpiUp(6)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 6</div>  
        <div id="gpi-7" class="square" onmousedown="gpiDown(7)" onmouseup="gpiUp(7)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 7</div>
        <div id="gpi-8" class="square" onmousedown="gpiDown(8)" onmouseup="gpiUp(8)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 8</div>  
        <div id="gpi-9" class="square" onmousedown="gpiDown(9)" onmouseup="gpiUp(9)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 9</div>
        <div id="gpi-10" class="square" onmousedown="gpiDown(10)" onmouseup="gpiUp(10)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 10</div>  
        <div id="gpi-11" class="square" onmousedown="gpiDown(11)" onmouseup="gpiUp(11)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 11</div>
        <div id="gpi-12" class="square" onmousedown="gpiDown(12)" onmouseup="gpiUp(12)" style="cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none; ">GPI 12</div>  
    </div>


    <div class="tab">
        <button id="default-tab" class="tablinks" onclick="changeTab(event, 'London')">Engineers</button>
        <button class="tablinks" onclick="changeTab(event, 'Paris')">Buttons</button>
        <button class="tablinks" onclick="changeTab(event, 'Tokyo')">Network</button>
    </div>
    

    <div id="London" class="tabcontent" style="display: block">
        <table id="engineers">

            <tr>
                <th  style="display:none" >Mask</th><th>Destination (AUX)</th> <th>Logic</th> <th></th><th>Name</th> <th>Return To Source</th>
            </tr>
    
            <tr id="eng_0">
                <td  style="display:none" ><input id="eng_0_mask" class="mask"></td><td><input id="eng_0_dest" class="route"></td> <td><button id="eng_0_logic" onclick="switchLogic(event, 'eng_0_type')">Toggle</button></td> <td><input style="display: none;" id="eng_0_type" type = "checkbox" class="type"></td><td><input id="eng_0_name" class="name"></td><td><input id="eng_0_returnsource" class="returntosource"></td><td><error id="eng_0-error"></error></td>
            </tr>
    
            <tr id="eng_1">
                <td  style="display:none" ><input id="eng_1_mask" class="mask"></td><td><input id="eng_1_dest" class="route"></td> <td><button id="eng_1_logic"  onclick="switchLogic(event, 'eng_1_type')">Toggle</button></td> <td><input style="display: none;" id="eng_1_type" type = "checkbox" class="type"></td><td><input id="eng_1_name" class="name"></td><td><input id="eng_1_returnsource" class="returntosource"></td><td><error id="eng_1-error"></error></td>
            </tr>
    
            <tr id="eng_2">
                <td  style="display:none" ><input id="eng_2_mask" class="mask"></td><td><input id="eng_2_dest" class="route"></td> <td><button id="eng_2_logic"  onclick="switchLogic(event, 'eng_2_type')">Toggle</button></td> <td><input style="display: none;" id="eng_2_type" type = "checkbox" class="type"></td><td><input id="eng_2_name" class="name"></td><td><input id="eng_2_returnsource" class="returntosource"></td><td><error id="eng_2-error"></error></td>
            </tr>
    
            <tr id="eng_3">
                <td  style="display:none" ><input id="eng_3_mask" class="mask"></td><td><input id="eng_3_dest" class="route"></td> <td><button id="eng_3_logic"  onclick="switchLogic(event, 'eng_3_type')">Toggle</button></td> <td><input style="display: none;" id="eng_3_type" type = "checkbox" class="type"></td><td><input id="eng_3_name" class="name"></td><td><input id="eng_3_returnsource" class="returntosource"></td><td><error id="eng_3-error"></error></td>
            </tr>
    
            <tr id="eng_4">
                <td  style="display:none" ><input id="eng_4_mask" class="mask"></td><td><input id="eng_4_dest" class="route"></td> <td><button id="eng_4_logic"  onclick="switchLogic(event, 'eng_4_type')">Toggle</button></td> <td><input style="display: none;" id="eng_4_type" type = "checkbox" class="type"></td><td><input id="eng_4_name" class="name"></td><td><input id="eng_4_returnsource" class="returntosource"></td><td><error id="eng_4-error"></error></td>
            </tr>
    
            <tr id="eng_5">
                <td  style="display:none" ><input id="eng_5_mask" class="mask"></td><td><input id="eng_5_dest" class="route"></td> <td><button id="eng_5_logic"  onclick="switchLogic(event, 'eng_5_type')">Toggle</button></td> <td><input style="display: none;" id="eng_5_type" type = "checkbox" class="type"></td><td><input id="eng_5_name" class="name"></td><td><input id="eng_5_returnsource" class="returntosource"></td><td><error id="eng_5-error"></error></td>
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
                <td>2</td><td><input id="button_1_source"></td> <td><select id="button_1_eng"></select></td> <td><error id="button_1-error"></error></td>
            </tr>
            
            <tr id="button_2">
                <td>3</td><td><input id="button_2_source"></td> <td><select id="button_2_eng"></select></td> <td><error id="button_2-error"></error></td>
            </tr>
            
            <tr id="button_3">
                <td>4</td><td><input id="button_3_source"></td> <td><select id="button_3_eng"></select></td> <td><error id="button_3-error"></error></td>
            </tr>
            
            <tr id="button_4">
                <td>5</td><td><input id="button_4_source"></td> <td><select id="button_4_eng"></select></td> <td><error id="button_4-error"></error></td>
            </tr>
            
            <tr id="button_5">
                <td>6</td><td><input id="button_5_source"></td> <td><select id="button_5_eng"></select></td> <td><error id="button_5-error"></error></td>
            </tr>
            
            <tr id="button_6">
                <td>7</td><td><input id="button_6_source"></td> <td><select id="button_6_eng"></select></td> <td><error id="button_6-error"></error></td>
            </tr>
            
            <tr id="button_7">
                <td>8</td><td><input id="button_7_source"></td> <td><select id="button_7_eng"></select></td> <td><error id="button_7-error"></error></td>
            </tr>
            
            <tr id="button_8">
                <td>9</td><td><input id="button_8_source"></td> <td><select id="button_8_eng"></select></td> <td><error id="button_8-error"></error></td>
            </tr>
            
            <tr id="button_9">
                <td>10</td><td><input id="button_9_source"></td> <td><select id="button_9_eng"></select></td> <td><error id="button_9-error"></error></td>
            </tr>
            
            <tr id="button_10">
                <td>11</td><td><input id="button_10_source"></td> <td><select id="button_10_eng"></select></td> <td><error id="button_10-error"></error></td>
            </tr>
            
            <tr id="button_11">
                <td>12</td><td><input id="button_11_source"></td> <td><select id="button_11_eng"></select></td> <td><error id="button_11-error"></error></td>
            </tr>

        </table> 
    </div>
        

    <div id="Tokyo" class="tabcontent">
    </div>
        <footer style="position:fixed;bottom:0px">
        	www.videowalrus.com
        </footer>

</body>
)rawLiteral";

#define PACKET_MAX_SIZE 1024 * 4

// Singleton for easy use of NativeEthernet library
class Network {


public:
  IPAddress ip;
  WebsocketsClient* webSocketClient = nullptr;

  bool isConnectedToVH = false;
  bool autoConnect = false; // used to auto retry to the videohub

  Network() { 
    // Get mac address
    teensyMAC(mac);

  }


  // Startup ethernet
  // @param (IPAddress) ip address to start ethernet on  
  void startEthernet(IPAddress _ip, IPAddress _gateway, IPAddress _subnet) {
    // Default dns
    IPAddress dns(_ip[0], _ip[1], _ip[2], 1);
    Ethernet.begin(mac, _ip, dns, _gateway, _subnet);
    Ethernet.setSocketSize(PACKET_MAX_SIZE);
    info("Ethernet has local IP: ", Ethernet.localIP());
    info("Ethernet has gateway IP: ", Ethernet.gatewayIP());
    info("Ethernet has subnet mask: ", Ethernet.subnetMask());
    ip = _ip;

  }


  // Startup ethernet with DHCP
  void startEthernet() {
    Ethernet.begin(mac);
    Ethernet.setSocketSize(PACKET_MAX_SIZE);
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

  IPAddress videohub_ip;
  uint16_t videohub_port;

  void connectToVideoHub(IPAddress _ip, uint16_t _port) {

    videohub_ip = _ip;
    videohub_port = _port;

    videoHubRouter = new EthernetClient();
    if(videoHubRouter->connect(_ip, _port)) {
      info("Connected to video hub!");
      isConnectedToVH = true;
      // tell websocket client
      if(webSocketClient != nullptr && webSocketClient->available())
        sendMessage(webSocketClient, "[\"vh-stat\", true]");


    }
    else {
      err("Could NOT connect to video hub!");
      isConnectedToVH = false;
    }

  }

  void reconnectToVideoHub(IPAddress _ip, uint16_t _port) {
    info("Attempting to reconnect to VideoHub...");

    videoHubRouter->stop();

    connectToVideoHub(_ip, _port);
    delay(1000); // Wait for telnet

  }

  bool didJustNoop = false;
  void pollVideoHub() {
    int currentMillis = millis();

    // if(videoHubRouter->available()) {
      
    //   char c = videoHubRouter->read();
    //   VideoHub.parse(c);
    //   Serial.print(c); // <- !!!uncomment for videohub messages!!!

    // }

    if (currentMillis % 1000 == 0) {
      if(!videoHubRouter->connected()) {
        sendMessage(webSocketClient, "[\"vh-stat\", false]");
        isConnectedToVH = false;
      }
      else {
        sendMessage(webSocketClient, "[\"vh-stat\", true]");
      }
    }

    // send no op every 10 seconds to persist connection
    if (currentMillis % 10000 == 0) {
      if(!didJustNoop && videoHubRouter->connected()) {
        didJustNoop = true;
        sendMessageToVideoHub("NOOP\r\n");
      }
    }

    if (currentMillis % 10000 != 0) {
      didJustNoop = false;
    }

  }

  // TODO REFACTOR
  void sendMessageToVideoHub(const char* _message) {
    if(isConnectedToVH) {
      info("Sending message to VideoHub:\n", _message);
      size_t bytes_written = videoHubRouter->write(_message);
      
      if(bytes_written == 0) {
        // failed to send a message
        info("Failed to send a message");
        info("Reconnecting...");
        reconnectToVideoHub(videohub_ip, videohub_port);
      }
      
      //VideoHub.expected_resp ++;
      info("End of message");
    }
    else {
      info("VideoHub is not connected! Not sending message...");
      info("Reconnecting...");
      reconnectToVideoHub(videohub_ip, videohub_port);
    }
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
    }

  }


  // Helper to send messages along with a nice debug output
  void sendMessage(WebsocketsClient* _client, const char* _message) {
    if(webSocketClient != nullptr && webSocketClient->available()) {
      //info("Sending message: ", _message);
      _client->send(_message);
    }

  }


private:
  byte mac[6];

  EthernetServer* webServer;
  // TODO: REMOVE
  EthernetClient* videoHubRouter;
  EthernetClient* tcpClient;

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
