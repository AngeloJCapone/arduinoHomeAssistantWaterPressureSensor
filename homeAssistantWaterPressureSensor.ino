#include <ArduinoMqttClient.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "secret_values.h" 
#include "WiFiS3.h"
// GLOBAL AND CONSTANT VARIABLES ======================================================

const int ANALOG_INPUT_SENSITIVITY = 1024;

const char MQTT_CLIENT_ID[] = "WaterPressureSensor";
const char MQTT_HOST[] = SECRET_MQTT_HOST;
const int MQTT_PORT = 1883;
const char MQTT_PASSWORD[] = SECRET_MQTT_PASSWORD;
const char MQTT_TOPIC[] = "arduino/waterPressureSensor";
const char MQTT_USERNAME[] = SECRET_MQTT_USERNAME;

const int PRESSURE_SENSOR_ANALOG_PIN = A3;
const float PRESSURE_SENSOR_FLOOR_VOLTAGE_READING = 0.5;
const int PRESSURE_SENSOR_MAX_PSI = 200;
const float PRESSURE_SENSOR_MAX_VOLTAGE_READING = 4.5;

const long PROGRAM_INTERVAL = 5000; // Milliseconds

const char WIFI_NAME[] = SECRET_WIFI_NAME; ;
const char WIFI_PASSWORD[] = SECRET_WIFI_PASSWORD;

ArduinoLEDMatrix ledMatrix;

int pressureSensorReading = 0;
int currentWiFiStatus = WL_IDLE_STATUS;
unsigned long previousTime = 0;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// SETUP FUNCTION ======================================================================

void setup()
{
  // SET UP THE SERIAL OUTPUT AND WAIT FOR IT TO INITIALIZE
  Serial.begin(9600);
  while (!Serial){}
  Serial.println("OJALA WATER PRESSURE SENSOR MONITOR");
  Serial.println("===============================");
  Serial.println("Beginning setup...");

  initializeLEDMatrix();
  connectToWiFi();
  connectToMQTTHost();

  Serial.println("===============================");
  Serial.println();
}

// MAIN PROGRAM FUNCTION =============================================================

void loop()
{
  // ENSURE THE MQTT CONNECTION STAYS OPEN
  mqttClient.poll();

  // RUN ON AN INTERVAL, NOT A DELAY
  unsigned long currentTime = millis();
  if (!(currentTime - previousTime >= PROGRAM_INTERVAL))
  {
    return;
  }
  previousTime = currentTime;

  checkWifiConnection();
  checkMQTTConnection();

  Serial.println("Attempting to gather reading and send to the MQTT host...");
  
  int psiReading = convertAnalogPressureInputToPSI(analogRead(PRESSURE_SENSOR_ANALOG_PIN));
  char psiReadingString[5];
  itoa(psiReading, psiReadingString, 10);

  mqttClient.beginMessage(MQTT_TOPIC);
  mqttClient.print(psiReadingString);
  mqttClient.endMessage();

  updateLedMatrixText(psiReadingString);
  Serial.println("Successfully sent reading to the MQTT host...");
}

// PROGRAM FUNCTIONS ====================================================================

void checkWifiConnection()
{
  currentWiFiStatus = WiFi.status();
  if (currentWiFiStatus == WL_CONNECTED)
  {
    return;
  }

  // Flush old connection, retry wifi. Once wifi is recconected, reconnect to the MQTT, as it may have timed out

  WiFi.disconnect();
  connectToWiFi();

  connectToMQTTHost();
}

void checkMQTTConnection()
{
  Serial.println("Checking mqtt connection: ");
  Serial.println(mqttClient.connected());
  Serial.println(mqttClient.connected() <= 0);
  while (mqttClient.connected() <= 0)
  {
    Serial.println("Inside mqtt loop");
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Unable to check mqtt connection, no wifi found");
      Serial.println(wifiClient.connected());
      break;

    }
    connectToMQTTHost();
  }
}

void connectToWiFi()
{
  displayScrollingTextOnLEDMatrix("Connecting wifi...");
  // CHECK FOR WIFI AVAILABILITY AND FIRMWARE ON THE BOARD
  while (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Unable to access WiFi module, ending now....");
    while(true);
  }

  if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Your WiFi firmware is out of date, please upgrade to the newest version...");
  }

  //ATTEMPT TO CONNECT TO THE LOCAL NETWORK
  while (currentWiFiStatus != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to ");
    Serial.print(WIFI_NAME);
    Serial.println("...");
    // ATTEMPT TO CONNECT TO THE WPA/WPA2 NETWORK
    currentWiFiStatus = WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
    delay(5000); // GIVE TIME FOR CONNECTION TO ESTABLISH BEFORE DOING ANYTHING ELSE...
    if (currentWiFiStatus != WL_CONNECTED)
    {
      Serial.println("Retrying Wifi connection...");
      displayScrollingTextOnLEDMatrix("Retrying Wifi connection...");
    }
  }

  Serial.print("Successfully Connected to ");
  Serial.print(WIFI_NAME);
  Serial.println("...");
  displayScrollingTextOnLEDMatrix("Wifi connected...");
}

void connectToMQTTHost()
{
  displayScrollingTextOnLEDMatrix("Connecting to MQTT...");

  mqttClient.setId(MQTT_CLIENT_ID);
  mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);

  Serial.print("Attempting to connect to the local MQTT host ");
  Serial.print(MQTT_HOST);
  Serial.println("...");
  if (!mqttClient.connect(MQTT_HOST, MQTT_PORT))
  {
    Serial.println("Unable to connect to MQTT host");
    Serial.println(mqttClient.connectError());
    displayScrollingTextOnLEDMatrix("Unable to connect to MQTT host");
    char mqttConnectionStatusString[5];
    itoa(mqttClient.connectError(), mqttConnectionStatusString, 10);
    displayScrollingTextOnLEDMatrix(mqttConnectionStatusString);
    return;
  }

  Serial.println("Successfully connected to MQTT host...");
  displayScrollingTextOnLEDMatrix("Connected to MQTT...");
}

int convertAnalogPressureInputToPSI(int analogPressureSensorReading)
{
  // CONVERT THE ANALOG INPUT TO A VOLTAGE READING (THE ARDUINO BOARD SUPPORTS 0V-5V)
  float voltage = (float)analogPressureSensorReading / (float)ANALOG_INPUT_SENSITIVITY * 5.0;

  // CONVERT THIS VOLTAGE READING TO AN APPROXIMATE PSI BASED ON THE SENSOR'S MINIMUM AND MAXIMUM VOLTAGE RETURNS (SET IN CONSTANTS BASED ON SENSOR'S DATASHEET)
  float psi = (voltage - PRESSURE_SENSOR_FLOOR_VOLTAGE_READING) / (PRESSURE_SENSOR_MAX_VOLTAGE_READING - PRESSURE_SENSOR_FLOOR_VOLTAGE_READING) * (float)PRESSURE_SENSOR_MAX_PSI;
  if (psi < 0) // VOLTAGE WILL OCASSIONALLY DROP TO 0.49, GIVING A SLIGHTLY NEGATIVE PSI. JUST IGNORE IT AND REPORT 0
  {
    psi = 0;
  }

  Serial.println("Pressure Conversions---");
  Serial.print("Raw analog: ");
  Serial.println(analogPressureSensorReading);

  Serial.print("Voltage Conversion: ");
  Serial.println(voltage);

  Serial.print("PSI Conversion: ");
  Serial.println(psi);

  Serial.println("-----------------------------------");
  return round(psi);
}

void initializeLEDMatrix()
{
  ledMatrix.begin();
  displayScrollingTextOnLEDMatrix("Starting...");
}

void displayScrollingTextOnLEDMatrix(char message[])
{
  ledMatrix.beginDraw();

  ledMatrix.stroke(0xFFFFFFFF);
  ledMatrix.textScrollSpeed(100);
  ledMatrix.textFont(Font_5x7);
  ledMatrix.beginText(0, 1, 0xFFFFFF);

  ledMatrix.print("   ");
  ledMatrix.println(message);
  ledMatrix.endText(SCROLL_LEFT);

  ledMatrix.endDraw();
}

void updateLedMatrixText(char message[])
{
  ledMatrix.endDraw();

  ledMatrix.beginDraw();
  ledMatrix.stroke(0xFFFFFFFF);ledMatrix.textFont(Font_5x7);
  ledMatrix.beginText(0, 1, 0xFFFFFF);

  ledMatrix.println(message);
  ledMatrix.endText();
}