# Lightroom Controller

A hardware controller for **Adobe Lightroom Classic** built on a Raspberry Pi 5.
It combines a touchscreen UI, physical rotary encoders, and USB MIDI to give you
hands-on control of Lightroom's develop parameters via the
[MIDI2LR](https://github.com/rsjaffe/MIDI2LR) plugin.

---

## Hardware

| Component | Details |
|---|---|
| **Raspberry Pi 5** | Runs Pi OS Lite Bookworm 64-bit |
| **Official 7" DSI Touchscreen** | 800×480, connected via DSI ribbon cable |
| **2× Adafruit Quad Rotary Encoder Breakout** | [#4991](https://www.adafruit.com/product/4991), seesaw ATSAMD09 over I²C |
| **USB-C cable** | Pi acts as USB MIDI class device to Mac |
| **Power** | 5V via GPIO pins 1/9, freeing USB-C for MIDI |

### Wiring

```
Pi GPIO Header
──────────────────────────────────────
Pin 1  (3.3V)  →  VIN  (both encoder boards)
Pin 9  (GND)   →  GND  (both encoder boards)
Pin 31 (GPIO6) →  SDA  (i2c3-pi5 overlay)
Pin 26 (GPIO7) →  SCL  (i2c3-pi5 overlay)

Encoder board addresses:
  Board 0 → 0x49  (default)
  Board 1 → 0x4A  (A0 jumper closed)
```

---

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Raspberry Pi 5                      │
│                                                      │
│  ┌──────────────┐      ┌───────────────────────────┐ │
│  │  Qt/QML UI   │      │     EncoderWorker         │ │
│  │  Touchscreen │      │  polls seesaw via I²C     │ │
│  │  800×480     │      │  8 encoders + 8 buttons   │ │
│  └──────┬───────┘      └────────────┬──────────────┘ │
│         │  signals/slots            │                 │
│         └──────────────┬────────────┘                 │
│                   ┌────┴──────────┐                   │
│                   │  MidiWorker   │                   │
│                   │  RtMidi       │                   │
│                   │  ALSA hw:2,0  │                   │
│                   └────┬──────────┘                   │
└────────────────────────┼────────────────────────────-─┘
                         │ USB-C  (MIDI class device)
┌────────────────────────┼─────────────────────────────┐
│              Mac       │                             │
│                   ┌────┴──────────┐                  │
│                   │    MIDI2LR    │                  │
│                   │    plugin     │                  │
│                   └────┬──────────┘                  │
│                        │                             │
│                   ┌────┴──────────┐                  │
│                   │   Lightroom   │                  │
│                   │   Classic     │                  │
│                   └───────────────┘                  │
└──────────────────────────────────────────────────────┘
```

---

## MIDI Protocol

### Encoders → Lightroom (NRPN)
Each encoder sends four CC messages forming an NRPN:

```
CC 99  NRPN parameter MSB
CC 98  NRPN parameter LSB   →  parameter = 211 + encoderId
CC  6  Data entry MSB
CC 38  Data entry LSB        →  value range 0..16383
```

MIDI channel: **11**

### Buttons → Lightroom (Note On/Off)
```
Note On  velocity 127  →  button pressed
Note Off velocity   0  →  button released
```

### Lightroom → Pi (MIDI feedback)
MIDI2LR sends NRPN back to the Pi when a parameter changes in Lightroom.
The Pi display updates to reflect the current Lightroom value.

### Encoder acceleration
Turn speed controls how fast values change:

| Speed | Interval | Factor |
|---|---|---|
| Slow  | ≥ 120ms | 2 |
| Medium | ≥ 60ms | 32 |
| Fast | ≥ 30ms | 64 |
| Very fast | < 30ms | 128 |

---

## UI Layout

```
┌──────────────────────────────────────────────────────┐
│  [Config 0][Config 1][Config 2][       ][       ]    │  row 0 — config selectors
│  [       ][       ][       ][       ][       ]       │  row 1 — config selectors
│  [ Action ][ Action][ Action][ Action][ Action]      │  row 2
│  [ Action ][ Action][ Action][ Action][ Action]      │  row 3
│  [ Action ][ Action][ Action][ Action][ Action]      │  row 4
│  [ Action ][ Action][ Action][ Action][ Action]      │  row 5
├──────────────────────────────────────────────────────┤
│  [Exposure][Contrast][Highl.][Shadow][Whites ]        │
│  [  8192  ][ 8192  ][ 8192 ][ 8192 ][ 8192  ]        │  encoder values
│  [Blacks  ][Clarity][Vibran][Temp. ]                  │
└──────────────────────────────────────────────────────┘
```

- **Rows 0–1** — config selector buttons (tap to switch profile)
- **Rows 2–5** — action buttons (send MIDI note to Lightroom)
- **Bottom bar** — 8 encoders showing label and current value

---

## Config Files

Configs are JSON files served from the Mac via HTTP.
The Pi fetches all configs at startup.

### File locations on Mac

```
~/lightroom-configs/
    index.json          ←  list of available configs
    develop-basic.json
    develop-hsl.json
    develop-detail.json
```

### Start the HTTP server on Mac

```bash
cd ~/lightroom-configs
python3 -m http.server 8080
```

### index.json format

```json
{
    "configs": [
        "develop-basic.json",
        "develop-hsl.json",
        "develop-detail.json"
    ]
}
```

### Config file format

```json
{
    "name": "Develop Basic",
    "buttons": [
        {
            "row": 2, "col": 0,
            "label": "Reset All",
            "color": "#922b21",
            "note": 36,
            "command": "ResetAll"
        }
    ],
    "encoders": [
        {
            "id": 0,
            "label": "Exposure",
            "color": "#2a2a2a",
            "cc": 211,
            "note": 24,
            "command": "Exposure"
        }
    ]
}
```

---

## Workflow — Creating a New Config

### Step 1 — Assign commands in MIDI2LR

1. Open Lightroom Classic with MIDI2LR running
2. Touch an encoder or press a button on the controller
3. MIDI2LR highlights the control — pick a command from the list
4. Repeat for all encoders and buttons
5. In MIDI2LR: **File → Save Profile** → save to:
   ```
   /Users/<you>/_Midi2Lr_Project/midi2lrfolder/MyProfile.xml
   ```

### Step 2 — Generate Pi config with the editor

```bash
python3 midi2lr_editor.py
```

1. Select your profile from the **Profile** dropdown
2. The editor reads the XML and pre-fills all commands
3. **Rows 0–1** are reserved for config selectors — leave them empty
4. **Rows 2–5** show action buttons pre-filled with command names
5. Customize labels and colors as needed
6. Set encoder labels and colors
7. Click **💾 Save Pi JSON**
8. The editor saves to `~/lightroom-configs/` and updates `index.json`

### Step 3 — Reload on Pi

The Pi fetches configs at startup. Restart the app to pick up new configs:

```bash
sudo systemctl restart lightroom-ctrl
# or during development:
QT_QPA_EGLFS_HIDECURSOR=1 ./build/lightroom-ctrl -platform eglfs
```

---

## Building the Qt App

### Requirements

```bash
sudo apt install -y qt6-base-dev qt6-declarative-dev \
                    qml6-module-qtquick qml6-module-qtquick-controls \
                    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
                    librtmidi-dev
```

### Build

```bash
cd ~/lightroom-ctrl
qmake6 lightroom-ctrl.pro
make -j4
```

Binary output: `build/lightroom-ctrl`

### Run

```bash
QT_QPA_EGLFS_HIDECURSOR=1 ./build/lightroom-ctrl -platform eglfs
```

---

## Auto-start on Boot

```bash
sudo nano /etc/systemd/system/lightroom-ctrl.service
```

```ini
[Unit]
Description=Lightroom Controller
After=network.target midi-gadget.service

[Service]
User=jam
Environment=QT_QPA_PLATFORM=eglfs
Environment=QT_QPA_EGLFS_HIDECURSOR=1
ExecStart=/home/jam/lightroom-ctrl/build/lightroom-ctrl
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable lightroom-ctrl
sudo systemctl start lightroom-ctrl
```

---

## Project Structure

```
~/lightroom-ctrl/
├── README.md
├── CLAUDE.md                  ←  AI assistant context
├── lightroom-ctrl.pro         ←  qmake project file
├── main.cpp                   ←  Qt app entry point
├── main.qml                   ←  QML UI
├── EncoderWorker.h/.cpp       ←  seesaw polling thread
├── MidiWorker.h/.cpp          ←  RtMidi send/receive thread
├── ConfigManager.h/.cpp       ←  HTTP config loader
├── seesaw.h/.cpp              ←  C++ seesaw I²C driver
└── build/                     ←  compiled output (git ignored)

~/lightroom-configs/           ←  config files served to Pi
├── index.json
├── develop-basic.json
└── ...

midi2lr_editor.py              ←  Mac config editor (PyQt6)
```

---

## Dependencies

| Dependency | Purpose |
|---|---|
| Qt 6.4+ | UI framework |
| RtMidi | MIDI send/receive |
| Adafruit seesaw (C++) | Custom I²C driver for encoders |
| MIDI2LR plugin | Lightroom ↔ MIDI bridge |
| PyQt6 | Mac config editor |
| python3 http.server | Serve config files to Pi |

---

## Known Issues / Notes

- **Cursor errors on DSI display** — `Failed to move cursor on screen DSI2` messages are harmless. Suppressed with `QT_QPA_EGLFS_HIDECURSOR=1`
- **I²C bus** — Pi 5 requires `i2c3-pi5` overlay on GPIO6/7. Default I²C bus (GPIO2/3) has compatibility issues with seesaw on Pi 5
- **USB MIDI gadget** — uses `dwc2` overlay in `dr_mode=peripheral`. Pi must be powered via GPIO pins when USB-C is used for MIDI
- **MIDI feedback guard** — a per-encoder atomic flag prevents feedback loops when Lightroom sends position updates back to the Pi