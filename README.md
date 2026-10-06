# dsp-volume-control

Physical volume control for car audio DSPs whose software has no remote or knob support.

A microcontroller (e.g. an ESP32 with rotary encoders) sends volume values over USB serial. A Python script on the Windows PC reads them and types them into the DSP's own control software, so you get real knobs for **main** and **subwoofer** volume.

## Scripts

| Script | DSP software | How it controls it |
|---|---|---|
| `dsi3_control.py` | DSI-3 | Finds the main/sub volume fields through Windows UI Automation (`pywinauto`) and types the new value |
| `esx_xcontrol.py` | ESX X-CONTROL 2 | Clicks the channel fields at positions relative to the window (`pyautogui`) and writes the value. Runs in background threads so new values can interrupt a write that is still in progress |

## Serial format

The microcontroller sends one line per change at **115200 baud**:

**dsi3_control.py**
```
CH1=-12.5     # channels 1–10 → main volume (dB)
CH11=-6.0     # channels 11–12 → sub volume (dB)
```

**esx_xcontrol.py**
```
MAIN=-12.5
PAIR4=-6.0    # sub channels 7 + 8
```

## Requirements

- Windows with the DSP software open
- Python 3
- `pip install pyserial pywinauto` (for DSI-3)
- `pip install pyserial pyautogui pygetwindow pyperclip` (for X-CONTROL 2)

## Configuration

The COM port is hard-coded near the top of each script (`COM7` / `COM3`). Change it to match your board.
For X-CONTROL 2, the click positions in `CHANNEL_COORDS` depend on the window layout and may need adjusting.

## Status

These are working scripts from my own setup. The microcontroller firmware isn't included yet.
