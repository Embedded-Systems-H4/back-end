// Wifi libraries
#include <WiFiS3.h>
#include <PubSubClient.h>

#include <Wire.h>
#include "SparkFun_Qwiic_Keypad_Arduino_Library.h"

// Wifi Constants
const char* ssid = "SkyLab Academy";
const char* password = "SkyLab_Academy";
const char* mqttServer = "10.71.202.218";
const int mqttPort = 1883;

const char* ArduinoType = "Keypad"; // RFID, Keypad, None

const char* DoorOpenTopic = "devices/doors";
const char* HeartbeatTopic = "devices/heartbeat";
const char* RegisterTopic = "devices/register";

byte Mac[6];
char MacStr[6];
String DeviceName;

// Wifi Client
WiFiClient espClient;
PubSubClient client(espClient);
int WiFiConnectionRetries = 10;
int WiFiRetries = 0;

// Keypad constants and variables
KEYPAD keypad1;
const char keyCode[] = {'1', '2', '3', '4', '5', '6'}; // the correct keyCode - change to your own unique set of keys if you like.
char userEntry[] = {0, 0, 0, 0, 0, 0}; // used to store the presses coming in from user
boolean userIsActive = false; // used to know when a user is active and therefore we want to engage timeout stuff

#define TIMEOUT 60 // 100s of milliseconds, used to reset input. 30 equates to 3 second.
byte timeOutCounter = 0; // variable this is incremented to keep track of timeouts.
byte userEntryIndex = 0; // used to keep track of where we are in the userEntry, incremented on each press, reset on timeout.


void(* resetFunc) (void) = 0; 

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) delay(2000);  // Delay for startup

  // Run any extras for this device
  SetupExtraPerf();

  // Get MacAddress
  WiFi.macAddress(Mac);

  // Convert MacAddress
  int num = 0;
  for (int i = 0; i < 6; i++) {
    num += (int)Mac[i];
  }
  memset(MacStr, '\0', sizeof(MacStr));
  itoa(num, MacStr, 10);
  if((String)MacStr == "0"){
    resetFunc();  //call reset
  }

  // Set device name
  DeviceName = "device-" + (String)MacStr + "-door";

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && WiFiRetries < WiFiConnectionRetries) {
    Serial.print("Connecting to WiFi... Try: ");
    Serial.println(WiFiRetries + 1);
    delay(1000);
    WiFiRetries += 1;
  }
  WiFiRetries = 0;
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("Connected to WiFi");
  }

  // Connect to MQTT broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  if(reconnect()){

    String device = "{\"id\":\"" + (String)MacStr + "\",\"name\":\"" + DeviceName + "\",\"type\":\"door\", \"accesstype\":\"" + (String)ArduinoType + "\"}";
    Serial.println(device);
    if(client.publish("devices/register", device.c_str())){
      Serial.println("Device has been registered!");
    } else {
      Serial.println("Failed to send registration message!");
    }

    // Needs to be subscribed to stuff here:

    // Subscribe to MQTT client
    if(client.subscribe(DoorOpenTopic)){
      Serial.println("Subscribed to topic!");
    }
  }
}

uint8_t Address = 0x4B; //Start address (Default 0x4B)

void SetupExtraPerf(){

  // Wire1 is for the Qwiic connector (Usually Wire when I2C)
  Wire1.begin();

  if (keypad1.begin(Wire1, Address) == false)
  {
    Serial.println("Keypad does not appear to be connected. Please check wiring. Freezing...");
    while (1);
  } else {
    Serial.print("Address: 0x");
    Serial.print(Address, HEX);
    Serial.print(" Version: ");
    Serial.println(keypad1.getVersion());
  }  
}

unsigned int LoopHeartbeatDelay = 3000;
unsigned long LoopTime;
unsigned long LoopTimeoutTime;

// Loop method running
void loop(void){
  // Regular loop code for MQtt
  if (!client.connected()) {
    reconnect();
  } else if (client.connected()) {
    client.loop();
  }
  
  LoopTime = millis();
  if (LoopTimeoutTime + LoopHeartbeatDelay < LoopTime) {
    LoopTimeoutTime = millis();

    // Heartbeat executed code
    if (client.connected()) {
      LoopHeartbeat();
    }
  }

  // Extras added to device
  // Loop delay is placed inside here or below call
  LoopExtras();
}

int MQTTConnectionRetries = 10;
int MQTTRetries = 0;
// MQTT reconnect code
bool reconnect() {
  while (!client.connected() && MQTTRetries < MQTTConnectionRetries ) {
    Serial.print("Attempting MQTT connection... Try: ");
    Serial.println(MQTTRetries + 1);
    // Attempt to connect
    if (client.connect(DeviceName.c_str(), "device", "DataIt2024")) {
      Serial.println("Connected to MQTT broker");
    } else {
      delay(100);
    }

    MQTTRetries += 1;
  }
  
  if (!client.connected()) { // Connection failed!
    if (MQTTRetries == MQTTConnectionRetries){
      Serial.println("Connection to MQTT failed!");
      MQTTRetries += 1;
    }

    return false;
  } else if (client.connected()) {
    MQTTRetries = 0;
  }

  return true;
}

void LoopExtras(void) {
  
  // Location for extra code used in the code loop
  keypad1.updateFIFO();  // necessary for keypad to pull button from stack to readable register
  char button = keypad1.getButton();

  if (button == -1)
  {
    Serial.println("No keypad detected");
    delay(1000);
  }
  else if (button != 0)
  {
    // CheckEntry checks input for correct code.
    if(button == '*' && checkEntry() == true){

      Serial.print("\n\rKeycode correct. Wahooooooooooo!");
      clearEntry();
      userEntryIndex = 0; // reset
      timeOutCounter = 0; // reset with any new presses.
      userIsActive = false; // don't display timeout stuff.
      delay(1000);

    } else {

      userEntry[userEntryIndex] = button; // store button into next spot in array, note, index is incremented later
      printEntry();
      userIsActive = true; // used to only timeout when user is active

      userEntryIndex++;
      if (userEntryIndex == sizeof(keyCode)){
        // userEntryIndex = sizeof(keyCode) - 1; // reset
        userEntryIndex = sizeof(keyCode) - 1; // reset
      }
      timeOutCounter = 0; // reset with any new presses.

    }
  }

  delay(10); //10 is good, more is better, note this effects total timeout time
  
  timeOutCounter++;
  
  if ((timeOutCounter == TIMEOUT) && (userIsActive == true)) // this means the user is actively inputing
  {
    Serial.println("\n\rTimed out... try again.");
    timeOutCounter = 0; // reset
    userEntryIndex = 0; // reset
    clearEntry();
    userIsActive = false; // so we don't continuously timeout while inactive.
  }

}

// check user entry against keyCode array.
// if they all match up, then respond with true.
boolean checkEntry() {

  for (byte i = 0 ; i < sizeof(keyCode) ; i++)
  {
    // do nothing, cause we're only looking for failures
    if ( userEntry[i] == keyCode[i] ){
    } else {
      return false;
    }
  }

  return true; // if we get here, all values were correct.
}

void clearEntry() {
  for (byte i = 0 ; i < sizeof(userEntry) ; i++)
  {
    userEntry[i] = 0; // fill with spaces
  }
}

void printEntry() {
  Serial.print("UserEntry: ");
  for (byte i = 0 ; i < sizeof(userEntry) ; i++)
  {
    Serial.print(char(userEntry[i]));
  }
  Serial.println();
}



// Only run if connected
void LoopHeartbeat(){
  String heartbeat = "{\"id\":\"" + (String)MacStr + "\"}";
  client.publish(HeartbeatTopic, heartbeat.c_str());
  Serial.println(heartbeat);

  // Location for extra code that should function as a heartbeat.
}

char* Topic;
byte* buffer;
char* bufferChar;

void callback(char* topic, byte* payload, unsigned int length) {
  //Payload=[];
  Topic = topic;
  length = length - 1; // Cuz why not just send the correct length... (Mosquitto bs)
  Serial.println(" "); // Spacing for console
  Serial.print("length message received in callback = ");
  Serial.println(length);

  bufferChar = new char[length];
  memset(bufferChar, '\0', sizeof(bufferChar)); // Reset

  int i = 0;
  for (i ; i < length + 1 ; i++) {
    bufferChar[i] = (char)payload[i];
  }
  bufferChar[i] = '\0';

  // Method for handling callback payload
  HandleRequest();

  memset(bufferChar, '\0', sizeof(bufferChar)); // Reset*
  Serial.println("Callback call has ended!");
}

void HandleRequest(){
  // Code for handling callback request
}