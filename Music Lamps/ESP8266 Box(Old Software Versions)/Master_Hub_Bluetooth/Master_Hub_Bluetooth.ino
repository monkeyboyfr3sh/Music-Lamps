#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <SoftwareSerial.h>
#include "reactive_common.h"

//Pins for input.
#define READ_PIN 0        //Mic input
#define BUTTON_PIN 5      //Button input for opmode control

#define NUMBER_OF_CLIENTS 2 //Define the amount of lamps
#define DEFAULT_OPMODE 1    //Set the default mode for 

//Misc. variables.
const int checkDelay = 5000;
const int buttonDoubleTapDelay = 200;
const int numOpModes = 5;

unsigned long lastChecked;
unsigned long buttonChecked;
unsigned long buttonRef;
unsigned long pressTime;
bool buttonClicked = false;
bool queueDouble = false;

bool clickTrigger;
bool doubleTapped;
//End of Variables

WiFiUDP UDP;   //Define the wifi class to call
const char *ssid = "myHost";
const char *password = "123456789";

SoftwareSerial BTserial(12, 13); // RX, TX //Communication lines with the HM-10
char c = ' '; //Input charater for HM-10

struct led_command {  //A struct to hold LED command info
  uint8_t opmode;
  uint32_t data;
};

bool heartbeats[NUMBER_OF_CLIENTS]; //Deine a heartbeat array. Allows for testing of connection

static int opMode = DEFAULT_OPMODE;  //Gives opmode a starting point. Control the start based off default int

void setup() {
  Serial.begin(115200);
  BTserial.begin(9600);

  Serial.print("Sketch:   ");   Serial.println(__FILE__);
  Serial.print("Uploaded: ");   Serial.println(__DATE__);
  Serial.println(" ");

  pinMode(READ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT );


  // Wifi setup
  Serial.print("Setting soft-AP ... ");
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);      //Seting to access point
  WiFi.softAP(ssid, password);
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  UDP.begin(7171);
  resetHeartBeats();
  waitForConnections();
  //end of wifi setup

  Serial.println("\nAll lamps connected!");
  Serial.print("OP mode: ");
  Serial.println(opMode);

  lastChecked = millis();
  buttonRef = millis();
  buttonChecked = 0;
}

void loop() {
  uint32_t analogRaw;

  buttonCheck();

  if (millis() - lastChecked > checkDelay) {
    if (!checkHeartBeats()) {
      waitForConnections();
    }
    lastChecked = millis();
  }

  BTRecieve();

  /*
    Serial.println(opMode);
    Serial.print("OP mode: ");
    Serial.println(opMode);
  */

  switch (opMode) {
    case 0://Soft Sleep
      sendLedData(0, opMode);
      delay(10);
      break;

    case 1://Sound React
      analogRaw = analogRead(READ_PIN);
      if (analogRaw <= 3)
        break;
      Serial.println(analogRaw);
      sendLedData(analogRaw, opMode);
      break;

    case 2://All White
      sendLedData(0, opMode);
      delay(10);
      break;

    case 3://Fade
      sendLedData(0, opMode);
      delay(10);
      break;

    case 4://Larson Scanner
      sendLedData(0, opMode);
      delay(10);
      break;

    case 5://Rainbow
      sendLedData(0, opMode);
      delay(10);
      break;

  }

  delay(4);
}//end of main

void BTRecieve() {
  // Read from the Bluetooth module and send to the Arduino Serial Monitor
  if (BTserial.available()) {
    c = BTserial.read();
    Serial.print("\nIncoming character: ");
    Serial.println(c);
    boolean set = false;

    switch (c) {
      case '0':
        opMode = 0;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;

      case '1':
        opMode = 1;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;

      case '2':
        opMode = 2;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;

      case '3':
        opMode = 3;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;

      case '4':
        opMode = 4;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;

      case '5':
        opMode = 5;
        Serial.printf("Setting opmode %d \n", opMode);
        set = true;
        delay(10);
        break;
    }

    if (set == false) clicked();
  }
  delay(10);
}

void sendLedData(uint32_t data, uint8_t op_mode) {  //Function that sends  the data to the other controllers
  //with the previous defined struct.
  struct led_command send_data;  //Create an instance of led_command
  send_data.opmode = op_mode;    //Set the opmode
  send_data.data = data;         //Set the data
  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {  //This is a for loop to send the data to the clients. All
    IPAddress ip(192, 168, 4, 2 + i);
    UDP.beginPacket(ip, 7001);
    UDP.write((char*)&send_data, sizeof(struct led_command));
    UDP.endPacket();
  }
}//end of send function

void waitForConnections() {       //A simple function to test/wait for connection
  while (true) {
    readHeartBeat();
    if (checkHeartBeats()) {
      return;
    }
    delay(checkDelay);
    resetHeartBeats();
  }
}//End of wait

void resetHeartBeats() {    //Function to reset heartbeat array. Similar to clearing LEDs before adding values again
  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {
    heartbeats[i] = false;
  }
}//end of reset

void readHeartBeat() {    //Function to test heartbeat/login verification
  struct heartbeat_message hbm;
  while (true) {
    int packetSize = UDP.parsePacket();
    if (!packetSize) {
      break;  //checks for null packets
    }

    UDP.read((char *)&hbm, sizeof(struct heartbeat_message));
    if (hbm.client_id > NUMBER_OF_CLIENTS) {
      Serial.println("Error: invalid client_id received");
      continue;
    }
    heartbeats[hbm.client_id - 1] = true; //Says client is active. Stored in the previous heartbeats array
  }
}//end of readHeartBeat

bool checkHeartBeats() {  //Function to test active clients
  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {
    if (!heartbeats[i]) {  //Loops through and if the heartbeat shows false in the array, outputs false
      return false;
    }
  }
  resetHeartBeats();  //Resets the heartbeats array to all false
  return true;        //Verifies reset complete
}


void buttonCheck() {
  int but = digitalRead(BUTTON_PIN);

  buttonRef = millis();
  while (but == 1) {
    but = digitalRead(BUTTON_PIN);
    if ((millis() - buttonRef) > 1000) {
      clicked();
      break;
    }
    //Serial.println(millis()-buttonRef);
  }
}//end of button check

void clicked() {
  if (opMode == numOpModes)
    opMode = 1;
  else
    opMode++;
  Serial.printf("Setting opmode %d \n", opMode);
}
