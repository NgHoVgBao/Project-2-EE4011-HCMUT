#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// RFID D5(SS), D18(SCK), D23(MOSI), D19(MISO), D4(RST)
// Vân tay D16(RX), D17(TX)
// Nút nhấn D26(RFID), D25(Fingerprint)
// LED đơn D27(Wait), D14(RFID), D12(Fingerprint)


/* ========== CAU HINH WIFI ========== */
const char* ssid = "capheden";
const char* pass = "conheosuy";

/* ========== CAU HINH MQTT ========== */
const char* MQTT_SERVER    = "10.34.42.214";
const int   MQTT_PORT      = 1883;
const char* MQTT_TOPIC     = "chamcong/esp32/sn";
const char* MQTT_CLIENT_ID = "ESP32_ChamCong_Main";

/* ========== CONFIG CHAN ========== */
#define SS_PIN     5
#define RST_PIN    4
#define BTN_RFID   26
#define BTN_FINGER 25

// ✅ LED pins
#define LED_WAIT   27
#define LED_RFID   14
#define LED_FINGER 12

/* ========== RFID UID MAPPING ========== */
uint8_t RFID_S1[4] = {0x2F, 0x87, 0xAB, 0xE6};
uint8_t RFID_S2[4] = {0x2A, 0x30, 0x4C, 0x1B};
uint8_t RFID_S3[4] = {0xED, 0xFE, 0xF0, 0x05};

/* ========== DEVICE ========== */
MFRC522 rfid(SS_PIN, RST_PIN);
HardwareSerial fingerSerial(2);
Adafruit_Fingerprint finger(&fingerSerial);

/* ========== MQTT CLIENT ========== */
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

/* ========== QUEUE ========== */
QueueHandle_t scanQueue;
QueueHandle_t mqttQueue;

enum Mode {
  MODE_RFID,
  MODE_FINGER
};

/* ========== ✅ HAM DIEU KHIEN LED ========== */
// Tắt hết 3 LED
void ledAllOff() {
  digitalWrite(LED_WAIT,   LOW);
  digitalWrite(LED_RFID,   LOW);
  digitalWrite(LED_FINGER, LOW);
}

// Chế độ chờ: chỉ LED Wait sáng
void ledSetWait() {
  ledAllOff();
  digitalWrite(LED_WAIT, HIGH);
}

// Chế độ quét RFID: chỉ LED RFID sáng
void ledSetRFID() {
  ledAllOff();
  digitalWrite(LED_RFID, HIGH);
}

// Chế độ quét vân tay: chỉ LED Finger sáng
void ledSetFinger() {
  ledAllOff();
  digitalWrite(LED_FINGER, HIGH);
}

/* ========== HAM KET NOI WIFI ========== */
bool wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.begin(ssid, pass);
  Serial.print("[WiFi] Connecting");

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    if (millis() - start > 10000) {
      Serial.println(" FAIL");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  Serial.println(" OK");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

/* ========== HAM KET NOI MQTT ========== */
bool mqttConnect() {
  if (mqttClient.connected()) return true;

  Serial.print("[MQTT] Connecting broker...");

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println(" OK!");
    return true;
  } else {
    Serial.print(" FAIL rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

/* ========== HAM GUI SN QUA MQTT ========== */
// ✅ Trả về true nếu gửi thành công
bool sendSn(int sn) {
  if (!wifiConnect()) {
    Serial.println("[MQTT] No WiFi, skip.");
    return false;
  }

  if (!mqttConnect()) {
    Serial.println("[MQTT] No broker, skip.");
    return false;
  }

  JsonDocument doc;
  doc["id"] = String(sn);
  String payload;
  serializeJson(doc, payload);

  Serial.print("[MQTT] Publish -> ");
  Serial.print(MQTT_TOPIC);
  Serial.print(" | ");
  Serial.println(payload);

  if (mqttClient.publish(MQTT_TOPIC, payload.c_str())) {
    Serial.println("[MQTT] Gui thanh cong!");
    mqttClient.loop();
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;  // ✅
  } else {
    Serial.println("[MQTT] Gui that bai!");
    return false; // ✅
  }
}

/* ========== ISR ========== */
void IRAM_ATTR isrRFID() {
  ets_printf("RFID Button Pressed\n");
  static uint32_t last = 0;
  uint32_t now = xTaskGetTickCountFromISR();

  if (now - last > pdMS_TO_TICKS(300)) {
    Mode m = MODE_RFID;
    xQueueSendFromISR(scanQueue, &m, NULL);
    last = now;
  }
}

void IRAM_ATTR isrFinger() {
  ets_printf("Finger Button Pressed\n");
  static uint32_t last = 0;
  uint32_t now = xTaskGetTickCountFromISR();

  if (now - last > pdMS_TO_TICKS(300)) {
    Mode m = MODE_FINGER;
    xQueueSendFromISR(scanQueue, &m, NULL);
    last = now;
  }
}

/* ========== MATCH FUNCTION ========== */
int matchRFID(uint8_t *uid) {
  if (memcmp(uid, RFID_S1, 4) == 0) return 1;
  if (memcmp(uid, RFID_S2, 4) == 0) return 2;
  if (memcmp(uid, RFID_S3, 4) == 0) return 3;
  return 0;
}

int matchFinger(uint8_t id) {
  if (id == 1) return 1;
  if (id == 2) return 2;
  if (id == 3) return 3;
  return 0;
}

/* ========== TASK SCAN ========== */
void taskScan(void *pv) {
  rfid.PCD_Init();
  Mode mode;

  while (1) {
    if (xQueueReceive(scanQueue, &mode, pdMS_TO_TICKS(100))) {
      int sn = 0;

      if (mode == MODE_RFID) {
        ledSetRFID(); // ✅ Chuyển LED sang RFID
        Serial.println("[SCAN] Waiting RFID...");

        uint32_t start = millis();
        while (millis() - start < 5000) {
          if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
            sn = matchRFID(rfid.uid.uidByte);
            Serial.printf("[SCAN] RFID matched sn=%d\n", sn);
            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(50));
        }

      } else {
        ledSetFinger(); // ✅ Chuyển LED sang Finger
        Serial.println("[SCAN] Waiting Finger...");

        uint32_t start = millis();
        while (millis() - start < 5000) {
          if (finger.getImage() == FINGERPRINT_OK &&
              finger.image2Tz() == FINGERPRINT_OK &&
              finger.fingerFastSearch() == FINGERPRINT_OK) {
            sn = matchFinger(finger.fingerID);
            Serial.printf("[SCAN] Finger matched sn=%d\n", sn);
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(50));
        }
      }

      if (sn > 0) {
        if (xQueueSend(mqttQueue, &sn, pdMS_TO_TICKS(2000)) != pdTRUE) {
          Serial.println("[SCAN] mqttQueue full, drop!");
          ledSetWait(); // ✅ Drop thì cũng về chờ
        }
        // LED chưa về Wait ở đây — chờ taskMQTT gửi xong mới về
      } else {
        Serial.println("[SCAN] No match / timeout");
        ledSetWait(); // ✅ Timeout không match → về chờ
      }
    }
  }
}

/* ========== TASK MQTT ========== */
void taskMQTT(void *pv) {
  int sn;

  while (1) {
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      mqttConnect();
    }
    mqttClient.loop();

    if (xQueueReceive(mqttQueue, &sn, pdMS_TO_TICKS(100))) {
      Serial.printf("[MQTT] Got sn=%d from queue\n", sn);
      bool ok = sendSn(sn);

      // ✅ Dù thành công hay thất bại → về chế độ chờ
      ledSetWait();

      if (!ok) {
        Serial.println("[MQTT] Send failed, LED back to wait anyway.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(9600);

  // ✅ Khởi tạo LED
  pinMode(LED_WAIT,   OUTPUT);
  pinMode(LED_RFID,   OUTPUT);
  pinMode(LED_FINGER, OUTPUT);
  ledSetWait(); // ✅ Mặc định LED chờ sáng ngay từ đầu

  // RFID
  SPI.begin();
  rfid.PCD_Init();

  // Fingerprint
  fingerSerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor OK");
  } else {
    Serial.println("Fingerprint sensor NOT FOUND");
  }

  // Buttons
  pinMode(BTN_RFID,   INPUT_PULLUP);
  pinMode(BTN_FINGER, INPUT_PULLUP);

  // Queue
  scanQueue = xQueueCreate(10, sizeof(Mode));
  mqttQueue = xQueueCreate(10, sizeof(int));

  // Interrupts
  attachInterrupt(BTN_RFID,   isrRFID,   FALLING);
  attachInterrupt(BTN_FINGER, isrFinger, FALLING);

  // WiFi + MQTT
  wifiConnect();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  mqttConnect();

  // Tasks
  xTaskCreatePinnedToCore(taskScan, "main", 8192,  NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskMQTT, "mqtt", 16384, NULL, 1, NULL, 0);

  Serial.println("System Ready!");
}

void loop() {
  vTaskDelete(NULL);  
}