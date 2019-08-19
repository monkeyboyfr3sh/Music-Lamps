//#include <ESP8266WiFi.h>    //<----esp8266
//#include <WiFiUDP.h>        //<----esp8266
//#include <SoftwareSerial.h> //<----esp8266

#include <WiFi.h>             //<----esp32
#include <WiFiUdp.h>          //<----esp32
#include "BluetoothSerial.h"  //<----esp32

#include <FastLED.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include "reactive_common.h"

//Pins for input.
#define WIFI_LED 32
#define MQTT_LED 33

#define READ_PIN 36        //Mic input
#define BUTTON_PIN 17      //Button input for opmode control
#define NUMBER_OF_CLIENTS 1 //Define the amount of lamps
#define DEFAULT_OPMODE 1    //Set the default mode for 

//Misc. variables.
const int checkDelay = 5000;
const int DefMQTTcheckDelay = 5000;
const int BTcheckDelay = 500;
const int buttonDoubleTapDelay = 200;
const int numOpModes = 6;
const int LowerVThreshold = 1300;
uint8_t btPointer = 0;

uint8_t redcolor;
uint8_t greencolor;
uint8_t bluecolor;
uint8_t globalBrightnessLocal;

char inputChar;
char btCommand[128];

int MQTTcheckDelay = 100;
unsigned long lastChecked;
unsigned long MQTTlastChecked;
unsigned long lastBTChecked;
unsigned long buttonChecked;
unsigned long buttonRef;
unsigned long pressTime;
bool buttonClicked = false;
bool queueDouble = false;

bool clickTrigger;
bool doubleTapped;
//End of Variables

WiFiUDP UDP;   //Define the wifi class to call
WiFiServer server(80);

const char *APssid = "myHost";
const char *APpassword = "123456789";

const char* STAssid = "UBIFLaffy";
const char* STApassword = "UBIFLaf2018";

//SoftwareSerial BTserial(12,14); // RX, TX  //Communication lines with the HM-10
BluetoothSerial BTserial;
char c = ' '; //Input charater for bluetooth

struct led_command {  //A struct to hold LED command info
  uint8_t opmode;
  uint32_t data;
  uint8_t redcolor;
  uint8_t greencolor;
  uint8_t bluecolor;
  uint8_t globalBrightnessLocal;
};

bool activeLamp[NUMBER_OF_CLIENTS + 1];

void sendLedData(uint32_t data, uint8_t op_mode, uint8_t redcolor = 0, uint8_t greencolor = 0, uint8_t bluecolor = 0);

bool heartbeats[NUMBER_OF_CLIENTS]; //Deine a heartbeat array. Allows for testing of connection

static int opMode = DEFAULT_OPMODE; //Gives opmode a starting point. Control the start based off default int

// Variable to store the HTTP request
String header;
boolean Booty;

unsigned long crtime;
unsigned long ortime;

//MQTT Setup
WiFiClient MQTTclient;

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "DeDz"
#define AIO_KEY         "047a480aa6a04a2d8bccc9c10cb1adb5"

Adafruit_MQTT_Client mqtt(&MQTTclient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish opModeFeedback = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/opmode-feedback");
Adafruit_MQTT_Publish activeLampPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/activelamp");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe MQTTopMode = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/opmode");

Adafruit_MQTT_Subscribe redSlider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/redslider");
Adafruit_MQTT_Subscribe greenSlider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/greenslider");
Adafruit_MQTT_Subscribe blueSlider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/blueslider");

Adafruit_MQTT_Subscribe activeLampSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/activelamp");
Adafruit_MQTT_Subscribe brightness = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/globalbrightness");

void MQTT_connect();

void setup() {
  Serial.begin(115200);
  //BTserial.begin(9600);

  Serial.print("Sketch:   ");   Serial.println(__FILE__);
  Serial.print("Uploaded: ");   Serial.println(__DATE__);
  Serial.println(" ");

  pinMode(READ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT );
  pinMode(WIFI_LED,OUTPUT);
  pinMode(MQTT_LED,OUTPUT);

  //STA Setup
  Serial.printf("Connecting to %s\n", STAssid);
  WiFi.begin(STAssid, STApassword);
  //WiFi.config(staticIP, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS Server: ");
  Serial.println(WiFi.dnsIP());
  digitalWrite(WIFI_LED,HIGH);

  // Setup MQTT subscription for feed.
  mqtt.subscribe(&MQTTopMode);
  mqtt.subscribe(&redSlider);
  mqtt.subscribe(&greenSlider);
  mqtt.subscribe(&blueSlider);
  mqtt.subscribe(&activeLampSub);
  mqtt.subscribe(&brightness);
  MQTT_connect();
  digitalWrite(MQTT_LED,HIGH);
  

  Serial.println("\nSTA setup complete, begining soft-AP setup");

  // AP setup
  Serial.print("Setting soft-AP ... ");
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);      //Seting to access point
  WiFi.softAP(APssid, APpassword);
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());


  UDP.begin(7171);
  resetHeartBeats();
  waitForConnections();
  //end of wifi setup

  Serial.println("\nAll lamps connected!");

  for (int i = 1; i < NUMBER_OF_CLIENTS + 1; i++) {
    activeLamp[i] = true;
  }

  BTserial.begin("Sound_React");
  Serial.println("The device started, now you can pair it with bluetooth!\n");


  lastChecked = millis();
  MQTTlastChecked = millis();
  lastBTChecked = millis();
  buttonRef = millis();
  buttonChecked = 0;

  server.begin();

  crtime = millis();

  Serial.println("Exiting Setup\n");
}

void loop() {
  if (millis() - MQTTlastChecked > MQTTcheckDelay) {
    MQTTUpdate();
    Serial.print("Your runtime is: ");
    Serial.println(crtime - ortime - MQTTcheckDelay);
    MQTTlastChecked = millis();
  }
  buttonCheck();
  if (millis() - lastChecked > checkDelay) {
    if (!checkHeartBeats()) {
      waitForConnections();
    }
    lastChecked = millis();
  }

  //ortime = crtime;
  //WebControl();
  //crtime = millis();
  //Serial.print("Your runtime is: ");
  //Serial.println(crtime - ortime);
  processBT();
  modeSelect();

  delay(10);
}//end of main

void MQTTUpdate() {
  MQTT_connect();

  ortime = crtime;
  Adafruit_MQTT_Subscribe *subscription;
  int opModeIn;
  int lampIn;
  int globalBrightnessIn;
  bool set = false;

  while ((subscription = mqtt.readSubscription(5))) {
    if (subscription == &MQTTopMode) {
      opModeIn = atoi((char *)MQTTopMode.lastread);
      changeOPMode(opModeIn);
    }

    if (subscription == &activeLampSub) {
      lampIn = atoi((char *)activeLampSub.lastread);

      if ((lampIn >= 0) || (lampIn <= NUMBER_OF_CLIENTS + 1)) {
        if (lampIn == NUMBER_OF_CLIENTS + 1) {
          set = true;
          for (int i = 1; i < NUMBER_OF_CLIENTS + 1; i++) {
            activeLamp[i] = true;
          }
        }
        if (set == false) activeLamp[lampIn] = !activeLamp[lampIn];

      } if (! activeLampPub.publish(0)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!\n"));
      }
    }

    if (subscription == &brightness) {
      globalBrightnessIn = atoi((char *)brightness.lastread);
      if ((globalBrightnessIn >= 0) || (globalBrightnessIn <= 100)) {
        globalBrightnessLocal = globalBrightnessIn;
      }
    }

    if (opMode == 6) {
      if (subscription == &redSlider) {
        redcolor = atoi((char *)redSlider.lastread);
      }
      if (subscription == &greenSlider) {
        greencolor = atoi((char *)greenSlider.lastread);
      }
      if (subscription == &blueSlider) {
        bluecolor = atoi((char *)blueSlider.lastread);
      }
    }

  }

  crtime = millis();
}

void processBT() {
  if (BTserial.available()) {
    inputChar = BTserial.read();
    lastBTChecked = millis();

    if (inputChar == '{') {
      while (!recieveBT());
    }

    switch (btCommand[0]) {
      case '0':
        changeOPMode(0);
        delay(10);
        break;

      case '1':
        changeOPMode(1);
        delay(10);
        break;

      case '2':
        changeOPMode(2);
        delay(10);
        break;

      case '3':
        changeOPMode(3);
        delay(10);
        break;

      case '4':
        changeOPMode(4);
        delay(10);
        break;

      case '5':
        changeOPMode(5);
        delay(10);
        break;

      case '6':
        changeOPMode(6);
        delay(10);
        break;
    }

    for (int i = 0; i < btPointer; i++) btCommand[i] = '\0';
    btPointer = 0;
  }
}

bool recieveBT() {
  inputChar = BTserial.read();

  if (inputChar == '}') return true;
  if (millis() - lastBTChecked > BTcheckDelay) {
    for (int i = 0; i < btPointer; i++) btCommand[i] = '\0';

    Serial.println("Bluetooth timeout error");

    return true;
  }

  btCommand[btPointer] = inputChar;
  btPointer++;
  return false;
}

void changeOPMode(uint8_t opModeIn) {
  if ((opMode == opModeIn) || (opModeIn < 0) || (opModeIn > numOpModes)) return;
  opMode = opModeIn;
  Serial.print("\nSetting to opMode: ");
  Serial.println(opMode);

  Serial.print(F("Sending opMode: "));
  Serial.print(opMode);
  Serial.print("...");
  if (! opModeFeedback.publish(opMode)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!\n"));
  }
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("\nConnecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(1000);  // wait 1 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}

void WebControl() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            // turns the GPIOs on and off

            if (header.indexOf("GET /5/MyBalls") >= 0) {
              Booty = false;
              clicked();
            }

            else if (header.indexOf("GET /5/MyBooty") >= 0) {
              Booty = true;
              clicked();
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Monospace; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px; text-align: center;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; opacity: 0.6; transition: 0.3s}");

            client.println(".button:hover {opacity: 1}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>Sound_React</h1>");

            // Display current state, and ON/OFF buttons for GPIO 5
            //client.println("<p>GPIO 5 - State " + output5State + "</p>");
            // If the output5State is off, it displays the ON button

            if (Booty == false) client.println("<p><a href=\"/5/MyBooty\"><button class=\"button\">ChangeState</button></a></p>");
            else client.println("<p><a href=\"/5/MyBalls\"><button class=\"button\">ChangeAgain</button></a></p>");

            client.println("<body><p>" + String(opMode) + "</p>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void modeSelect() {
  uint32_t analogRaw;
  switch (opMode) {
    case 0://Soft Sleep
      MQTTcheckDelay = DefMQTTcheckDelay;
      sendLedData(0, opMode, 0);
      delay(10);
      break;

    case 1://Sound React
      MQTTcheckDelay = 5000;
      analogRaw = analogRead(READ_PIN);
      if (analogRaw <= LowerVThreshold)
        break;
      //Serial.println(analogRaw);
      sendLedData(analogRaw, opMode);
      break;

    case 2://All White
      MQTTcheckDelay = DefMQTTcheckDelay;
      sendLedData(0, opMode);
      delay(10);
      break;

    case 3://Fade
      MQTTcheckDelay = DefMQTTcheckDelay;
      sendLedData(0, opMode);
      delay(10);
      break;

    case 4://Larson Scanner
      MQTTcheckDelay = DefMQTTcheckDelay;
      sendLedData(0, opMode);
      delay(10);
      break;

    case 5://Rainbow
      MQTTcheckDelay = DefMQTTcheckDelay;
      sendLedData(0, opMode);
      delay(10);
      break;

    case 6://Color Select
      MQTTcheckDelay = 20;
      sendLedData(0, opMode, redcolor, greencolor, bluecolor);
      delay(10);
      break;
  }
}

void sendLedData(uint32_t data, uint8_t op_mode, uint8_t redcolor, uint8_t greencolor, uint8_t bluecolor) { //Function that sends  the data to the other controllers
  struct led_command send_data;   //Create an instance of led_command
  send_data.data = data;          //Set the data

  if (opMode == 6) {
    send_data.redcolor = redcolor;
    send_data.greencolor = greencolor;
    send_data.bluecolor = bluecolor;
  }

  //activeLamp[2] = false;

  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {  //This is a for loop to send the data to the clients. All
    IPAddress ip(192, 168, 4, 101 + i);

    if (activeLamp[i + 1] == false) {
      send_data.opmode = 0;
    }
    else send_data.opmode = opMode;

    /*
      Serial.print("Lamp ");
      Serial.print(i + 1);
      Serial.print(" is active : ");
      Serial.println(activeLamp[i + 1]);
      Serial.print("Sending to : ");
      Serial.println(ip);
      Serial.print("Server opMode : ");
      Serial.println(opMode);
      Serial.print("Sending opMode : ");
      Serial.println(send_data.opmode);
      Serial.println(" ");
    */

    UDP.beginPacket(ip, 7001);
    UDP.write((byte*)&send_data, sizeof(struct led_command));
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
  while (but == 0) {
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
    changeOPMode(1);
  else
    changeOPMode(opMode + 1); //minus 1 so it
}
