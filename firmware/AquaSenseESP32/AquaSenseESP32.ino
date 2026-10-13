#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Wi-Fi and dashboard API. Use your computer's LAN IP, not localhost.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_URL = "http://192.168.1.100:3000/api/readings";
const char* DEVICE_ID = "aquasense-esp32";

// Sensor pins
#define DS18B20_PIN 4
#define TURBIDITY_PIN 34

// LED pins
#define GREEN_LED 16
#define YELLOW_LED 17
#define RED_LED 18
#define BLUE_LED 19

// Active buzzer
#define BUZZER_PIN 23

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

const float TEMP_COLD = 25.0;
const float TEMP_NORMAL_MAX = 32.75;
const float TEMP_HOT = 37.0;

const unsigned long COLD_BEEP = 4000;
const unsigned long NORMAL_BEEP = 5000;
const unsigned long MODERATE_BEEP = 3000;
const unsigned long HOT_BEEP = 1000;

const unsigned long BEEP_DURATION = 200;
const unsigned long SENSOR_INTERVAL = 5000;

const int TURBIDITY_SAMPLES = 20;
const float ADC_REFERENCE_VOLTAGE = 3.3;
const int ADC_MAX = 4095;

// Calibrate these two values using clean water and very cloudy water.
const float CLEAR_WATER_VOLTAGE = 2.70;
const float CLOUDY_WATER_VOLTAGE = 0.80;

unsigned long lastSensorRead = 0;
unsigned long lastBeepStart = 0;
unsigned long beepInterval = NORMAL_BEEP;

bool buzzerOn = false;
bool temperatureValid = false;
bool firstReading = true;
int currentStatus = -1;

void allLEDsOff() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
}

void stopBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);
  buzzerOn = false;
}

void startBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOn = true;
  lastBeepStart = millis();
}

void updateBuzzer() {
  if (!temperatureValid) {
    stopBuzzer();
    return;
  }

  unsigned long now = millis();

  if (buzzerOn && now - lastBeepStart >= BEEP_DURATION) {
    stopBuzzer();
  }

  if (!buzzerOn && now - lastBeepStart >= beepInterval) {
    startBeep();
  }
}

String updateTemperature(float temp) {
  int newStatus;
  String label;

  if (temp < TEMP_COLD) {
    newStatus = 0;
    beepInterval = COLD_BEEP;
    label = "COLD";
  } else if (temp <= TEMP_NORMAL_MAX) {
    newStatus = 1;
    beepInterval = NORMAL_BEEP;
    label = "NORMAL";
  } else if (temp < TEMP_HOT) {
    newStatus = 2;
    beepInterval = MODERATE_BEEP;
    label = "MODERATE";
  } else {
    newStatus = 3;
    beepInterval = HOT_BEEP;
    label = "HOT";
  }

  allLEDsOff();

  switch (newStatus) {
    case 0: digitalWrite(BLUE_LED, HIGH); break;
    case 1: digitalWrite(GREEN_LED, HIGH); break;
    case 2: digitalWrite(YELLOW_LED, HIGH); break;
    case 3: digitalWrite(RED_LED, HIGH); break;
  }

  Serial.print("Status: ");
  Serial.println(label);
  Serial.print("Buzzer interval: ");
  Serial.print(beepInterval / 1000);
  Serial.println(" seconds");

  if (newStatus != currentStatus) {
    currentStatus = newStatus;
    startBeep();
  }

  return label;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed. Will retry.");
  }
}

int readTurbidityRaw() {
  long total = 0;
  for (int i = 0; i < TURBIDITY_SAMPLES; i++) {
    total += analogRead(TURBIDITY_PIN);
    delay(2);
  }
  return total / TURBIDITY_SAMPLES;
}

float rawToVoltage(int rawValue) {
  return rawValue * (ADC_REFERENCE_VOLTAGE / ADC_MAX);
}

float voltageToNtu(float voltage) {
  float clear = max(CLEAR_WATER_VOLTAGE, CLOUDY_WATER_VOLTAGE);
  float cloudy = min(CLEAR_WATER_VOLTAGE, CLOUDY_WATER_VOLTAGE);
  float normalized = (clear - voltage) / (clear - cloudy);
  normalized = constrain(normalized, 0.0, 1.0);
  return normalized * 100.0;
}

String classifyTurbidity(float ntu) {
  if (ntu <= 25.0) return "Clear";
  if (ntu <= 50.0) return "Moderate";
  return "High";
}

void postReading(float temperatureC, int turbidityRaw, float turbidityVoltage, float turbidityNTU, const String& temperatureStatus, const String& turbidityCondition) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }

  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"temperatureC\":" + String(temperatureC, 2) + ",";
  payload += "\"turbidityRaw\":" + String(turbidityRaw) + ",";
  payload += "\"turbidityVoltage\":" + String(turbidityVoltage, 3) + ",";
  payload += "\"turbidityNTU\":" + String(turbidityNTU, 1) + ",";
  payload += "\"temperatureStatus\":\"" + temperatureStatus + "\",";
  payload += "\"turbidityCondition\":\"" + turbidityCondition + "\"";
  payload += "}";

  int responseCode = http.POST(payload);
  Serial.print("Dashboard POST response: ");
  Serial.println(responseCode);
  http.end();
}

void setup() {
  Serial.begin(115200);

  sensors.begin();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  allLEDsOff();
  stopBuzzer();

  analogReadResolution(12);
  analogSetPinAttenuation(TURBIDITY_PIN, ADC_11db);

  sensors.setResolution(11);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  Serial.println("========================");
  Serial.println("FISH FARM WATER MONITOR");
  Serial.println("========================");

  connectWiFi();
}

void loop() {
  updateBuzzer();

  unsigned long now = millis();

  if (firstReading || now - lastSensorRead >= SENSOR_INTERVAL) {
    if (firstReading) {
      firstReading = false;
      lastSensorRead = now;
      return;
    }

    lastSensorRead = now;

    float temp = sensors.getTempCByIndex(0);
    sensors.requestTemperatures();

    Serial.println();
    Serial.println("----- WATER MONITOR -----");

    String temperatureStatus = "ERROR";
    if (temp == DEVICE_DISCONNECTED_C || temp < -10 || temp > 85) {
      temperatureValid = false;
      currentStatus = -1;
      allLEDsOff();
      stopBuzzer();
      Serial.println("ERROR: DS18B20 NOT DETECTED!");
    } else {
      temperatureValid = true;
      Serial.print("Temperature: ");
      Serial.print(temp, 2);
      Serial.println(" C");
      temperatureStatus = updateTemperature(temp);
    }

    int turbidityRaw = readTurbidityRaw();
    float turbidityVoltage = rawToVoltage(turbidityRaw);
    float turbidityNTU = voltageToNtu(turbidityVoltage);
    String turbidityCondition = classifyTurbidity(turbidityNTU);

    Serial.print("Turbidity ADC: ");
    Serial.println(turbidityRaw);
    Serial.print("Turbidity voltage: ");
    Serial.println(turbidityVoltage, 3);
    Serial.print("Estimated turbidity: ");
    Serial.print(turbidityNTU, 1);
    Serial.println(" NTU");
    Serial.print("Water Clarity: ");
    Serial.println(turbidityCondition);

    if (temperatureValid) {
      postReading(temp, turbidityRaw, turbidityVoltage, turbidityNTU, temperatureStatus, turbidityCondition);
    }

    Serial.println("-------------------------");
  }
}
