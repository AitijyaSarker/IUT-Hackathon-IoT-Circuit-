# 🏢 Smart Office – Hardware Simulation Module

> This is the **hardware layer** of the Smart Office system. It simulates an ESP32 microcontroller in [Wokwi](https://wokwi.com), communicating with your backend via **MQTT** in real-time.

---

## 📁 Project Structure

```
hardware/
├── wokwi/
│   ├── diagram.json       ← Wokwi circuit schematic (ESP32 + Relays + Switches + PIR)
│   ├── sketch.ino         ← ESP32 Arduino firmware (WiFi + MQTT logic)
│   ├── libraries.txt      ← Wokwi library dependency (PubSubClient)
│   └── wokwi.toml         ← VS Code Wokwi extension config
└── generator/
    ├── generate.ts        ← TypeScript script that generated diagram.json
    ├── officeConfig.ts    ← Room/device configuration
    └── package.json
```

---

## ⚡ Quick Start: Run the Simulation

### Method 1: Wokwi.com (Browser) — Recommended for Demos

1. Go to **[wokwi.com](https://wokwi.com/projects/new/esp32)**
2. In the `sketch.ino` tab → paste the contents of `hardware/wokwi/sketch.ino`
3. In the `diagram.json` tab → paste the contents of `hardware/wokwi/diagram.json`
4. Click **Library Manager** tab → click `+` → search **`PubSubClient`** (by Nick O'Leary) → add it
5. Press the green **▶ Play** button

The ESP32 will connect to `Wokwi-GUEST` WiFi, then connect to the MQTT broker automatically.

### Method 2: VS Code with Wokwi Extension

1. Install the [Wokwi VS Code Extension](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)
2. Open this project folder in VS Code
3. Press `F1` → type `Wokwi: Start Simulator` → Enter

> **Note:** VS Code simulation requires a pre-compiled `.elf` binary. For direct browser simulation (Method 1), no compilation step is needed.

---

## 🔌 Hardware Layout (Drawing Room)

The circuit simulates **1 room (Drawing Room)** with:

| Component | Count | Notes |
|-----------|-------|-------|
| ESP32 DevKit V1 | 1 | The main gateway/controller |
| Relay Modules | 5 | Controls Lights and Fans |
| Slide Switches | 5 | Simulates physical wall switches |
| PIR Motion Sensor | 1 | Detects occupancy (GPIO 14) |

### GPIO Mapping Contract

| GPIO | Role | Direction |
|------|------|-----------|
| `GPIO 2` | Relay → Light 1 | OUTPUT |
| `GPIO 4` | Relay → Light 2 | OUTPUT |
| `GPIO 5` | Relay → Light 3 | OUTPUT |
| `GPIO 12` | Relay → Fan 1 | OUTPUT |
| `GPIO 13` | Relay → Fan 2 | OUTPUT |
| `GPIO 15` | Switch → Light 1 | INPUT_PULLUP |
| `GPIO 27` | Switch → Light 2 | INPUT_PULLUP |
| `GPIO 26` | Switch → Light 3 | INPUT_PULLUP |
| `GPIO 25` | Switch → Fan 1 | INPUT_PULLUP |
| `GPIO 33` | Switch → Fan 2 | INPUT_PULLUP |
| `GPIO 14` | PIR Motion Sensor OUT | INPUT |

---

## 📡 MQTT Integration (For Backend Developers)

### Broker Configuration

| Setting | Value |
|---------|-------|
| **Broker** | `broker.hivemq.com` |
| **Port** | `1883` |
| **Auth** | None (public, for development) |
| **Client ID** | `esp32-smartoffice-client` |

> For production, swap to **EMQX Cloud** or **HiveMQ Cloud** with auth credentials. Update `mqtt_server`, `mqtt_port`, and `client.connect(client_id, "user", "pass")` in `sketch.ino`.

---

### Topic Reference Table

**Format:** `smartoffice/drawing/<device>/<action>`

#### 🔻 Command Topics (Backend → ESP32)
Your backend publishes here to control devices.

| Topic | Payload | Effect |
|-------|---------|--------|
| `smartoffice/drawing/light1/set` | `ON` / `OFF` | Toggle Light 1 relay |
| `smartoffice/drawing/light2/set` | `ON` / `OFF` | Toggle Light 2 relay |
| `smartoffice/drawing/light3/set` | `ON` / `OFF` | Toggle Light 3 relay |
| `smartoffice/drawing/fan1/set` | `ON` / `OFF` | Toggle Fan 1 relay |
| `smartoffice/drawing/fan2/set` | `ON` / `OFF` | Toggle Fan 2 relay |

**Accepted Payloads:** `"ON"`, `"1"`, `"true"` → turns ON. `"OFF"`, `"0"`, `"false"` → turns OFF.

#### 🔺 State Topics (ESP32 → Backend)
The ESP32 publishes here automatically when state changes. Your backend should subscribe to these.

| Topic | Payload | Trigger |
|-------|---------|---------|
| `smartoffice/drawing/light1/state` | `ON` / `OFF` | Relay state changed |
| `smartoffice/drawing/light2/state` | `ON` / `OFF` | Relay state changed |
| `smartoffice/drawing/light3/state` | `ON` / `OFF` | Relay state changed |
| `smartoffice/drawing/fan1/state` | `ON` / `OFF` | Relay state changed |
| `smartoffice/drawing/fan2/state` | `ON` / `OFF` | Relay state changed |
| `smartoffice/drawing/motion/state` | `true` / `false` | PIR sensor triggered |

**Subscribe pattern (wildcard):** `smartoffice/drawing/#` captures everything from this room.

---

## 🗄️ Backend Integration Guide (Node.js / mqtt.js)

### Step 1: Connect to MQTT Broker

```typescript
import mqtt from 'mqtt';

const mqttClient = mqtt.connect('mqtt://broker.hivemq.com:1883');

mqttClient.on('connect', () => {
  console.log('✅ Connected to MQTT broker');
  
  // Subscribe to all state updates from the drawing room
  mqttClient.subscribe('smartoffice/drawing/#');
});
```

### Step 2: Handle Incoming Messages (State Updates)

```typescript
mqttClient.on('message', async (topic: string, message: Buffer) => {
  const payload = message.toString();
  console.log(`📩 ${topic} → ${payload}`);

  // Parse topic: smartoffice/drawing/<device>/<action>
  const parts = topic.split('/');
  const room = parts[1];   // "drawing"
  const device = parts[2]; // "light1", "fan1", "motion"
  const action = parts[3]; // "state"

  if (action === 'state') {
    if (device === 'motion') {
      // Update occupancy in MongoDB
      await OccupancyModel.findOneAndUpdate(
        { room },
        { motion: payload === 'true', lastMotionAt: new Date() },
        { upsert: true }
      );

      // Broadcast to React frontend via Socket.IO
      io.emit('occupancy_update', { room, occupied: payload === 'true' });

    } else {
      // Update device state in MongoDB
      await DeviceModel.findOneAndUpdate(
        { room, deviceId: device },
        { state: payload, updatedAt: new Date() },
        { upsert: true }
      );

      // Broadcast to React frontend via Socket.IO
      io.emit('device_update', { room, device, state: payload });
    }
  }
});
```

### Step 3: Control Devices from Your API

```typescript
// POST /api/devices/control
router.post('/control', async (req, res) => {
  const { room, device, state } = req.body;
  // room = "drawing", device = "light1", state = "ON"

  const topic = `smartoffice/${room}/${device}/set`;
  mqttClient.publish(topic, state);

  res.json({ success: true, topic, payload: state });
});
```

### Step 4: React Frontend (Socket.IO Client)

```typescript
// In your React component
import { useEffect } from 'react';
import { io } from 'socket.io-client';

const socket = io('http://localhost:5000');

useEffect(() => {
  socket.on('device_update', ({ room, device, state }) => {
    // Update your Zustand store / TanStack Query cache
    updateDeviceState(room, device, state);
  });

  socket.on('occupancy_update', ({ room, occupied }) => {
    updateOccupancy(room, occupied);
  });

  return () => socket.off('device_update').off('occupancy_update');
}, []);
```

---

## 🧠 Smart Energy Management (Occupancy-Aware Alerts)

This is the **key differentiator** of the system. Beyond simple ON/OFF monitoring, the backend can detect **energy waste in real-time**.

### Rule Engine Logic

```typescript
// node-cron job — runs every minute
cron.schedule('* * * * *', async () => {
  const rooms = await OccupancyModel.find({ motion: false });

  for (const room of rooms) {
    const minutesSinceMotion =
      (Date.now() - room.lastMotionAt.getTime()) / 60000;

    if (minutesSinceMotion >= 15) {
      const activeDevices = await DeviceModel.find({
        room: room.room,
        state: 'ON'
      });

      if (activeDevices.length > 0) {
        const lightsOn = activeDevices.filter(d => d.deviceId.startsWith('light')).length;
        const fansOn = activeDevices.filter(d => d.deviceId.startsWith('fan')).length;
        const confidence = Math.min(96, 70 + minutesSinceMotion * 1.5);

        // Send Discord alert
        await sendDiscordAlert({
          title: '⚠️ Possible Energy Waste',
          room: room.room,
          lightsOn,
          fansOn,
          minutesEmpty: Math.round(minutesSinceMotion),
          confidence: Math.round(confidence)
        });

        // Broadcast to dashboard
        io.emit('energy_waste_alert', {
          room: room.room,
          lightsOn,
          fansOn,
          minutesEmpty: Math.round(minutesSinceMotion),
          confidence: Math.round(confidence)
        });
      }
    }
  }
});
```

### Discord Bot Alert Format

```
⚠️ Possible Energy Waste (Confidence: 96%)
Room: Drawing Room

Reason:
• No motion detected for 15 minutes
• 2 Lights are ON
• 1 Fan is ON
• Office hours are active
```

### Dashboard Card Design

```
┌──────────────────────────────┐
│  🏠 Drawing Room              │
├──────────────────────────────┤
│  Occupancy   🔴 Empty         │
│  Last Motion  18 min ago      │
│  Lights ON    2 / 3           │
│  Fans ON      1 / 2           │
├──────────────────────────────┤
│  ⚠️ Energy Waste Alert        │
│  Confidence: 96%             │
└──────────────────────────────┘
```

---

## 🖥️ How to Interact with the Simulation (Wokwi)

### Turning Lights/Fans ON manually
1. In the Wokwi simulator, locate the **blue Slide Switches** next to each relay.
2. **Click a switch** to toggle it. The relay's LED will light up = device is ON.
3. The ESP32 will automatically publish the new state to MQTT.

### Testing the PIR Sensor
1. In the Wokwi simulator, click on the **PIR Motion Sensor** component.
2. A control slider will appear — drag it to **simulate motion**.
3. Check the Serial Monitor — you'll see `Motion detected!` printed.
4. Your backend will receive `true` on `smartoffice/drawing/motion/state`.

### Controlling devices from the Backend
Use **MQTT Explorer** ([download free](https://mqtt-explorer.com/)) to test manually:
1. Connect to `broker.hivemq.com:1883`
2. Publish `ON` to `smartoffice/drawing/light1/set`
3. Watch the relay LED in Wokwi turn on instantly!

---

## 🔄 Full System Data Flow

```
[Wokwi Simulator]
     │
     │  WiFi: Wokwi-GUEST
     ▼
[MQTT Broker: broker.hivemq.com:1883]
     │
     │  Topics: smartoffice/drawing/#
     ▼
[Node.js Backend]
     ├──► MongoDB Atlas  (persist device states & occupancy)
     ├──► Socket.IO      (broadcast real-time to React UI)
     └──► Discord Bot    (send energy waste alerts)
          │
          ▼
[React + Vite Frontend]
     (Dashboard cards, device toggles, occupancy status)
```

---

## ⚙️ Customization

### Changing the MQTT Broker (e.g., to EMQX Cloud)
In `hardware/wokwi/sketch.ino`, update:
```cpp
const char* mqtt_server = "your-emqx-endpoint.emqx.cloud";
const int mqtt_port = 1883; // or 8883 for TLS
const char* mqtt_user = "your_username";
const char* mqtt_pass = "your_password";
// And in reconnect():
client.connect(client_id, mqtt_user, mqtt_pass);
```

### Adding More Rooms
Edit `hardware/generator/officeConfig.ts`:
```typescript
export const office = {
  rooms: [
    { id: "drawing", lights: 3, fans: 2 },
    { id: "work1",   lights: 3, fans: 2 }, // Add more rooms here
    { id: "work2",   lights: 3, fans: 2 }
  ]
};
```
Then run the generator:
```bash
cd hardware/generator
npm start
# This regenerates hardware/wokwi/diagram.json
```

---

*Smart Office Hardware Module — IUT Hackathon 2026*
