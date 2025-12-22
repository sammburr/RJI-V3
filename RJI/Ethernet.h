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
#include "RouterProtocol.h"
#include "VideoHubProtocol.h"
#include "SWP08Protocol.h"

typedef void (*MessageHandle)(WebsocketsClient&, WebsocketsMessage);


// The following are raw literal strings that make up the webpage
// delivered by the web server which contains the instructions for
// a client to initalise a WebSocket connection


const char webpageA[] PROGMEM =R"rawLiteral(
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>GPI-12 Interface</title>
    <script>

    var socket;
    var lastMessageDate;
    var reconnectAttempts = 0;
    var maxReconnectAttempts = 10;
    var isRouterConnected = false;
    var currentProtocol = "Unknown";

    function connectWebSocket() {
        socket = new WebSocket("ws://" + window.location.hostname + ":8080");

        socket.addEventListener('open', function() {
            reconnectAttempts = 0;
            updateStatusBar();
        });

        socket.addEventListener('message', webSocketMessage);

        socket.addEventListener('close', function() {
            setConnectionStatus(false);
            attemptReconnect();
        });

        socket.addEventListener('error', function() {
            setConnectionStatus(false);
        });
    }

    function attemptReconnect() {
        if(reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            var delay = Math.min(1000 * Math.pow(2, reconnectAttempts - 1), 30000);
            updateStatusBar();
            setTimeout(connectWebSocket, delay);
        }
    }

    // On webpage loaded
    document.addEventListener('DOMContentLoaded', function() {
        populateInputObjects();
        connectWebSocket();
    });

    function webSocketMessage(_event){
        try {
            const json = JSON.parse(_event.data);

            switch(json[0]) {
                case "conn-stat":
                    setConnectionStatus(json[1]);
                    break;
                case "vh-stat":
                case "router-stat":
                    isRouterConnected = json[1];
                    setRouterConnectionStatus(json[1]);
                    break;
                case "settings":
                    readSettings(json);
                    break;
                case "gpi":
                    setGPI(json[1], json[2]);
                    break;
            }
            lastMessageDate = new Date();
            updateStatusBar();
        } catch(e) {
            console.error("Error parsing message:", e);
        }
    }

    function checkConnection() {
        const currDate = new Date();
        if(lastMessageDate && currDate - lastMessageDate > 3000) {
            if(socket && socket.readyState === WebSocket.OPEN) {
                socket.close();
            }
            setConnectionStatus(false);
        }
        updateStatusBar();
    }
    setInterval(checkConnection, 2000);

    function updateStatusBar() {
        var statusBar = document.getElementById('status-bar');
        if(!statusBar) return;

        var wsState = socket ? socket.readyState : 3;
        var wsStatus = wsState === 1 ? 'Connected' : (reconnectAttempts > 0 ? 'Reconnecting (' + reconnectAttempts + ')' : 'Disconnected');
        var routerStatus = isRouterConnected ? 'Connected' : 'Disconnected';
        var lastComm = lastMessageDate ? formatTime(lastMessageDate) : '--:--:--';
        var protocol = document.getElementById('router-protocol');
        currentProtocol = protocol ? (protocol.selectedIndex === 0 ? 'VideoHub' : 'SWP-08') : currentProtocol;

        statusBar.innerHTML = 'WS: <span class="' + (wsState === 1 ? 'status-ok' : 'status-err') + '">' + wsStatus + '</span> | ' +
            'Router: <span class="' + (isRouterConnected ? 'status-ok' : 'status-err') + '">' + routerStatus + '</span> | ' +
            'Protocol: ' + currentProtocol + ' | Last: ' + lastComm;
    }

    function formatTime(date) {
        return date.toLocaleTimeString();
    }

    function setConnectionStatus(_val) {
        if(_val) {
            overlay2.classList.remove("active");
        } else {
            overlay2.classList.add("active");
            setRouterConnectionStatus(_val);
        }
        updateStatusBar();
    }

    function setRouterConnectionStatus(_val) {
        isRouterConnected = _val;

        if(_val) {

            overlay1.classList.remove("active");
        }
        else {

            overlay1.classList.add("active");

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

            // Special case for protocol selector
            if(element[0] == "router-protocol") {
                var protocolSelect = document.getElementById("router-protocol");
                if(protocolSelect != null) {
                    protocolSelect.selectedIndex = element[1];
                    updateProtocolFields();
                }
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

                case "level":
                    inputObject.value = element[1];
                    break;

            }
            }

        });
        updateButtonOptions(false);
        updateSwitchLogic();
        updateProtocolFields();

    }

    function updateProtocolFields() {
        var protocolSelect = document.getElementById("router-protocol");
        var swp08Fields = document.getElementById("swp08-fields");
        if(protocolSelect && swp08Fields) {
            if(protocolSelect.selectedIndex == 1) {
                swp08Fields.style.display = "block";
            } else {
                swp08Fields.style.display = "none";
            }
        }
    }









	function setGPI(id, state) {

	    var button = document.getElementById(id);
	    if(state) {

		button.style = "background: var(--green-accent); cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none;";

	    }
	    else {
		button.style = "class: square; cursor: pointer; -webkit-user-select: none; -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; -o-user-select: none; user-select: none;";
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
            dest.value = engineers[i+1][1] + 1;
            type.checked = engineers[i+1][2];
            name.value = engineers[i+1][3];

        }

        updateSwitchLogic();

    }


    function populateButtons(buttons) {
        for(let i=0; i<12; i++) {
            const source = document.getElementById('button_' + i + '_source');
            if(source) source.value = buttons[i+1] + 1;
        }
    }


    var inputObjects = [
        { id:"interface-ip", input_type:"ip",  label:"Interface IP ", error_message:" Invalid IP address"},
        { id:"interface-gw", input_type:"ip",  label:"Interface Gateway IP ", error_message:" Invalid IP address"},
        { id:"interface-sub", input_type:"ip",  label:"Interface Subnet Mask ", error_message:" Invalid subnet mask"},
        { id:"interface-dhcp", input_type:"bool", label:"DHCP ", error_message:"" },
        { id:"router-ip", input_type:"ip", label:"Router IP ", error_message:" Invalid IP address"},
        { id:"router-port", input_type:"port", label:"Router Port ", error_message:" Invalid port (0-65535)"},
        { id:"swp08-level", input_type:"level", label:"SWP-08 Level ", error_message:" Invalid level (0-15)"}
    ];


    function populateInputObjects() {
        var tokyo = document.getElementById("Tokyo")

        // Create a container for SWP-08 specific fields
        var swp08Container = document.createElement("div");
        swp08Container.id = "swp08-fields";
        swp08Container.style.display = "none";

        // Create protocol selector (will be added after DHCP)
        var protocolDiv = document.createElement("div");
        var protocolLabel = document.createElement("label");
        protocolLabel.innerHTML = "Router Protocol ";
        var protocolSelect = document.createElement("select");
        protocolSelect.id = "router-protocol";
        protocolSelect.classList.add("protocol");
        protocolSelect.onchange = function() { updateProtocolFields(); };
        var optVH = document.createElement("option");
        optVH.value = "0";
        optVH.innerHTML = "VideoHub";
        var optSWP = document.createElement("option");
        optSWP.value = "1";
        optSWP.innerHTML = "SWP-08";
        protocolSelect.appendChild(optVH);
        protocolSelect.appendChild(optSWP);
        protocolDiv.appendChild(protocolLabel);
        protocolDiv.appendChild(protocolSelect);

        inputObjects.forEach(obj => {

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

                case "level":
                    inputObjectInput.type = "number";
                    inputObjectInput.min = "0";
                    inputObjectInput.max = "15";
                    inputObjectInput.inputMode = "numeric";
                    break;

                case "port":
                    inputObjectInput.inputMode = "numeric";
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

            // Put SWP-08 level in separate container
            if(obj.id == "swp08-level") {
                swp08Container.appendChild(inputObject);
                swp08Container.appendChild(document.createElement("br"));
            } else {
                tokyo.appendChild(inputObject);
                tokyo.appendChild(document.createElement("br"));
            }

            // Add protocol selector after DHCP checkbox
            if(obj.id == "interface-dhcp") {
                tokyo.appendChild(protocolDiv);
                tokyo.appendChild(document.createElement("br"));
            }

        });

        // Add the SWP-08 fields container
        tokyo.appendChild(swp08Container);

        // var button = document.createElement("button");
        // button.innerHTML = "submit";
        // button.onclick = function(){submitSettings()};
        // document.body.appendChild(button);

        // button = document.createElement("button");
        // button.innerHTML = "Reboot";
        // button.onclick = function(){sendReset()};
        // button.id = "reboot-button";
        // document.body.appendChild(button);

        // var div = document.createElement("div");



        // button = document.createElement("button");
        // button.innerHTML = "connect to videohub";
        // button.onclick = function(){sendConnectToVideohub()};
        // button.id = "retry-video-button";
        // div.appendChild(button);
        // div.appendChild(document.createElement("br"));
        // div.appendChild(document.createElement("br"));
        // div.appendChild(document.createElement("br"));
        // var span = document.createElement("span");
        // span.style.display = "none";
        // span.id = "auto-connect-span";
        // var label = document.createElement("label");
        // label.innerHTML = "auto connect";
        // button = document.createElement("input");
        // button.type = "checkbox";
        // button.id = "auto-connect";
        // button.checked = true;

        // span.appendChild(label);
        // span.appendChild(button);

        // div.appendChild(span);

        // document.body.appendChild(div);
        

    }

    function sendConnectToRouter() {

        socket.send("[\"router_retry\", " + false + "]");

    }

	function sendReset() {
	    if(confirm("Are you sure you want to reboot the interface?")) {
	        socket.send("[\"reset\"]");
	    }
	}


    function submitSettings() {
        // Show saving feedback
        var submitBtn = document.getElementById("submit-button");
        var originalText = submitBtn.innerHTML;
        submitBtn.innerHTML = "Saving...";
        submitBtn.disabled = true;

        // Loop through the list of input objects, check the input type,
        // check the validity of this input object

        // Gotta update the masks first based on values given in 'Buttons':
        updateMaskOnButtons();

        // Send protocol selection first
        var protocolSelect = document.getElementById("router-protocol");
        if(protocolSelect) {
            var protocolMsg = "[\"router-protocol\"," + protocolSelect.selectedIndex + "]";
            socket.send(protocolMsg);
        }

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

                case "level":
                    falsifiable = !isLevelValid(inputObj.value);
                    message = "[\"" + obj.id + "\"," + inputObj.value + "]"
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
                    error.innerHTML = " &#9989;";
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
            message += parseInt(dest.value) - 1 + ",";
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
                    error.innerHTML = " &#9989;";
                    error.style = "color: var(--dark-green-accent)";
                }

            }

        }

        // Again for the buttons
        for(let i=0; i<12; i++) {
            message = "[\"button_" + i + "\",";
            const source = document.getElementById('button_' + i + '_source');
            const error = document.getElementById('button_' + i + '-error');

            message += "\"" + (source.value - 1) + "\"]";

            if(!isPortValid(source.value)) {
                error.innerHTML = "Invalid Button! (0-256)";
                error.style = "color: var(--dark-orange-accent)";
            }
            else {
                if(message != "") {
                    socket.send(message);
                    error.innerHTML = " &#9989;";
                    error.style = "color: var(--dark-green-accent)";
                }
            }
        }

        // Reset button and show success after brief delay
        setTimeout(function() {
            var submitBtn = document.getElementById("submit-button");
            submitBtn.innerHTML = "Saved!";
            submitBtn.disabled = false;
            setTimeout(function() {
                submitBtn.innerHTML = "Submit";
            }, 1500);
        }, 300);

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
        // Check format and octet ranges (0-255)
        const parts = ip.split(".");
        if(parts.length !== 4) return false;
        for(let i = 0; i < 4; i++) {
            const num = parseInt(parts[i], 10);
            if(isNaN(num) || num < 0 || num > 255) return false;
        }
        return true;
    }


    function isPortValid(port) {
        // Check if the port is a non-empty string and is a valid positive integer
        return /^\d+$/.test(port) && parseInt(port, 10) >= 0 && parseInt(port, 10) <= 65535;
    }

    function isLevelValid(level) {
        // Check if the level is a valid number between 0-15
        return /^\d+$/.test(level) && parseInt(level, 10) >= 0 && parseInt(level, 10) <= 15;
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
        var engMasks = [0, 0, 0, 0, 0, 0];

        // loop through buttons
        for(let i=0; i<12; i++) {
            const eng = document.getElementById('button_' + i + '_eng');
            if(eng && eng.selectedIndex > 0) {
                engMasks[eng.selectedIndex - 1] |= 1 << i;
            }
        }

        // set masks
        for(let i=0; i<6; i++) {
            document.getElementById('eng_' + i + '_mask').value = engMasks[i].toString(2);
        }
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
        if(updateMask) {
            updateMaskOnButtons();
        }

        // Loop through the engineer list of names and add them to a new list:
        var names = ["None"];
        for(let i=0; i<6; i++) {
            const name = document.getElementById('eng_' + i + '_name');
            names.push(name.value);
        }

        // Then for each button row, update the list of engineers
        for(let i=0; i<12; i++) {
            const eng = document.getElementById('button_' + i + '_eng');
            eng.innerHTML = "";
            names.forEach(name => {
                var option = document.createElement("option");
                option.innerHTML = name;
                eng.appendChild(option);
            });
        }

        // Again loop through engineers to look at the mask and set the default option based on that
        for(let i=0; i<6; i++) {
            const mask = document.getElementById('eng_' + i + '_mask');
            var maskVal = parseInt(mask.value, 2);
            if(isValid16Bit(maskVal)) {
                for(let j=0; j<12; j++) {
                    if(maskVal & (1 << j)) {
                        const eng = document.getElementById('button_' + j + '_eng');
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

    // Dark mode toggle
    function toggleDarkMode() {
        document.body.classList.toggle('dark-mode');
        var isDark = document.body.classList.contains('dark-mode');
        localStorage.setItem('darkMode', isDark);
        var btn = document.getElementById('dark-mode-btn');
        if(btn) btn.innerHTML = isDark ? 'Light Mode' : 'Dark Mode';
    }

    // Load dark mode preference on page load (default to dark mode)
    document.addEventListener('DOMContentLoaded', function() {
        if(localStorage.getItem('darkMode') !== 'false') {
            document.body.classList.add('dark-mode');
            var btn = document.getElementById('dark-mode-btn');
            if(btn) btn.innerHTML = 'Light Mode';
        }
    });

    // Export settings to JSON file
    function exportSettings() {
        var settings = {};

        // Gather network settings
        inputObjects.forEach(function(obj) {
            var el = document.getElementById(obj.id);
            if(el) {
                if(obj.input_type === 'bool') {
                    settings[obj.id] = el.checked;
                } else {
                    settings[obj.id] = el.value;
                }
            }
        });

        // Protocol
        var protocol = document.getElementById('router-protocol');
        if(protocol) settings['router-protocol'] = protocol.selectedIndex;

        // Engineers
        var engineers = [];
        for(var i = 0; i < 6; i++) {
            engineers.push({
                mask: document.getElementById('eng_' + i + '_mask').value,
                dest: document.getElementById('eng_' + i + '_dest').value,
                type: document.getElementById('eng_' + i + '_type').checked,
                name: document.getElementById('eng_' + i + '_name').value
            });
        }
        settings.engineers = engineers;

        // Buttons
        var buttons = [];
        for(var i = 0; i < 12; i++) {
            buttons.push(document.getElementById('button_' + i + '_source').value);
        }
        settings.buttons = buttons;

        // Download as JSON
        var json = JSON.stringify(settings, null, 2);
        var blob = new Blob([json], {type: 'application/json'});
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = 'gpi12-settings.json';
        a.click();
        URL.revokeObjectURL(url);
    }

    // Import settings from JSON file
    function importSettings() {
        var input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json';
        input.onchange = function(e) {
            var file = e.target.files[0];
            var reader = new FileReader();
            reader.onload = function(e) {
                try {
                    var settings = JSON.parse(e.target.result);
                    applySettings(settings);
                    alert('Settings imported! Click Submit to save to device.');
                } catch(err) {
                    alert('Error reading settings file: ' + err.message);
                }
            };
            reader.readAsText(file);
        };
        input.click();
    }

    function applySettings(settings) {
        // Apply network settings
        inputObjects.forEach(function(obj) {
            if(settings[obj.id] !== undefined) {
                var el = document.getElementById(obj.id);
                if(el) {
                    if(obj.input_type === 'bool') {
                        el.checked = settings[obj.id];
                    } else {
                        el.value = settings[obj.id];
                    }
                }
            }
        });

        // Protocol
        if(settings['router-protocol'] !== undefined) {
            var protocol = document.getElementById('router-protocol');
            if(protocol) {
                protocol.selectedIndex = settings['router-protocol'];
                updateProtocolFields();
            }
        }

        // Engineers
        if(settings.engineers) {
            for(var i = 0; i < 6 && i < settings.engineers.length; i++) {
                var eng = settings.engineers[i];
                document.getElementById('eng_' + i + '_mask').value = eng.mask || '';
                document.getElementById('eng_' + i + '_dest').value = eng.dest || '';
                document.getElementById('eng_' + i + '_type').checked = eng.type || false;
                document.getElementById('eng_' + i + '_name').value = eng.name || '';
            }
            updateSwitchLogic();
        }

        // Buttons
        if(settings.buttons) {
            for(var i = 0; i < 12 && i < settings.buttons.length; i++) {
                document.getElementById('button_' + i + '_source').value = settings.buttons[i];
            }
        }

        updateButtonOptions(false);
    }

    </script>

    <style>


    /* Color Scheme - Light Mode */
    :root {
        --bg-col: #d9d9d9;
        --type-col: #2F2F2F;
        --green-accent: #D1EFB5;
        --dark-green-accent: #587c36;
        --blue-accent: #E3F9FF;
        --dark-blue-accent: #BAD5DC;
        --orange-accent: #F59D46;
        --dark-orange-accent: #ff8000;
        --light-gray-accent: #eeeeee;
        --input-bg: #fff;
        --table-border: #ccc;
    }

    /* Dark Mode */
    .dark-mode {
        --bg-col: #1a1a2e;
        --type-col: #eaeaea;
        --green-accent: #4a7c36;
        --dark-green-accent: #6aad4a;
        --blue-accent: #2a3a4a;
        --dark-blue-accent: #3a5a7a;
        --orange-accent: #c06030;
        --dark-orange-accent: #e08050;
        --light-gray-accent: #2a2a3e;
        --input-bg: #2a2a3e;
        --table-border: #444;
    }


    html, body {
        margin: 0;
        padding: 0;
        padding-bottom: 10px;
    }

    * {
        font-family: 'Courier New', Courier, monospace;
        font-family: Arial, Helvetica, sans-serif;
        background-color: var(--bg-col);
        color: var(--type-col);
        font-size: large;
        box-sizing: border-box;
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

    .hidden {
        display: none;
    }

    #status-bar {
        background-color: var(--light-gray-accent);
        color: var(--type-col);
        padding: 8px 12px;
        font-size: 12px;
        border-bottom: 1px solid var(--table-border);
        position: sticky;
        top: 0;
        z-index: 100;
    }

    .status-ok {
        color: var(--dark-green-accent);
        font-weight: bold;
    }

    .status-err {
        color: var(--dark-orange-accent);
        font-weight: bold;
    }


    error {
        color: var(--dark-orange-accent);

    }


    /* Style for GPI button grid at top of screen */
    .grid-cont {
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 4px;
    }

    .square {
        background-color: var(--light-gray-accent);
        padding: 10px 8px;
        text-align: center;
        border-radius: 5px;
        font-size: 12pt;
        font-weight: bold;
        min-height: 36px;
    }

    .gpi-btn {
        cursor: pointer;
        user-select: none;
        -webkit-user-select: none;
        -webkit-touch-callout: none;
        touch-action: manipulation;
    }

    .square:hover {
        background-color: var(--dark-blue-accent);
    }

    .square:active {
        background-color: var(--green-accent);
    }

    /* Responsive: 3 columns on smaller screens */
    @media (max-width: 600px) {
        .grid-cont {
            grid-template-columns: repeat(3, 1fr);
        }
        .square {
            padding: 12px 8px;
            font-size: 12pt;
        }
        table {
            font-size: 14px;
        }
        input, select {
            max-width: 100px;
        }
    }

    /* Responsive: 2 columns on very small screens */
    @media (max-width: 400px) {
        .grid-cont {
            grid-template-columns: repeat(2, 1fr);
        }
    }


    /* Style the tab */
    .tab {
        overflow: hidden;
        border: 0px;

    }


    #reboot-button {
        background-color: var(--orange-accent);

    }

    
    #reboot-button:hover {
        background-color: var(--dark-orange-accent);

    }


    input, select {
        border: solid 1px var(--table-border);
        background-color: var(--input-bg);
        color: var(--type-col);
        padding: 4px 8px;
        border-radius: 4px;
    }

    select {
        appearance: none;
        -webkit-appearance: none;
        -moz-appearance: none;
        background-image: url("data:image/svg+xml;charset=UTF-8,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%23888' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3e%3cpolyline points='6 9 12 15 18 9'%3e%3c/polyline%3e%3c/svg%3e");
        background-repeat: no-repeat;
        background-position: right 4px center;
        background-size: 14px;
        padding-right: 22px;
        cursor: pointer;
    }

    select:hover {
        border-color: var(--blue-accent);
    }

    select:focus {
        outline: none;
        border-color: var(--blue-accent);
        box-shadow: 0 0 0 2px rgba(76, 175, 80, 0.2);
    }


    button {
        background-color: var(--blue-accent);
        float: left;
        border: none;
        outline: none;
        cursor: pointer;
        padding: 8px 12px;
        transition: 0.3s;
        font-size: 14px;

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
        padding: 4px 8px;
    }

    .tabcontent.active-tab {
        display: block;
    }


    .buttons-grid {
        display: flex;
        gap: 20px;
    }

    #buttons, #buttons2, #engineers {
        font-size: 13px;
    }

    #buttons td, #buttons th,
    #buttons2 td, #buttons2 th {
        padding: 2px 4px;
        text-align: center;
    }

    #buttons input, #buttons2 input {
        font-size: 13px;
        padding: 2px 4px;
        width: 50px;
    }

    #buttons select, #buttons2 select {
        font-size: 13px;
        padding: 2px 4px;
        min-width: 70px;
    }

    #engineers td,
    #engineers th {
        padding: 2px 4px;
    }

    #engineers input {
        font-size: 13px;
        padding: 2px 4px;
    }

    #engineers .route {
        width: 50px;
    }

    #engineers .name {
        width: 80px;
    }

    #Tokyo {
        font-size: 13px;
    }

    #Tokyo div {
        margin-bottom: 2px;
    }

    #Tokyo label {
        display: inline-block;
        min-width: 130px;
        font-size: 13px;
    }

    #Tokyo input, #Tokyo select {
        font-size: 13px;
        padding: 2px 4px;
    }

    #Tokyo br {
        display: none;
    }

    td button {
        padding: 2px 20px;
        width: 100%;
        font-size: 13px;
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

    .container {
    position: relative;
    width: 100%;
    padding: 5px;
  }
  
  .content {
    padding: 0px;
  }
  
  .overlay {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0, 0, 0, 0.5);
    display: flex;
    justify-content: center;
    align-items: center;
    color: white;
    font-size: 24px;
    font-weight: bold;
    visibility: hidden;
    opacity: 0;
    transition: visibility 0s, opacity 0.3s ease-in-out;
  }
  
  .overlay.active {
    visibility: visible;
    opacity: 1;
  }
  
  .message {
    padding: 10px 20px;
    background-color: rgba(0, 0, 0, 0.7);
    border-radius: 8px;
  }

    </style>

</head>


<body>
    <div id="status-bar">WS: <span class="status-err">Connecting...</span> | Router: <span class="status-err">--</span> | Protocol: -- | Last: --:--:--</div>

            <table>
            <thead>
              <tr>
                <td>
                  <img
                    src="data:image/png;base64,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"
                  />
                </td>
                 <td><page-title>GPI 12</page-title> Joystick Interface</td>
              </tr>
            </thead>
            </table>


    <div class="container">
        <div class="content">
        <div class="grid-cont">
            <div id="gpi-1" class="square gpi-btn" onmousedown="gpiDown(1)" onmouseup="gpiUp(1)" ontouchstart="gpiDown(1);event.preventDefault();" ontouchend="gpiUp(1)">GPI 1</div>
            <div id="gpi-2" class="square gpi-btn" onmousedown="gpiDown(2)" onmouseup="gpiUp(2)" ontouchstart="gpiDown(2);event.preventDefault();" ontouchend="gpiUp(2)">GPI 2</div>
            <div id="gpi-3" class="square gpi-btn" onmousedown="gpiDown(3)" onmouseup="gpiUp(3)" ontouchstart="gpiDown(3);event.preventDefault();" ontouchend="gpiUp(3)">GPI 3</div>
            <div id="gpi-4" class="square gpi-btn" onmousedown="gpiDown(4)" onmouseup="gpiUp(4)" ontouchstart="gpiDown(4);event.preventDefault();" ontouchend="gpiUp(4)">GPI 4</div>
            <div id="gpi-5" class="square gpi-btn" onmousedown="gpiDown(5)" onmouseup="gpiUp(5)" ontouchstart="gpiDown(5);event.preventDefault();" ontouchend="gpiUp(5)">GPI 5</div>
            <div id="gpi-6" class="square gpi-btn" onmousedown="gpiDown(6)" onmouseup="gpiUp(6)" ontouchstart="gpiDown(6);event.preventDefault();" ontouchend="gpiUp(6)">GPI 6</div>
            <div id="gpi-7" class="square gpi-btn" onmousedown="gpiDown(7)" onmouseup="gpiUp(7)" ontouchstart="gpiDown(7);event.preventDefault();" ontouchend="gpiUp(7)">GPI 7</div>
            <div id="gpi-8" class="square gpi-btn" onmousedown="gpiDown(8)" onmouseup="gpiUp(8)" ontouchstart="gpiDown(8);event.preventDefault();" ontouchend="gpiUp(8)">GPI 8</div>
            <div id="gpi-9" class="square gpi-btn" onmousedown="gpiDown(9)" onmouseup="gpiUp(9)" ontouchstart="gpiDown(9);event.preventDefault();" ontouchend="gpiUp(9)">GPI 9</div>
            <div id="gpi-10" class="square gpi-btn" onmousedown="gpiDown(10)" onmouseup="gpiUp(10)" ontouchstart="gpiDown(10);event.preventDefault();" ontouchend="gpiUp(10)">GPI 10</div>
            <div id="gpi-11" class="square gpi-btn" onmousedown="gpiDown(11)" onmouseup="gpiUp(11)" ontouchstart="gpiDown(11);event.preventDefault();" ontouchend="gpiUp(11)">GPI 11</div>
            <div id="gpi-12" class="square gpi-btn" onmousedown="gpiDown(12)" onmouseup="gpiUp(12)" ontouchstart="gpiDown(12);event.preventDefault();" ontouchend="gpiUp(12)">GPI 12</div>
        </div>
        </div>
        <div class="overlay" id="overlay1">
          <button id="retry-router-button" onclick="sendConnectToRouter()">Reconnect</button>
        </div>
      </div>

    <div class="container">
      <div class="content">

    <div class="tab">
        <button id="default-tab" class="tablinks" onclick="changeTab(event, 'London')">Position</button>
        <button class="tablinks" onclick="changeTab(event, 'Paris')">GPI Patch</button>
        <button class="tablinks" onclick="changeTab(event, 'Tokyo')">Network</button>
    </div>
    

    <div id="London" class="tabcontent active-tab">
        <table id="engineers">
            <tr>
                <th class="hidden">Mask</th><th>Destination</th><th>Logic</th><th></th><th>Name</th>
            </tr>
            <tr id="eng_0">
                <td class="hidden"><input id="eng_0_mask" class="mask"></td><td><input id="eng_0_dest" class="route" inputmode="numeric"></td><td><button id="eng_0_logic" onclick="switchLogic(event, 'eng_0_type')">Latch</button></td><td><input class="hidden" id="eng_0_type" type="checkbox"></td><td><input id="eng_0_name" class="name"></td><td><error id="eng_0-error"></error></td>
            </tr>
            <tr id="eng_1">
                <td class="hidden"><input id="eng_1_mask" class="mask"></td><td><input id="eng_1_dest" class="route" inputmode="numeric"></td><td><button id="eng_1_logic" onclick="switchLogic(event, 'eng_1_type')">Latch</button></td><td><input class="hidden" id="eng_1_type" type="checkbox"></td><td><input id="eng_1_name" class="name"></td><td><error id="eng_1-error"></error></td>
            </tr>
            <tr id="eng_2">
                <td class="hidden"><input id="eng_2_mask" class="mask"></td><td><input id="eng_2_dest" class="route" inputmode="numeric"></td><td><button id="eng_2_logic" onclick="switchLogic(event, 'eng_2_type')">Latch</button></td><td><input class="hidden" id="eng_2_type" type="checkbox"></td><td><input id="eng_2_name" class="name"></td><td><error id="eng_2-error"></error></td>
            </tr>
            <tr id="eng_3">
                <td class="hidden"><input id="eng_3_mask" class="mask"></td><td><input id="eng_3_dest" class="route" inputmode="numeric"></td><td><button id="eng_3_logic" onclick="switchLogic(event, 'eng_3_type')">Latch</button></td><td><input class="hidden" id="eng_3_type" type="checkbox"></td><td><input id="eng_3_name" class="name"></td><td><error id="eng_3-error"></error></td>
            </tr>
            <tr id="eng_4">
                <td class="hidden"><input id="eng_4_mask" class="mask"></td><td><input id="eng_4_dest" class="route" inputmode="numeric"></td><td><button id="eng_4_logic" onclick="switchLogic(event, 'eng_4_type')">Latch</button></td><td><input class="hidden" id="eng_4_type" type="checkbox"></td><td><input id="eng_4_name" class="name"></td><td><error id="eng_4-error"></error></td>
            </tr>
            <tr id="eng_5">
                <td class="hidden"><input id="eng_5_mask" class="mask"></td><td><input id="eng_5_dest" class="route" inputmode="numeric"></td><td><button id="eng_5_logic" onclick="switchLogic(event, 'eng_5_type')">Latch</button></td><td><input class="hidden" id="eng_5_type" type="checkbox"></td><td><input id="eng_5_name" class="name"></td><td><error id="eng_5-error"></error></td>
            </tr>
        </table>
    
    </div>
        

    <div id="Paris" class="tabcontent">
        <div class="buttons-grid">
        <table id="buttons" class="buttons-left">
            <tr><th>Btn</th><th>Src</th><th>Pos</th></tr>
            <tr id="button_0"><td>1</td><td><input id="button_0_source" inputmode="numeric"></td><td><select id="button_0_eng"></select></td><td><error id="button_0-error"></error></td></tr>
            <tr id="button_1"><td>2</td><td><input id="button_1_source" inputmode="numeric"></td><td><select id="button_1_eng"></select></td><td><error id="button_1-error"></error></td></tr>
            <tr id="button_2"><td>3</td><td><input id="button_2_source" inputmode="numeric"></td><td><select id="button_2_eng"></select></td><td><error id="button_2-error"></error></td></tr>
            <tr id="button_3"><td>4</td><td><input id="button_3_source" inputmode="numeric"></td><td><select id="button_3_eng"></select></td><td><error id="button_3-error"></error></td></tr>
            <tr id="button_4"><td>5</td><td><input id="button_4_source" inputmode="numeric"></td><td><select id="button_4_eng"></select></td><td><error id="button_4-error"></error></td></tr>
            <tr id="button_5"><td>6</td><td><input id="button_5_source" inputmode="numeric"></td><td><select id="button_5_eng"></select></td><td><error id="button_5-error"></error></td></tr>
        </table>
        <table id="buttons2" class="buttons-right">
            <tr><th>Btn</th><th>Src</th><th>Pos</th></tr>
            <tr id="button_6"><td>7</td><td><input id="button_6_source" inputmode="numeric"></td><td><select id="button_6_eng"></select></td><td><error id="button_6-error"></error></td></tr>
            <tr id="button_7"><td>8</td><td><input id="button_7_source" inputmode="numeric"></td><td><select id="button_7_eng"></select></td><td><error id="button_7-error"></error></td></tr>
            <tr id="button_8"><td>9</td><td><input id="button_8_source" inputmode="numeric"></td><td><select id="button_8_eng"></select></td><td><error id="button_8-error"></error></td></tr>
            <tr id="button_9"><td>10</td><td><input id="button_9_source" inputmode="numeric"></td><td><select id="button_9_eng"></select></td><td><error id="button_9-error"></error></td></tr>
            <tr id="button_10"><td>11</td><td><input id="button_10_source" inputmode="numeric"></td><td><select id="button_10_eng"></select></td><td><error id="button_10-error"></error></td></tr>
            <tr id="button_11"><td>12</td><td><input id="button_11_source" inputmode="numeric"></td><td><select id="button_11_eng"></select></td><td><error id="button_11-error"></error></td></tr>
        </table>
        </div>
    </div>
        

    <div id="Tokyo" class="tabcontent">
    </div>
<div class="tab">
        <button id="submit-button" onclick="submitSettings()">Submit</button>
        <button id="reboot-button" onclick="sendReset()">Reboot</button>
        <button id="export-button" onclick="exportSettings()">Export</button>
        <button id="import-button" onclick="importSettings()">Import</button>
        <button id="dark-mode-btn" onclick="toggleDarkMode()">Dark Mode</button>
      </div>

      </div>
      <div class="overlay" id="overlay2">

            <button id="reload-page" onclick="location.reload()">Reload</button>

      </div>


    </div>
    <footer style="position:fixed;bottom:0px;left:0;right:0;padding:12px 12px;">
      <a href="https://videowalrus.com" style="color: var(--dark-blue-accent);">www.videowalrus.com</a>
      <span style="margin-left:20px;color:var(--type-col);opacity:0.6;">v3.1.0</span>
    </footer>


</body>
)rawLiteral";

#define PACKET_MAX_SIZE 1024 * 4

// Protocol type enum
enum RouterProtocolType {
  PROTOCOL_VIDEOHUB = 0,
  PROTOCOL_SWP08 = 1
};

// Singleton for easy use of NativeEthernet library
class Network {


public:
  IPAddress ip;
  WebsocketsClient* webSocketClient = nullptr;

  bool isConnectedToRouter = false;
  bool autoConnect = false; // used to auto retry to the router

  // Protocol instances
  VideoHubProtocol videoHubProtocol;
  SWP08Protocol swp08Protocol;
  RouterProtocol* currentProtocol = nullptr;
  RouterProtocolType protocolType = PROTOCOL_VIDEOHUB;

  Network() {
    // Get mac address
    teensyMAC(mac);
    // Default to VideoHub protocol
    currentProtocol = &videoHubProtocol;
  }

  // Set the active protocol
  void setProtocol(RouterProtocolType type) {
    protocolType = type;
    if(type == PROTOCOL_SWP08) {
      currentProtocol = &swp08Protocol;
      info("Protocol set to SWP-08");
    } else {
      currentProtocol = &videoHubProtocol;
      info("Protocol set to VideoHub");
    }
    // Reset protocol state and reconnect
    currentProtocol->reset();
    if(isConnectedToRouter && routerClient != nullptr) {
      info("Reconnecting after protocol change...");
      reconnectToRouter(router_ip, router_port);
    }
  }

  // Set SWP-08 level
  void setSWP08Level(uint8_t level) {
    swp08Protocol.level = level;
    info("SWP-08 Level set to ", level);
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

  IPAddress router_ip;
  uint16_t router_port;

  void connectToRouter(IPAddress _ip, uint16_t _port) {

    router_ip = _ip;
    router_port = _port;

    routerClient = new EthernetClient();
    if(routerClient->connect(_ip, _port)) {
      info("Connected to router (", currentProtocol->getName(), ")!");
      isConnectedToRouter = true;
      // tell websocket client
      if(webSocketClient != nullptr && webSocketClient->available())
        sendMessage(webSocketClient, "[\"router-stat\", true]");

      // For SWP-08, poll the configured destinations to populate routing pairs
      if(protocolType == PROTOCOL_SWP08) {
        delay(100);  // Let connection settle
        pollConfiguredDestinations();
      }
    }
    else {
      err("Could NOT connect to router!");
      isConnectedToRouter = false;
    }

  }

  // Poll the router for current state of all configured engineer destinations
  void pollConfiguredDestinations() {
    if(protocolType != PROTOCOL_SWP08 || routerClient == nullptr) return;

    info("Polling configured destinations...");

    // Wait for connection to be fully established
    delay(500);

    // Loop through all 6 engineers and interrogate their destinations
    for(uint8_t i = 0; i < 6; i++) {
      // Check connection is still valid before each send
      if(!routerClient->connected()) {
        err("Connection lost during polling");
        isConnectedToRouter = false;
        return;
      }

      byte dest[1];
      Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)i * 14));

      // Only poll if destination is configured (non-zero)
      if(*dest > 0) {
        info("Interrogating destination: ", *dest);
        swp08Protocol.interrogate(*dest);
        routerClient->write(swp08Protocol.getRouteMessage(), swp08Protocol.getRouteMessageLength());
        delay(100);  // Delay between queries to let router respond
      }
    }
  }

  void reconnectToRouter(IPAddress _ip, uint16_t _port) {
    info("Attempting to reconnect to router...");

    if(routerClient != nullptr) {
      routerClient->stop();
      delete routerClient;
      routerClient = nullptr;
    }
    delay(100); // Brief delay before reconnecting

    connectToRouter(_ip, _port);
    delay(500); // Wait for connection to stabilize

  }

  void pollRouter() {
    if(routerClient == nullptr) return;

    if(routerClient->available()) {
      uint8_t c = routerClient->read();
      currentProtocol->parse(c);
      //Serial.print((char)c); // <- uncomment for router debug
    }

    if (millis() % 1000 == 0) {
      if(!routerClient->connected()) {
        sendMessage(webSocketClient, "[\"router-stat\", false]");
        isConnectedToRouter = false;
      }
      else {
        sendMessage(webSocketClient, "[\"router-stat\", true]");
        isConnectedToRouter = true;
      }
    }

  }

  // Send a route command using the current protocol
  void sendRouteToRouter(uint16_t dest, uint16_t source) {
    // Check actual connection state, not just the flag
    if(routerClient != nullptr && routerClient->connected()) {
      isConnectedToRouter = true;
      currentProtocol->sendRoute(dest, source);

      if(protocolType == PROTOCOL_VIDEOHUB) {
        VideoHubProtocol* vh = (VideoHubProtocol*)currentProtocol;
        info("Sending to VideoHub:\n", vh->getRouteMessage());
        routerClient->write(vh->getRouteMessage());
      }
      else if(protocolType == PROTOCOL_SWP08) {
        SWP08Protocol* swp = (SWP08Protocol*)currentProtocol;
        info("Sending to SWP-08: ", swp->getRouteMessageLength(), " bytes");
        routerClient->write(swp->getRouteMessage(), swp->getRouteMessageLength());
      }

      currentProtocol->expected_resp++;
      info("End of message");
    }
    else {
      isConnectedToRouter = false;
      info("Router is not connected! Not sending message...");
      info("Reconnecting...");
      reconnectToRouter(router_ip, router_port);
    }
  }

  // Legacy method for compatibility - redirects to sendRouteToRouter
  void sendMessageToRouter(const char* _message) {
    if(isConnectedToRouter && protocolType == PROTOCOL_VIDEOHUB) {
      info("Sending message to Router:\n", _message);
      routerClient->write(_message);
      currentProtocol->expected_resp++;
      info("End of message");
    }
    else if(!isConnectedToRouter) {
      info("Router is not connected! Not sending message...");
      info("Reconnecting...");
      reconnectToRouter(router_ip, router_port);
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
  EthernetClient* routerClient;

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
