import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox

import serial
from serial.tools import list_ports

DEFAULT_PORT = "COM1"
BAUD = 9600

FAULT_NAMES = {0: "OK", 1: "OVER-VOLTAGE", 2: "UNDER-VOLTAGE", 3: "OVER-CURRENT"}


class ProtectorGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Protector - Monitor & Settings")
        self.resizable(False, False)

        self.ser = None
        self.reader_thread = None
        self.rx_queue = queue.Queue()
        self.connected = False
        self.device_lock = False
        self.pending = None

        self._build_widgets()
        self.refresh_ports()
        self.after(100, self._poll_rx_queue)

    # ---------------------------------------------------------------- UI
    def _build_widgets(self):
        pad = dict(padx=8, pady=4)

        # --- connection bar -------------------------------------------
        conn = ttk.LabelFrame(self, text="Connection")
        conn.grid(row=0, column=0, columnspan=2, sticky="ew", **pad)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=14)
        self.port_combo.grid(row=0, column=0, padx=6, pady=6)
        ttk.Button(conn, text="Refresh", command=self.refresh_ports).grid(
            row=0, column=1
        )
        self.btn_connect = ttk.Button(conn, text="Connect", command=self.toggle_connect)
        self.btn_connect.grid(row=0, column=2, padx=6)
        self.conn_lbl = ttk.Label(conn, text="Disconnected", foreground="red")
        self.conn_lbl.grid(row=0, column=3, padx=10)

        # --- live monitor ----------------------------------------------
        mon = ttk.LabelFrame(self, text="Live monitoring")
        mon.grid(row=1, column=0, sticky="nsew", **pad)
        ttk.Label(mon, text="Voltage:").grid(
            row=0, column=0, sticky="w", padx=8, pady=2
        )
        self.v_lbl = ttk.Label(mon, text="---.- V", font=("Consolas", 16, "bold"))
        self.v_lbl.grid(row=0, column=1, sticky="e", padx=8)
        ttk.Label(mon, text="Current:").grid(
            row=1, column=0, sticky="w", padx=8, pady=2
        )
        self.a_lbl = ttk.Label(mon, text="--.-- A", font=("Consolas", 16, "bold"))
        self.a_lbl.grid(row=1, column=1, sticky="e", padx=8)
        ttk.Label(mon, text="Status:").grid(row=2, column=0, sticky="w", padx=8, pady=2)
        self.status_lbl = ttk.Label(mon, text="---", font=("Segoe UI", 12, "bold"))
        self.status_lbl.grid(row=2, column=1, sticky="e", padx=8)
        ttk.Label(mon, text="Retry in:").grid(
            row=3, column=0, sticky="w", padx=8, pady=2
        )
        self.retry_lbl = ttk.Label(mon, text="--- s", font=("Consolas", 12))
        self.retry_lbl.grid(row=3, column=1, sticky="e", padx=8)
        ttk.Label(mon, text="Relay:").grid(row=4, column=0, sticky="w", padx=8, pady=2)
        self.relay_lbl = ttk.Label(mon, text="---", font=("Segoe UI", 10))
        self.relay_lbl.grid(row=4, column=1, sticky="e", padx=8)

        # --- settings ----------------------------------------------------
        st = ttk.LabelFrame(self, text="Threshold settings")
        st.grid(row=1, column=1, sticky="nsew", **pad)
        self._lock_widgets = []  # controls disabled while device menu open

        ttk.Label(st, text="Over-voltage (V):").grid(
            row=0, column=0, sticky="w", padx=8, pady=3
        )
        self.ov_var = tk.StringVar()
        self.ov_entry = ttk.Entry(st, textvariable=self.ov_var, width=10)
        self.ov_entry.grid(row=0, column=1, padx=4)
        self.ov_btn = ttk.Button(
            st, text="Set", command=lambda: self.send_threshold("OV", self.ov_var, 10.0)
        )
        self.ov_btn.grid(row=0, column=2, padx=4)
        self._lock_widgets += [self.ov_entry, self.ov_btn]

        ttk.Label(st, text="Under-voltage (V):").grid(
            row=1, column=0, sticky="w", padx=8, pady=3
        )
        self.uv_var = tk.StringVar()
        self.uv_entry = ttk.Entry(st, textvariable=self.uv_var, width=10)
        self.uv_entry.grid(row=1, column=1, padx=4)
        self.uv_btn = ttk.Button(
            st, text="Set", command=lambda: self.send_threshold("UV", self.uv_var, 10.0)
        )
        self.uv_btn.grid(row=1, column=2, padx=4)
        self._lock_widgets += [self.uv_entry, self.uv_btn]

        ttk.Label(st, text="Over-current (A):").grid(
            row=2, column=0, sticky="w", padx=8, pady=3
        )
        self.oc_var = tk.StringVar()
        self.oc_entry = ttk.Entry(st, textvariable=self.oc_var, width=10)
        self.oc_entry.grid(row=2, column=1, padx=4)
        self.oc_btn = ttk.Button(
            st, text="Set", command=lambda: self.send_threshold("OC", self.oc_var, 10.0)
        )
        self.oc_btn.grid(row=2, column=2, padx=4)
        self._lock_widgets += [self.oc_entry, self.oc_btn]

        # Read from device is always allowed (read-only)
        ttk.Button(st, text="Read from device", command=self.request_settings).grid(
            row=3, column=0, columnspan=3, sticky="ew", padx=8, pady=6
        )
        self.save_btn = ttk.Button(
            st, text="Save to EEPROM", command=lambda: self.send_cmd("SAVE")
        )
        self.save_btn.grid(row=4, column=0, columnspan=3, sticky="ew", padx=8, pady=2)
        self._lock_widgets.append(self.save_btn)

        # --- log -----------------------------------------------------------
        logf = ttk.LabelFrame(self, text="Log")
        self.log_txt = tk.Text(
            self, height=8, width=70, state="disabled", font=("Consolas", 9)
        )
        logf.grid(row=2, column=0, columnspan=2, sticky="ew", **pad)
        self.log_txt.grid(in_=logf, padx=6, pady=6)

    def log(self, msg):
        self.log_txt.configure(state="normal")
        self.log_txt.insert("end", msg + "\n")
        self.log_txt.see("end")
        self.log_txt.configure(state="disabled")

    # ----------------------------------------------------- device lock
    def _set_device_lock(self, locked, warn=True):
        """GUI side of the M=1 / M=0 protocol."""
        if locked == self.device_lock:
            return
        self.device_lock = locked
        for w in self._lock_widgets:
            w.configure(state="disabled" if locked else "normal")
        if locked:
            self.status_lbl.configure(
                text="DEVICE MENU OPEN - PC LOCKED", foreground="orange"
            )
            self.log("<< M=1  (device settings menu open - PC access locked)")
            if warn:
                messagebox.showwarning(
                    "Device settings menu is open",
                    "The settings menu is open on the device itself.\n\n"
                    "Threshold changes from the PC are denied until the menu "
                    "is closed on the device (SAVE & EXIT).",
                )
        else:
            self.log("<< M=0  (device menu closed - PC access restored)")
            self.request_settings()  # re-sync fields with device values

    def _warn_busy(self):
        self.log("<< BUSY  (device refused - its settings menu is open)")
        messagebox.showwarning(
            "Access denied",
            "The device settings menu is open.\n\n"
            "Close it on the device (SAVE & EXIT) and try again.",
        )

    # ----------------------------------------------------------- serial
    def refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_combo["values"] = ports
        if not self.port_var.get():
            self.port_var.set(DEFAULT_PORT or (ports[0] if ports else ""))

    def toggle_connect(self):
        if self.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("No port", "Select a COM port first.")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.5)
        except serial.SerialException as e:
            messagebox.showerror("Open failed", str(e))
            return
        self.connected = True
        self.btn_connect.configure(text="Disconnect")
        self.conn_lbl.configure(text=f"Connected to {port}", foreground="green")
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()
        self.request_settings()
        self.log(f"Opened {port} @ {BAUD}")

    def disconnect(self):
        self.connected = False
        if self.device_lock:
            self._set_device_lock(False, warn=False)
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.btn_connect.configure(text="Connect")
        self.conn_lbl.configure(text="Disconnected", foreground="red")
        self.log("Disconnected")

    def _reader_loop(self):
        """Background thread: push received lines into the queue."""
        buf = b""
        while self.connected and self.ser:
            try:
                chunk = self.ser.read(64)
            except serial.SerialException:
                break
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    self.rx_queue.put(line.decode(errors="replace").strip())

    def _poll_rx_queue(self):
        """Main thread: consume queued lines, update the UI safely."""
        try:
            while True:
                line = self.rx_queue.get_nowait()
                self.handle_line(line)
        except queue.Empty:
            pass
        if self.pending and time.time() - self.pending[1] > 1.5:
            cmd = self.pending[0]
            self.pending = None
            self.log(
                f"!! NO REPLY to '{cmd}': the device never received it "
                "(PC->MCU path: COMPIM TXD pin 3 -> MCU RX/PD0, COM ports)"
            )
        self.after(100, self._poll_rx_queue)

    def send_cmd(self, cmd):
        if not self.connected or not self.ser:
            self.log("Not connected - command ignored")
            return
        try:
            self.ser.write((cmd + "\n").encode())
            self.ser.flush()
        except (serial.SerialException, OSError) as e:
            self.log(f"!! write failed: {e}")
            return
        self.pending = (cmd, time.time())
        self.log(f">> {cmd}")

    # --------------------------------------------------------- protocol
    def handle_line(self, line):
        self.log(f"<< {line}")
        if line in ("OK", "ERR", "BUSY") or (
            self.pending and self.pending[0] == "GET" and line.startswith("OV=")
        ):
            self.pending = None
        try:
            if line == "M=1":  # device menu opened
                self._set_device_lock(True)
            elif line == "M=0":  # device menu closed
                self._set_device_lock(False)
            elif line == "BUSY":  # write refused by device
                self._warn_busy()
            elif line.startswith("V="):  # telemetry
                kv = dict(p.split("=", 1) for p in line.split(","))
                v, a = float(kv["V"]), float(kv["A"])
                fault, retry = int(kv["F"]), int(kv["R"])
                self.v_lbl.configure(text=f"{v:.1f} V")
                self.a_lbl.configure(text=f"{a:.2f} A")
                name = FAULT_NAMES.get(fault, "?")
                color = "green" if fault == 0 else "red"
                if fault == 0 and retry > 0:
                    name, color = "RETRY WAIT", "orange"
                if not self.device_lock:
                    self.status_lbl.configure(text=name, foreground=color)
                self.retry_lbl.configure(text=f"{retry} s")
                self.relay_lbl.configure(
                    text="OFF (protected)" if (fault or retry) else "ON"
                )
            elif line.startswith("OV="):  # GET reply OR device->PC sync
                kv = dict(p.split("=", 1) for p in line.split(","))
                self.ov_var.set(f"{int(kv['OV'])/10:.1f}")
                self.uv_var.set(f"{int(kv['UV'])/10:.1f}")
                self.oc_var.set(f"{int(kv['OC'])/10:.1f}")
            # "OK" / "ERR" are already shown in the log
        except (ValueError, KeyError):
            pass  # ignore junk

    def send_threshold(self, key, var, scale):
        """Convert human units -> tenths and send, e.g. 260.0 -> OV=2600."""
        if self.device_lock:  # refuse locally, don't even send
            self._warn_busy()
            return
        try:
            x10 = int(round(float(var.get()) * scale))
        except ValueError:
            messagebox.showwarning("Invalid value", f"Bad number: {var.get()!r}")
            return
        self.send_cmd(f"{key}={x10}")

    def request_settings(self):
        self.send_cmd("GET")  # read-only: allowed even while device menu open

    def on_close(self):
        self.disconnect()
        self.destroy()


if __name__ == "__main__":
    app = ProtectorGUI()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
