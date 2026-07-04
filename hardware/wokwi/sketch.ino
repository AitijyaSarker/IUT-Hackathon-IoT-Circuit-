#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials for Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Broker settings
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* client_id = "esp32-smartoffice-ctrl";

WiFiClient espClient;
PubSubClient client(espClient);

// Device config: relay pin + individual switch pin
struct Device {
  const char* id;
  int relayPin;
  int switchPin;
  int lastSwitchState;
  bool relayState;
};

Device devices[] = {
  {"light1", 2,  27, HIGH, false},
  {"light2", 4,  26, HIGH, false},
  {"light3", 5,  25, HIGH, false},
  {"fan1",   12, 33, HIGH, false},
  {"fan2",   13, 32, HIGH, false}
};
const int numDevices = sizeof(devices) / sizeof(devices[0]);

// Master switch (turns ALL devices on/off)
const int masterSwitchPin = 15;
int lastMasterSwitchState = HIGH;

// PIR sensor
const int pirPin = 14;
int lastPirState = LOW;

const char* topicPrefix = "smartoffice/drawing/";

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
}

// Publish a single device's state
void publishDeviceState(int i) {
  String stateTopic = String(topicPrefix) + devices[i].id + "/state";
  client.publish(stateTopic.c_str(), devices[i].relayState ? "ON" : "OFF", true);
}

// Set ALL devices and publish master + individual states
void setAllDevices(bool state) {
  for (int i = 0; i < numDevices; i++) {
    devices[i].relayState = state;
    digitalWrite(devices[i].relayPin, state ? HIGH : LOW);
    publishDeviceState(i);
  }
  String masterTopic = String(topicPrefix) + "master/state";
  client.publish(masterTopic.c_str(), state ? "ON" : "OFF", true);
  Serial.print("MASTER -> All: ");
  Serial.println(state ? "ON" : "OFF");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("MQTT ["); Serial.print(topic); Serial.print("] "); Serial.println(msg);

  bool isOn  = (msg == "ON"  || msg == "1" || msg == "true");
  bool isOff = (msg == "OFF" || msg == "0" || msg == "false");
  if (!isOn && !isOff) return;

  // Master command from web app
  if (String(topic) == String(topicPrefix) + "master/set") {
    setAllDevices(isOn);
    return;
  }

  // Individual device command from web app
  for (int i = 0; i < numDevices; i++) {
    if (String(topic) == String(topicPrefix) + devices[i].id + "/set") {
      devices[i].relayState = isOn;
      digitalWrite(devices[i].relayPin, isOn ? HIGH : LOW);
      publishDeviceState(i);
      Serial.print("Device "); Serial.print(devices[i].id);
      Serial.print(" -> "); Serial.println(isOn ? "ON" : "OFF");
      break;
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(client_id)) {
      Serial.println("connected");
      // Subscribe to master/set
      String mt = String(topicPrefix) + "master/set";
      client.subscribe(mt.c_str());
      // Subscribe to each device's /set topic
      for (int i = 0; i < numDevices; i++) {
        String ct = String(topicPrefix) + devices[i].id + "/set";
        client.subscribe(ct.c_str());
        Serial.print("Subscribed: "); Serial.println(ct);
      }
    } else {
      Serial.print("failed, rc="); Serial.print(client.state());
      Serial.println(" retry in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Relay pins - all OFF at start
  for (int i = 0; i < numDevices; i++) {
    pinMode(devices[i].relayPin, OUTPUT);
    digitalWrite(devices[i].relayPin, LOW);
    // Individual switch - INPUT_PULLUP (slide switch connects to GND)
    pinMode(devices[i].switchPin, INPUT_PULLUP);
    devices[i].lastSwitchState = digitalRead(devices[i].switchPin);
  }

  // Master switch
  pinMode(masterSwitchPin, INPUT_PULLUP);
  lastMasterSwitchState = digitalRead(masterSwitchPin);

  // PIR
  pinMode(pirPin, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // ── Master switch ──────────────────────────────────────────
  int curMaster = digitalRead(masterSwitchPin);
  if (curMaster != lastMasterSwitchState) {
    delay(50);
    curMaster = digitalRead(masterSwitchPin);
    if (curMaster != lastMasterSwitchState) {
      lastMasterSwitchState = curMaster;
      bool on = (curMaster == LOW); // LOW = switch slid to ON (connected GND)
      setAllDevices(on);
    }
  }

  // ── Individual switches ────────────────────────────────────
  for (int i = 0; i < numDevices; i++) {
    int cur = digitalRead(devices[i].switchPin);
    if (cur != devices[i].lastSwitchState) {
      delay(50);
      cur = digitalRead(devices[i].switchPin);
      if (cur != devices[i].lastSwitchState) {
        devices[i].lastSwitchState = cur;
        bool on = (cur == LOW); // LOW = switch slid ON
        devices[i].relayState = on;
        digitalWrite(devices[i].relayPin, on ? HIGH : LOW);
        publishDeviceState(i);
        Serial.print("Individual switch ["); Serial.print(devices[i].id);
        Serial.print("] -> "); Serial.println(on ? "ON" : "OFF");
      }
    }
  }

  // ── PIR sensor ─────────────────────────────────────────────
  int curPir = digitalRead(pirPin);
  if (curPir != lastPirState) {
    lastPirState = curPir;
    String motionTopic = String(topicPrefix) + "motion/state";
    client.publish(motionTopic.c_str(), curPir == HIGH ? "true" : "false");
    Serial.println(curPir == HIGH ? "Motion detected!" : "Motion ended.");
  }
}
