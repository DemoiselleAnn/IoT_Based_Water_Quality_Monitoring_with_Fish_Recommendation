# AquaSense ESP32 Water Quality Monitoring

AquaSense is a local dashboard for an ESP32 with a DS18B20 water temperature sensor and an analog turbidity sensor. The ESP32 posts readings over Wi-Fi, and the dashboard shows live status, history, alerts, and basic fish compatibility recommendations.

The firmware treats an ADC reading at or above `4090` as a disconnected
turbidity sensor. It reports the sensor error, leaves the turbidity LEDs off,
and skips the dashboard POST until a valid turbidity reading is available.

## Hardware

- ESP32 development board
- DS18B20 waterproof temperature sensor
- 4.7k resistor for the DS18B20 data pull-up
- Analog turbidity sensor module
- Four temperature LEDs and three turbidity LEDs with resistors
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
| Turbidity green (clean/clear) LED | GPIO 25 |
| Turbidity yellow (cloudy/moderate) LED | GPIO 26 |
| Turbidity red (dirty/high) LED | GPIO 27 |
| Active buzzer | GPIO 23 |

Connect all grounds together. Power the DS18B20 from 3.3V and place a 4.7k resistor between DS18B20 data and 3.3V. Make sure the turbidity module analog output is safe for the ESP32 ADC input; use a voltage divider if the module can output above 3.3V.

## LED Status

Temperature and turbidity use separate LED groups. The four temperature LEDs
depend only on temperature:

| LED | Meaning |
| --- | --- |
| Green | Temperature is normal |
| Blue | Temperature is cold |
| Yellow | Temperature is moderate |
| Red | Temperature is hot |

The three turbidity LEDs depend only on turbidity:

| LED | Meaning |
| --- | --- |
| Green | Clean / clear water (`0-25 NTU`) |
| Yellow | Cloudy / moderate water (above `25` through `65 NTU`) |
| Red | Dirty / high turbidity (above `65 NTU`) |

The default fish water temperature range is `20-30°C`, with `25°C` as the ideal target.
Temperatures above `30°C` and below `35°C` are moderate; `35°C` and above are hot.

The active buzzer sounds only for cold, hot, or dirty/high-alert conditions:
hot temperature or dirty water beeps every 1 second, and cold temperature
beeps every 3 seconds. Normal and moderate temperatures, clean water, and
cloudy/moderate water are silent. Dirty water has priority over temperature
for the buzzer alert. Switching to a silent condition stops any active beep. Each
beep lasts 200 milliseconds; intervals are measured from beep start to beep
start. Switching to blue or red starts a beep immediately. High turbidity selects the
red pattern even when the temperature is cold or normal. A separate ESP32 task
keeps the buzzer running during Wi-Fi retries and slow dashboard requests.
An invalid or disconnected temperature sensor silences the buzzer, as before.
Upload the updated sketch to the ESP32 for firmware changes to take effect.
At startup, Serial Monitor at `115200` baud must show
`Buzzer control: COLD=3s HOT/DIRTY=1s NORMAL/MODERATE=OFF`.
If this message is absent, check that the updated sketch was uploaded to the
correct board and port.

## Run the Dashboard

Install Node.js 18 or newer, then run:

```bash
npm start
```

Open `http://localhost:3000` on the computer running the server.

## Admin Settings

Sign in to the dashboard and open **Admin Settings**. You can change the
normal and critical temperature limits plus the clear and moderate turbidity
limits. Save the form to apply the values immediately to the dashboard,
alerts, and fish recommendations. The server stores these local settings in
`aquasense-settings.json`; the file is created automatically and is excluded
from Git. The ESP32 continues to send raw sensor readings, so no firmware
change or re-upload is needed when thresholds change.

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

- Normal (clear water): `0-25 NTU`, green indicator
- Moderate: above `25` through `65 NTU`, yellow indicator
- Critical / High Turbidity: above `65 NTU`, red indicator

The dashboard shows the turbidity LED color on both the overview and Live Monitoring cards. The ESP32 uses the same turbidity thresholds (its firmware reports normal water as `Clear`). The physical temperature LEDs and turbidity LEDs operate independently, so a hot and dirty reading lights both temperature red and turbidity red.

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
