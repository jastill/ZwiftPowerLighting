import machine
import time

try:
    from picographics import PicoGraphics, DISPLAY_PICO_DISPLAY
    HAS_DISPLAY = True
except ImportError:
    HAS_DISPLAY = False

class Display:
    def __init__(self):
        self.display = None
        if HAS_DISPLAY:
            try:
                self.display = PicoGraphics(display=DISPLAY_PICO_DISPLAY, rotate=0)
                self.display.set_backlight(0.8)
                self.width, self.height = self.display.get_bounds()
                self.clear()
            except Exception as e:
                print(f"Failed to init display: {e}")
                self.display = None
        else:
            print("PicoGraphics not found. Display disabled.")

    def clear(self):
        if self.display:
            self.display.set_pen(0)
            self.display.clear()
            self.display.update()

    def text(self, msg, color=(255, 255, 255)):
        """Draw simple text to the center/log area"""
        if self.display:
            self.display.set_pen(0)
            self.display.clear() # Simple approach: clear then draw
            
            self.display.set_pen(self.display.create_pen(*color))
            self.display.set_font("bitmap8")
            # Draw text roughly centered or top
            self.display.text(msg, 10, 10, 220, 4) # wordwrap width, scale
            self.display.update()

    def update_status(self, connected, power, zone_color):
        """Update status screen with connection, power, and zone"""
        if self.display:
            self.display.set_pen(0)
            self.display.clear()
            
            # Status Bar
            if connected:
                self.display.set_pen(self.display.create_pen(0, 255, 0))
                status = "CONNECTED"
            else:
                self.display.set_pen(self.display.create_pen(255, 0, 0))
                status = "SCANNING..."
            
            self.display.set_font("bitmap8")
            self.display.text(status, 10, 10, 220, 3)
            
            # Power
            self.display.set_pen(self.display.create_pen(255, 255, 255))
            self.display.text(f"Power: {power}W", 10, 50, 220, 4)
            
            # Zone Indicator (Box of color)
            self.display.set_pen(self.display.create_pen(*zone_color))
            self.display.rectangle(10, 90, 220, 30)
            
            self.display.update()
