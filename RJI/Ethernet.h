#include "core_pins.h"
#include "IPAddress.h"
#include <stdint.h>
#ifndef Ethernet_h
#define Ethernet_h


// This header acts as an interface for the QNEthernet library
// with WebSockets2_Generic for WebSocket support
// Containing helper functions, classes and definitions for hosting the webpage/
// WebSockets server and connecting to the BlackMagic router


// QNEthernet for Teensy 4.1
#include <QNEthernet.h>
#include <QNMDNS.h>
using namespace qindesign::network;

// WebSockets2_Generic for WebSocket server
#include <WebSockets2_Generic.h>
using namespace websockets2_generic;

#include "Debug.h"
#include "RouterProtocol.h"
#include "VideoHubProtocol.h"
#include "SWP08Protocol.h"
#include "TSL31Protocol.h"

// Message callback function pointer type
typedef void (*WebSocketMessageCallback)(const char* data, size_t len);


// The following are raw literal strings that make up the webpage
// delivered by the web server which contains the instructions for
// a client to initalise a WebSocket connection


const char webpageA[] PROGMEM =R"rawLiteral(
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>GPI-12 Interface</title>
    <script>

    var socket;
    var lastMessageDate = new Date();  // Initialize to now to prevent early timeout
    var reconnectAttempts = 0;
    var maxReconnectAttempts = 10;
    var isRouterConnected = false;
    var currentProtocol = "Unknown";
    var fwUpdateInProgress = false;  // Global flag for firmware update

    var connectTimeout = null;
    var hasEverConnected = false;

    function connectWebSocket() {
        var wsUrl = "ws://" + window.location.hostname + ":8080";
        console.log("[WS] Connecting to", wsUrl);
        socket = new WebSocket(wsUrl);

        // If socket doesn't open within 5s, close and let reconnect handle it
        connectTimeout = setTimeout(function() {
            if(socket && socket.readyState === WebSocket.CONNECTING) {
                console.warn("[WS] Connection timeout");
                socket.close();
            }
        }, 5000);

        socket.addEventListener('open', function() {
            console.log("[WS] Connected");
            clearTimeout(connectTimeout);
            hasEverConnected = true;
            reconnectAttempts = 0;
            lastMessageDate = new Date();  // Reset on connect to prevent immediate timeout
            updateStatusBar();
        });

        socket.addEventListener('message', webSocketMessage);

        socket.addEventListener('close', function(e) {
            console.warn("[WS] Closed (code:" + e.code + " reason:" + (e.reason || "none") + ")");
            clearTimeout(connectTimeout);
            setConnectionStatus(false);
            attemptReconnect();
        });

        socket.addEventListener('error', function(e) {
            console.error("[WS] Error", e);
            clearTimeout(connectTimeout);
            setConnectionStatus(false);
        });
    }

    var reconnectTimer = null;

    function attemptReconnect() {
        // Prevent duplicate reconnect attempts
        if(reconnectTimer) return;

        // If never connected and already tried 2 times, force page reload
        // (Safari has a bug where WS connections fail after page reload,
        // but a second reload always works)
        if(!hasEverConnected && reconnectAttempts >= 2) {
            console.log("[WS] Forcing page reload to reset connection (Safari workaround)");
            location.reload();
            return;
        }

        if(reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            var delay = reconnectAttempts <= 1 ? 500 : reconnectAttempts <= 4 ? 1500 : Math.min(1000 * Math.pow(2, reconnectAttempts - 4), 30000);
            console.log("[WS] Reconnecting in " + delay + "ms (attempt " + reconnectAttempts + "/" + maxReconnectAttempts + ")");
            updateStatusBar();
            reconnectTimer = setTimeout(function() {
                reconnectTimer = null;
                connectWebSocket();
            }, delay);
        } else {
            console.error("[WS] Max reconnect attempts reached, giving up");
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
                case "version":
                    console.log("[WS] Firmware version:", json[1]);
                    var vEl = document.getElementById('fw-version');
                    var vfEl = document.getElementById('fw-version-footer');
                    if(vEl) vEl.textContent = 'v' + json[1];
                    if(vfEl) vfEl.textContent = 'v' + json[1];
                    break;
                case "vh-stat":
                case "router-stat":
                    console.log("[WS] Router status:", json[1] ? "connected" : "disconnected");
                    isRouterConnected = json[1];
                    setRouterConnectionStatus(json[1]);
                    break;
                case "settings":
                    console.log("[WS] Received settings");
                    readSettings(json);
                    break;
                case "gpi":
                    console.log("[WS] GPI:", json[1], json[2] ? "DOWN" : "UP");
                    setGPI(json[1], json[2]);
                    break;
                case "rts":
                    console.log("[WS] Route update: dest", json[1], "-> src", json[2]);
                    updateRTS(json[1], json[2]);
                    break;
                case "fw-progress":
                case "fw-error":
                case "fw-ready":
                case "fw-flashing":
                    console.log("[WS] Firmware:", json[0], json[1] || "");
                    handleFirmwareMessage(json);
                    break;
                case "error":
                    console.error("[WS] Server error:", json[1]);
                    // Don't stop reconnecting - server busy is transient (e.g. during page reload)
                    break;
                default:
                    console.warn("[WS] Unknown message type:", json[0]);
            }
            lastMessageDate = new Date();
            updateStatusBar();
        } catch(e) {
            console.error("[WS] Error parsing message:", e, _event.data);
        }
    }

    function checkConnection() {
        // Skip connection check during firmware update
        if(fwUpdateInProgress) {
            updateStatusBar();
            return;
        }
        // Only check timeout if socket is open (not while connecting)
        if(!socket || socket.readyState !== WebSocket.OPEN) return;
        const currDate = new Date();
        var elapsed = currDate - lastMessageDate;
        if(lastMessageDate && elapsed > 10000) {
            console.warn("[WS] No message for " + Math.round(elapsed/1000) + "s, closing connection");
            socket.close();
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
        currentProtocol = protocol ? ['VideoHub', 'SWP-08', 'TSL 3.1'][protocol.selectedIndex] || 'VideoHub' : currentProtocol;

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
            // Don't show overlay on Network/Firmware tabs - user needs access to configure
            var activeTab = document.querySelector('.tablinks.active');
            var tabId = activeTab ? activeTab.textContent : '';
            if(tabId !== 'Network' && tabId !== 'Firmware') {
                overlay2.classList.add("active");
            }
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
        var tabPosition = document.getElementById("tab-position");
        var tabGpiPatch = document.getElementById("tab-gpi-patch");

        if(protocolSelect && swp08Fields) {
            // Show SWP-08 fields only when SWP-08 is selected
            if(protocolSelect.selectedIndex == 1) {
                swp08Fields.style.display = "block";
            } else {
                swp08Fields.style.display = "none";
            }
        }

        // Hide Position and GPI Patch tabs when TSL 3.1 is selected
        if(protocolSelect && tabPosition && tabGpiPatch) {
            if(protocolSelect.selectedIndex == 2) {
                tabPosition.style.display = "none";
                tabGpiPatch.style.display = "none";
                // Switch to Network tab if currently on a hidden tab
                var london = document.getElementById("London");
                var paris = document.getElementById("Paris");
                if((london && london.classList.contains("active-tab")) ||
                   (paris && paris.classList.contains("active-tab"))) {
                    changeTab(null, 'Tokyo');
                }
            } else {
                tabPosition.style.display = "";
                tabGpiPatch.style.display = "";
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

    function updateRTS(dest, source) {
        // Update RTS display for any engineer position that matches this destination
        // Router uses 0-indexed, UI uses 1-indexed
        for(var i = 0; i < 6; i++) {
            var destInput = document.getElementById('eng_' + i + '_dest');
            var rtsSpan = document.getElementById('eng_' + i + '_rts');
            if(destInput && rtsSpan) {
                var engDest = parseInt(destInput.value, 10);
                if(engDest === dest + 1) {
                    rtsSpan.textContent = source + 1;
                }
            }
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
        { id:"interface-ip", input_type:"ip",  label:"Interface IP ", error_message:" Invalid IP address", requiresReboot: true},
        { id:"interface-gw", input_type:"ip",  label:"Interface Gateway IP ", error_message:" Invalid IP address", requiresReboot: true},
        { id:"interface-sub", input_type:"ip",  label:"Interface Subnet Mask ", error_message:" Invalid subnet mask", requiresReboot: true},
        { id:"interface-dhcp", input_type:"bool", label:"DHCP ", error_message:"", requiresReboot: true },
        { id:"router-ip", input_type:"ip", label:"Router IP ", error_message:" Invalid IP address", requiresReboot: false},
        { id:"router-port", input_type:"port", label:"Router Port ", error_message:" Invalid port (0-65535)", requiresReboot: false},
        { id:"swp08-level", input_type:"level", label:"SWP-08 Level ", error_message:" Invalid level (0-15)", requiresReboot: false}
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
        var optTSL = document.createElement("option");
        optTSL.value = "2";
        optTSL.innerHTML = "TSL 3.1";
        protocolSelect.appendChild(optVH);
        protocolSelect.appendChild(optSWP);
        protocolSelect.appendChild(optTSL);
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

            // Add objects
            inputObject.appendChild(inputObjectLabel);
            inputObject.appendChild(inputObjectInput);
            inputObject.appendChild(inputObjectError);

            // Add reboot indicator for settings that require restart
            if(obj.requiresReboot) {
                var rebootIndicator = document.createElement("span");
                rebootIndicator.className = "reboot-indicator";
                rebootIndicator.title = "Requires reboot to take effect";
                rebootIndicator.innerHTML = "*";
                inputObjectLabel.appendChild(rebootIndicator);
            }

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

        // Add reboot legend at bottom of Network settings
        var legend = document.createElement("div");
        legend.className = "reboot-legend";
        legend.innerHTML = '<span class="reboot-indicator">*</span> Requires reboot to take effect';
        tokyo.appendChild(legend);

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
        console.log("[WS] Sending router retry");
        socket.send("[\"router_retry\", " + false + "]");
    }

	function sendReset() {
	    if(confirm("Are you sure you want to reboot the interface?")) {
	        console.log("[WS] Sending reboot command");
	        socket.send("[\"reset\"]");
	    }
	}


    function submitSettings() {
        console.log("[Settings] Saving settings...");
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
            console.log("[Settings] Settings saved");
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

        // Hide/show overlay2 based on tab - Network and Firmware should always be accessible
        if(tabid === 'Tokyo' || tabid === 'Firmware') {
            overlay2.classList.remove("active");
        } else if(socket && socket.readyState !== WebSocket.OPEN) {
            overlay2.classList.add("active");
        }
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

    // Firmware update functions
    var fwTotalLines = 0;
    var fwSentLines = 0;

    function selectFirmware() {
        if(fwUpdateInProgress) {
            alert('Firmware update already in progress');
            return;
        }
        document.getElementById('hexFileInput').click();
    }

    function handleFirmwareFile(input) {
        if(!input.files || !input.files[0]) return;

        var file = input.files[0];
        if(!file.name.endsWith('.hex')) {
            alert('Please select a .hex file');
            return;
        }

        document.getElementById('fw-filename').textContent = file.name;
        document.getElementById('fw-filesize').textContent = (file.size / 1024).toFixed(1) + ' KB';
        document.getElementById('fw-upload-btn').disabled = false;
        document.getElementById('fw-status').textContent = 'Ready to upload';
        document.getElementById('fw-status').className = '';
    }

    var fwStartConfirmed = false;

    async function uploadFirmware() {
        var fileInput = document.getElementById('hexFileInput');
        if(!fileInput.files || !fileInput.files[0]) {
            alert('Please select a firmware file first');
            return;
        }

        if(!socket || socket.readyState !== WebSocket.OPEN) {
            alert('WebSocket not connected');
            return;
        }

        fwUpdateInProgress = true;
        fwStartConfirmed = false;
        document.getElementById('fw-upload-btn').disabled = true;
        document.getElementById('fw-select-btn').disabled = true;
        document.getElementById('fw-status').textContent = 'Starting update...';
        document.getElementById('fw-status').className = '';
        document.getElementById('fw-progress-bar').style.width = '0%';
        document.getElementById('fw-progress-text').textContent = '0%';

        try {
            var file = fileInput.files[0];
            var text = await file.text();
            var lines = text.split('\n').filter(function(l) { return l.trim().startsWith(':'); });

            fwTotalLines = lines.length;
            fwSentLines = 0;

            // Start the update and wait for confirmation
            console.log('Sending fw-start...');
            socket.send(JSON.stringify(["fw-start"]));

            // Give device time to process fw-start
            await new Promise(function(r) { setTimeout(r, 500); });

            // Wait for fw-progress confirmation (indicates startUpdate succeeded)
            var waitCount = 0;
            while(!fwStartConfirmed && waitCount < 100) {
                await new Promise(function(r) { setTimeout(r, 100); });
                waitCount++;
                if(waitCount % 10 === 0) {
                    console.log('Waiting for start confirmation... ' + waitCount);
                }
            }

            if(!fwStartConfirmed) {
                throw new Error('Timeout waiting for update start confirmation. Check serial for fw-start message.');
            }

            console.log('Start confirmed, sending ' + lines.length + ' lines...');

            // Additional delay before sending data
            await new Promise(function(r) { setTimeout(r, 200); });

            // Send lines one at a time with delay to prevent overwhelming device
            // Flash erase takes ~30-100ms per sector, so we need to pace the upload
            for(var i = 0; i < lines.length; i++) {
                // Check socket is still open
                if(socket.readyState !== WebSocket.OPEN) {
                    throw new Error('WebSocket disconnected during upload');
                }

                socket.send(JSON.stringify(["fw-data", lines[i].trim()]));
                fwSentLines++;

                // Update progress every 50 lines
                if(i % 50 === 0 || i === lines.length - 1) {
                    var pct = Math.round((fwSentLines / fwTotalLines) * 100);
                    document.getElementById('fw-progress-bar').style.width = pct + '%';
                    document.getElementById('fw-progress-text').textContent = pct + '%';
                    document.getElementById('fw-status').textContent = 'Uploading... ' + fwSentLines + '/' + fwTotalLines + ' lines';
                }

                // Delay to let device process and maintain network
                // This is critical - without it, the device gets overwhelmed
                if(i % 10 === 0) {
                    await new Promise(function(r) { setTimeout(r, 50); });
                }
            }

            // Wait a bit before sending end
            await new Promise(function(r) { setTimeout(r, 500); });

            // Finish the update
            document.getElementById('fw-status').textContent = 'Verifying firmware...';
            socket.send(JSON.stringify(["fw-end"]));

        } catch(err) {
            document.getElementById('fw-status').textContent = 'Error: ' + err.message;
            document.getElementById('fw-status').className = 'fw-error';
            fwUpdateInProgress = false;
            document.getElementById('fw-upload-btn').disabled = false;
            document.getElementById('fw-select-btn').disabled = false;
        }
    }

    function handleFirmwareMessage(msg) {
        switch(msg[0]) {
            case 'fw-progress':
                var lines = msg[1];
                var bytes = msg[2];
                // First fw-progress (0,0) confirms startUpdate succeeded
                if(lines === 0 && bytes === 0) {
                    fwStartConfirmed = true;
                    document.getElementById('fw-status').textContent = 'Update started, uploading...';
                } else {
                    document.getElementById('fw-status').textContent = 'Received: ' + lines + ' lines, ' + bytes + ' bytes';
                }
                break;

            case 'fw-error':
                document.getElementById('fw-status').textContent = 'Error: ' + msg[1];
                document.getElementById('fw-status').className = 'fw-error';
                fwUpdateInProgress = false;
                document.getElementById('fw-upload-btn').disabled = false;
                document.getElementById('fw-select-btn').disabled = false;
                break;

            case 'fw-ready':
                document.getElementById('fw-status').textContent = 'Firmware verified! Flashing...';
                document.getElementById('fw-status').className = 'fw-success';
                document.getElementById('fw-progress-bar').style.width = '100%';
                document.getElementById('fw-progress-text').textContent = '100%';
                break;

            case 'fw-flashing':
                document.getElementById('fw-status').textContent = 'Flashing firmware... Device will reboot.';
                document.getElementById('fw-status').className = 'fw-success';
                break;
        }
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
        --bg-col: #f5f5f5;
        --type-col: #2F2F2F;
        --header-bg: #4db6c5;
        --header-text: #fff;
        --card-bg: #fff;
        --card-border: #ddd;
        --table-header-bg: #5a5a5a;
        --table-header-text: #fff;
        --table-row-odd: #fff;
        --table-row-even: #f9f9f9;
        --green-accent: #2ecc71;
        --dark-green-accent: #27ae60;
        --blue-accent: #3498db;
        --dark-blue-accent: #2980b9;
        --red-accent: #e74c3c;
        --dark-red-accent: #c0392b;
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
        --header-bg: #2a5a6a;
        --header-text: #fff;
        --card-bg: #2a2a3e;
        --card-border: #444;
        --table-header-bg: #3a3a4e;
        --table-header-text: #fff;
        --table-row-odd: #2a2a3e;
        --table-row-even: #252535;
        --green-accent: #4a7c36;
        --dark-green-accent: #6aad4a;
        --blue-accent: #2a3a4a;
        --dark-blue-accent: #3a5a7a;
        --red-accent: #c0392b;
        --dark-red-accent: #e74c3c;
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

    .reboot-indicator {
        color: var(--dark-orange-accent);
        font-weight: bold;
        margin-left: 2px;
        cursor: help;
    }

    .reboot-legend {
        font-size: 11px;
        color: var(--dark-gray-accent);
        margin: 10px 0 5px 0;
        padding: 5px;
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

    /* Firmware update styles */
    .fw-section {
        padding: 10px;
    }

    .fw-section h3 {
        margin-top: 0;
    }

    .fw-file-info {
        margin: 10px 0;
        padding: 8px;
        background: var(--light-gray-accent);
        border-radius: 4px;
    }

    .fw-progress-container {
        margin: 15px 0;
        background: var(--light-gray-accent);
        border-radius: 4px;
        height: 24px;
        position: relative;
    }

    .fw-progress-bar {
        background: var(--green-accent);
        height: 100%;
        border-radius: 4px;
        width: 0%;
        transition: width 0.2s;
    }

    .fw-progress-text {
        position: absolute;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        font-size: 12px;
        font-weight: bold;
    }

    .fw-status {
        margin: 10px 0;
        padding: 8px;
        border-radius: 4px;
    }

    .fw-error {
        background: var(--orange-accent);
        color: #fff;
    }

    .fw-success {
        background: var(--green-accent);
    }

    .fw-buttons {
        margin-top: 10px;
    }

    .fw-buttons button {
        margin-right: 10px;
        padding: 10px 20px;
        font-size: 14px;
        cursor: pointer;
    }

    .fw-buttons button:disabled {
        background-color: #555;
        cursor: not-allowed;
        opacity: 0.6;
    }

    .fw-buttons button:not(:disabled):hover {
        opacity: 0.9;
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

    /* New Header Styles */
    .main-header {
        background-color: var(--header-bg);
        padding: 15px 20px;
        text-align: center;
        margin-bottom: 20px;
    }

    .main-header h1 {
        margin: 0;
        font-size: 28px;
        font-weight: bold;
        color: var(--header-text);
        background-color: transparent;
    }

    .main-header .subtitle {
        margin: 5px 0 0 0;
        font-size: 14px;
        color: rgba(255,255,255,0.7);
        background-color: transparent;
    }

    /* Card Styles */
    .card {
        background-color: var(--card-bg);
        border: 1px solid var(--card-border);
        border-radius: 8px;
        margin: 15px auto;
        max-width: 700px;
        overflow: hidden;
    }

    .card-title {
        text-align: center;
        font-size: 20px;
        font-weight: bold;
        padding: 15px;
        margin: 0;
        background-color: var(--card-bg);
        color: var(--type-col);
        border-bottom: 1px solid var(--card-border);
    }

    /* Config Table Styles */
    .config-table {
        width: 100%;
        border-collapse: collapse;
        font-size: 14px;
    }

    .config-table th {
        background-color: var(--table-header-bg);
        color: var(--table-header-text);
        padding: 10px 8px;
        font-weight: bold;
        text-align: center;
        border: none;
    }

    .config-table td {
        padding: 8px;
        text-align: center;
        border-bottom: 1px solid var(--card-border);
        background-color: var(--card-bg);
        color: var(--type-col);
    }

    .config-table tr:nth-child(even) td {
        background-color: var(--table-row-even);
    }

    .config-table tr:nth-child(odd) td {
        background-color: var(--table-row-odd);
    }

    .config-table input {
        width: 60px;
        text-align: center;
        padding: 4px;
        border: 1px solid var(--table-border);
        border-radius: 3px;
        background-color: var(--input-bg);
        color: var(--type-col);
    }

    .config-table select {
        padding: 4px;
        border: 1px solid var(--table-border);
        border-radius: 3px;
        background-color: var(--input-bg);
        color: var(--type-col);
    }

    /* State Indicator */
    .state-indicator {
        display: inline-block;
        padding: 4px 12px;
        border-radius: 3px;
        font-weight: bold;
        font-size: 12px;
        min-width: 60px;
    }

    .state-pushed {
        background-color: var(--red-accent);
        color: #fff;
    }

    .state-idle {
        background-color: var(--blue-accent);
        color: #fff;
    }

    /* Button Bar */
    .button-bar {
        text-align: center;
        padding: 15px;
        background-color: var(--card-bg);
    }

    .btn {
        display: inline-block;
        padding: 10px 25px;
        margin: 0 5px;
        border: none;
        border-radius: 5px;
        font-size: 16px;
        font-weight: bold;
        cursor: pointer;
        float: none;
    }

    .btn-save {
        background-color: var(--green-accent);
        color: #fff;
    }

    .btn-save:hover {
        background-color: var(--dark-green-accent);
    }

    .btn-reboot {
        background-color: var(--red-accent);
        color: #fff;
    }

    .btn-reboot:hover {
        background-color: var(--dark-red-accent);
    }

    .btn-secondary {
        background-color: var(--blue-accent);
        color: #fff;
    }

    .btn-secondary:hover {
        background-color: var(--dark-blue-accent);
    }

    /* Mode Toggle in Header */
    .mode-toggle {
        position: absolute;
        top: 15px;
        right: 15px;
        background: rgba(255,255,255,0.2);
        border: 1px solid rgba(255,255,255,0.3);
        color: #fff;
        padding: 5px 10px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 12px;
    }

    .mode-toggle:hover {
        background: rgba(255,255,255,0.3);
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
                    src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAMAAAAp4XiDAAAC91BMVEVHcEyge1uQl6/EakvjUTO2io/6XyhhYWUtMDrvWij3ViOjn5/vXy4sNTf7Th3RNSxubmn7NhL4Viz5eZn5XSR8dHTXMyTzVi/7OxNATEzbLST7UyD/PBX4YiPyNxzZPCjzUSz/UhpybGzuNB1jPCj6UyDzORT+URn/NhL4YSbvRRpya2f5PBiSkI79RxzDurj3MBiTlZX/URu3s7HBu7nQKxr4ZyL8ORaOKCh+KSj+VBT/RRj/LxH+PxX9Nhf/JhD/Uxj/PBFxaGViPi78YBz8QBdxbmryUBWwqarKHiW/ubb8Xxf/SBZER0f6OBb+RRRzbGfMHx//Wxr/OhK9JR8lLjGIiYj+YhYeKi6sqacqMDL+QRQ0KiuYmZX/URbMRxpBKChVJyY7Pz9vJyZ0JydkKCb/YhW7KyAmMDL/PBCHIiK4tbGFICAlMzf9YhTXNiCsJyOVlpSuQB77XhScMSW7IR7lXhfTUxUqPD2INyhwKCqZQh77XhS8OCVCTlN+NylgJy09MCr///////7+/////v/+/v7+/vz6+vn49/b29vXv7enj5OTg4N3d29nb2NbX09DPzs3RysXJycjIv7m+vr26vLrBuK+7sKe1qZ+pqaixo5qvopWio6Kvn5Ckn5msmYqbnJqnmIq+jn2klIWVlZWhjoGdi36YhHaWgnWXgnKWgnSVgnSUgXOTfG/wWxTmWxXcVxSJbGHVTxXDThVrZWCxSBlgYF1ZW1lWVlNUVVxSUlBaTFSZNSJLUlZFUlaDPBuoJyNMTUtITlBXRkxTR0BKSkddQUJpODtHR0ZASUxyOBiTJSOELCZ1MyVEREJCREY8RkqBKSdAQ0REQUBqMihrNBg5REhYNzg3QkU9Pz9BPTpgMRk5PT08PDk5OjlENC42Nzc3NDFNLRlHLSAyNDMtMTEtLy8tLi0qLi4nLS05JhooKysxJiImKiomKSgqJyMjKCghJykiJiUrIBsZJiggIiEdIiMdIB8XISUhHRoaHx8VHB3O76tNAAAAgnRSTlMAAQIEBgoLDQ4QFBoaGh4fISUmKCsrLjMzNjk6PEBISU5QVVVaW1xjZGVlam5ydXd7fX1/h4iJi4yOj5GSlpycnZ+foaWmqaqrrK6ur7Czur2/wMTFxcvOztPV1tbX19fX2NnZ2trb3d7g4eLk5efu7vDy9PT19vn5+vz9/v7+/v7+dawK8gAABKlJREFUeNq9lcWW68wVRvepKpHV9m3uvsyMYWZmzjTTPEZeIpzMQpPgMPQzMzOjm9soWQUhLy33ujjKHok+7eLDTqLPc100O0lHBYzJK26E5LMJYxY+z40xfRYARH8y5wY5O9YcWeDKKNiJvPEFAxBlqzfYfSj0/HpizIdfGnBl5AriL6UaGr8puDLmMm28993A93jlEY/13np7VYuEqZmpZqsVHfwgBOAmKNimKle7vc2Bv8ySLZ09lb1LRIm6YzrPY+D8qE//bx/yt30whAc6Tz7zptthyb5w5oOHmilZrIBg6x8K+NGQ8q32TU//czhhyb7z3Q/sChZbFc4zgYBoFcetBU49sXnnROTAdz6sO9YOuwAaHBqApCkhMCoJomY/dufTG9SRQx9Uw/DgM30UO7DJ9HuXQ/DDKoqZ/8I9t4R69pdTGx56rAP+P1gL1la+8qFY+/sr4Drdjb53x06o2iJzzku7wgHaRY1GNhoMS4dDwuD5o44umOliqqnd2ELECHoA4KS51IryhdkYIDizDTr7N4rw8ZhxhDSzKOtAKUU2lxBCPNVQgMIHoCzLEaHUad0w7UGhnQIkdpZ0VJEoPChBicotMRIaaW1JJpa0xHZ11XTbw0QBICLQL0uAVlRbTAwKMR4w6bDgeU9nOu2BV4ColiM3kJjaAiMFIQDSwIkWJcFPG8WYWGswjvEg1zhQRPOSbDXxA1JiCwDKtBwJgJ6woDUAxIlOVBynumFMFsCDwWSr0bRoYCJS8j984iVoJSIKmgYAhJdefLKHWGwdsaMS0CiUAZSA0qJTAwRQyKIbCJRuoi8dNbbkCAYJiASSIiBo9N7R2kyFp6ojJRZwQByHACGgDLh4PCh+trsrdoQwoo5UFgV4oyUIChnPFypoEHWH0xc0bmtQ92VoxQcFOBDQFjTA/4RAWAmPINxc1ZEwvKkkAZQdOVAGFKDLINolyrDrYz6HslNHYP2bFQdSEe87BBSgBan64ly6D+TE22fOyGb3JVvPJ7k/OHeo2A5RSkvJf/FKNp34Xec+hlg1n4p7ovmjzbr7vNLvvrbw8Q89dmQXRbcEgKTRoGsXwalGoBq+tecHL02cY3LgG99ftbNTWZao2u200zh81a+K7VLP/vLXb05EYM/79ra+GlzhDVGSECvwo7KksqBy4S9bL925zY6I6Pz0z0Z9IJcIACoiqtAHyP9w05N9e/kxHn91Od/qGTF4BAgoLMH61jSv3vTWFetLevrjv+txBb6Q/KkXrlhfCv1223M58tRH+4ErRqievVICeXF/4P+PisHoyTZKzPhWK8ZgFABaUKRfbqj3XmwRf7w+Or6czc+bC8Ce3YyRo7PEAkcyDIO394+KVT87t8fkrpvm/aF9+/Cpt9eJmjZlStnCGNUL8z3e/XTR2ttGw9r5U/dfMO8ezNn5veVZjr7G5sfL9NJd72mdl/75jfOrJ07HbQ5tdY8NT4XlV0sFw2JlK8urDXbvm0vVWgy97v0vr2yfagctnfbMbPbGcUWYSqu1tN0BDfhXhtEbfvGpN9PVF9WxN9dhuz3c2Hz9UKc9qDajva/F5ZshWuq5tfJkZ8X+C5rlAVjEmfFsAAAAAElFTkSuQmCC"
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
        <button id="tab-position" class="tablinks" onclick="changeTab(event, 'London')">Position</button>
        <button id="tab-gpi-patch" class="tablinks" onclick="changeTab(event, 'Paris')">GPI Patch</button>
        <button id="tab-network" class="tablinks" onclick="changeTab(event, 'Tokyo')">Network</button>
        <button id="tab-firmware" class="tablinks" onclick="changeTab(event, 'Firmware')">Firmware</button>
    </div>
    

    <div id="London" class="tabcontent active-tab">
        <table id="engineers">
            <tr>
                <th class="hidden">Mask</th><th>Destination</th><th>RTS</th><th>Logic</th><th></th><th>Name</th>
            </tr>
            <tr id="eng_0">
                <td class="hidden"><input id="eng_0_mask" class="mask"></td><td><input id="eng_0_dest" class="route" inputmode="numeric"></td><td><span id="eng_0_rts">-</span></td><td><button id="eng_0_logic" onclick="switchLogic(event, 'eng_0_type')">Latch</button></td><td><input class="hidden" id="eng_0_type" type="checkbox"></td><td><input id="eng_0_name" class="name"></td><td><error id="eng_0-error"></error></td>
            </tr>
            <tr id="eng_1">
                <td class="hidden"><input id="eng_1_mask" class="mask"></td><td><input id="eng_1_dest" class="route" inputmode="numeric"></td><td><span id="eng_1_rts">-</span></td><td><button id="eng_1_logic" onclick="switchLogic(event, 'eng_1_type')">Latch</button></td><td><input class="hidden" id="eng_1_type" type="checkbox"></td><td><input id="eng_1_name" class="name"></td><td><error id="eng_1-error"></error></td>
            </tr>
            <tr id="eng_2">
                <td class="hidden"><input id="eng_2_mask" class="mask"></td><td><input id="eng_2_dest" class="route" inputmode="numeric"></td><td><span id="eng_2_rts">-</span></td><td><button id="eng_2_logic" onclick="switchLogic(event, 'eng_2_type')">Latch</button></td><td><input class="hidden" id="eng_2_type" type="checkbox"></td><td><input id="eng_2_name" class="name"></td><td><error id="eng_2-error"></error></td>
            </tr>
            <tr id="eng_3">
                <td class="hidden"><input id="eng_3_mask" class="mask"></td><td><input id="eng_3_dest" class="route" inputmode="numeric"></td><td><span id="eng_3_rts">-</span></td><td><button id="eng_3_logic" onclick="switchLogic(event, 'eng_3_type')">Latch</button></td><td><input class="hidden" id="eng_3_type" type="checkbox"></td><td><input id="eng_3_name" class="name"></td><td><error id="eng_3-error"></error></td>
            </tr>
            <tr id="eng_4">
                <td class="hidden"><input id="eng_4_mask" class="mask"></td><td><input id="eng_4_dest" class="route" inputmode="numeric"></td><td><span id="eng_4_rts">-</span></td><td><button id="eng_4_logic" onclick="switchLogic(event, 'eng_4_type')">Latch</button></td><td><input class="hidden" id="eng_4_type" type="checkbox"></td><td><input id="eng_4_name" class="name"></td><td><error id="eng_4-error"></error></td>
            </tr>
            <tr id="eng_5">
                <td class="hidden"><input id="eng_5_mask" class="mask"></td><td><input id="eng_5_dest" class="route" inputmode="numeric"></td><td><span id="eng_5_rts">-</span></td><td><button id="eng_5_logic" onclick="switchLogic(event, 'eng_5_type')">Latch</button></td><td><input class="hidden" id="eng_5_type" type="checkbox"></td><td><input id="eng_5_name" class="name"></td><td><error id="eng_5-error"></error></td>
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

    <div id="Firmware" class="tabcontent">
        <div class="fw-section">
            <h3>Firmware Update</h3>
            <p style="font-size:12px;opacity:0.7;">Current version: <span id="fw-version">--</span></p>

            <input type="file" id="hexFileInput" accept=".hex" style="display:none" onchange="handleFirmwareFile(this)">

            <div class="fw-file-info">
                <div><strong>File:</strong> <span id="fw-filename">No file selected</span></div>
                <div><strong>Size:</strong> <span id="fw-filesize">-</span></div>
            </div>

            <div class="fw-progress-container">
                <div id="fw-progress-bar" class="fw-progress-bar"></div>
                <span id="fw-progress-text" class="fw-progress-text">0%</span>
            </div>

            <div id="fw-status" class="fw-status">Select a .hex firmware file to upload</div>

            <div class="fw-buttons" style="position:relative;z-index:10;">
                <button id="fw-select-btn" onclick="selectFirmware()">Select File</button>
                <button id="fw-upload-btn" onclick="uploadFirmware()" disabled>Upload & Flash</button>
            </div>

            <p style="font-size:11px;margin-top:20px;opacity:0.6;margin-bottom:60px;">
                Warning: Do not power off the device during firmware update.<br>
                The device will automatically reboot after flashing.
            </p>
        </div>
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
      <span id="fw-version-footer" style="margin-left:20px;color:var(--type-col);opacity:0.6;">--</span>
    </footer>


</body>
)rawLiteral";

#define PACKET_MAX_SIZE 1024 * 4

// Protocol type enum
enum RouterProtocolType {
  PROTOCOL_VIDEOHUB = 0,
  PROTOCOL_SWP08 = 1,
  PROTOCOL_TSL31 = 2
};

// Connection state machine for non-blocking reconnection
enum ConnectionState {
  CONN_IDLE = 0,           // Not trying to connect
  CONN_DISCONNECTING,      // Stopping old connection
  CONN_WAIT_DISCONNECT,    // Waiting after disconnect
  CONN_CONNECTING,         // Attempting connection
  CONN_WAIT_CONNECT,       // Waiting for connection to settle
  CONN_POLLING_DESTS,      // Polling configured destinations (SWP-08)
  CONN_WAIT_POLL_RESP,     // Waiting for poll response
  CONN_CONNECTED           // Fully connected
};

// Singleton for easy use of QNEthernet library with WebSockets2_Generic
class Network {


public:
  IPAddress ip;

  // Multiple WebSocket client support
  static const uint8_t MAX_WS_CLIENTS = 4;
  WebsocketsClient wsClients[MAX_WS_CLIENTS];
  bool wsClientConnected[MAX_WS_CLIENTS] = {false, false, false, false};
  bool needToSendSettings[MAX_WS_CLIENTS] = {false, false, false, false};
  unsigned long wsClientConnectTime[MAX_WS_CLIENTS] = {0, 0, 0, 0};
  uint32_t wsClientGeneration[MAX_WS_CLIENTS] = {0, 0, 0, 0};

  bool isConnectedToRouter = false;
  bool autoConnect = false; // used to auto retry to the router
  unsigned long lastKeepaliveTime = 0;
  const unsigned long keepaliveInterval = 5000; // Poll every 5 seconds

  // Non-blocking connection state machine
  ConnectionState connState = CONN_IDLE;
  unsigned long connStateTime = 0;  // When we entered current state
  uint8_t pollDestIndex = 0;        // Current destination being polled

  // Protocol instances
  VideoHubProtocol videoHubProtocol;
  SWP08Protocol swp08Protocol;
  TSL31Protocol tsl31Protocol;
  RouterProtocol* currentProtocol = nullptr;
  RouterProtocolType protocolType = PROTOCOL_VIDEOHUB;

  Network() {
    // QNEthernet handles MAC address automatically
    // Default to VideoHub protocol
    currentProtocol = &videoHubProtocol;
  }

  // Set the active protocol
  void setProtocol(RouterProtocolType type) {
    protocolType = type;
    if(type == PROTOCOL_SWP08) {
      currentProtocol = &swp08Protocol;
      info("Protocol set to SWP-08");
    } else if(type == PROTOCOL_TSL31) {
      currentProtocol = &tsl31Protocol;
      info("Protocol set to TSL 3.1");
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

    // QNEthernet uses static IP configuration differently
    Ethernet.begin(_ip, _subnet, _gateway, dns);

    // Wait for link
    if (!Ethernet.waitForLink(5000)) {
      err("Ethernet link not detected!");
    }

    info("Ethernet has local IP: ", Ethernet.localIP());
    info("Ethernet has gateway IP: ", Ethernet.gatewayIP());
    info("Ethernet has subnet mask: ", Ethernet.subnetMask());
    ip = _ip;
  }


  // Startup ethernet with DHCP
  void startEthernet() {
    info("Starting DHCP...");

    // Start DHCP - this initializes hardware and starts DHCP process
    if (!Ethernet.begin()) {
      err("DHCP failed to start!");
      return;
    }
    info("DHCP started, waiting for link and IP...");

    // Wait for link to come up
    if (!Ethernet.waitForLink(10000)) {
      err("Ethernet link not detected!");
      return;
    }
    info("Ethernet link detected");

    // Wait for DHCP to complete (IP address to be assigned)
    // Note: Check for non-zero IP, not INADDR_NONE (which is 255.255.255.255)
    info("Waiting for DHCP address...");
    unsigned long startTime = millis();
    while (true) {
      IPAddress currentIP = Ethernet.localIP();
      // Check if we have a valid (non-zero) IP
      if (currentIP[0] != 0 || currentIP[1] != 0 || currentIP[2] != 0 || currentIP[3] != 0) {
        break;
      }
      if (millis() - startTime > 15000) {
        err("DHCP timeout - no IP assigned!");
        return;
      }
      delay(100);
    }

    info("Ethernet has local IP: ", Ethernet.localIP());
    info("Ethernet has gateway IP: ", Ethernet.gatewayIP());
    info("Ethernet has subnet mask: ", Ethernet.subnetMask());
    ip = Ethernet.localIP();
  }


  // Start simple web server for serving HTML page
  // @param (uint16_t) port number to listen from
  void startWebServer(uint16_t _port) {
    webServerPort = _port;
    webServer = new EthernetServer(_port);
    webServer->begin();
    info("Web Server is on Port: ", _port);
  }


  // Start mDNS responder for network discovery
  // Device will be accessible as "js3.local"
  void startMDNS() {
    if (!MDNS.begin("js3")) {
      err("Failed to start mDNS!");
      return;
    }
    // Add HTTP service
    MDNS.addService("_http", "_tcp", webServerPort);
    info("mDNS started: js3.local");
  }


  // Start WebSocket server using WebSockets2_Generic
  // @param(uint16_t) port number for WebSocket
  void startWebSocketServer(uint16_t _port, WebSocketMessageCallback _messageCallback) {
    wsMessageCallback = _messageCallback;
    wsServerPort = _port;

    wsServer.listen(_port);
    info("WebSocket Server is on Port: ", _port);
    info("WebSocket Server running: ", wsServer.available() ? "Yes" : "No");
  }

  // Helper to find a free client slot
  int8_t findFreeClientSlot() {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (!wsClientConnected[i]) {
        return i;
      }
    }
    return -1;  // No free slots
  }

  // Helper to count connected clients
  uint8_t countConnectedClients() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (wsClientConnected[i]) count++;
    }
    return count;
  }

  // Helper to check if any client is connected
  bool hasConnectedClient() {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (wsClientConnected[i]) return true;
    }
    return false;
  }

  // Poll for new WebSocket connections and messages
  void pollWebSocket() {
    // Poll existing clients first to process any pending closes
    // (e.g. browser reload closes old connection, freeing the slot for the new one)
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (wsClientConnected[i]) {
        wsClients[i].poll();
      }
    }

    // Check for new connections
    // Note: wsServer.poll() always returns true (library bug), but accept()
    // returns quickly if no TCP connection is pending
    if (wsServer.poll()) {
      WebsocketsClient newClient = wsServer.accept();
      Ethernet.loop();  // Flush handshake response to client

      if (newClient.available()) {
        int8_t freeSlot = findFreeClientSlot();

        if (freeSlot >= 0) {
          uint8_t clientIndex = (uint8_t)freeSlot;
          wsClients[clientIndex] = newClient;
          wsClientConnected[clientIndex] = true;
          wsClientConnectTime[clientIndex] = millis();
          wsClientGeneration[clientIndex]++;
          uint32_t gen = wsClientGeneration[clientIndex];
          info("WebSocket client ", clientIndex, " connected (", countConnectedClients(), "/", MAX_WS_CLIENTS, " clients)");

          // Set up message callback for this client
          wsClients[clientIndex].onMessage([this](WebsocketsMessage msg) {
            // Only log non-fw-data messages to reduce serial spam during OTA
            if (msg.data().indexOf("fw-data") == -1) {
              info("Got WebSocket message: ", msg.data().c_str());
            }
            if (wsMessageCallback) {
              wsMessageCallback(msg.data().c_str(), msg.length());
            }
          });

          // Set up close callback - capture clientIndex and generation by value
          // Generation check prevents a stale close from an old connection
          // from killing a newer connection in the same slot
          wsClients[clientIndex].onEvent([this, clientIndex, gen](WebsocketsEvent event, String data) {
            if (event == WebsocketsEvent::ConnectionClosed) {
              if (wsClientGeneration[clientIndex] == gen) {
                info("WebSocket client ", clientIndex, " disconnected");
                wsClientConnected[clientIndex] = false;
              } else {
                info("WebSocket client ", clientIndex, " stale close ignored (old connection)");
              }
            }
          });

          needToSendSettings[clientIndex] = true;
        } else {
          info("WebSocket connection rejected - max clients reached (", MAX_WS_CLIENTS, ")");
          newClient.close();
        }
      }
    }
  }

  // Poll for HTTP requests and serve the webpage
  void pollWebServer() {
    EthernetClient client = webServer->available();
    if (client) {
      bool currentLineIsBlank = true;
      String requestLine = "";
      bool firstLine = true;

      while (client.connected()) {
        if (client.available()) {
          char c = client.read();

          if (firstLine && c != '\r' && c != '\n') {
            requestLine += c;
          }

          if (c == '\n' && currentLineIsBlank) {
            // End of headers, send response
            if (requestLine.indexOf("GET / ") >= 0 || requestLine.indexOf("GET /index") >= 0) {
              // Send the main page
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html");
              client.println("Connection: close");
              client.println();
              // Use writeFully to ensure entire page is sent
              size_t pageLen = strlen(webpageA);
              client.writeFully(reinterpret_cast<const uint8_t*>(webpageA), pageLen);
              client.flush();
            } else if (requestLine.indexOf("favicon") >= 0) {
              client.println("HTTP/1.1 204 No Content");
              client.println("Connection: close");
              client.println();
            } else {
              client.println("HTTP/1.1 404 Not Found");
              client.println("Connection: close");
              client.println();
            }
            break;
          }

          if (c == '\n') {
            currentLineIsBlank = true;
            firstLine = false;
          } else if (c != '\r') {
            currentLineIsBlank = false;
          }
        }
      }
      // Give time for data to be sent before closing
      client.flush();
      delay(10);
      client.stop();
    }
  }

  IPAddress router_ip;
  uint16_t router_port;

  void connectToRouter(IPAddress _ip, uint16_t _port) {

    router_ip = _ip;
    router_port = _port;

    routerClient = new EthernetClient();
    routerClient->setConnectionTimeout(1000);  // 1 second max blocking
    if(routerClient->connect(_ip, _port)) {
      info("Connected to router (", currentProtocol->getName(), ")!");
      isConnectedToRouter = true;
      // tell websocket clients
      if(hasConnectedClient())
        sendMessage("[\"router-stat\", true]");

      // For SWP-08, start non-blocking destination polling
      if(protocolType == PROTOCOL_SWP08) {
        connState = CONN_WAIT_CONNECT;
        connStateTime = millis();
        pollDestIndex = 0;
      } else {
        connState = CONN_CONNECTED;
      }
    }
    else {
      err("Could NOT connect to router!");
      isConnectedToRouter = false;
      connState = CONN_IDLE;
    }

  }

  // Poll the router for current state of all configured engineer destinations (legacy blocking version - kept for reference)
  // Now handled by pollConnectionStateMachine() in a non-blocking way

  // Non-blocking connection state machine - call this from pollRouter()
  void pollConnectionStateMachine() {
    unsigned long now = millis();
    unsigned long elapsed = now - connStateTime;

    switch(connState) {
      case CONN_IDLE:
      case CONN_CONNECTED:
        // Nothing to do
        break;

      case CONN_DISCONNECTING:
        // Stop the old connection
        if(routerClient != nullptr) {
          routerClient->stop();
          delete routerClient;
          routerClient = nullptr;
        }
        connState = CONN_WAIT_DISCONNECT;
        connStateTime = now;
        break;

      case CONN_WAIT_DISCONNECT:
        // Wait 100ms before reconnecting
        if(elapsed >= 100) {
          connState = CONN_CONNECTING;
          connStateTime = now;
        }
        break;

      case CONN_CONNECTING:
        // Attempt connection
        routerClient = new EthernetClient();
        routerClient->setConnectionTimeout(1000);  // 1 second max blocking
        if(routerClient->connect(router_ip, router_port)) {
          info("Connected to router (", currentProtocol->getName(), ")!");
          isConnectedToRouter = true;
          sendMessage("[\"router-stat\", true]");

          if(protocolType == PROTOCOL_SWP08) {
            connState = CONN_WAIT_CONNECT;
            pollDestIndex = 0;
          } else {
            connState = CONN_CONNECTED;
          }
        } else {
          err("Could NOT connect to router!");
          isConnectedToRouter = false;
          connState = CONN_IDLE;
        }
        connStateTime = now;
        break;

      case CONN_WAIT_CONNECT:
        // Wait 500ms for connection to settle before polling destinations
        if(elapsed >= 500) {
          connState = CONN_POLLING_DESTS;
          connStateTime = now;
          pollDestIndex = 0;
          info("Polling configured destinations...");
        }
        break;

      case CONN_POLLING_DESTS:
        // Poll one destination at a time
        if(!routerClient || !routerClient->connected()) {
          err("Connection lost during polling");
          isConnectedToRouter = false;
          connState = CONN_IDLE;
          break;
        }

        // Find next configured destination
        while(pollDestIndex < 6) {
          byte dest[1];
          Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)pollDestIndex * 14));

          if(*dest > 0) {
            info("Interrogating destination: ", *dest);
            swp08Protocol.interrogate(*dest);
            routerClient->write(swp08Protocol.getRouteMessage(), swp08Protocol.getRouteMessageLength());
            connState = CONN_WAIT_POLL_RESP;
            connStateTime = now;
            return;
          }
          pollDestIndex++;
        }

        // All destinations polled
        info("Finished polling destinations");
        connState = CONN_CONNECTED;
        break;

      case CONN_WAIT_POLL_RESP:
        // Read any available response data
        while(routerClient && routerClient->available()) {
          uint8_t c = routerClient->read();
          swp08Protocol.parse(c);
        }

        // Check for RTS update
        if(swp08Protocol.updatedDest >= 0) {
          char rtsMsg[64];
          snprintf(rtsMsg, sizeof(rtsMsg), "[\"rts\", %d, %d]",
                   swp08Protocol.updatedDest, swp08Protocol.updatedSource);
          sendMessage(rtsMsg);
          swp08Protocol.updatedDest = -1;
          swp08Protocol.updatedSource = -1;
        }

        // Wait up to 200ms for response, then move to next destination
        if(elapsed >= 200) {
          pollDestIndex++;
          connState = CONN_POLLING_DESTS;
          connStateTime = now;
        }
        break;
    }
  }

  // Start non-blocking reconnection process
  void reconnectToRouter(IPAddress _ip, uint16_t _port) {
    // Don't start another reconnection if one is in progress
    if(connState != CONN_IDLE && connState != CONN_CONNECTED) {
      return;
    }

    info("Starting reconnection to router...");
    router_ip = _ip;
    router_port = _port;
    connState = CONN_DISCONNECTING;
    connStateTime = millis();
  }

  void pollRouter() {
    // Always run the connection state machine (handles reconnection)
    pollConnectionStateMachine();

    // If we're in the middle of connecting/polling, don't do normal polling
    if(connState != CONN_IDLE && connState != CONN_CONNECTED) {
      return;
    }

    if(routerClient == nullptr) return;

    // Read any available data from router
    if(routerClient->available()) {
      uint8_t c = routerClient->read();
      currentProtocol->parse(c);
      //Serial.print((char)c); // <- uncomment for router debug
    }

    // Check for routing pair updates and send to web UI
    if(currentProtocol->updatedDest >= 0) {
      char rtsMsg[64];
      snprintf(rtsMsg, sizeof(rtsMsg), "[\"rts\", %d, %d]",
               currentProtocol->updatedDest, currentProtocol->updatedSource);
      sendMessage(rtsMsg);
      currentProtocol->updatedDest = -1;
      currentProtocol->updatedSource = -1;
    }

    // Periodic keepalive and connection check (skip for TSL 3.1)
    // TSL 3.1 is send-only, no keepalive or connection monitoring needed
    if(protocolType == PROTOCOL_TSL31) {
      return;
    }

    unsigned long now = millis();
    if(now - lastKeepaliveTime >= keepaliveInterval) {
      lastKeepaliveTime = now;

      if(!routerClient->connected()) {
        sendMessage("[\"router-stat\", false]");
        isConnectedToRouter = false;
        info("Router connection lost, starting reconnect...");
        reconnectToRouter(router_ip, router_port);
      }
      else {
        sendMessage("[\"router-stat\", true]");
        isConnectedToRouter = true;

        // Send keepalive poll based on protocol
        if(protocolType == PROTOCOL_VIDEOHUB) {
          // VideoHub: send PING command
          routerClient->write("PING:\n\n");
        }
        else if(protocolType == PROTOCOL_SWP08) {
          // SWP-08: interrogate first configured destination
          for(uint8_t i = 0; i < 6; i++) {
            byte dest[1];
            Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)i * 14));
            if(*dest > 0) {
              swp08Protocol.interrogate(*dest);
              routerClient->write(swp08Protocol.getRouteMessage(), swp08Protocol.getRouteMessageLength());
              break; // Only poll one destination for keepalive
            }
          }
        }
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
        info("VideoHub TX: D:", dest, " S:", source);
        routerClient->write(vh->getRouteMessage());
      }
      else if(protocolType == PROTOCOL_SWP08) {
        SWP08Protocol* swp = (SWP08Protocol*)currentProtocol;
        routerClient->write(swp->getRouteMessage(), swp->getRouteMessageLength());
      }

      currentProtocol->expected_resp++;
    }
    else {
      isConnectedToRouter = false;
      info("Router disconnected, reconnecting...");
      reconnectToRouter(router_ip, router_port);
    }
  }

  // Legacy method for compatibility - redirects to sendRouteToRouter
  void sendMessageToRouter(const char* _message) {
    if(isConnectedToRouter && protocolType == PROTOCOL_VIDEOHUB) {
      routerClient->write(_message);
      currentProtocol->expected_resp++;
    }
    else if(!isConnectedToRouter) {
      info("Router disconnected, reconnecting...");
      reconnectToRouter(router_ip, router_port);
    }
  }

  // Send TSL 3.1 tally message
  // @param address - TSL display address (0-126), typically maps to GPI number
  // @param tallyOn - true for tally on, false for off
  void sendTallyToRouter(uint8_t address, bool tallyOn) {
    if(protocolType != PROTOCOL_TSL31) {
      return;  // Only for TSL 3.1 protocol
    }

    // TSL 3.1 is send-only, so we can't rely on connected() status
    // Just try to send if we have a client
    if(routerClient != nullptr) {
      tsl31Protocol.sendTally(address, tallyOn);
      size_t written = routerClient->write(tsl31Protocol.getRouteMessage(), tsl31Protocol.getRouteMessageLength());
      if(written == tsl31Protocol.getRouteMessageLength()) {
        isConnectedToRouter = true;
      } else {
        // Write failed, need to reconnect
        isConnectedToRouter = false;
        info("TSL 3.1 write failed, reconnecting...");
        reconnectToRouter(router_ip, router_port);
      }
    }
    else {
      isConnectedToRouter = false;
      info("No router client, reconnecting...");
      reconnectToRouter(router_ip, router_port);
    }
  }


  // WebSocket keepalive timing
  unsigned long lastWsKeepaliveTime = 0;
  const unsigned long wsKeepaliveInterval = 2000;  // Send keepalive every 2 seconds

  // Periodic WebSocket keepalive and settings send (call from loop)
  void pollWebSocketKeepalive() {
    // Send settings to newly connected clients (deferred from callback)
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (needToSendSettings[i] && wsClientConnected[i]) {
        // Wait 100ms after connection before sending to let socket stabilise
        if (millis() - wsClientConnectTime[i] < 100) continue;
        needToSendSettings[i] = false;
        info("Sending settings to client ", i, "...");

        // Send version and conn-stat first
        char versionMsg[64];
        snprintf(versionMsg, sizeof(versionMsg), "[\"version\", \"%s\"]", FW_VERSION);
        sendMessageToClient(i, versionMsg);
        sendMessageToClient(i, "[\"conn-stat\", true]");

        // Send current settings
        char buffer[2048];
        size_t len = serializeJson(Settings.getJson(), buffer, sizeof(buffer));
        info("Settings JSON length: ", len);
        sendMessageToClient(i, buffer);

        // Send RTS values to this client
        sendCurrentRTSValuesToClient(i);

        lastWsKeepaliveTime = millis();
      }
    }

    // Time-based keepalive (every 2 seconds) - broadcast to all
    unsigned long now = millis();
    if (hasConnectedClient() && (now - lastWsKeepaliveTime >= wsKeepaliveInterval)) {
      lastWsKeepaliveTime = now;
      // Send keepalive message (don't use wsClient.ping() - it causes blocking timeout issues)
      sendMessage("[\"conn-stat\", true]");
    }
  }


  // Helper to send messages to WebSocket client
  // Broadcast message to all connected WebSocket clients
  void sendMessage(const char* _message) {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (wsClientConnected[i]) {
        wsClients[i].send(_message);
      }
    }
  }

  // Send message to a specific client
  void sendMessageToClient(uint8_t clientIndex, const char* _message) {
    if (clientIndex < MAX_WS_CLIENTS && wsClientConnected[clientIndex]) {
      wsClients[clientIndex].send(_message);
    }
  }

  // Send current RTS values for all configured destinations (broadcast to all clients)
  void sendCurrentRTSValues() {
    if(currentProtocol == nullptr) return;

    for(uint8_t i = 0; i < 6; i++) {
      byte dest[1];
      Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)i * 14));

      // dest is already 0-indexed in EEPROM (client subtracts 1 before saving)
      uint16_t source = currentProtocol->routingPairs[*dest];

      char rtsMsg[64];
      snprintf(rtsMsg, sizeof(rtsMsg), "[\"rts\", %d, %d]", *dest, source);
      sendMessage(rtsMsg);
    }
  }

  // Send current RTS values to a specific client
  void sendCurrentRTSValuesToClient(uint8_t clientIndex) {
    if(currentProtocol == nullptr) return;

    for(uint8_t i = 0; i < 6; i++) {
      byte dest[1];
      Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)i * 14));

      // dest is already 0-indexed in EEPROM (client subtracts 1 before saving)
      uint16_t source = currentProtocol->routingPairs[*dest];

      char rtsMsg[64];
      snprintf(rtsMsg, sizeof(rtsMsg), "[\"rts\", %d, %d]", *dest, source);
      sendMessageToClient(clientIndex, rtsMsg);
    }
  }


private:
  EthernetServer* webServer = nullptr;
  uint16_t webServerPort = 80;
  EthernetClient* routerClient = nullptr;

  WebsocketsServer wsServer;
  uint16_t wsServerPort = 8080;
  WebSocketMessageCallback wsMessageCallback = nullptr;

};


// Global definition for Network
Network Network;


#endif
