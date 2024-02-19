#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// Servo library
#include <Servo.h>
Servo projectServo;
int MOTOR_PIN = 12;
int ServoDegreesOpen = 180;
int ServoDegreesClosed = 0;

// Wifi libraries
#include <WiFiS3.h>
#include <PubSubClient.h>

// Adafruit RFID Reader values
#define PN532_IRQ (2)
#define PN532_RESET (3)
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Wifi Constants
const char* ssid = "SkyLab Academy";
const char* password = "SkyLab_Academy";
// const char* ssid = "bruh";
// const char* password = "Datait2024!";
const char* mqttServer = "10.71.204.218";
const int mqttPort = 1883;

const char* ArduinoType = "RFID"; // RFID, Keypad, None
// Topic constants for publishing and subscribing
const char* AccessTopic = "devices/access";
const char* RegisterTopic = "devices/register";
const char* CardLinkTopic = "devices/link";
const char* CardUpdateTopic = "devices/linkupdate";
const char* DeviceLockTopic = "devices/lock";
const char* HeartbeatTopic = "devices/heartbeat";
const char* SensorStatusTopic = "devices/sensorstatus";
const char* AccessUpdateTopic = "devices/accessupdate";

// Varibles for Mac address and unique id based on mac address
byte Mac[6];
char MacStr[6];
String DeviceName;

// Wifi Client
WiFiClient espClient;
PubSubClient client(espClient);
int WiFiConnectionRetries = 10;
int WiFiRetries = 0;

// Reset function if arduino encounters something that isn't right
void(* resetFunc) (void) = 0; 


// Pin variables for accessing and controlling LED's or Sensors
int LED_PIN = 8;
int LED_PIN2 = 9;
int TestPin = 10; // Switch
int TestPinVal = 0; // Switch value

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);

  // Start Serial connection
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
    if(client.publish(RegisterTopic, device.c_str())){
      Serial.println("Device has been registered!");
    } else {
      Serial.println("Failed to send registration message!");
    }

    // Subscribe to topics
    if(client.subscribe(CardLinkTopic)){
      Serial.println("Subscribed to link topic!");
    }

    if(client.subscribe(DeviceLockTopic)){
      Serial.println("Subscribed to lock topic!");
    }

    if(client.subscribe(AccessUpdateTopic)){
      Serial.println("Subscribed to access topic!");
    }
  }
}

// Section for extra setup code
void SetupExtraPerf(){
  // Start NFC board
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1);  // Halt
  }

  // Servo init
  projectServo.attach(MOTOR_PIN);

  // Set Sensor input
  pinMode(TestPin, INPUT);

  // Set LED pin-outs
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
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

bool DevicePermaLocked = false;

// Variables for NFC check/send
unsigned long myTime;
unsigned long timeoutTime;
bool timeoutIsActive;
uint8_t lastUid[] = { 0x00, 0x00, 0x00, 0x00 };

// Default value for Uid
uint8_t defaultUid[] = { 0x00, 0x00, 0x00, 0x00 };
uint8_t keyuniversal[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

uint8_t Block4Auth;
uint8_t Block4[30];

uint8_t Block5Auth;
uint8_t Block5[30];

const int LoopDelay = 100;

bool WriteToCard = false;

char UidValue[6];

char EditValue0[16];
char EditValue1[30];
char EditValue2[30];
char EditValue3[30];

char* Topic;
byte* buffer;
char* bufferChar;
boolean Rflag = false;

unsigned int LoopHeartbeatDelay = 3000;
unsigned long LoopTime;
unsigned long LoopTimeoutTime;


void LoopExtras(void) {
  
  LoopTime = millis();
  if (LoopTimeoutTime + LoopHeartbeatDelay < LoopTime) {
    LoopTimeoutTime = millis();

    // Heartbeat executed code
    if (client.connected()) {
      LoopHeartbeat();
    }
  }

  // Faster Heartbeat for more precise Sensor reads
  if(client.connected()){
    FasterHeartbeat();
  }

  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  bool LoopInterupted = false;

  myTime = millis();

  if (timeoutIsActive) {
    digitalWrite(LED_BUILTIN, HIGH);
    if (timeoutTime + 5000 < myTime) {
      SetLast(defaultUid);  // Reset
      timeoutIsActive = false;
    }
  }

  if (!Rflag && !DevicePermaLocked) {
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, LoopDelay);
    if (success) {
      LoopInterupted = true;
      CheckRFID(uid, uidLength);
    }
    if (LoopInterupted) {
      delay(LoopDelay);
    }
  } else {
    delay(LoopDelay);
  }

  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN2, HIGH);
  projectServo.write(ServoDegreesClosed);
}


void LoopHeartbeat(){
  String heartbeat = "{\"deviceId\":" + (String)MacStr + "}";
  client.publish(HeartbeatTopic, heartbeat.c_str());
  Serial.println(heartbeat);
}

int LastSentSensorStatus = 0;
void FasterHeartbeat(){
  TestPinVal = digitalRead(TestPin);
  // TestPinVal = round(((double) rand() / (RAND_MAX)) + 1) - 1;

  if(TestPinVal != LastSentSensorStatus){
    LastSentSensorStatus = TestPinVal;
    String sensor = "{\"parentId\":" + (String)MacStr + ",\"sensorId\":" + TestPin + ",\"state\":\"" + TestPinVal + "\"}";
    client.publish(SensorStatusTopic, sensor.c_str());
    Serial.println(sensor);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  //Payload=[];
  Topic = topic;
  Rflag = true; //will use in main loop
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

  HandleRequest();

  memset(bufferChar, '\0', sizeof(bufferChar)); // Reset*
  Rflag = false;
  Serial.println("Callback call has ended!");
}

void HandleRequest(){
  memset(EditValue0, '\0', sizeof(EditValue0));
  memset(EditValue1, '\0', sizeof(EditValue1));
  memset(EditValue2, '\0', sizeof(EditValue2));
  memset(EditValue3, '\0', sizeof(EditValue3));

  char * pch;
  pch = strtok (bufferChar,",");
  strcpy(EditValue0,pch);

  int index = 1; // First buffer has been made
  while (pch != NULL)
  {
    pch = strtok (NULL, ",");
    if (index == 1) {
      strcpy(EditValue1,pch);
      EditValue1[29] = '\0';
    } else if (index == 2) {
      strcpy(EditValue2,pch);
      EditValue2[29] = '\0';
    } else if (index == 3) {
      strcpy(EditValue3,pch);
      EditValue3[29] = '\0';
    }
    index++;
  }

  if(strstr(EditValue0, MacStr)){ // Correct arduino
    Serial.print("Correct Id Callback was recieved on topic:");
    Serial.println(Topic);
    
    String SentData = String(EditValue0) + "," + String(EditValue1) + "," + String(EditValue2) + "," + String(EditValue3);
    Serial.println(SentData);

    // Check if topic is Link topic
    if(strstr(Topic, CardLinkTopic)){
      // Check Card Write
      WriteRFID();
      delay(1000);
    }

    if(strstr(Topic, AccessUpdateTopic)){
      // Device access
      HandleAccess();
      delay(1000);
    }
  } else{
    if(strstr(EditValue0, "all") && strstr(Topic, DeviceLockTopic)){
      // Check Device Lock
      HandlePermaLock();
      delay(1000);
    }

    Serial.println("Callback not sent with correct Door Id!");
  }
}

void HandlePermaLock(){
  if (strstr(EditValue1, "false")){
    DevicePermaLocked = false;
  } else {
    DevicePermaLocked = true;
  }
}

void HandleAccess(){
  if(strstr(EditValue2, "true")){
    // If Access is granted turn on LED on LED_PIN2 (Low for direct LED setup)
    digitalWrite(LED_PIN2, LOW);
    projectServo.write(ServoDegreesOpen);
    Serial.println("Access was granted to door!");
    delay(3000);
  } else {
    // If Access was not granted turn on LED on LED_PIN (High for Keyestudio LED setup)
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Access was NOT granted to door!");
  }
}

void WriteRFID(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;

  if(strlen(EditValue2) < 2 || strlen(EditValue3) < 2){
    Serial.println("Card write data input was incorrect!");
    return;
  }

  Serial.println("Ready to read and update card!");

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 10000);

  if (success) {

    Block4Auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 1, keyuniversal);
    Block5Auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 5, 1, keyuniversal);

    if (Block4Auth && Block5Auth) {
      Block4Auth = nfc.mifareclassic_WriteDataBlock(4, (uint8_t*)EditValue1);
      Block5Auth = nfc.mifareclassic_WriteDataBlock(5, (uint8_t*)EditValue2);

      if (Block4Auth && Block5Auth){
        Serial.println("Card has been updated!");

        SetUidValue(uid);

        UpdateCardLink();

      }else{
        Serial.println("ERROR! - Card was not updated!");
        // Send if Error!
        UpdateCardLink();
      }
    }
  } else {
    // Send if Timeout!
    UpdateCardLink();
  }

  Serial.println("Card update has terminated/ended.");
}

void UpdateCardLink(){

  if (client.connected()){
    String cardinfo = "{\"deviceId\":" + String(MacStr) + ", \"profileId\":" + String(EditValue1) + ", \"cardId\": " + String(UidValue) + " }";

    client.publish(CardUpdateTopic, cardinfo.c_str());
    Serial.println(cardinfo);
  }

}

void SetUidValue(uint8_t uid[]){
  
  // Uid to string
  int uidNum = 0;
  for (int i = 0; i < 6; i++) {
    uidNum += (int)uid[i];
  }
  memset(UidValue, '\0', sizeof(UidValue));
  itoa(uidNum, UidValue, 10);

}


void CheckRFID(uint8_t uid[], uint8_t uidLength){

  if (sizeof lastUid == uidLength && memcmp(lastUid, uid, sizeof lastUid) != 0) {
    SetLast(uid);
    timeoutIsActive = true;

    if (uidLength == 4) {

      Serial.println(" "); // Spacing for console

      // Block 4
      Block4Auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 1, keyuniversal);
      if (Block4Auth) {
        Block4Auth = nfc.mifareclassic_ReadDataBlock(4, Block4);
        nfc.PrintHexChar(Block4, 30);
      } else {
        SetLast(defaultUid);  // Reset
        return;
      }

      // Only run if Block 4 authenticated
      if(Block4Auth){

        // Block 5
        Block5Auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 5, 1, keyuniversal);
        if (Block5Auth) {
          Block5Auth = nfc.mifareclassic_ReadDataBlock(5, Block5);
          nfc.PrintHexChar(Block5, 30);
        } else {
          SetLast(defaultUid);  // Reset
          return;
        }
      }

      if(Block4Auth && Block5Auth){
          // check connection and fix
          if (reconnect()){
            
            SetUidValue(uid);

            // Generate Message
            String Block4String = String(reinterpret_cast<char*>(Block4));
            String Block5String = String(reinterpret_cast<char*>(Block5));

            String cardInfo = "{\"deviceId\":" + String(MacStr) + ",\"cardId\":" + String(UidValue) + ",\"value1\":\"" + Block4String + "\",\"value2\":\"" + Block5String + "\"}";
            Serial.println(cardInfo);

            //Send request to broker
            if(client.publish(AccessTopic, cardInfo.c_str())){
              Serial.println("Message has been sent!");
              Serial.println(" ");
            } else {
              Serial.println("Failed to send message!");
            }          
          }

          delay(LoopDelay);
      } else {
        Serial.println("Error reading block 4 & 5");
      }
    }
  }
}

void SetLast(uint8_t id[]) {
  timeoutTime = millis();
  digitalWrite(LED_BUILTIN, LOW);
  if (sizeof id == sizeof lastUid) {
    memcpy(lastUid, id, sizeof id);
  } else {
    memcpy(lastUid, (const uint8_t[]){ 0x00, 0x00, 0x00, 0x00 }, sizeof lastUid);
  }
}
