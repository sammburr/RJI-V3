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

  <!DOCTYPE HTML>
  <html>

    <head>

      <style>
        * {
          font-family: 'Courier New', Courier, monospace;
        }
        #ws-connection-status {
          background-color: red;
          border: red 5px solid;
          border-radius: 5px;
        }
        .input {
          background-color: rgb(224, 224, 224);
          border: rgb(224, 224, 224) 5px solid;
          border-radius: 5px;
        }
      </style>


      <script>

        var socket = new WebSocket("ws://" + window.location.hostname + ":80");
        var lastMessageDate;

        var inputContent = "<>"
        var inputObjects = [

          { id: "interface-id" }

        ];

        function webSocketConnected() {
          console.log("connected to arudino!");
        }

        function webSocketMessage(_event) {
          lastMessageDate = new Date();

          const json = JSON.parse(_event.data);

          switch(json[0]) {

            case "conn-stat":
              setConnectionStatus(json[1]);
              break;

            case "pong":
              break;

          }
        }

        function connectCallbacks(_socket) {
          _socket.addEventListener('open', webSocketConnected);
          _socket.addEventListener('message', webSocketMessage);

        }

        connectCallbacks(socket);


        function setConnectionStatus(_val) {
          const element = document.getElementById("ws-connection-status");

          if(_val) {
            element.innerHTML = "Connected!";
            element.style = "background-color: greenyellow; border-color: greenyellow;";


          }
          else {
            element.innerHTML = "Not Connected!";
            element.style = "";

          }
          
        }

        function checkConnection() {
          const currDate = new Date();
          if(currDate - lastMessageDate > 2000) {
            socket.close();
            setConnectionStatus(false);

          }

        }
        setInterval(checkConnection, 2000);

        function isValidIPAddress(ipAddress) {
          // Regular expression to match a valid IPv4 address
          const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;

          return ipRegex.test(ipAddress);
        }

        function isValidPort(port) {
          // Check if the port is a non-empty string and is a valid positive integer
          return /^\d+$/.test(port) && parseInt(port, 10) >= 0 && parseInt(port, 10) <= 65535;
        }

        function submit() {

          const interfaceIP = document.getElementById("interface-ip").value;
          const interfaceIPMessage = document.getElementById("interface-ip-msg");
          if (isValidIPAddress(interfaceIP)) {
            console.log("valid!");
            const interfaceIPParts = interfaceIP.split(".");
            socket.send("[\"interface-ip\"," + interfaceIPParts[0] + "," +
              interfaceIPParts[1] + "," +
              interfaceIPParts[2] + "," +
              interfaceIPParts[3] + "]");

            interfaceIPMessage.innerHTML = "";
          }
          else {
            console.log("invalid!");
            interfaceIPMessage.innerHTML = " Not a valid IP!";
          }

        }

      </script>

    </head>

    <body>
      
      <span id="ws-connection-status">Not connected!</span>

      <br>
      <br>

      <span class="input-field"><label>Interface IP: </label><input id="interface-ip"><button onclick="submit()">Submit</button><span class="input-message" id="interface-ip-msg" style="color: red;"></span></span>

    </body>

  </html>

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
      if (clock%100000 == 0) sendMessage(webSocketClient, "[\"conn-stat\", true]");
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