#include <Wire.h>
#include <LiquidCrystal_I2C.h> // Library for LCD
#define BACKLIGHT_PIN 13

// Wifi libraries
#include <WiFiS3.h>
#include <PubSubClient.h>

// Wifi Constants
const char* ssid = "SkyLab Academy";
const char* password = "SkyLab_Academy";
const char* mqttServer = "10.71.204.218";
const int mqttPort = 1883;

const char* ArduinoType = "None"; // RFID, Keypad, None

const char* AccessTopic = "devices/access";
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

void(* resetFunc) (void) = 0; 

// LiquidCrystal_I2C lcd(0x27);  // Set the LCD I2C address
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows


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

    String device = "{\"deviceId\":" + (String)MacStr + ",\"deviceName\":\"" + DeviceName + "\",\"deviceType\":\"door\", \"deviceAccessType\":\"" + (String)ArduinoType + "\"}";
    Serial.println(device);
    if(client.publish("devices/register", device.c_str())){
      Serial.println("Device has been registered!");
    } else {
      Serial.println("Failed to send registration message!");
    }

    // Needs to be subscribed to stuff here:

    // Subscribe to MQTT client
    if(client.subscribe(AccessTopic)){
      Serial.println("Subscribed to topic!");
    }
  }
}

void SetupExtraPerf(){
  // Switch on the backlight
  pinMode ( BACKLIGHT_PIN, OUTPUT );
  digitalWrite ( BACKLIGHT_PIN, HIGH );

  Serial.println("Init");
  lcd.begin(16,2);               // initialize the lcd
  lcd.clear();

  lcd.home ();                   // go home
  Serial.println("Hello, ARDUINO ");
  lcd.print("Hello, ARDUINO ");  
  // lcd.setCursor ( 0, 1 );        // go to the next line
  // Serial.println(" FORUM - fm   ");
  // lcd.print (" FORUM - fm   ");
  delay ( 1000 );
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
  
}

// Only run if connected
void LoopHeartbeat(){
  String heartbeat = "{\"deviceId\":" + (String)MacStr + "}";
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