#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "time.h"
#include <HTTPClient.h>

// ================= WiFi Settings =================
const char* ssid = "msot";
const char* wifi_password = "momtafa6060";

// ================= HiveMQ Cloud Settings =================
const char* mqtt_server = "b69ee0c9344646f691ec2fd02eb973e2.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "hivemq.webclient.1754122344657";
const char* mqtt_password = "79>#taA$!0XDCbMm3Lsp";

// ================= Supabase =================
const char* supabase_url = "https://xrmgfnesrrqqdtbaljti.supabase.co/functions/v1/esp32-webhook"; 
const char* supabase_anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InhybWdmbmVzcnJxcWR0YmFsanRpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTYxNjIzOTYsImV4cCI6MjA3MTczODM5Nn0.UZX0NG_Epx06cSbUR2fc0mjcOBpCFGAKRMCwBZEDpVU";   
const char* device_token = "e83fa93fb01d0cfc63f7e9549a695783"; // من جدول devices

// ================= MQTT Topics =================
const char* mqtt_sub_topic = "student/momen/subscribe";
const char* mqtt_pub_topic = "student/momen/publish";

WiFiClientSecure secureClient;
PubSubClient client(secureClient);

// ================= Keypad Setup =================
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= Hardware =================
#define LED_PIN 2
#define BUZZER_PIN 15
#define SERVO_PIN 5
#define LDR_PIN 34
#define IR_PIN 35
Servo doorServo;

// ================= LCD Setup =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= Password Setup =================
String password = "123";
String inputPassword = "";
int wrongAttempts = 0;
bool lockActive = false;
unsigned long lockStartTime = 0;
unsigned long buzzerStartTime = 0;

// ================= Security Mode =================
bool securityEnabled = false;
bool alarmTriggered = false;

// ================= LED Control Override =================
int ledOverride = -1;  
// -1 = Auto (LDR), 0 = Forced OFF, 1 = Forced ON

// ================= Time Config =================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 2 * 3600;   // GMT+2 for Egypt
const int daylightOffset_sec = 3600;   // +1 hour for summer time

// ================= Supabase POST Helper =================
void sendToSupabase(String eventType, String payload) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(supabase_url);
  http.addHeader("apikey", supabase_anon_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_anon_key));
  http.addHeader("Content-Type", "application/json");

  String body = "{\"device_token\":\"" + String(device_token) +
                "\",\"event_type\":\"" + eventType +
                "\",\"payload\":" + payload + "}";

  int httpResponseCode = http.POST(body);
  Serial.print("📩 Supabase Response: ");
  Serial.println(httpResponseCode);
  Serial.println(http.getString());
  http.end();
}

// ================= MQTT Callback =================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("📩 MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (msg == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    ledOverride = 1;
    client.publish(mqtt_pub_topic, "✅ LED turned ON");
    sendToSupabase("alert", "{\"type\":\"led_on\",\"source\":\"cloud\"}");
  } else if (msg == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    ledOverride = 0;
    client.publish(mqtt_pub_topic, "✅ LED turned OFF");
    sendToSupabase("alert", "{\"type\":\"led_off\",\"source\":\"cloud\"}");
  } else if (msg == "AUTO_LED") {
    ledOverride = -1;
    client.publish(mqtt_pub_topic, "♻ LED back to AUTO mode");
  } else if (msg == "DOOR_OPEN") {
    doorServo.write(180);
    delay(800);
    doorServo.write(90);
    client.publish(mqtt_pub_topic, "🚪 Door OPENED");
    sendToSupabase("alert", "{\"type\":\"door_open\",\"source\":\"cloud\"}");
  } else if (msg == "DOOR_CLOSE") {
    doorServo.write(0);
    delay(800);
    doorServo.write(90);
    client.publish(mqtt_pub_topic, "🚪 Door CLOSED");
    sendToSupabase("alert", "{\"type\":\"door_close\",\"source\":\"cloud\"}");
  } else if (msg == "SECURITY_ON") {
    securityEnabled = true;
    client.publish(mqtt_pub_topic, "🔒 Security Mode ENABLED");
    sendToSupabase("alert", "{\"type\":\"security_on\"}");
  } else if (msg == "SECURITY_OFF") {
    securityEnabled = false;
    alarmTriggered = false;
    noTone(BUZZER_PIN);
    client.publish(mqtt_pub_topic, "🔓 Security Mode DISABLED");
    sendToSupabase("alert", "{\"type\":\"security_off\"}");
  }
}

// ================= MQTT Reconnect =================
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔄 Connecting to MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("✅ Connected!");
      client.subscribe(mqtt_sub_topic);
      client.publish(mqtt_pub_topic, "📡 ESP32 Connected to HiveMQ");
    } else {
      Serial.print("❌ Failed, rc=");
      Serial.print(client.state());
      Serial.println(" → retry in 5s");
      delay(5000);
    }
  }
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  doorServo.attach(SERVO_PIN);
  doorServo.write(90);

  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid, wifi_password);
  Serial.print("🌐 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("✅ WiFi Connected");

  secureClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.println("Enter Password:");
}

// ================= Loop =================
unsigned long lastSensorSend = 0;

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long currentTime = millis();

  // ========== Security Mode with IR ==========
  if (securityEnabled && digitalRead(IR_PIN) == HIGH && !alarmTriggered) {
    tone(BUZZER_PIN, 1000);
    alarmTriggered = true;
    client.publish(mqtt_pub_topic, "🚨 Intruder detected! Alarm triggered.");
    sendToSupabase("alert", "{\"type\":\"motion_detected\"}");
  }

  // ========== Keypad ==========
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == '#') {
      if (inputPassword == password) {
        Serial.println("✅ Correct Password");
        doorServo.write(180);
        delay(800);
        doorServo.write(90);
        wrongAttempts = 0;
        client.publish(mqtt_pub_topic, "✅ Password accepted - Door opened");
        sendToSupabase("alert", "{\"type\":\"door_open\",\"by\":\"keypad\"}");
      } else {
        wrongAttempts++;
        Serial.println("❌ Wrong Password!");
        if (wrongAttempts >= 3) {
          digitalWrite(BUZZER_PIN, HIGH);
          buzzerStartTime = currentTime;
          lockStartTime = currentTime;
          lockActive = true;
          client.publish(mqtt_pub_topic, "🚨 Alarm! Too many wrong attempts.");
          sendToSupabase("alert", "{\"type\":\"wrong_password\"}");
        }
      }
      inputPassword = "";
    } else if (key == '*') {
      inputPassword = "";
    } else if (key == 'D') {
      securityEnabled = !securityEnabled;
      if (securityEnabled) {
        client.publish(mqtt_pub_topic, "🔒 Security Mode ENABLED via Keypad");
        sendToSupabase("alert", "{\"type\":\"security_on\"}");
      } else {
        noTone(BUZZER_PIN);
        alarmTriggered = false;
        client.publish(mqtt_pub_topic, "🔓 Security Mode DISABLED via Keypad");
        sendToSupabase("alert", "{\"type\":\"security_off\"}");
      }
    } else {
      inputPassword += key;
    }
  }

  // ========== LED Control ==========
  if (ledOverride == -1) {
    int ldrValue = analogRead(LDR_PIN);
    if (ldrValue < 2000) digitalWrite(LED_PIN, HIGH);
    else digitalWrite(LED_PIN, LOW);
  }

  // ========== LCD Time Display ==========
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    lcd.setCursor(0, 0);
    lcd.print("Time: ");
    lcd.print(buffer);

    char dateBuffer[16];
    strftime(dateBuffer, sizeof(dateBuffer), "%d-%m-%Y", &timeinfo);
    lcd.setCursor(0, 1);
    lcd.print("Date: ");
    lcd.print(dateBuffer);
  }

  // ========== Send Sensor Readings every 5 sec ==========
  if (millis() - lastSensorSend > 5000) {
    int ldrValue = analogRead(LDR_PIN);
    int irValue = digitalRead(IR_PIN);

    // ابعت LDR
    String payloadLDR = "{\"sensor_type\":\"LDR\",\"sensor_value\":" + String(ldrValue) + "}";
    sendToSupabase("sensor_reading", payloadLDR);

    // ابعت IR
    String payloadIR = "{\"sensor_type\":\"IR\",\"sensor_value\":" + String(irValue) + "}";
    sendToSupabase("sensor_reading", payloadIR);

    lastSensorSend = millis();
  }

  delay(200);
}