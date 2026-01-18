# 🌲 Forest Fire Detection System (Hybrid LoRa/WiFi)

**A distributed forest fire detection network supporting both LoRa (Heltec V3) and WiFi (ESP32-S2) nodes, monitored by a Raspberry Pi Central Node.**

This system leverages a hybrid communication approach to monitor forest areas. Sensor nodes wake up periodically to measure air quality (`BME680`/`SGP30`). If a potential fire is detected, they transmit alerts via **LoRa** (Long Range) or **WiFi UDP** to the Central Node. The Central Node automatically rotates a camera to the specific sensor's location to capture visual evidence.

---

## 🏗️ System Architecture

```mermaid
graph LR
    subgraph "Sensor Node A (Heltec V3)"
        SensorsA[BME680 / SGP30] --> ESP32_V3
        ESP32_V3 -->|LoRa (868MHz)| SX1262
    end

    subgraph "Sensor Node B (ESP32-S2)"
        SensorsB[BME680 / SGP30] --> ESP32_S2
        ESP32_S2 -->|WiFi UDP| WiFi_Antenna
    end

    subgraph "Central Node (Raspberry Pi)"
        LoRaModule[SX126x LoRa Hat] -->|SPI| RPi[Raspberry Pi 3B+]
        WiFi_RPi[WiFi Interface] -->|UDP Port 5005| RPi
        RPi -->|GPIO| Servo[Camera Servo]
        RPi -->|CSI| Cam[Pi Camera]
    end

    SX1262 -.->|LoRa Packet| LoRaModule
    WiFi_Antenna -.->|UDP Packet| WiFi_RPi
```

---

## ✨ Features

- **Hybrid Communication**: Supports both **LoRa** (for long-range, off-grid areas) and **WiFi** (for areas with coverage) simultaneously.
- **Intelligent Alerting**:
    - **Heltec V3**: Sends alerts via LoRa (868MHz, SF7).
    - **ESP32-S2**: Sends alerts via UDP over WiFi.
- **Multi-Stage Detection**:
    - Monitors gas resistance/TVOC values.
    - Classifies status as `NO-FIRE`, `POSSIBLE-FIRE`, or `FIRE-DETECTED`.
    - Requires **3 consecutive confirmations** before triggering a full alarm.
- **Automated Surveillance**:
    - Central Node listens on both channels.
    - Upon receiving `FIRE-DETECTED`, it **rotates the camera** to the node's assigned angle (Left/Right).
    - Captures and saves a photo labeled with the Node ID and timestamp.
    - Enforces a 2-minute cooldown per node to prevent spam.

---

## 🛠️ Hardware & Pinout

### 1. Sensor Nodes

#### Option A: Heltec V3 (LoRa)
| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **I2C SDA** | 41 | Sensor Data |
| **I2C SCL** | 42 | Sensor Clock |
| **LoRa Interface** | Internal | SX1262 (NSS:8, DIO1:14, RST:12, BUSY:13) |
| **OLED** | Internal | Status Display |
| **LEDs** | 1, 2, 3, 4 | Red, White, Yellow, Blue |

#### Option B: ESP32-S2 (WiFi)
| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **I2C SDA** | 8 | Sensor Data |
| **I2C SCL** | 9 | Sensor Clock |
| **LEDs** | 4, 6, 7, 10 | Red, White, Yellow, Blue |

### 2. Raspberry Pi Central Node
| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **LoRa HAT** | SPI0 | SX126x Communication |
| **Servo** | 12 | PWM Camera Control |
| **Camera** | CSI Port | Visual Verification |

---

## 💻 Installation & Usage

### Sensor Nodes (Arduino IDE)
1.  **Libraries**: Install `Adafruit BME680`, `Adafruit SGP30`, `RadioLib` (for LoRa), and `U8g2` (for OLED).
2.  **Configuration**:
    - Open `sensor_node.ino`.
    - Uncomment `#define BOARD_V3` for Heltec LoRa **OR** `#define BOARD_S2` for ESP32 WiFi.
    - Set `NODE_ID` (1 for S2, 2 for V3).
3.  **Upload**: Select the correct board and flash.

### Central Node (Raspberry Pi)
1.  **Prerequisites**: Enable SPI and Camera (`sudo raspi-config`).
2.  **Dependencies**:
    ```bash
    pip install -r requirements.txt
    ```
    *Requires `opencv-python`, `gpiozero`, `lora-rf` (or compatible SX126x driver).*
3.  **Run**:
    ```bash
    python central_node_lora.py
    ```
    The script will start two threads: one for LoRa and one for WiFi UDP.

---

## 📊 Packet Format

The system uses a human-readable string key-value format:

**Format:** `ID:<NODE_ID>,LABEL:<STATUS>,VAL:<SENSOR_VALUE>`

**Examples:**
- `ID:2,LABEL:FIRE-DETECTED,VAL:450` (Critical Alert)
- `ID:1,LABEL:NO-FIRE,VAL:25000` (Heartbeat/Normal)

**Status Codes:**
- `NO-FIRE`: Normal operation (White LED).
- `POSSIBLE-FIRE`: Pre-alert threshold crossed (Yellow LED).
- `FIRE-DETECTED`: Confirmed fire risk (Red LED) -> Triggers Camera.

---

*Project for Faculdade SAIoT Class*
