#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>


// MAC Addresses
uint8_t forwardMAC[] = {0x48, 0xCA, 0x43, 0x58, 0xD2, 0xA8};
uint8_t esp32MAC[] = {0x48, 0xCA, 0x43, 0x3D, 0xFA, 0x68};


Adafruit_AHTX0 aht;
unsigned long lastSendTime = 0; // Timestamp to track last transmission
const unsigned long sendInterval = 120000;


// Callback function when data is sent
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
 Serial.print("Last Packet Send Status: ");
 Serial.println(sendStatus == 0 ? "Delivery Success" : "Delivery Fail");
}


// Callback function when data is received
void onDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len) {
 char incomingMessage[len + 1];
 memcpy(incomingMessage, incomingData, len);
 incomingMessage[len] = '\0';


 Serial.print("Received message: ");
 Serial.println(incomingMessage);


 // Forward the message to the next device
 uint8_t sendResult = esp_now_send(forwardMAC, (uint8_t *)incomingMessage, len);
 if (sendResult == 0) {
   Serial.println("Message forwarded successfully");
 } else {
   Serial.println("Error forwarding the message");
 }
}


void setup() {
 Serial.begin(9600);
 delay(1000);


 // Set device as a Wi-Fi Station
 WiFi.mode(WIFI_STA);


 // Initialize ESP-NOW
 if (esp_now_init() != 0) {
   Serial.println("Error initializing ESP-NOW");
   return;
 }


 // Set device role to COMBO
 esp_now_set_self_role(ESP_NOW_ROLE_COMBO);


 // Register send and receive callbacks
 esp_now_register_send_cb(onDataSent);
 esp_now_register_recv_cb(onDataRecv);


 // Add ESP32 as peer
 esp_now_add_peer(esp32MAC, ESP_NOW_ROLE_COMBO, 0, NULL, 0);


 // Add the forwarding peer
 esp_now_add_peer(forwardMAC, ESP_NOW_ROLE_COMBO, 0, NULL, 0);


 // Initialize I2C and AHT20 sensor
 Wire.begin(); // Use appropriate SDA and SCL pins for ESP8266
 if (!aht.begin()) {
   Serial.println("Failed to find AHT sensor. Check wiring!");
   while (1) delay(10);
 }
 Serial.println("AHT20 sensor initialized!");


 Serial.println("ESP8266 is ready to send, receive, and forward messages.");
}


void loop() {
 // Get current time
 unsigned long currentTime = millis();


 // Check if it's time to send the temperature
 if (currentTime - lastSendTime >= sendInterval) {
   sensors_event_t humidity, temp;
   aht.getEvent(&humidity, &temp);
   float temperature = temp.temperature;


   // Convert temperature to string
   String message = "Temperature: " + String(temperature) + "°C";


   Serial.print("Sending message: ");
   Serial.println(message);


   // Send temperature to ESP32
   uint8_t sendResult = esp_now_send(esp32MAC, (uint8_t *)message.c_str(), message.length());


   if (sendResult == 0) {
     Serial.println("Temperature sent successfully");
   } else {
     Serial.println("Error sending the temperature");
   }


   // Update last send time
   lastSendTime = currentTime;
 }


 // Check if there's data available from the Serial Monitor
 if (Serial.available()) {
   String inputMessage = Serial.readStringUntil('\n'); // Read input until newline character
   inputMessage.trim(); // Remove extra whitespace


   if (inputMessage.length() > 0) {
     Serial.print("Sending custom message: ");
     Serial.println(inputMessage);


     // Send custom message to ESP32
     uint8_t sendResult = esp_now_send(esp32MAC, (uint8_t *)inputMessage.c_str(), inputMessage.length());


     if (sendResult == 0) {
       Serial.println("Custom message sent successfully");
     } else {
       Serial.println("Error sending the custom message");
     }
   }
 }


 delay(100); // Small delay to prevent overwhelming the Serial buffer
}
