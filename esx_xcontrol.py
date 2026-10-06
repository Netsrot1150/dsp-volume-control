import serial
import time
import pyautogui
import pygetwindow as gw
from threading import Thread, Lock, Event

# ===== Relativa koordinater =====
CHANNEL_COORDS = {
    7: (751, 240),  # Sub ch1
    8: (844, 240),  # Sub ch2
    "MAIN": (27, 245)
}

PAIR_MAP = {4: [7, 8]}  # Sub

# ===== Lås, senaste värden och skrivtrådar =====
lock = Lock()
latest_vals = {"PAIR4": None, "MAIN": None}
written_vals = {"PAIR4": None, "MAIN": None}
write_threads = {"PAIR4": None, "MAIN": None}
write_events = {"PAIR4": Event(), "MAIN": Event()}
last_update_time = {"MAIN": 0}

# ===== Hämta fönsterposition =====
def get_window_pos(title="X-CONTROL 2"):
    windows = gw.getWindowsWithTitle(title)
    if not windows:
        return None
    win = windows[0]
    return win.left, win.top

# ===== Läs av värdet i aktivt fält =====
def read_field_text():
    pyautogui.hotkey('ctrl', 'a')
    pyautogui.hotkey('ctrl', 'c')
    time.sleep(0.02)
    try:
        import pyperclip
        text = pyperclip.paste().strip()
        return text
    except:
        return ""

# ===== Skriv in värde och avbryt vid nytt värde =====
def write_value(channel, value, win_pos):
    # Avbryt eventuell tidigare skrivning
    if write_threads[channel] and write_threads[channel].is_alive():
        write_events[channel].set()
        write_threads[channel].join()

    evt = Event()
    write_events[channel] = evt
    last_write_time = [0]  # tidpunkt för senaste skrivning

    def worker():
        try:
            x, y = win_pos

            def reliable_write(rel_x, rel_y, val):
                if evt.is_set():
                    return
                pyautogui.click(x + rel_x, y + rel_y, clicks=3, interval=0.04)
                pyautogui.hotkey("ctrl", "a")
                pyautogui.typewrite(f"{val:g}dB", interval=0.008)
                pyautogui.press("enter")
                last_write_time[0] = time.time()

            def verify_if_idle(rel_x, rel_y, val):
                # Kontrollera endast om det gått 0.3 sek sedan senaste skrivning
                while not evt.is_set():
                    if time.time() - last_write_time[0] >= 0.4:
                        entered = read_field_text().replace("dB", "").strip()
                        try:
                            entered_val = float(entered)
                        except ValueError:
                            entered_val = None

                        if entered_val is None or abs(entered_val - val) > 0.05:
                            print(f"[⚠️ Verifiering: '{entered}' != '{val:g}' → rättar]")
                            pyautogui.click(x + rel_x, y + rel_y, clicks=3, interval=0.04)
                            pyautogui.hotkey("ctrl", "a")
                            pyautogui.typewrite(f"{val:g}dB", interval=0.008)
                            pyautogui.press("enter")
                        break
                    time.sleep(0.03)

            if channel == "MAIN":
                rel_x, rel_y = CHANNEL_COORDS["MAIN"]
                reliable_write(rel_x, rel_y, value)
                Thread(target=verify_if_idle, args=(rel_x, rel_y, value), daemon=True).start()
            else:
                # PAIR4: skriv direkt alla sub-kanaler, avbryt om nytt värde kommer
                for ch in PAIR_MAP[4]:
                    if evt.is_set():
                        return
                    rel_x, rel_y = CHANNEL_COORDS[ch]
                    reliable_write(rel_x, rel_y, value)
                    # Starta verifiering i bakgrunden för sista sub-kanalen
                    if ch == PAIR_MAP[4][-1]:
                        Thread(target=verify_if_idle, args=(rel_x, rel_y, value), daemon=True).start()

        except Exception as e:
            print(f"[⚠️ Fel i write_value för {channel}: {e}]")

    t = Thread(target=worker)
    write_threads[channel] = t
    t.start()



# ===== UI-tråd =====
def ui_updater():
    global written_vals, last_update_time
    print("✅ X-CONTROL 2 styrning igång.")
    while True:
        win_pos = get_window_pos()
        if not win_pos:
            time.sleep(0.01)
            continue

        with lock:
            pair_val = latest_vals["PAIR4"]
            main_val = latest_vals["MAIN"]

        # ---- Pair 4 ----
        if pair_val is not None and pair_val != written_vals["PAIR4"]:
            write_value("PAIR4", pair_val, win_pos)
            written_vals["PAIR4"] = pair_val

        # ---- Main ----
        if main_val is not None:
            # Skriv endast om det skiljer
            if main_val != written_vals["MAIN"]:
                write_value("MAIN", main_val, win_pos)
                written_vals["MAIN"] = main_val
            # Uppdatera tid för paus-verifiering
            last_update_time["MAIN"] = time.time()

        # ---- Efter paus, verifiera MAIN ---
     #   if main_val is not None and (time.time() - last_update_time["MAIN"]) > 0.5:
    #        try:
     #           pyautogui.click(win_pos[0] + CHANNEL_COORDS["MAIN"][0], win_pos[1] + CHANNEL_COORDS["MAIN"][1], clicks=3, interval=0.05)
     #           entered_text = read_field_text()
    #            expected = f"{main_val:g}dB"
    #            if expected not in entered_text:
   #                 print(f"[⚠️ MAIN fel efter paus:] '{entered_text}' != '{expected}' → rättar")
   #                 pyautogui.typewrite(f"{main_val:g}dB\n", interval=0.001)
     #        except:
      #           pass

        time.sleep(0.002)

# ===== Serial-tråd =====
def serial_reader():
    ser = serial.Serial("COM3", 115200, timeout=0.05)
    time.sleep(2)
    global latest_vals, last_update_time
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            continue
        try:
            with lock:
                if line.startswith("PAIR4="):
                    vals = line.replace("PAIR4=", "").split(",")
                    latest_vals["PAIR4"] = float(vals[0])
                elif line.startswith("MAIN="):
                    latest_vals["MAIN"] = float(line.replace("MAIN=", ""))
                    last_update_time["MAIN"] = time.time()
        except:
            continue

# ===== Starta trådar =====
Thread(target=ui_updater, daemon=True).start()
Thread(target=serial_reader, daemon=True).start()

# ===== Håll programmet igång =====
while True:
    time.sleep(1)