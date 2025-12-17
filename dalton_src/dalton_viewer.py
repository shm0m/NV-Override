#!/usr/bin/env python3
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import os
import mmap
import time

# Configuration
FB_DEVICE = "/dev/fb0" # Might be fb1, logic to detect below
WIDTH = 800
HEIGHT = 600
BPP = 4 # 32 bits = 4 bytes

class DaltonViewer:
    def __init__(self, root, fb_path):
        self.root = root
        self.root.title("DaltonFix: Sortie Moniteur Virtuel")
        self.root.geometry(f"{WIDTH}x{HEIGHT}")
        
        self.fb_path = fb_path
        self.img_data = None
        self.tk_img = None
        
        self.label = tk.Label(root)
        self.label.pack(fill="both", expand=True)

        try:
            self.f = open(self.fb_path, 'rb')
            # Memory map the screen buffer
            # Size = W * H * BPP (assuming we know resolution, or read from fbset stuff)
            # For simplicity, we try to read small chunk or use os.fstat
            stat = os.fstat(self.f.fileno())
            self.fsize = stat.st_size
            self.mm = mmap.mmap(self.f.fileno(), self.fsize, mmap.MAP_SHARED, mmap.PROT_READ)
            
            print(f"Opened {fb_path}, Size: {self.fsize} bytes")
            self.refresh()
        except Exception as e:
            messagebox.showerror("Erreur", f"Impossible d'ouvrir {fb_path}:\n{e}\n\nAssurez-vous que le driver est chargé et que vous êtes root.")
            root.destroy()

    def refresh(self):
        # Seek to start
        self.mm.seek(0)
        
        # Read raw data. 
        # CAUTION: This is raw ARGB/XRGB. PIL Image.frombuffer can handle "RGBA" or "RGBX".
        # Linux fb often BGRA or RGBA. We try "RGBA".
        try:
            # We assume a standard resolution for the viewer (e.g. 800x600 or whatever kernel default)
            # A robust viewer would parse /sys/class/graphics/fbX/virtual_size
            # Let's try to detect square size
            import math
            pixels = self.fsize // 4
            w = int(math.sqrt(pixels * (4/3))) # aspect ratio guess
            h = pixels // w
            
            # Or just use fixed generic size if small?
            # 320x240 min config in driver.
            # Let's try a safe approach: read resolution from sysfs if possible
            w, h = self.get_res()
            
            raw = self.mm.read(w * h * 4)
            image = Image.frombytes('RGB', (w, h), raw, 'raw', 'BGRX')
            
            # Resize for viewing
            display_img = image.resize((WIDTH, HEIGHT))
            self.tk_img = ImageTk.PhotoImage(display_img)
            
            self.label.config(image=self.tk_img)
            self.label.image = self.tk_img
            
        except Exception as e:
            print(f"Frame error: {e}")

        # Refresh loop (10 FPS)
        self.root.after(100, self.refresh)

    def get_res(self):
        # Try to read /sys
        try:
            # name = fb0 or fb1
            name = os.path.basename(self.fb_path)
            with open(f"/sys/class/graphics/{name}/virtual_size", 'r') as f:
                # "800,600"
                data = f.read().strip().split(',')
                return int(data[0]), int(data[1])
        except:
            # Fallback
            return 800, 600

if __name__ == "__main__":
    import sys
    
    # Detect FB device
    target_fb = None
    # We look for the one created by dalton_drv.
    # Usually it's the last one.
    fbs = sorted([f for f in os.listdir("/dev") if f.startswith("fb")])
    if not fbs:
        print("Aucun framebuffer trouvé.")
        exit(1)
        
    # Pick the last one (likely the new one) or explicit argument
    if len(sys.argv) > 1:
        target_fb = sys.argv[1]
    else:
        # Heuristic: fb0 is usually system (VMware), fb1 is ours.
        # Check driver name in sysfs
        for fb in fbs:
            try:
                with open(f"/sys/class/graphics/{fb}/name", 'r') as f:
                    name = f.read().strip()
                    if "dalton" in name or "drm" in name: 
                        target_fb = f"/dev/{fb}"
                        break
            except: pass
        
        if not target_fb: target_fb = f"/dev/{fbs[-1]}"

    root = tk.Tk()
    app = DaltonViewer(root, target_fb)
    root.mainloop()
