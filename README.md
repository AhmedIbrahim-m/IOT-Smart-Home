# Smart Home Security & IoT Control System

## 📌 Overview
This project is a **Smart Home Security and IoT Control System** built using an **ESP32 microcontroller**.  
It integrates multiple sensors and actuators with cloud services to provide **real-time monitoring, access control, and security alerts**.

---

## ⚙️ Features
- 🔑 **Password-based Door Lock** (Keypad + Servo motor)
- 💡 **Automatic LED Control** using LDR sensor
- 🚨 **Intruder Detection & Alarm** via IR sensor + Buzzer
- 🔒 **Security Mode ON/OFF** (via Keypad or Cloud)
- 🌍 **Cloud Integration** with **HiveMQ (MQTT)** & **Supabase**
- 📟 **LCD Display** for real-time time & date
- 📡 **Remote Monitoring & Control**

---

## 🛠️ Hardware Components
- ESP32
- Keypad (4x4)
- Servo Motor
- Buzzer
- LDR Sensor (Light detection)
- IR Sensor (Motion detection)
- LCD 16x2 (I2C)
- LED

---

## ☁️ Cloud Services
- **HiveMQ Cloud** → MQTT broker for device communication  
- **Supabase** → Database & webhook events for storing alerts & sensor data  

---

## 🚀 How It Works
1. User enters password on the keypad → Door lock opens via Servo.  
2. LDR controls LED brightness automatically.  
3. Wrong password attempts (≥3) → Buzzer alarm triggered.  
4. IR motion detection → Intruder alert sent to Supabase.  
5. MQTT commands from the cloud → Control LED, Door, Security mode.  
6. LCD displays current **time & date** (via NTP server).  
