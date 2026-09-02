// /*
//  * ESP32 - Gui bien sn (random 1-10) len HiveMQ broker qua MQTT
//  * Thu vien can: knolleary/PubSubClient
//  * Them vao platformio.ini:
//  *   lib_deps =
//  *       knolleary/PubSubClient @ ^2.8
//  */

// #include <WiFi.h>
// #include <PubSubClient.h>
// #include <ArduinoJson.h>

// // ─── CAU HINH WiFi ───────────────────────────────────────────
// #define WIFI_SSID     "capheden"
// #define WIFI_PASSWORD "conheosuy"

// // ─── CAU HINH MQTT ───────────────────────────────────────────
// #define MQTT_SERVER   "broker.hivemq.com"
// #define MQTT_PORT     1883
// #define MQTT_TOPIC    "chamcong/esp32/sn"
// #define MQTT_CLIENT_ID "ESP32_ChamCong_001"  // phai unique tren broker

// // ─── KHOANG THOI GIAN GUI (ms) ───────────────────────────────
// #define SEND_INTERVAL 5000

// // ─── BIEN TOAN CUC ───────────────────────────────────────────
// int sn = 0;
// WiFiClient   espClient;
// PubSubClient mqttClient(espClient);

// // ─── HAM KET NOI WiFi ────────────────────────────────────────
// void connectWiFi() {
//   Serial.print("[WiFi] Dang ket noi: ");
//   Serial.println(WIFI_SSID);

//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//     attempts++;
//     if (attempts > 30) {
//       Serial.println("\n[WiFi] That bai! Khoi dong lai...");
//       ESP.restart();
//     }
//   }

//   Serial.println("\n[WiFi] Ket noi thanh cong!");
//   Serial.print("[WiFi] IP: ");
//   Serial.println(WiFi.localIP());
// }

// // ─── HAM KET NOI MQTT ────────────────────────────────────────
// void connectMQTT() {
//   while (!mqttClient.connected()) {
//     Serial.print("[MQTT] Dang ket noi broker...");

//     if (mqttClient.connect(MQTT_CLIENT_ID)) {
//       Serial.println(" Thanh cong!");
//     } else {
//       Serial.print(" That bai! rc=");
//       Serial.print(mqttClient.state());
//       Serial.println(" Thu lai sau 3 giay...");
//       delay(3000);
//     }
//   }
// }

// // ─── HAM PUBLISH DU LIEU ─────────────────────────────────────
// void publishData(int value) {
//   // JSON payload: {"id": "2"}
//   JsonDocument doc;
//   doc["id"] = String(value);
//   String payload;
//   serializeJson(doc, payload);

//   Serial.print("[MQTT] Publish -> topic: ");
//   Serial.print(MQTT_TOPIC);
//   Serial.print(" | payload: ");
//   Serial.println(payload);

//   if (mqttClient.publish(MQTT_TOPIC, payload.c_str())) {
//     Serial.println("[MQTT] Gui thanh cong!");
//   } else {
//     Serial.println("[MQTT] Gui that bai!");
//   }
// }

// // ─── SETUP ───────────────────────────────────────────────────
// void setup() {
//   Serial.begin(9600);
//   delay(1000);

//   Serial.println("=== ESP32 MQTT Cham Cong ===");

//   randomSeed(analogRead(36));

//   connectWiFi();

//   mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
//   mqttClient.setKeepAlive(60);
// }

// // ─── LOOP ────────────────────────────────────────────────────
// void loop() {
//   // Giu ket noi MQTT
//   if (!mqttClient.connected()) {
//     connectMQTT();
//   }
//   mqttClient.loop();

//   // Random sn tu 1 den 3 (khop voi nhan_vien trong Flask)
//   sn = random(1, 4);

//   Serial.print("\n[SN] Gia tri: ");
//   Serial.println(sn);

//   publishData(sn);

//   delay(SEND_INTERVAL);
// }