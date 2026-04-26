# Pi 5 Automation Setup

Two services run automatically at boot on the Pi 5:

1. **midi-gadget** — configures the USB MIDI device (runs first)
2. **lightroom-ctrl** — starts the Qt controller app (runs after MIDI is ready)

---

## 1. USB MIDI Gadget Service

Configures the Pi USB-C port as a USB MIDI class device.
The Mac sees it as **LightroomCtrl** in Audio MIDI Setup and MIDI2LR.

### Gadget script

```bash
sudo nano /usr/local/bin/midi-gadget.sh
```

```bash
#!/bin/bash

GADGET_DIR=/sys/kernel/config/usb_gadget/midi_gadget

modprobe libcomposite

mkdir -p $GADGET_DIR
cd $GADGET_DIR

echo 0x1d6b > idVendor
echo 0x0104 > idProduct
echo 0x0100 > bcdDevice
echo 0x0200 > bcdUSB

mkdir -p strings/0x409
echo "fedcba9876543210" > strings/0x409/serialnumber
echo "RaspberryPi"      > strings/0x409/manufacturer
echo "LightroomCtrl"    > strings/0x409/product

mkdir -p configs/c.1/strings/0x409
echo "MIDI Gadget" > configs/c.1/strings/0x409/configuration
echo 120           > configs/c.1/MaxPower

mkdir -p functions/midi.usb0
echo "LightroomCtrl" > functions/midi.usb0/id
echo "RPi MIDI"      > functions/midi.usb0/subname

ln -s functions/midi.usb0 configs/c.1/

ls /sys/class/udc > UDC

echo "MIDI gadget activated"
```

Make it executable:
```bash
sudo chmod +x /usr/local/bin/midi-gadget.sh
```

### Systemd service

```bash
sudo nano /etc/systemd/system/midi-gadget.service
```

```ini
[Unit]
Description=USB MIDI Gadget
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/midi-gadget.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target

### Enable network-online target

The service waits for the network to be fully ready before starting.
Enable the appropriate wait-online service:

```bash
# Check which is available
systemctl list-units | grep wait-online

# Enable it (use whichever is available)
sudo systemctl enable NetworkManager-wait-online.service
# or
sudo systemctl enable systemd-networkd-wait-online.service
```

### Config retry logic

`ConfigManager` retries fetching configs every 5 seconds if the Mac
server is unreachable at startup. This handles Pi booting before the
Mac HTTP server is ready, temporary network issues, or Mac waking from sleep.
```

Enable and start:
```bash
sudo systemctl enable midi-gadget
sudo systemctl start midi-gadget
```

Verify:
```bash
sudo systemctl status midi-gadget
amidi -l   # Should show hw:2,0 or similar
```

### Required config.txt entries

Verify these are in `/boot/firmware/config.txt`:

```
dtoverlay=dwc2,dr_mode=peripheral
dtparam=i2c_arm=on
dtoverlay=i2c3-pi5
```

---

## 2. Lightroom Controller Qt App

Starts the Qt/QML touchscreen application after the MIDI gadget is ready.

### Systemd service

```bash
sudo nano /etc/systemd/system/lightroom-ctrl.service
```

```ini
[Unit]
Description=Lightroom Controller
After=network-online.target midi-gadget.service
Wants=network-online.target

[Service]
User=jam
WorkingDirectory=/home/jam/lightroom-ctrl
Environment=QT_QPA_PLATFORM=eglfs
Environment=QT_QPA_EGLFS_HIDECURSOR=1
ExecStart=/home/jam/lightroom-ctrl/build/lightroom-ctrl
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target

### Enable network-online target

The service waits for the network to be fully ready before starting.
Enable the appropriate wait-online service:

```bash
# Check which is available
systemctl list-units | grep wait-online

# Enable it (use whichever is available)
sudo systemctl enable NetworkManager-wait-online.service
# or
sudo systemctl enable systemd-networkd-wait-online.service
```

### Display rotation (DSI-2, 720×1280 portrait)

The DSI display is physically portrait (720×1280). The UI is designed for landscape (800×480). Rotation is handled in QML — Pi 5's DRM driver does not support KMS plane rotation so hardware approaches have no effect. Rotation is done via a `Rotation` + `Scale` transform in `main.qml`, wrapping all UI content:

```qml
Item {
    width: 800; height: 480
    x: 0; y: 1240
    transform: [
        Rotation { angle: 270; origin.x: 0; origin.y: 0 },
        Scale { xScale: 1.5; yScale: 1.5; origin.x: 0; origin.y: 0 }
    ]
}
```

Scale 1.5 = 720 ÷ 480, filling the screen width exactly (720×1200, 40 px margin top/bottom). Qt maps touch coordinates through the transform automatically.

### Config retry logic

`ConfigManager` retries fetching configs every 5 seconds if the Mac
server is unreachable at startup. This handles Pi booting before the
Mac HTTP server is ready, temporary network issues, or Mac waking from sleep.

Enable and start:
```bash
sudo systemctl enable lightroom-ctrl
sudo systemctl start lightroom-ctrl
```

Verify:
```bash
sudo systemctl status lightroom-ctrl
```

---

## Service Management

### Check status of both services

```bash
sudo systemctl status midi-gadget
sudo systemctl status lightroom-ctrl
```

### Restart after code update

```bash
# Rebuild first
cd ~/lightroom-ctrl
make -j4

# Then restart
sudo systemctl restart lightroom-ctrl
```

### View live logs

```bash
# Qt app logs
journalctl -u lightroom-ctrl -f

# MIDI gadget logs
journalctl -u midi-gadget -f

# Last 50 lines
journalctl -u lightroom-ctrl -n 50
```

### Stop everything

```bash
sudo systemctl stop lightroom-ctrl
sudo systemctl stop midi-gadget
```

---

## Boot Sequence

```
Power on
    │
    ▼
kernel + RP1 I²C (i2c3-pi5 on GPIO6/7)
    │
    ▼
dwc2 USB peripheral mode (USB-C → MIDI gadget)
    │
    ▼
midi-gadget.service
  → creates USB MIDI device (LightroomCtrl)
  → ALSA port hw:2,0 available
    │
    ▼
lightroom-ctrl.service
  → retries until Mac HTTP server responds
  → fetches midi_map.ini from Mac (http://Js-iMac.local:8080)
  → fetches index.json + config files
  → initialises seesaw encoders on /dev/i2c-3
  → opens ALSA MIDI port
  → renders QML UI on DSI display
    │
    ▼
Ready — connect USB-C to Mac
Mac sees LightroomCtrl in MIDI2LR
```

---

## Useful Commands

| Task | Command |
|---|---|
| Rebuild Qt app | `cd ~/lightroom-ctrl && make -j4` |
| Restart app | `sudo systemctl restart lightroom-ctrl` |
| View app logs | `journalctl -u lightroom-ctrl -f` |
| Check MIDI port | `amidi -l` |
| Check I²C encoders | `sudo i2cdetect -y 3` |
| Check USB gadget | `cat /sys/kernel/config/usb_gadget/midi_gadget/UDC` |
| Check all services | `systemctl list-units --state=running` |

---

## File Locations

| File | Location |
|---|---|
| Qt app binary | `/home/jam/lightroom-ctrl/build/lightroom-ctrl` |
| Qt source | `/home/jam/lightroom-ctrl/` |
| QML UI | `/home/jam/lightroom-ctrl/main.qml` |
| MIDI gadget script | `/usr/local/bin/midi-gadget.sh` |
| MIDI gadget service | `/etc/systemd/system/midi-gadget.service` |
| Qt app service | `/etc/systemd/system/lightroom-ctrl.service` |
| Boot config | `/boot/firmware/config.txt` |