#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "setup.h"

// ================= MACROS & PINS =================
#define RST_PIN 4
#define SS_PIN 5
#define SERVO_PIN 13

#define TRIG_PIN 16
#define ECHO_PIN 17

#define BTN_TOGGLE_PIN 25
#define BTN_LOCK_PIN 26

#define LED_LOCK_PIN 14
#define BUZZER_PIN 27

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define ANGLE_CLOSED 110
#define ANGLE_OPENED 20
#define ANGLE_SAFE_ZONE ((ANGLE_CLOSED + ANGLE_OPENED) / 2)

// ================= NETWORK CONFIG =================
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT; 
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS; 
const char* mqtt_topic_status = "garage/status";
const char* mqtt_topic_cmd = "garage/command";
// ================= GLOBAL OBJECTS =================
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Servo doorServo;
WiFiClientSecure espClient; 
PubSubClient client(espClient);


Preferences preferences;
// ================= STATES =================
enum DoorState {
  DOOR_CLOSED,
  DOOR_OPENING,
  DOOR_OPENED,
  DOOR_CLOSING,
  DOOR_STOPPED,
  DOOR_EMERGENCY
};

DoorState currentDoorState = DOOR_CLOSED;
bool isHardLocked = false;
bool isPendingLock = false;
bool isLearningCard = false;
unsigned long learnModeStartTime = 0;

// Variables for Inputs & Sensors
bool lastLockSwitchState = false;
bool lastToggleState = HIGH;
int currentAngle = ANGLE_CLOSED;
float currentDistance = 999.0;
String masterUID = "B2:19:F5:06";

// Timers
unsigned long lastToggleTime = 0;
unsigned long lastLockTime = 0;
unsigned long lastRfidTime = 0;
unsigned long lastSonarTime = 0;
unsigned long lastServoTime = 0;
unsigned long lastPublishTime = 0;
unsigned long lastReconnectAttempt = 0;

int buzzerPattern[5] = {0};
int buzzerPatternLen = 0;
int buzzerPatternIndex = 0;
unsigned long lastBuzzerToggleTime = 0;
bool isBuzzerPlaying = false;
unsigned long oledRestoreTime = 0;
bool isOledTemp = false;

// ================= PROTOTYPES =================
void updateOLED(String line1, String line2);
void updateOLEDTemp(String line1, String line2, int duration);
void beep(int t1, int t2 = 0, int t3 = 0, int t4 = 0, int t5 = 0);
void handleTasks(unsigned long now);
void requestOpenDoor(String source);
void requestCloseDoor(String source);
void requestToggleDoor(String source);
void requestLock();
void requestUnlock();
void publishStatusToWeb(bool force = false);
void setup_wifi();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  
  preferences.begin("garage", false);
  String savedUID = preferences.getString("masterUID", "");
  if (savedUID != "") {
    masterUID = savedUID;
  }

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR");
    while (true);
  }

  pinMode(BTN_TOGGLE_PIN, INPUT_PULLUP);
  pinMode(BTN_LOCK_PIN, INPUT_PULLUP);
  
  pinMode(LED_LOCK_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  doorServo.attach(SERVO_PIN);
  doorServo.write(currentAngle);

  updateOLED("SYSTEM READY", "Door: CLOSED");
  Serial.println("SMART GARAGE READY");

  setup_wifi();
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

}

// ================= LOOP =================
void loop() {
  unsigned long now = millis();
  
  if (isLearningCard && (now - learnModeStartTime > 15000)) {
    isLearningCard = false;
    Serial.println("LEARN MODE TIMEOUT: Hết 15s chờ thẻ. Tự động hủy!");
    updateOLEDTemp("LEARN TIMEOUT", doorStateToString(currentDoorState), 2000);
    beep(200, 100, 200);
  }

  // 1. NETWORK & BACKGROUND TASKS
  handleNetwork(now);
  handleTasks(now);
  
  // 2. SAVE PREVIOUS STATE FOR SYNC
  DoorState oldDoorState = currentDoorState;
  bool oldLockState = isHardLocked;

  // 3. READ SENSORS
  readSonar(now);
  safetyCheck();

  // 4. READ PHYSICAL INPUTS
  readLockSwitch(now);
  if (!isHardLocked && !isPendingLock) {
    readToggleButton(now);
    readRFID(now);
  }

  // 5. UPDATE ACTUATORS
  runServo(now);

  // 6. SYNC TO WEB
  if (client.connected()) {
    if (currentDoorState != oldDoorState || isHardLocked != oldLockState) {
      publishStatusToAll(true);
      Serial.println("[ĐỒNG BỘ WEB] Phát hiện thay đổi vật lý -> Báo cáo tức thời!");
    } else {
      publishStatusToAll(false);
    }
  }
}

// ================= DOOR CONTROL ACTIONS =================
void requestOpenDoor(String source) {
  if (currentDoorState == DOOR_CLOSED) {
    currentDoorState = DOOR_OPENING;
    updateOLED("OPENING DOOR", source);
    Serial.println("OPENING DOOR by: " + source);
  }
}

void requestCloseDoor(String source) {
  if (currentDoorState == DOOR_OPENED || currentDoorState == DOOR_EMERGENCY) {
    currentDoorState = DOOR_CLOSING;
    updateOLED("CLOSING DOOR", source);
    Serial.println("CLOSING DOOR by: " + source);
  }
}

void requestToggleDoor(String source) {
  if (currentDoorState == DOOR_CLOSED) {
    requestOpenDoor(source);
  } else if (currentDoorState == DOOR_OPENED || currentDoorState == DOOR_EMERGENCY) {
    requestCloseDoor(source);
  } else if (currentDoorState == DOOR_STOPPED) {
    if (currentDistance >= 10.0) {
      currentDoorState = DOOR_OPENING;
      updateOLED("RESUMING...", source);
      Serial.println("RESUME OPENING by: " + source);
    } else {
      Serial.println("CANNOT RESUME: Obstacle still present!");
    }
  }
}

void requestLock() {
  beep(80);
  if (currentDoorState == DOOR_CLOSED) {
    isHardLocked = true;
    digitalWrite(LED_LOCK_PIN, HIGH);
    Serial.println("SYSTEM LOCKED");
    updateOLED("SYSTEM LOCKED", "Inside Only");
  } else {
    isPendingLock = true;
    Serial.println("LOCK PENDING... CLOSING DOOR");
    updateOLED("LOCK PENDING", "Closing...");
    currentDoorState = DOOR_CLOSING;
  }
}

void requestUnlock() {
  isHardLocked = false;
  isPendingLock = false; 
  digitalWrite(LED_LOCK_PIN, LOW);
  beep(80);
  Serial.println("SYSTEM UNLOCKED");
  updateOLED("SYSTEM READY", doorStateToString(currentDoorState));
}

// ================= INPUT READING =================
void readToggleButton(unsigned long now) {
  bool currentToggleState = digitalRead(BTN_TOGGLE_PIN);
  
  if (currentToggleState == LOW && lastToggleState == HIGH && (now - lastToggleTime > 50)) {
    lastToggleTime = now;
    if (isLearningCard) {
      isLearningCard = false; 
      updateOLED("LEARN CANCELLED", doorStateToString(currentDoorState));
      beep(200);
    } else {
      requestToggleDoor("BUTTON"); 
    }
  }

  lastToggleState = currentToggleState;
}

void readLockSwitch(unsigned long now) {
  bool currentState = digitalRead(BTN_LOCK_PIN) == LOW; 
  
  if (currentState != lastLockSwitchState && (now - lastLockTime > 50)) {
    lastLockSwitchState = currentState;
    if (currentState) {
      lastLockTime = now;
      if (isLearningCard) {
        isLearningCard = false;
        Serial.println("LEARN MODE CANCELLED");
        updateOLED("LEARN CANCELLED", doorStateToString(currentDoorState));
        beep(200);
      } else {
        if (isHardLocked || isPendingLock) requestUnlock();
        else requestLock();
      }
    }
  }
}

void readRFID(unsigned long now) {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (now - lastRfidTime > 3000) {
      lastRfidTime = now;
      
      String scannedUID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        scannedUID += (rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        scannedUID += String(rfid.uid.uidByte[i], HEX);
        if (i != rfid.uid.size - 1) scannedUID += ":"; 
      }
      scannedUID.toUpperCase();

      Serial.println("-------------------------");
      Serial.print("MÃ THẺ: "); Serial.println(scannedUID);

      if (isLearningCard) {
         masterUID = scannedUID;
         preferences.putString("masterUID", masterUID);
         isLearningCard = false;
         Serial.println("-> LƯU THÀNH CÔNG THẺ CHÍNH MỚI: " + masterUID);
         beep(100, 100, 100, 100, 300);
         updateOLEDTemp("CARD SAVED!", "System Ready", 2000);
      } else {
        if (scannedUID == masterUID) {
          Serial.println("-> HỢP LỆ! Mở/Đóng cửa.");
          beep(80);
          requestToggleDoor("RFID");
        } else {
          Serial.println("-> SAI MÃ THẺ! Cảnh báo đột nhập.");
          beep(500);
          updateOLED("ACCESS DENIED", "Invalid Card!");
        }
      }
      Serial.println("-------------------------");
    }
    rfid.PICC_HaltA(); 
  }
}

// ================= SENSORS & ACTUATORS =================
void readSonar(unsigned long now) {
  if (now - lastSonarTime < 60) return;
  lastSonarTime = now;
  
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 3000);
  currentDistance = (duration == 0) ? 999.0 : (duration / 2.0) * 0.0343;
}

void safetyCheck() {
  // 1. CLOSING
  if (currentDoorState == DOOR_CLOSING && currentDistance < 10.0) {
    currentDoorState = DOOR_EMERGENCY;
    if (isPendingLock) {
      isPendingLock = false;
      Serial.println("LOCK CANCELLED DUE TO OBSTACLE");
    }
    beep(300);
    Serial.println("OBSTACLE DETECTED - EMERGENCY OPEN");
    updateOLED("OBSTACLE!", "EMERGENCY OPEN");
  }

  // 2. OPENING
  if (currentDoorState == DOOR_OPENING
      && currentAngle > ANGLE_SAFE_ZONE
      && currentDistance < 10.0) {
    currentDoorState = DOOR_STOPPED;
    beep(300);
    Serial.println("OBSTACLE DETECTED - DOOR STOPPED (not past safe zone)");
    updateOLED("OBSTACLE!", "Door STOPPED");
  }
}

//==== SERVO =========
void runServo(unsigned long now) {
  // 1. THIẾT LẬP TỐC ĐỘ
  unsigned long currentDelay = 15; 
  
  if (currentDoorState == DOOR_OPENING) {
    currentDelay = 30; 
  } else if (currentDoorState == DOOR_EMERGENCY) {
    currentDelay = 5;  
  }

  if (now - lastServoTime < currentDelay) return;
  lastServoTime = now;

  switch (currentDoorState) {

    case DOOR_OPENING:
      currentAngle -= 1; 
      if (currentAngle == ANGLE_OPENED) { 
        currentAngle = ANGLE_OPENED;
        currentDoorState = DOOR_OPENED;
        if (!isHardLocked)
          updateOLED("ACCESS GRANTED", "Door: OPENED");
      }
      doorServo.write(currentAngle);
      break;

    case DOOR_CLOSING:
      currentAngle += 1; 
      if (currentAngle == ANGLE_CLOSED) { 
        currentAngle = ANGLE_CLOSED;
        currentDoorState = DOOR_CLOSED;

        if (isPendingLock) {
          isPendingLock = false;
          isHardLocked = true;
          digitalWrite(LED_LOCK_PIN, HIGH);
          Serial.println("SYSTEM LOCKED");
          updateOLED("SYSTEM LOCKED", "Inside Only");
        } else if (!isHardLocked) {
          updateOLED("SYSTEM READY", "Door: CLOSED");
        }
      }
      doorServo.write(currentAngle);
      break;

    case DOOR_EMERGENCY:
      currentAngle -= 3;
      if (currentAngle <= ANGLE_OPENED) {
        currentAngle = ANGLE_OPENED;
        currentDoorState = DOOR_OPENED;
      }
      doorServo.write(currentAngle);
      break;

    case DOOR_STOPPED:
      doorServo.write(currentAngle);
      break;

    default:
      break;
  }
}

// ================= HARDWARE PERIPHERALS =================
void updateOLED(String line1, String line2) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 15);
  oled.println(line1);
  oled.setCursor(0, 40);
  oled.println(line2);
  oled.display();
}

void updateOLEDTemp(String line1, String line2, int duration) {
  updateOLED(line1, line2);
  oledRestoreTime = millis() + duration;
  isOledTemp = true;
}

void handleTasks(unsigned long now) {
  if (isOledTemp && now > oledRestoreTime) {
    isOledTemp = false;
      updateOLED("SYSTEM READY", doorStateToString(currentDoorState));
  }
  
  if (!isBuzzerPlaying) return;
  if (now - lastBuzzerToggleTime >= buzzerPattern[buzzerPatternIndex]) {
    lastBuzzerToggleTime = now;
    buzzerPatternIndex++;
    if (buzzerPatternIndex >= buzzerPatternLen) {
      isBuzzerPlaying = false;
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      digitalWrite(BUZZER_PIN, (buzzerPatternIndex % 2 == 0) ? HIGH : LOW);
    }
  }
}

void beep(int t1, int t2, int t3, int t4, int t5) {
  buzzerPattern[0] = t1; buzzerPattern[1] = t2; buzzerPattern[2] = t3; 
  buzzerPattern[3] = t4; buzzerPattern[4] = t5;
  buzzerPatternLen = 0;
  for (int i = 0; i < 5; i++) {
    if (buzzerPattern[i] > 0) buzzerPatternLen++;
    else break;
  }
  buzzerPatternIndex = 0;
  isBuzzerPlaying = true;
  lastBuzzerToggleTime = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

// ================= UTILITIES =================
String doorStateToString(DoorState state) {
  switch (state) {
    case DOOR_CLOSED:    return "Door: CLOSED";
    case DOOR_OPENED:    return "Door: OPENED";
    case DOOR_STOPPED:   return "Door: STOPPED";
    case DOOR_EMERGENCY: return "EMERGENCY!";
    default:             return "Moving...";
  }
}

String getWebDoorState(DoorState state) {
  switch (state) {
    case DOOR_CLOSED:    return "DOOR_CLOSED";
    case DOOR_OPENING:   return "DOOR_OPENING";
    case DOOR_OPENED:    return "DOOR_OPENED";
    case DOOR_CLOSING:   return "DOOR_CLOSING";
    case DOOR_STOPPED:   return "DOOR_STOPPED";
    case DOOR_EMERGENCY: return "DOOR_EMERGENCY";
    default:             return "DOOR_CLOSED";
  }
}

// ================= NETWORK & MQTT =================
void handleNetwork(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) {
    
    if (isHardLocked) {
      if (now - lastReconnectAttempt > 10000) {
        Serial.println("[WIFI] Reconnecting in background...");
        WiFi.disconnect();
        WiFi.reconnect();
        lastReconnectAttempt = now;
      }
    }
    
    return; 
  }
  if (!client.connected()) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnectMQTT(); 
    }
  } else {
    client.loop(); 
  }
}
    


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WARNING] WiFi failed! Offline mode.");
  }
}

bool reconnectMQTT() {
  Serial.print("Connecting MQTT... ");
  String clientId = "ESP32Garage-Minh-";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println(" SUCCESS!");
    client.subscribe(mqtt_topic_cmd);
    return true;
  } else {
    Serial.print(" FAILED, rc=");
    Serial.println(client.state());
    return false;
  }
}



void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  Serial.print("Web Command ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);

  if (String(topic) == mqtt_topic_cmd && !isHardLocked && !isPendingLock) {
    if (message == "OPEN") requestOpenDoor("Web Command");
    else if (message == "CLOSE") requestCloseDoor("Web Command");
    else if (message == "TOGGLE") requestToggleDoor("Web Command");
    else if (message.startsWith("LEARN_CARD:")) {

      String password = message.substring(11);
      if (password == "123456") {
        isLearningCard = true;
        learnModeStartTime = millis();
        beep(500);
        updateOLED("LEARN MODE", "Scan new card");
        Serial.println("LEARN MODE (Web): Xác thực hệ thống hợp lệ!");
      } else {
        beep(200, 100, 200);
        updateOLEDTemp("AUTH FAILED", "Wrong Password", 2000);
        Serial.println("AUTH FAILED (Web): Lệnh chặn vì Sai mật khẩu!");
      }
    }
  }
}



void publishStatusToWeb(bool force) {
  unsigned long now = millis();
  if (force || now - lastPublishTime > 500) {
    lastPublishTime = now;

    String payload = "{";
    payload += "\"door\":\"" + getWebDoorState(currentDoorState) + "\",";
    payload += "\"lock\":\"" + String(isHardLocked ? "LOCKED" : "UNLOCKED") + "\"";
    payload += "}";

    client.publish(mqtt_topic_status, payload.c_str());
  }
}


void publishStatusToAll(bool force) {
  publishStatusToWeb(force);

}
