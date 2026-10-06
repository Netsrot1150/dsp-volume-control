import serial
import time
from pywinauto import Application

# ===== Connect DSI-3 =====
app = Application(backend="uia").connect(title_re="DSI-3")
window = app.window(title_re="DSI-3")

# ===== Sliders =====
main_field = None  # automation_id = "2812"
sub_field  = None  # automation_id = "2827"

for e in window.descendants(control_type="Edit"):
    aid = getattr(e.element_info,'automation_id','')
    if aid == "2812":
        main_field = e
    elif aid == "2827":
        sub_field = e

if not main_field or not sub_field:
    raise Exception("Could not find main or sub fields")

print("Python → DSI-3 READY")

# ===== Serial =====
ser = serial.Serial("COM7", 115200, timeout=0.01)
time.sleep(1)

# ===== Last values =====
last_main = None
last_sub  = None

while True:
    latest = None

    # Läs alla inkommande rader
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            break
        try:
            ch,val = line.split("=")
            channel = int(ch.replace("CH",""))
            volume = float(val)
            latest = (channel, volume)
        except:
            continue

    if not latest:
        continue

    channel, volume = latest

    # ===== MAIN → 2812 =====
    if 1 <= channel <= 10:
        if last_main != volume:
            main_field.set_focus()
            main_field.type_keys("^a{BACKSPACE}", pause=0)
            main_field.type_keys(f"{volume:.1f}dB{{ENTER}}", pause=0)
            last_main = volume
            print(f"MAIN → {volume:.1f} dB")
        continue

    # ===== SUB → 2827 =====
    if channel in [11, 12]:
        if last_sub != volume:
            sub_field.set_focus()
            sub_field.type_keys("^a{BACKSPACE}", pause=0)
            sub_field.type_keys(f"{volume:.1f}dB{{ENTER}}", pause=0)
            last_sub = volume
            print(f"SUB → {volume:.1f} dB")
        continue
