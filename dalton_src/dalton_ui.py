#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk, messagebox
import os
import subprocess
import re

# Paths to Sysfs interface
SYSFS_MODE = "/sys/module/dalton_drv/parameters/mode"
SYSFS_INTENSITY = "/sys/module/dalton_drv/parameters/intensity"

class DaltonApp:
    def __init__(self, root):
        self.root = root
        self.root.title("DaltonFix Controller & Preview (VMware Edition)")
        self.root.geometry("800x550")
        
        self.check_permissions()
        self.monitor_name = self.detect_monitor()

        # Layout
        left_frame = tk.Frame(root, padx=10, pady=10)
        left_frame.pack(side="left", fill="y")
        
        right_frame = tk.Frame(root, padx=10, pady=10)
        right_frame.pack(side="right", fill="both", expand=True)

        # Title
        tk.Label(left_frame, text="DaltonFix Controls", font=("Arial", 14, "bold")).pack(pady=5)
        
        # System Info
        sys_msg = f"Ecran: {self.monitor_name}" if self.monitor_name else "Ecran: Non détecté"
        tk.Label(left_frame, text=sys_msg, font=("Arial", 8)).pack(pady=2)

        # Mode Selection
        frame_mode = tk.LabelFrame(left_frame, text="Mode", padx=10, pady=10)
        frame_mode.pack(fill="x", pady=10)

        self.mode_var = tk.IntVar(value=0)
        modes = [
            ("Normal (Off)", 0),
            ("Protanopie (Red)", 1),
            ("Deutéranopie (Green)", 2),
            ("Tritanopie (Blue)", 3)
        ]

        for text, val in modes:
            tk.Radiobutton(frame_mode, text=text, variable=self.mode_var, value=val, command=self.on_change).pack(anchor="w")

        # Intensity Slider
        frame_int = tk.LabelFrame(left_frame, text="Intensité (%)", padx=10, pady=10)
        frame_int.pack(fill="x", pady=10)

        self.intensity_var = tk.IntVar(value=0)
        self.slider = tk.Scale(frame_int, from_=0, to=100, orient="horizontal", variable=self.intensity_var, command=self.on_change)
        self.slider.pack(fill="x")
        
        # Global Effect Checkbox
        frame_global = tk.LabelFrame(left_frame, text="Application Globale", padx=10, pady=10)
        frame_global.pack(fill="x", pady=10)
        
        self.apply_global_var = tk.BooleanVar(value=False)
        self.chk_global = tk.Checkbutton(frame_global, text="Forcer VM (Gamma Hack)", variable=self.apply_global_var, command=self.on_change)
        self.chk_global.pack()
        tk.Label(frame_global, text="Utilise xrandr --gamma au lieu de CTM.\nMoins précis mais marche sur VMware.", font=("Arial", 7, "italic"), fg="gray").pack()

        # Preview Area
        tk.Label(right_frame, text="Simulation (Algorithme Réel)", font=("Arial", 12, "bold")).pack(pady=5)
        self.canvas = tk.Canvas(right_frame, width=400, height=400, bg="black")
        self.canvas.pack()
        
        # Color Palettes for Test
        self.rects = []
        self.base_colors = []
        self.create_test_pattern()

        # Initial Load
        self.read_current_state()
        self.update_preview()

    def detect_monitor(self):
        try:
            out = subprocess.check_output(["xrandr"]).decode("utf-8")
            match = re.search(r"^([\w-]+) connected", out, re.MULTILINE)
            if match: return match.group(1)
        except:
            return None
        return None

    def create_test_pattern(self):
        width = 400; height = 400
        cols = 5; rows = 5
        w = width / cols; h = height / rows
        colors_hex = [
            "#FF0000", "#00FF00", "#0000FF", "#FFFF00", "#00FFFF", 
            "#FF00FF", "#FFFFFF", "#FFCCAA", "#228822", "#55AAFF",
            "#AA0000", "#00AA00", "#0000AA", "#AAAA00", "#00AAAA",
            "#AA00AA", "#888888", "#AA8866", "#114411", "#225588",
            "#550000", "#005500", "#000055", "#555500", "#005555"
        ]
        for i in range(rows * cols):
            color = colors_hex[i] if i < len(colors_hex) else "#888888"
            rr = int(color[1:3], 16); gg = int(color[3:5], 16); bb = int(color[5:7], 16)
            self.base_colors.append((rr, gg, bb))
            rect = self.canvas.create_rectangle((i%cols)*w, (i//cols)*h, ((i%cols)+1)*w, ((i//cols)+1)*h, fill=color, outline="black")
            self.rects.append(rect)

    def on_change(self, val=None):
        self.update_driver()
        self.update_preview()
        if self.apply_global_var.get():
            self.apply_gamma_hack()
        else:
            self.reset_gamma()

    def get_correction_matrix(self):
        mode = self.mode_var.get()
        intensity = self.intensity_var.get() / 100.0
        
        sim_protan = [[0.567, 0.433, 0], [0.558, 0.442, 0], [0, 0.242, 0.758]]
        sim_deutan = [[0.625, 0.375, 0], [0.700, 0.300, 0], [0, 0.300, 0.700]]
        sim_tritan = [[0.950, 0.050, 0], [0, 0.433, 0.567], [0, 0.475, 0.525]]
        
        target_sim = [[1,0,0],[0,1,0],[0,0,1]] # Identity
        if mode == 1: target_sim = sim_protan
        elif mode == 2: target_sim = sim_deutan
        elif mode == 3: target_sim = sim_tritan
            
        c_mat = [[0.0]*3 for _ in range(3)]
        for i in range(3):
            for j in range(3):
                ident = 1.0 if i == j else 0.0
                correction_val = (2.0 * ident) - target_sim[i][j]
                c_mat[i][j] = ident * (1.0 - intensity) + correction_val * intensity
        
        return c_mat

    def update_preview(self):
        c_mat = self.get_correction_matrix()
        for idx, (r, g, b) in enumerate(self.base_colors):
            nr = c_mat[0][0]*r + c_mat[0][1]*g + c_mat[0][2]*b
            ng = c_mat[1][0]*r + c_mat[1][1]*g + c_mat[1][2]*b
            nb = c_mat[2][0]*r + c_mat[2][1]*g + c_mat[2][2]*b
            nr = min(255, max(0, int(nr))); ng = min(255, max(0, int(ng))); nb = min(255, max(0, int(nb)))
            self.canvas.itemconfig(self.rects[idx], fill=f"#{nr:02x}{ng:02x}{nb:02x}")

    def apply_gamma_hack(self):
        if not self.monitor_name: return
        mode = self.mode_var.get()
        intensity = self.intensity_var.get() / 100.0
        
        # Gamma Hack: Simple channel scaling
        # Not a true Dalton matrix, but gives a visual tint effect "Globally"
        r_g = 1.0; g_g = 1.0; b_g = 1.0
        
        if mode == 1: # Protan (Red blind) -> Reduce Red, Boost Cyan?
            # Reduce Red: 1.0 -> 0.0 based on intensity
            # But xrandr gamma is 1.0 (normal) ... 0.1 (dark)
            r_g = 1.0 - (0.8 * intensity)
        elif mode == 2: # Deutan (Green blind)
            g_g = 1.0 - (0.8 * intensity)
        elif mode == 3: # Tritan (Blue blind)
            b_g = 1.0 - (0.8 * intensity)

        # Apply using xrandr --gamma
        cmd = ["xrandr", "--output", self.monitor_name, "--gamma", f"{r_g}:{g_g}:{b_g}"]
        try:
            subprocess.run(cmd, check=False)
        except: pass

    def reset_gamma(self):
        if not self.monitor_name: return
        try:
            subprocess.run(["xrandr", "--output", self.monitor_name, "--gamma", "1:1:1"], check=False)
        except: pass

    def check_permissions(self):
        if not os.access(SYSFS_MODE, os.W_OK):
            pass # Demo mode

    def read_current_state(self):
        # ... same as before
        pass
    
    def update_driver(self):
        if not os.access(SYSFS_MODE, os.W_OK): return
        mode = self.mode_var.get()
        intensity = self.intensity_var.get()
        try:
            with open(SYSFS_MODE, 'w') as f: f.write(str(mode))
            with open(SYSFS_INTENSITY, 'w') as f: f.write(str(intensity))
        except: pass

if __name__ == "__main__":
    try: import tkinter
    except: exit(1)
    root = tk.Tk()
    app = DaltonApp(root)
    root.mainloop()
