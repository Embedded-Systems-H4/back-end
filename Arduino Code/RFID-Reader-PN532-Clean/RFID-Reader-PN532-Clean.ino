#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

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
const char* mqttServer = "10.71.202.218";
const int mqttPort = 1883;

const char* DoorOpenTopic = "devices/doors";
const char* RegisterTopic = "devices/register";
const char* CardLinkTopic = "devices/link";
const char* CardUpdateTopic = "devices/linkupdate";
const char* HeartbeatTopic = "devices/heartbeat";
const char* SensorStatusTopic = "devices/sensorstatus";
int TestPin = 10; // Connected to something, but not used (RFID)
int TestPinVal = 0;

byte Mac[6];
char MacStr[6];
String DeviceName;

// Wifi Client
WiFiClient espClient;
PubSubClient client(espClient);

void(* resetFunc) (void) = 0; 

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(TestPin, INPUT);

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
    if(client.publish(RegisterTopic, device.c_str())){
      Serial.println("Device has been registered!");
    } else {
      Serial.println("Failed to send registration message!");
    }

    if(client.subscribe(CardLinkTopic)){
      Serial.println("Subscribed to link topic!");
    }

    if(client.subscribe(DoorOpenTopic)){
      Serial.println("Subscribed to doors topic!");
    }
  }
}

void SetupExtraPerf(){
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1);  // Halt
  }
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


// Variables for NFC check/send
unsigned long myTime;
unsigned long timeoutTime;
bool timeoutIsActive;
uint8_t lastUid[] = { 0x00, 0x00, 0x00, 0x00 };

// Default value for Uid
uint8_t defaultUid[] = { 0x00, 0x00, 0x00, 0x00 };
uint8_t keyuniversal[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

uint8_t Block4Auth;
uint8_t Block4[16];

uint8_t Block5Auth;
uint8_t Block5[16];

const int LoopDelay = 100;

bool WriteToCard = false;

char UidValue[6];

char EditValue0[16];
char EditValue1[16];
char EditValue2[16];
char EditValue3[16];

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

  if (!Rflag) {
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

}


int LastSentSensorStatus = 0;
void LoopHeartbeat(){
  String heartbeat = "{\"id\":\"" + (String)MacStr + "\"}";
  client.publish(HeartbeatTopic, heartbeat.c_str());
  Serial.println(heartbeat);
  
  // TestPinVal = digitalRead(TestPin);
  TestPinVal = round(((double) rand() / (RAND_MAX)) + 1) - 1;

  if(TestPinVal != LastSentSensorStatus){
    LastSentSensorStatus = TestPinVal;
    String sensor = "{\"parent\":\"" + (String)MacStr + "\",\"id\":\"" + TestPin + "\",\"state\":\"" + TestPinVal + "\"}";
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
      EditValue1[15] = '\0';
    } else if (index == 2) {
      strcpy(EditValue2,pch);
      EditValue2[15] = '\0';
    } else if (index == 3) {
      strcpy(EditValue3,pch);
      EditValue3[15] = '\0';
    }
    index++;
  }

  if(strstr(EditValue0, MacStr)){ // Correct arduino
    Serial.println("Correct Id Callback was recieved.");    

    // Check if topic is Link topic
    if(strstr(Topic, CardLinkTopic)){
      // Check Card Write
      WriteRFID();
    }

    delay(5000);
  } else{
    Serial.println("Callback not sent with correct Door Id!");
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
      Block4Auth = nfc.mifareclassic_WriteDataBlock(4, (uint8_t*)EditValue2);
      Block5Auth = nfc.mifareclassic_WriteDataBlock(5, (uint8_t*)EditValue3);

      if (Block4Auth && Block5Auth){
        Serial.println("Card has been updated!");

        SetUidValue(uid);

        UpdateCardLink();

      }else{
        Serial.println("ERROR! - Card was not updated!");
      }
    }
  }

  Serial.println("Card update has terminated/ended.");
}

void UpdateCardLink(){

  String cardinfo = "{\"id\": \"" + String(EditValue1) + "\", \"card\": \"" + String(UidValue) + "\" }";

  client.publish(CardUpdateTopic, cardinfo.c_str());
  Serial.println(cardinfo);

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
        nfc.PrintHexChar(Block4, 16);
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
          nfc.PrintHexChar(Block5, 16);
        } else {
          SetLast(defaultUid);  // Reset
          return;
        }
      }

      if(Block4Auth && Block5Auth){
          // check connection and fix
          reconnect();

          SetUidValue(uid);

          // Generate Message
          String Block4String = String(reinterpret_cast<char*>(Block4));
          memset(EditValue2, '\0', sizeof(EditValue2));
          EditValue2[15] = '\0';
          strcpy(EditValue2, Block4String.c_str());

          String Block5String = String(reinterpret_cast<char*>(Block5));
          memset(EditValue3, '\0', sizeof(EditValue3));
          EditValue3[15] = '\0';
          strcpy(EditValue3, Block5String.c_str());

          int macLen = strlen(MacStr);
          int uiLen = strlen(UidValue);
          int block4Len = strlen(EditValue2);
          int block5Len = strlen(EditValue3);

          // Combining MacAddress and Message;
          int length = macLen + uiLen + block4Len + block5Len;
          char* finalmessage = new char[length]{};

          memset(finalmessage, 0, length); // Reset mem
          strcpy(finalmessage, MacStr);
          strcat(finalmessage, ",");
          strcat(finalmessage, UidValue);
          strcat(finalmessage, ",");
          strcat(finalmessage, EditValue2);
          strcat(finalmessage, ",");
          strcat(finalmessage, EditValue3);

          Serial.println(finalmessage);

          //Send request to broker
          if(client.publish(DoorOpenTopic, finalmessage)){
            Serial.println("Message has been sent!");
            Serial.println(" ");
          } else {
            Serial.println("Failed to send message!");
          }

          memset(finalmessage, 0, length); // Reset mem

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
