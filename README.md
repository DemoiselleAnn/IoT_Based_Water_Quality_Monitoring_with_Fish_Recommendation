# AquaSense ESP32 Water Quality Monitoring

AquaSense is a local dashboard for an ESP32 with a DS18B20 water temperature sensor and an analog turbidity sensor. The ESP32 posts readings over Wi-Fi, and the dashboard shows live status, history, alerts, and basic fish compatibility recommendations.

## Hardware

- ESP32 development board
- DS18B20 waterproof temperature sensor
- 4.7k resistor for the DS18B20 data pull-up
- Analog turbidity sensor module
- Green, yellow, red, and blue LEDs with resistors
- Active buzzer

## Default ESP32 Wiring

| Part | ESP32 pin |
| --- | --- |
| DS18B20 data | GPIO 4 |
| Turbidity analog output | GPIO 34 |
| Green LED | GPIO 16 |
| Yellow LED | GPIO 17 |
| Red LED | GPIO 18 |
| Blue LED | GPIO 19 |
| Active buzzer | GPIO 23 |

Connect all grounds together. Power the DS18B20 from 3.3V and place a 4.7k resistor between DS18B20 data and 3.3V. Make sure the turbidity module analog output is safe for the ESP32 ADC input; use a voltage divider if the module can output above 3.3V.

## LED Status

The ESP32 chooses the LED color after reading both temperature and turbidity:

| LED | Meaning |
| --- | --- |
| Green | Temperature is normal and turbidity is clear |
| Blue | Temperature is cold and turbidity is clear |
| Yellow | Temperature is moderate or turbidity is moderate |
| Red | Temperature is hot or turbidity is high |

## Run the Dashboard

Install Node.js 18 or newer, then run:

```bash
npm start
```

Open `http://localhost:3000` on the computer running the server.

Find your computer LAN IP address:

```powershell
ipconfig
```

Use that address in the ESP32 sketch, for example:

```cpp
const char* API_URL = "http://192.168.1.100:3000/api/readings";
```

## Flash the ESP32

Open [firmware/AquaSenseESP32/AquaSenseESP32.ino](firmware/AquaSenseESP32/AquaSenseESP32.ino) in the Arduino IDE.

Install these Arduino libraries:

- OneWire
- DallasTemperature

Update these values before uploading:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_URL = "http://192.168.1.100:3000/api/readings";
```

Upload to the ESP32 and open the Serial Monitor at `115200` baud. The dashboard should switch from offline to live after the first successful POST.

## Turbidity Calibration

The firmware estimates turbidity using two calibration voltages:

```cpp
const float CLEAR_WATER_VOLTAGE = 2.70;
const float CLOUDY_WATER_VOLTAGE = 0.80;
```

Put the probe in clean water, note the voltage printed in Serial Monitor, and set `CLEAR_WATER_VOLTAGE`. Put it in very cloudy water, note the voltage, and set `CLOUDY_WATER_VOLTAGE`. The dashboard classifies estimated turbidity as:

- Clear: `0-25 NTU`
- Moderate: `25-50 NTU`
- High: above `50 NTU`

## API

The ESP32 posts JSON to:

```text
POST /api/readings
```

Example:

```json
{
  "deviceId": "aquasense-esp32",
  "temperatureC": 27.4,
  "turbidityRaw": 1780,
  "turbidityVoltage": 1.435,
  "turbidityNTU": 28.1,
  "temperatureStatus": "NORMAL",
  "turbidityCondition": "Moderate"
}
```

The browser polls:

- `GET /api/reading`
- `GET /api/history`
