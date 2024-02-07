// Wifi libraries
#include <WiFiS3.h>
#include <PubSubClient.h>

// Wifi Constants
const char* ssid = "SkyLab Academy";
const char* password = "SkyLab_Academy";
const char* mqttServer = "10.71.202.218";
const int mqttPort = 1883;

byte Mac[6];
char MacStr[6];
String DeviceName;

// Wifi Client
WiFiClient espClient;
PubSubClient client(espClient);

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

  // Set device name
  DeviceName = "device-" + (String)MacStr + "-door";

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Connect to MQTT broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  if(reconnect()){

    String device = "{\"id\":\"" + (String)MacStr + "\",\"name\":\"" + DeviceName + "\",\"type\":\"door\"}";
    Serial.println(device);
    if(client.publish("devices/register", device.c_str())){
      Serial.println("Device has been registered!");
    } else {
      Serial.println("Failed to send registration message!");
    }

    // Needs to be subscribed to stuff here:

    // Subscribe to MQTT client
    if(client.subscribe("devices/doors")){
      Serial.println("Subscribed to topic!");
    }
  }
}

void SetupExtraPerf(){

}

// Loop method running
void loop(void){
  // Regular loop code for MQtt
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
  }

  // Extras added to device
  // Loop delay is placed inside here or below call
  LoopExtras();
}

// MQTT reconnect code
bool reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(DeviceName.c_str(), "device", "DataIt2024")) {
      Serial.println("Connected to MQTT broker");
    } else {
      delay(100);
    }
  }
  return true;
}

void LoopExtras(void) {
  // Code for extra code used in the code loop
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