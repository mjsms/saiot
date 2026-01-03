# 🌲 Forest Fire Detection System (LoRa P2P)

**A distributed forest fire detection network using ESP32-S2 (Sensor Nodes) and Raspberry Pi (Central Node).**

This system uses **LoRa (Long Range) Point-to-Point** communication at 868MHz to monitor forest areas. Sensor nodes wake up periodically to check air quality and BME680/SGP30 readings. If fire is detected, they send an immediate alert to the Central Node, which rotates a camera to the event location and captures evidence.

---

## 🏗️ System Architecture

```mermaid
graph LR
    subgraph "Sensor Node (ESP32-S2)"
        Sensors[BME680 + SGP30] --> ESP32
        ESP32 -->|SPI| RFM95[RFM95W (LoRa)]
        ESP32 -- "Deep Sleep (5min)" --> ESP32
    end

    subgraph "Central Node (Raspberry Pi)"
        LoRaHAT[Waveshare LoRa HAT] -->|SPI| RPi[Raspberry Pi 3B+]
        RPi -->|GPIO| Servo[Camera Servo]
        RPi -->|CSI| Cam[Pi Camera]
    end

    RFM95 -.->|868MHz (SF12)| LoRaHAT
```

---

## ✨ Features

- **Long Range Communication**: Uses LoRa with Spreading Factor 12 (SF12) for maximum forest penetration.
- **Ultra Low Power**: ESP32 nodes sleep for 5 minutes (Deep Sleep), waking only to measure.
- **Panic Mode**: If fire is detected, the node wakes up every 30 seconds to send continuous updates.
- **Visual Verification**: The Central Node automatically points a camera at the angle of the alert and takes a photo.
- **Robustness**: Raspberry Pi filters radio noise and only acts on valid `ID:ANGLE` packets.

---

## 🛠️ Hardware & Pinout

### 1. ESP32-S2 Sensor Node
| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **RFM95 NSS** | 10 | LoRa Chip Select |
| **RFM95 RST** | 5 | LoRa Reset |
| **RFM95 DIO0** | 14 | LoRa IRQ |
| **RFM95 MOSI** | 35 | SPI Data |
| **RFM95 MISO** | 37 | SPI Data |
| **RFM95 SCK** | 36 | SPI Clock |
| **SDA (I2C)** | 8 | BME680/SGP30 Data |
| **SCL (I2C)** | 9 | BME680/SGP30 Clock |

### 2. Raspberry Pi Central Node
| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **LoRa HAT** | Standard | Uses SPI0 (CE0) |
| **Servo** | 22 | PWM Signal |
| **Camera** | CSI Port | Ribon Cable |

---

## 💻 Installation & Usage

### Sensor Node (ESP32)
1.  **Dependencies**: Install `Adafruit BME680`, `Adafruit SGP30`, and `LoRa` (Sandeep Mistry) in Arduino IDE.
2.  **Upload**: Flash `sensor_node.ino` to your ESP32-S2.
3.  **Deploy**: Mount the sensor on a tree. It will automatically start its Sleep/Wake cycle.

### Central Node (Raspberry Pi)
1.  **Enable Interfaces**: Run `sudo raspi-config` and enable **SPI** and **Camera**.
2.  **Install Python Libs**:
    ```bash
    pip install -r requirements.txt
    ```
    *Note: Ensure `SX127x.py` is in the folder (from Waveshare demo).*
3.  **Run**:
    ```bash
    python central_node_lora.py
    ```

---

## 📊 Packet Format
The system uses a simple string protocol:
`ID:<NODE_ID>,ANG:<ANGLE>`

Example: `ID:1,ANG:45` -> Node 1 detected fire at 45 degrees.

---

*Project for Faculdade SAIoT Class*
