#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials for Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Broker settings
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* client_id = "esp32-smartoffice-client";

WiFiClient espClient;
PubSubClient client(espClient);

// Device configuration
struct Device {
  const char* id;
  int relayPin;
  int switchPin;
  int lastSwitchState;
  bool relayState;
};

Device devices[] = {
  {"light1", 2, 15, HIGH, false},
  {"light2", 4, 27, HIGH, false},
  {"light3", 5, 26, HIGH, false},
  {"fan1", 12, 25, HIGH, false},
  {"fan2", 13, 33, HIGH, false}
};
const int numDevices = sizeof(devices) / sizeof(devices[0]);

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

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.println(msg);

  // Check if topic is a command for one of our devices
  for (int i = 0; i < numDevices; i++) {
    String commandTopic = String(topicPrefix) + devices[i].id + "/set";
    if (String(topic) == commandTopic) {
      if (msg == "ON" || msg == "1" || msg == "true") {
        devices[i].relayState = true;
        digitalWrite(devices[i].relayPin, HIGH);
      } else if (msg == "OFF" || msg == "0" || msg == "false") {
        devices[i].relayState = false;
        digitalWrite(devices[i].relayPin, LOW);
      }
      
      // Publish new state
      String stateTopic = String(topicPrefix) + devices[i].id + "/state";
      client.publish(stateTopic.c_str(), devices[i].relayState ? "ON" : "OFF");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id)) {
      Serial.println("connected");
      // Subscribe to command topics
      for (int i = 0; i < numDevices; i++) {
        String commandTopic = String(topicPrefix) + devices[i].id + "/set";
        client.subscribe(commandTopic.c_str());
        Serial.print("Subscribed to ");
        Serial.println(commandTopic);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize pins
  for (int i = 0; i < numDevices; i++) {
    pinMode(devices[i].relayPin, OUTPUT);
    digitalWrite(devices[i].relayPin, LOW); // Initial state OFF
    
    // Wokwi slide switches connect to GND, so use INPUT_PULLUP
    pinMode(devices[i].switchPin, INPUT_PULLUP);
    devices[i].lastSwitchState = digitalRead(devices[i].switchPin);
  }
  
  pinMode(pirPin, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Check for physical switch toggles
  for (int i = 0; i < numDevices; i++) {
    int currentSwitchState = digitalRead(devices[i].switchPin);
    
    // If switch state changed
    if (currentSwitchState != devices[i].lastSwitchState) {
      delay(50); // Simple debounce
      currentSwitchState = digitalRead(devices[i].switchPin);
      
      if (currentSwitchState != devices[i].lastSwitchState) {
        devices[i].lastSwitchState = currentSwitchState;
        
        // Toggle the relay state
        devices[i].relayState = !devices[i].relayState;
        digitalWrite(devices[i].relayPin, devices[i].relayState ? HIGH : LOW);
        
        // Publish new state via MQTT
        String stateTopic = String(topicPrefix) + devices[i].id + "/state";
        client.publish(stateTopic.c_str(), devices[i].relayState ? "ON" : "OFF");
        
        Serial.print("Switch toggled manually: ");
        Serial.print(devices[i].id);
        Serial.print(" -> ");
        Serial.println(devices[i].relayState ? "ON" : "OFF");
      }
    }
  }

  // Check PIR sensor
  int currentPirState = digitalRead(pirPin);
  if (currentPirState != lastPirState) {
    lastPirState = currentPirState;
    String motionTopic = String(topicPrefix) + "motion/state";
    if (currentPirState == HIGH) {
      client.publish(motionTopic.c_str(), "true");
      Serial.println("Motion detected!");
    } else {
      client.publish(motionTopic.c_str(), "false");
      Serial.println("Motion ended.");
    }
  }
}
