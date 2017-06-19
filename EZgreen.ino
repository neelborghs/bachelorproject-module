/* 
   ///////////////////////////////////////
   SmartPlant

   Tools:
      board:    NodeMCU 1.0 (ESP-12 Module)
      sensors:  Moisture Sensor
                Humidity and Temperature Sensor (DHT)
                LDR     
   Programmers:
      Borghs Neel
      Milio Tomas
  ///////////////////////////////
*/

#include <ESP8266WiFi.h>    //ESP8266 Chip Library
#include "DHT.h"            //DHT Chip Library (For Humidity and Temperature)
#include <PubSubClient.h>   //MQTT Library for connection to Raspberry Pi
#include <Ticker.h>         //Timer for Watchdog

#define DHTTYPE DHT11   // DHT 11 //Defines the DHT11 from the DHT Chip Library
  
//Credentials for internet access are hidden
char* ssid = "RaspberryPi";
char* password = "raspberry";

//Watchdog variable
Ticker secondTick;
volatile int watchdogCount = 0;

//Valve
const int valvePin = 4; //  ~D2

//DHT Sensor
const int dhtPin = 0;   //  ~D1
const int dhtPower = 5; //  ~D3
float t = 0.00;
float h = 0.00;
DHT dht(dhtPin, DHTTYPE);

//Moisture sensor uses the only analog port on the NodeMCU
const int moisturePower = 15; //  ~D8
int chartValue = 0;
double analogValue = 0.0;
double analogVolts = 0.0;

//LDR sensor
const int ldrPin = 13;   //  ~D7
const int ldrPower = 12; //  ~D6
int ldrValue = 0;

//MQTT
const char* mqtt_server = "172.24.1.1";
WiFiClient espClient;
PubSubClient client(espClient);
String msg;
char msgChar[111];


// Setup code will only run once on boot
void setup() { 
  // Initializing serial port for debugging purposes
  Serial.begin(115200);


  //Setup watchdog
  secondTick.attach(1, ISRwatchdog);

    //Starting wifi connection to RaspberryPi AP
  startWifiConnection();

  
  //Put LDR pin on input
  pinMode(ldrPin, INPUT);
  //Put LDRPower on output
  pinMode(ldrPower, OUTPUT);
  //Put valvePin on output
  pinMode(valvePin, OUTPUT);
  //Put moisturePower on output
  pinMode(moisturePower, OUTPUT);
  //Put dhtPower on output
  pinMode(dhtPower, OUTPUT);

  
  //Set valvePin low 
  digitalWrite(valvePin, LOW);
  //Set ldrPower high 
  digitalWrite(ldrPower, HIGH);
  //Set moisturePower high
  digitalWrite(moisturePower, HIGH);
  //Set dhtPower high 
  digitalWrite(dhtPower, HIGH);
  
  // Initialize DHT sensor (Humidity and Temperature).
  dht.begin();
}

void loop() {
  // put your main code here, to run repeatedly:

  //Feed the watchdog
  watchdogCount = 0;
  
  //Read Humidity and Temperature sensor (DHT)
  readDHT();
  
  //Read Moisture sensor
  readMoisture();
  
  //Read Light sensor
  readLDR();
  
  //Send data to mqtt
  sendMQTT();
  
  //Go to deepsleep for low power con
  //Go in deep sleep for 30 minutes, then wakeup like a reset
  ESP.deepSleep(1800000000, WAKE_RF_DEFAULT);
  delay(100); //without this delay it won't work (bug in the esp8266 chip)
}

void startWifiConnection(){
  //Start connection to predefined SSID
  WiFi.begin(ssid, password);

  //Show a "." while we're trying to connect to the RaspberryPi AP
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void readDHT() {

  
  h = dht.readHumidity();     // Read humidity
  t = dht.readTemperature();  // Read temperature as Celsius (the default)

  //Stop powering sensor
  digitalWrite(dhtPower, LOW);
}

void readMoisture(){
  analogValue = analogRead(A0);   //Read the analog signal
  

  chartValue = (analogValue * 100)/956;   //Convert analog value (0-1024) to value between 0 and 100

  chartValue = 100 - chartValue; //Reverse the value so that the value goes up as moisture increases

  //Stop powering sensor
  digitalWrite(moisturePower, LOW);
  //Put on valve if soil is to dry
  if (chartValue <= 55) {
    digitalWrite(valvePin, HIGH);
  }
  //Close valve after 2 seconds
  delay(5000);
  digitalWrite(valvePin, LOW);
  
}

void readLDR(){
  //Read out LDR value (low light or high light)
  if ( digitalRead(ldrPin) == HIGH ) {
    ldrValue = 0; //Light is low
  }
  else {
    ldrValue = 1; //Light is high (wich is good)
  }
  //Stop powering sensor
  digitalWrite(ldrPower, LOW);
}

void sendMQTT(){
  //Connect to MQTT server with port 1883
  client.setServer(mqtt_server, 1883);
  
  //Check if MQTT client is connected
  if (!client.connected()) {
    reconnect(); //reconnect if not
  }
  client.loop();
  
  //Make a message to send to the MQTT broker
  msg += "\"module_id\": \"";
  msg += ESP.getChipId();
  msg += "\", \"moisture\": \"";
  msg += chartValue;
  msg += "\", \"humidity\": \"";
  msg += h;
  msg += "\", \"temperature\": \"";
  msg += t;
  msg += "\", \"light\": \"";
  msg += ldrValue;
  msg += "\"}";
  //Converse string to char for sending to mqtt
  msg.toCharArray(msgChar, 111);
  //Send data to MQTT with "..." as keyword
  client.publish("smartplant", msgChar);  
  msg = "";
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (!client.connect("ESP8266Client")) {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//Interrupt Service Routine
void ISRwatchdog() {
  watchdogCount++;
  //Specified time when program needs to reset
  if (watchdogCount == 10) {

    ESP.reset();
  }
}

