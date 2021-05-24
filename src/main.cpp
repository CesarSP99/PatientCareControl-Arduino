#include <Arduino.h>
#include <WiFiManager.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DateTime.h>

MAX30105 particleSensor;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;

long samplesTaken = 0;
long unblockedValue;
long startTime;

int perCent; 
int degOffset = 0.5;
int irOffset = 1800;
int count;
int noFinger;

int avgIr;
int avgTemp;
int beatsok = 0;

const String APIUrl = "";

void setupDateTime() {
  DateTime.setServer("time.google.com");
  DateTime.setTimeZone("UTC");
  DateTime.begin(5);
  if (!DateTime.isTimeValid()) {
    Serial.println("No se pudo obtener la fecha y hora del servidor");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Inicializando...");
  //Habilitando WiFi
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  if(!wm.autoConnect("Patient Care Control")){
    Serial.println("No se pudo conectar");
    ESP.restart();
    delay(5000);
  }

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("ERROR Crítico: Sensor no encontrado");
    while (1);
  }
  Serial.println("Ponga su dedo en el sensor");

  particleSensor.setup(0);
  particleSensor.enableDIETEMPRDY();
  
  byte ledBrightness = 25;
  byte sampleAverage = 4;
  byte ledMode = 2;
  int sampleRate = 400;
  int pulseWidth = 411;
  int adcRange = 2048;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  particleSensor.enableDIETEMPRDY();
  pinMode(LED_BUILTIN, OUTPUT);
  setupDateTime();
}

void mandarDatos(){
  HTTPClient http;
  WiFiClient client;
  http.begin(client, APIUrl);
  http.addHeader("Content-Type", "application/json");
  String datos;
  //TODO: Revisar el datetime
  StaticJsonDocument<96> doc;
  doc["idPaciente"] = 1;
  doc["ritmoCardiaco"] = beatsPerMinute;
  doc["saturacionOxigeno"] = perCent;
  doc["fechaMedicion"] = DateTime.toUTCString();
  serializeJson(doc, datos);
  Serial.println(datos);
  // int respuesta = http.POST(datos);
  // String payload = http.getString();
  // Serial.print(respuesta);
  // Serial.println(": " + payload);
}

void loop()
{
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true)
  {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);


  Serial.print(" ");
  perCent = irValue / irOffset;
  Serial.print("Oxygen=");
  Serial.print(perCent);
  Serial.print("%");
  //Serial.print((float)samplesTaken / ((millis() - startTime) / 1000.0), 2);

  float temperatureC = particleSensor.readTemperature();
  temperatureC = temperatureC + degOffset;
  
  Serial.print(" Temp(C)=");
  Serial.print(temperatureC, 2);
  Serial.print("°");

  Serial.print(" IR=");
  Serial.print(irValue);

  if (irValue < 50000) {
    Serial.print(" No finger?");
    noFinger = noFinger+1;
  } else {
    count = count+1;
    avgIr = avgIr + irValue;
    avgTemp = avgTemp + temperatureC;
    Serial.print(" ... ");
  }
  if (count == 100) {
    avgIr = avgIr / count;
    avgTemp = avgTemp / count;
    Serial.print(" avgO=");
    Serial.print(avgIr);
    Serial.print(" avgC=");
    Serial.print(avgTemp);

    Serial.print(" count=");
    Serial.print(count);
    count = 0; 
    avgIr = 0;
    avgTemp = 0;
  }
  //Si se encuentra un bpm aceptable, se toman 10 muestras y se manda
  if(beatsPerMinute>=60){
    beatsok++;
    if(beatsok == 30){  
      if(perCent > 100)
        perCent = 100;
      digitalWrite(LED_BUILTIN, HIGH);
      mandarDatos();
      delay(20000);
      digitalWrite(LED_BUILTIN, LOW);
      beatsPerMinute = 0;
      beatsok = 0;
    }
  }
  Serial.println();
}