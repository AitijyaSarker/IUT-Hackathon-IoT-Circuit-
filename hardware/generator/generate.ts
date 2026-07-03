import fs from "fs";

let components: any[] = [];
let wires: any[] = [];

// Clean title, no unnecessary long notes
components.push({
  type: "wokwi-text",
  id: "title",
  top: -50,
  left: 300,
  attrs: { text: "SMART OFFICE: 1 ROOM" }
});

// ESP32 placed on the left
components.push({
  type: "wokwi-esp32-devkit-v1",
  id: "esp32",
  top: 150,
  left: 0,
  attrs: { env: "micro", nickname: "Gateway" }
});

// GPIO Mapping Contract Box
components.push({
  type: "wokwi-text",
  id: "gpio_contract",
  top: 400,
  left: 0,
  attrs: {
    text: "--- GPIO MAPPING CONTRACT ---\n\nOUTPUTS (Relays):\nGPIO 2  -> Light 1\nGPIO 4  -> Light 2\nGPIO 5  -> Light 3\nGPIO 12 -> Fan 1\nGPIO 13 -> Fan 2\n\nINPUTS (Switches):\nGPIO 14 -> Switch 1\nGPIO 27 -> Switch 2\nGPIO 26 -> Switch 3\nGPIO 25 -> Switch 4\nGPIO 33 -> Switch 5"
  }
});

// 5 Relays (Outputs) and 5 Slide Switches (Inputs)
const relayPins = [2, 4, 5, 12, 13];
const switchPins = [14, 27, 26, 25, 33];

const devices = [
  { id: "light1", name: "LIGHT 1" },
  { id: "light2", name: "LIGHT 2" },
  { id: "light3", name: "LIGHT 3" },
  { id: "fan1",   name: "FAN 1" },
  { id: "fan2",   name: "FAN 2" }
];

let currentY = 0;

for (let i = 0; i < devices.length; i++) {
  const device = devices[i];
  const rPin = relayPins[i];
  const sPin = switchPins[i];
  
  const relayX = 300;
  const switchX = 500;

  // RELAY (The Load)
  components.push({
    type: "wokwi-relay-module",
    id: `relay_${device.id}`,
    top: currentY,
    left: relayX,
    attrs: {}
  });

  // Short, clean label for Relay with GPIO
  components.push({
    type: "wokwi-text",
    id: `txt_${device.id}`,
    top: currentY + 10,
    left: relayX + 130,
    attrs: { text: `${device.name}\n(GPIO ${rPin})` }
  });

  // SWITCH (Manual Wall Switch)
  components.push({
    type: "wokwi-slide-switch",
    id: `switch_${device.id}`,
    top: currentY + 20,
    left: switchX,
    attrs: {}
  });

  // WIRING for Relay
  wires.push([`esp32:D${rPin}`, `relay_${device.id}:IN`, "green"]);
  wires.push([`esp32:3V3`, `relay_${device.id}:VCC`, "red"]);
  wires.push([`esp32:GND.1`, `relay_${device.id}:GND`, "black"]);

  // WIRING for Switch
  // Middle pin of switch goes to ESP32 GPIO, Left pin goes to GND (using INPUT_PULLUP in code)
  wires.push([`esp32:D${sPin}`, `switch_${device.id}:2`, "blue"]);
  wires.push([`esp32:GND.2`, `switch_${device.id}:1`, "black"]);

  currentY += 120; // vertical spacing
}

const diagram = {
  version: 1,
  author: "auto-generator",
  parts: components,
  connections: wires
};

fs.writeFileSync(
  "../wokwi/diagram.json",
  JSON.stringify(diagram, null, 2)
);

console.log("diagram.json generated! (Clean layout with switches)");
