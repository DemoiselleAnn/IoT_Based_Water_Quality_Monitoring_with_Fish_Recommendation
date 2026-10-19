#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

const float TEMP_COLD = 20.0;
const float TEMP_IDEAL = 25.0;
const float TEMP_NORMAL_MAX = 30.0;
const float TEMP_HOT = 30.0;

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
portMUX_TYPE buzzerMux = portMUX_INITIALIZER_UNLOCKED;
int requestedStatus = -1;
unsigned long requestedInterval = NORMAL_BEEP;

void setBuzzerPattern(int status, unsigned long interval) {
  portENTER_CRITICAL(&buzzerMux);
  requestedStatus = status;
  requestedInterval = interval;
  portEXIT_CRITICAL(&buzzerMux);
}

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
  portENTER_CRITICAL(&buzzerMux);
  int status = requestedStatus;
  unsigned long interval = requestedInterval;
  portEXIT_CRITICAL(&buzzerMux);

  if (status < 0) {
    stopBuzzer();
    currentStatus = -1;
    return;
  }

  beepInterval = interval;
  if (status != currentStatus) {
    currentStatus = status;
    startBeep();
  }

  unsigned long now = millis();

  if (buzzerOn && now - lastBeepStart >= BEEP_DURATION) {
    stopBuzzer();
  }

  if (!buzzerOn && now - lastBeepStart >= beepInterval) {
    startBeep();
  }
}

// Only this task controls the buzzer after setup. Network calls can block
// the sensor loop for seconds without stretching or delaying the beeps.
void buzzerTask(void* parameter) {
  while (true) {
    updateBuzzer();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

String updateTemperature(float temp) {
  String label;

  if (temp < TEMP_COLD) {
    label = "COLD";
  } else if (temp <= TEMP_NORMAL_MAX) {
    label = "NORMAL";
  } else if (temp < TEMP_HOT) {
    label = "MODERATE";
  } else {
    label = "HOT";
  }

  Serial.print("Status: ");
  Serial.println(label);
  return label;
}

void updateStatusLEDs(const String& temperatureStatus, const String& turbidityCondition) {
  allLEDsOff();

  if (temperatureStatus == "HOT" || turbidityCondition == "High") {
    digitalWrite(RED_LED, HIGH);
    setBuzzerPattern(3, HOT_BEEP);
    Serial.println("LED Status: RED - critical temperature or high turbidity");
    Serial.println("Buzzer interval: 1 second (200 ms beep)");
    return;
  }

  if (temperatureStatus == "MODERATE" || turbidityCondition == "Moderate") {
    digitalWrite(YELLOW_LED, HIGH);
    setBuzzerPattern(2, MODERATE_BEEP);
    Serial.println("LED Status: YELLOW - warning temperature or moderate turbidity");
    Serial.println("Buzzer interval: 3 seconds (200 ms beep)");
    return;
  }

  if (temperatureStatus == "COLD") {
    digitalWrite(BLUE_LED, HIGH);
    setBuzzerPattern(0, COLD_BEEP);
    Serial.println("LED Status: BLUE - cold water");
    Serial.println("Buzzer interval: 4 seconds (200 ms beep)");
    return;
  }

  digitalWrite(GREEN_LED, HIGH);
  setBuzzerPattern(1, NORMAL_BEEP);
  Serial.println("LED Status: GREEN - temperature normal and water clear");
  Serial.println("Buzzer interval: 5 seconds (200 ms beep)");
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
  if (ntu <= 65.0) return "Moderate";
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

  if (xTaskCreate(buzzerTask, "water-buzzer", 2048, nullptr, 2, nullptr) != pdPASS) {
    Serial.println("ERROR: Could not start buzzer task. Check ESP32 memory.");
  }

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
      allLEDsOff();
      setBuzzerPattern(-1, NORMAL_BEEP);
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
      updateStatusLEDs(temperatureStatus, turbidityCondition);
      postReading(temp, turbidityRaw, turbidityVoltage, turbidityNTU, temperatureStatus, turbidityCondition);
    }

    Serial.println("-------------------------");
  }
}
