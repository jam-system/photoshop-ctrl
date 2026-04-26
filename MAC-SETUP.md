# Mac Automation Setup

Two things run automatically on the Mac at login:

1. **Config HTTP server** — serves JSON config files and `midi_map.ini` to the Pi
2. **MIDI2LR Editor** — available as a double-click app in the Dock

---

## 1. Config HTTP Server (LaunchAgent)

The HTTP server serves `~/lightroom-configs/` on port 8080.
The Pi fetches config files from this server at startup.

### Create the LaunchAgent

```bash
cat > ~/Library/LaunchAgents/com.lightroom.configserver.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.lightroom.configserver</string>

    <key>ProgramArguments</key>
    <array>
        <string>/usr/bin/python3</string>
        <string>-m</string>
        <string>http.server</string>
        <string>8080</string>
    </array>

    <key>WorkingDirectory</key>
    <string>/Users/jameelberg/lightroom-configs</string>

    <key>RunAtLoad</key>
    <true/>

    <key>KeepAlive</key>
    <true/>

    <key>StandardOutPath</key>
    <string>/tmp/lightroom-configserver.log</string>

    <key>StandardErrorPath</key>
    <string>/tmp/lightroom-configserver.log</string>
</dict>
</plist>
EOF
```

### Load it immediately (no reboot needed)

```bash
launchctl load ~/Library/LaunchAgents/com.lightroom.configserver.plist
```

### Verify it is running

```bash
# Check port 8080 is active
lsof -i :8080

# Check logs
cat /tmp/lightroom-configserver.log

# Test from browser or curl
curl http://localhost:8080/index.json
```

### Management commands

```bash
# Stop
launchctl unload ~/Library/LaunchAgents/com.lightroom.configserver.plist

# Start
launchctl load ~/Library/LaunchAgents/com.lightroom.configserver.plist

# Check status
launchctl list | grep lightroom
```

---

## 2. MIDI2LR Config Editor (Automator App)

The editor is a native macOS app built with Automator.
It uses `uv` to run `midi2lr_editor.py` without needing
a virtual environment.

### Install uv

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
source ~/.zshrc
```

Verify:
```bash
uv --version
```

### Add script dependencies header to midi2lr_editor.py

Add these lines at the very top of `midi2lr_editor.py`
(before the docstring):

```python
# /// script
# dependencies = ["PyQt6"]
# ///
```

Then run it directly with:
```bash
uv run ~/midi2lr_editor.py
```

### Create the Automator App

1. Open **Automator** → New Document → **Application**
2. Search for **Run Shell Script** and drag it in
3. Set **Shell** to `/bin/zsh`
4. Set **Pass input** to `as arguments`
5. Paste this script:

```bash
# Load PATH so uv is found
source ~/.zshrc 2>/dev/null || source ~/.zprofile 2>/dev/null

# Use full path to uv for reliability
/Users/jameelberg/.local/bin/uv run ~/midi2lr_editor.py
```

6. **File → Save** → name it `MIDI2LR Editor`
7. Save to `/Applications`

### Add to Dock

Drag `MIDI2LR Editor.app` from `/Applications` to the Dock.

### Optional — Add a custom icon

1. Find or create a `.icns` icon file
2. Right-click `MIDI2LR Editor.app` → **Get Info**
3. Drag the `.icns` file onto the icon in the top-left corner

---

## File Locations

| File | Location |
|---|---|
| Config JSON files | `~/lightroom-configs/` |
| MIDI map | `~/lightroom-configs/midi_map.ini` |
| Config index | `~/lightroom-configs/index.json` |
| Editor script | `~/midi2lr_editor.py` |
| Editor app | `/Applications/MIDI2LR Editor.app` |
| LaunchAgent | `~/Library/LaunchAgents/com.lightroom.configserver.plist` |
| Server log | `/tmp/lightroom-configserver.log` |
| MIDI2LR profiles | `/Users/jameelberg/_Midi2Lr_Project/midi2lrfolder/` |

---

## Workflow — Creating a New Profile

1. **Assign commands in MIDI2LR**
   - Touch encoders/buttons on the controller
   - Pick commands from the MIDI2LR popup
   - File → Save Profile to the midi2lrfolder

2. **Generate Pi config**
   - Launch `MIDI2LR Editor` from the Dock
   - Select the profile from the dropdown
   - Customize labels and colors
   - Click **💾 Save Pi JSON**

3. **Reload on Pi**
   ```bash
   sudo systemctl restart lightroom-ctrl
   ```
   Or just restart the Pi — it fetches configs at startup.
