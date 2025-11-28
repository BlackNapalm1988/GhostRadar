import json
import os
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

COMPLICATION_CHOICES = [
    ("Off", "off"),
    ("Temperature (°C)", "temperature_c"),
    ("Humidity (%)", "humidity_percent"),
    ("Battery (%)", "battery_percent"),
    ("Wi-Fi strength (%)", "wifi_strength_percent"),
]

DEFAULT_CONFIG = {
    "brightness": 2,
    "heartbeat_speed": 120,
    "language": "en",
    "logging": {
        "enabled": True,
        "level": "info"
    },
    "ui": {
        "complications": {
            "top_left":    {"type": "temperature_c", "label": "T"},
            "top_right":   {"type": "humidity_percent", "label": "H"},
            "bottom_left": {"type": "battery_percent", "label": "BAT"},
            "bottom_right":{"type": "wifi_strength_percent", "label": "WiFi"},
        }
    }
}

CONFIG_REL_PATH = os.path.join("config", "system.json")

class GhostRadarConfigApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Ghost Radar SD Config Tool")
        self.geometry("520x520")

        self.sd_root = tk.StringVar(value="")
        self.config_data = json.loads(json.dumps(DEFAULT_CONFIG))
        self.comp_vars = {}

        self._build_ui()

    def _build_ui(self):
        # SD path selection
        frame_sd = ttk.LabelFrame(self, text="SD Card Location")
        frame_sd.pack(fill="x", padx=10, pady=10)

        entry = ttk.Entry(frame_sd, textvariable=self.sd_root)
        entry.pack(side="left", fill="x", expand=True, padx=(10, 5), pady=5)

        btn_browse = ttk.Button(frame_sd, text="Browse...", command=self.browse_sd)
        btn_browse.pack(side="left", padx=(0, 10), pady=5)

        # Config fields
        frame_cfg = ttk.LabelFrame(self, text="System Settings")
        frame_cfg.pack(fill="both", expand=True, padx=10, pady=5)

        # Brightness
        ttk.Label(frame_cfg, text="Brightness (0-3):").grid(row=0, column=0, sticky="w", padx=10, pady=5)
        self.brightness_var = tk.IntVar(value=DEFAULT_CONFIG["brightness"])
        self.brightness_scale = ttk.Scale(
            frame_cfg, from_=0, to=3,
            orient="horizontal",
            variable=self.brightness_var
        )
        self.brightness_scale.grid(row=0, column=1, sticky="ew", padx=10, pady=5)

        # Heartbeat speed
        ttk.Label(frame_cfg, text="Heartbeat speed (BPM):").grid(row=1, column=0, sticky="w", padx=10, pady=5)
        self.heartbeat_var = tk.IntVar(value=DEFAULT_CONFIG["heartbeat_speed"])
        self.heartbeat_entry = ttk.Entry(frame_cfg, textvariable=self.heartbeat_var)
        self.heartbeat_entry.grid(row=1, column=1, sticky="ew", padx=10, pady=5)

        # Language
        ttk.Label(frame_cfg, text="Language:").grid(row=2, column=0, sticky="w", padx=10, pady=5)
        self.language_var = tk.StringVar(value=DEFAULT_CONFIG["language"])
        self.language_combo = ttk.Combobox(
            frame_cfg,
            textvariable=self.language_var,
            values=["en", "es", "fr", "de"],
            state="readonly"
        )
        self.language_combo.grid(row=2, column=1, sticky="ew", padx=10, pady=5)

        # Logging enabled
        self.logging_enabled_var = tk.BooleanVar(value=DEFAULT_CONFIG["logging"]["enabled"])
        chk = ttk.Checkbutton(frame_cfg, text="Enable logging", variable=self.logging_enabled_var)
        chk.grid(row=3, column=0, columnspan=2, sticky="w", padx=10, pady=5)

        # Logging level
        ttk.Label(frame_cfg, text="Logging level:").grid(row=4, column=0, sticky="w", padx=10, pady=5)
        self.logging_level_var = tk.StringVar(value=DEFAULT_CONFIG["logging"]["level"])
        self.logging_level_combo = ttk.Combobox(
            frame_cfg,
            textvariable=self.logging_level_var,
            values=["debug", "info", "warn", "error"],
            state="readonly"
        )
        self.logging_level_combo.grid(row=4, column=1, sticky="ew", padx=10, pady=5)

        frame_cfg.columnconfigure(1, weight=1)

        # Complications
        frame_comp = ttk.LabelFrame(self, text="Radar Complications")
        frame_comp.pack(fill="both", expand=True, padx=10, pady=5)

        positions = [
            ("top_left", "Top-left"),
            ("top_right", "Top-right"),
            ("bottom_left", "Bottom-left"),
            ("bottom_right", "Bottom-right"),
        ]
        for idx, (key, label) in enumerate(positions):
            ttk.Label(frame_comp, text=f"{label} type:").grid(row=idx, column=0, sticky="w", padx=10, pady=4)
            type_var = tk.StringVar(value=DEFAULT_CONFIG["ui"]["complications"][key]["type"])
            type_combo = ttk.Combobox(
                frame_comp,
                textvariable=type_var,
                values=[choice[1] for choice in COMPLICATION_CHOICES],
                state="readonly",
                width=20
            )
            type_combo.grid(row=idx, column=1, sticky="ew", padx=6, pady=4)

            ttk.Label(frame_comp, text="Label:").grid(row=idx, column=2, sticky="e", padx=4, pady=4)
            label_var = tk.StringVar(value=DEFAULT_CONFIG["ui"]["complications"][key]["label"])
            label_entry = ttk.Entry(frame_comp, textvariable=label_var, width=10)
            label_entry.grid(row=idx, column=3, sticky="w", padx=(0, 10), pady=4)

            self.comp_vars[key] = {"type": type_var, "label": label_var}

        frame_comp.columnconfigure(1, weight=1)

        # Buttons
        frame_btn = ttk.Frame(self)
        frame_btn.pack(fill="x", padx=10, pady=(5, 10))

        btn_load = ttk.Button(frame_btn, text="Load from SD", command=self.load_from_sd)
        btn_load.pack(side="left", padx=(0, 5))

        btn_save = ttk.Button(frame_btn, text="Save to SD", command=self.save_to_sd)
        btn_save.pack(side="right", padx=(5, 0))

    def browse_sd(self):
        path = filedialog.askdirectory(title="Select SD card root directory")
        if path:
            self.sd_root.set(path)

    def _get_config_path(self):
        root = self.sd_root.get().strip()
        if not root:
            return None
        return os.path.join(root, CONFIG_REL_PATH)

    def ensure_directories(self):
        root = self.sd_root.get().strip()
        if not root:
            return False

        for sub in ["config", "logs", os.path.join("logs", "sessions"), "dictionary", "ui"]:
            full = os.path.join(root, sub)
            if not os.path.exists(full):
                os.makedirs(full, exist_ok=True)
        return True

    def load_from_sd(self):
        cfg_path = self._get_config_path()
        if not cfg_path:
            messagebox.showerror("Error", "Please select the SD card root directory first.")
            return

        if not os.path.exists(cfg_path):
            messagebox.showwarning("Not found", "No system.json found, using defaults.")
            self.apply_config_to_ui(DEFAULT_CONFIG)
            return

        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to read config:\n{e}")
            return

        # Merge into defaults so missing fields don’t crash
        cfg = json.loads(json.dumps(DEFAULT_CONFIG))  # deep copy
        try:
            cfg["brightness"] = data.get("brightness", cfg["brightness"])
            cfg["heartbeat_speed"] = data.get("heartbeat_speed", cfg["heartbeat_speed"])
            cfg["language"] = data.get("language", cfg["language"])

            logging_data = data.get("logging", {})
            cfg["logging"]["enabled"] = logging_data.get("enabled", cfg["logging"]["enabled"])
            cfg["logging"]["level"] = logging_data.get("level", cfg["logging"]["level"])

            ui_data = data.get("ui", {})
            comp_data = ui_data.get("complications", {})
            if isinstance(comp_data, dict):
                for key in cfg["ui"]["complications"].keys():
                    incoming = comp_data.get(key, {})
                    if isinstance(incoming, dict):
                        cfg["ui"]["complications"][key]["type"] = incoming.get("type", cfg["ui"]["complications"][key]["type"])
                        cfg["ui"]["complications"][key]["label"] = incoming.get("label", cfg["ui"]["complications"][key]["label"])
        except Exception as e:
            messagebox.showerror("Error", f"Invalid config structure:\n{e}")
            return

        self.config_data = cfg
        self.apply_config_to_ui(cfg)
        messagebox.showinfo("Loaded", "Configuration loaded from SD.")

    def apply_config_to_ui(self, cfg):
        self.brightness_var.set(cfg["brightness"])
        self.heartbeat_var.set(cfg["heartbeat_speed"])
        self.language_var.set(cfg["language"])
        self.logging_enabled_var.set(cfg["logging"]["enabled"])
        self.logging_level_var.set(cfg["logging"]["level"])
        comps = cfg.get("ui", {}).get("complications", {})
        for key, vars_dict in self.comp_vars.items():
            comp_cfg = comps.get(key, DEFAULT_CONFIG["ui"]["complications"][key])
            vars_dict["type"].set(comp_cfg.get("type", "off"))
            vars_dict["label"].set(comp_cfg.get("label", ""))

    def collect_config_from_ui(self):
        cfg = json.loads(json.dumps(DEFAULT_CONFIG))  # deep copy

        try:
            brightness = int(self.brightness_var.get())
        except ValueError:
            brightness = DEFAULT_CONFIG["brightness"]
        brightness = max(0, min(3, brightness))

        try:
            hb = int(self.heartbeat_var.get())
        except ValueError:
            hb = DEFAULT_CONFIG["heartbeat_speed"]

        cfg["brightness"] = brightness
        cfg["heartbeat_speed"] = hb
        cfg["language"] = self.language_var.get()
        cfg["logging"]["enabled"] = bool(self.logging_enabled_var.get())
        cfg["logging"]["level"] = self.logging_level_var.get()

        comp_cfg = {}
        for key, vars_dict in self.comp_vars.items():
            comp_cfg[key] = {
                "type": vars_dict["type"].get(),
                "label": vars_dict["label"].get()
            }
        cfg["ui"]["complications"] = comp_cfg

        return cfg

    def save_to_sd(self):
        if not self.ensure_directories():
            messagebox.showerror("Error", "Please select the SD card root directory first.")
            return

        cfg_path = self._get_config_path()
        if not cfg_path:
            messagebox.showerror("Error", "Invalid SD card root path.")
            return

        cfg = self.collect_config_from_ui()

        try:
            with open(cfg_path, "w", encoding="utf-8") as f:
                json.dump(cfg, f, indent=2)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to write config:\n{e}")
            return

        self.config_data = cfg
        messagebox.showinfo("Saved", f"Configuration saved to:\n{cfg_path}")


if __name__ == "__main__":
    app = GhostRadarConfigApp()
    app.mainloop()
