#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "reactive_common.h"

//Pins for input.
#define READ_PIN 0        //Mic input
#define BUTTON_PIN 5      //Button input for opmode control

#define NUMBER_OF_CLIENTS 1 //Define the amount of lamps
#define DEFAULT_OPMODE 1    //Set the default mode for 

const char* ssid     = "myHost";
const char* password = "123456789";

//Misc. variables.
const int checkDelay = 9000;
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
WiFiServer server(80);

struct led_command {  //A struct to hold LED command info
  uint8_t opmode;
  uint32_t data;
};

bool heartbeats[NUMBER_OF_CLIENTS]; //Deine a heartbeat array. Allows for testing of connection

static int opMode = DEFAULT_OPMODE;  //Gives opmode a starting point. Control the start based off default int

// Variable to store the HTTP request
String header;

boolean Booty;

int crtime;
int ortime;

void setup() {
  pinMode(READ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT );

  // Wifi setup
  Serial.begin(115200);
  Serial.println();
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

  lastChecked = millis();
  buttonRef = millis();
  buttonChecked = 0;
  
  server.begin();

  crtime = millis();
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
  ortime = crtime;
  WebControl();
    crtime= millis();
  Serial.print("Your runtime is: ");
  Serial.println(crtime-ortime);
  /*
  switch (opMode) {
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
  */
}//end of main

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

  void doubleClicked() {}
