#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>  // Include the WebServer library
#include <HTTPClient.h> // Include HTTPClient library


const char* ssid = "home";
const char* password = "pswd";


const char* flaskServerIP = "192.168.0.46";
const int flaskServerPort = 5000;


// Create a web server on port 80
WebServer server(80);


// Define ShipInfo struct
struct ShipInfo {
 uint8_t mac[6];
 const char* name;
};


// List of peers (other ESP32 devices) with their MAC addresses and ship names
ShipInfo ships[] = {
 {{0xa4, 0xcf, 0x12, 0xbe, 0x5f, 0x69}, "Ship1"},
 {{0x4c, 0x75, 0x25, 0x37, 0xe0, 0x90}, "Ship2"}
};


// Total number of ships
const int numShips = sizeof(ships) / sizeof(ships[0]);


// Device's own MAC address
uint8_t localMac[6];


// Function to get ship name from MAC address
const char* getShipName(const uint8_t* mac) {
 for (int i = 0; i < numShips; i++) {
   if (memcmp(mac, ships[i].mac, 6) == 0) {
     return ships[i].name;
   }
 }
 return "Unknown";
}


// Callback when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
 Serial.print("Last Packet Sent to: ");
 char macStr[18];
 snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
          mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
 Serial.println(macStr);
 Serial.print("Send Status: ");
 Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}


// Callback when data is received via ESP-NOW
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
 char receivedMessage[250]; // Adjust the size as needed
 memcpy(receivedMessage, incomingData, len);
 receivedMessage[len] = '\0'; // Null-terminate the string
  // Get ship name based on sender's MAC address
 const char* shipName = getShipName(mac);
  Serial.print("Received message via ESP-NOW from ");
 Serial.print(shipName);
 Serial.print(": ");
 Serial.println(receivedMessage);


 // Forward the received message to the Flask server via HTTP POST request
 if(WiFi.status() == WL_CONNECTED){ // Check WiFi connection status
   HTTPClient http;
   String serverPath = String("http://") + flaskServerIP + ":" + flaskServerPort + "/receive-message";
   http.begin(serverPath);
   http.addHeader("Content-Type", "application/x-www-form-urlencoded");


   // Include the sender's MAC address and ship name
   char senderMacStr[18];
   snprintf(senderMacStr, sizeof(senderMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);


   String postData = "message=" + String(receivedMessage) + "&sender=" + String(senderMacStr) + "&ship=" + String(shipName);


   int httpResponseCode = http.POST(postData);


   if (httpResponseCode > 0) {
     Serial.print("HTTP Response code: ");
     Serial.println(httpResponseCode);
     String response = http.getString();
     Serial.println(response);
   }
   else {
     Serial.print("Error code: ");
     Serial.println(httpResponseCode);
   }
   http.end();
 }
 else {
   Serial.println("WiFi Disconnected");
 }
}


// Function to send a message via ESP-NOW to all peers
void sendMessageToPeers(String message) {
 message.trim();


 if (message.length() > 0) {
   Serial.print("Sending message via ESP-NOW: ");
   Serial.println(message);


   // Convert message to byte array
   const char *msg = message.c_str();
   size_t msgLen = message.length();


   // Send message to all peers
   for (int i = 0; i < numShips; i++) {
     esp_err_t result = esp_now_send(ships[i].mac, (uint8_t *)msg, msgLen);


     if (result == ESP_OK) {
       Serial.print("Message sent successfully to ");
       Serial.println(ships[i].name);
     } else {
       Serial.print("Error sending message to ");
       Serial.println(ships[i].name);
     }
   }
 } else {
   Serial.println("Message is empty.");
 }
}


// Function to handle incoming HTTP POST requests from Flask server
void handleReceiveMessage() {
 if (server.hasArg("message")) {
   String message = server.arg("message");
   message.trim();


   if (message.length() > 0) {
     Serial.print("Received message from Flask server: ");
     Serial.println(message);


     // Forward the message to other peers via ESP-NOW
     sendMessageToPeers(message);


     // Send HTTP response
     server.send(200, "text/plain", "Message received and forwarded via ESP-NOW.");
   } else {
     server.send(400, "text/plain", "Message is empty.");
   }
 } else {
   server.send(400, "text/plain", "No message received.");
 }
}


// Function to handle sending message from web interface
void handleSendMessage() {
 if (server.hasArg("message")) {
   String message = server.arg("message");
   message.trim();


   if (message.length() > 0) {
     Serial.print("Received message via HTTP POST: ");
     Serial.println(message);


     // Forward the message to other peers via ESP-NOW
     sendMessageToPeers(message);


     // Send HTTP response
     server.send(200, "text/plain", "Message sent via ESP-NOW.");
   } else {
     server.send(400, "text/plain", "Message is empty.");
   }
 } else {
   server.send(400, "text/plain", "No message received.");
 }
}


void setup() {
 Serial.begin(9600);


 // Initialize Wi-Fi
 WiFi.mode(WIFI_STA);
 WiFi.begin(ssid, password);
 Serial.println("Connecting to Wi-Fi...");


 int attempts = 0;
 const int maxAttempts = 40;
 while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
   delay(500);
   Serial.print(".");
   attempts++;
 }


 if (WiFi.status() != WL_CONNECTED) {
   Serial.println("\nFailed to connect to Wi-Fi.");
   return;
 }


 Serial.println("\nConnected to Wi-Fi.");
 Serial.print("IP Address: ");
 Serial.println(WiFi.localIP());


 // Get device's MAC address
 WiFi.macAddress(localMac);
 Serial.print("Device MAC Address: ");
 Serial.print(WiFi.macAddress());
 Serial.println();


 // Initialize ESP-NOW
 if (esp_now_init() != ESP_OK) {
   Serial.println("Error initializing ESP-NOW");
   return;
 }


 // Register callbacks
 esp_now_register_send_cb(onDataSent);
 esp_now_register_recv_cb(onDataRecv);


 // Add peers
 for (int i = 0; i < numShips; i++) {
   esp_now_peer_info_t peerInfo = {};
   memcpy(peerInfo.peer_addr, ships[i].mac, 6);
   peerInfo.channel = 0;
   peerInfo.encrypt = false;


   if (esp_now_add_peer(&peerInfo) != ESP_OK){
     Serial.print("Failed to add peer: ");
     Serial.println(ships[i].name);
   } else {
     Serial.print("Peer added: ");
     Serial.println(ships[i].name);
   }
 }


 // Set Wi-Fi protocol to 802.11b/g/n for ESP-NOW
 esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);


 // Start the web server
 server.on("/send-message", HTTP_POST, handleSendMessage);
 server.on("/receive-message", HTTP_POST, handleReceiveMessage); // Endpoint for Flask server to send messages
 server.begin();
 Serial.println("Web server started. Waiting for incoming messages...");
}


void loop() {
 // Handle client requests
 server.handleClient();
}
