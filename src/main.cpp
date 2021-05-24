#include <Arduino.h>
#include <WiFiManager.h>
#include <MAX30105.h>
#include <heartRate.h>

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

const String APIUrl = "";

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

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.
}

void mandarDatos(){
  HTTPClient http;
  http.begin(APIUrl);
  http.addHeader("Content-Type", "application/json");
  //TODO: JSON de Datos
  String datos;
  int respuesta = http.POST(datos);
  String payload = http.getString();
  Serial.print(respuesta);
  Serial.println(": " + payload);
}

void loop()
{
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

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

  float temperatureC = particleSensor.readTemperature(); //Because I am a bad global citizen
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
    //only count and grab the reading if there's something there.
    count = count+1;
    avgIr = avgIr + irValue;
    avgTemp = avgTemp + temperatureC;
    Serial.print(" ... ");
  }

  //Get an average IR value over 100 loops
  if (count == 100) {
    avgIr = avgIr / count;
    avgTemp = avgTemp / count;
    Serial.print(" avgO=");
    Serial.print(avgIr);
    Serial.print(" avgF=");
    Serial.print(avgTemp);

    Serial.print(" count=");
    Serial.print(count);
    //reset for the next 100
    count = 0; 
    avgIr = 0;
    avgTemp = 0;
  }
  Serial.println();
}