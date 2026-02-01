import machine
import neopixel
import time
import config

class LEDController:
    def __init__(self):
        self.np = neopixel.NeoPixel(machine.Pin(config.PIN_NUM), config.NUM_LEDS)
        self.clear()

    def clear(self):
        """Turn off all LEDs"""
        self.fill((0, 0, 0))

    def fill(self, color):
        """Set all LEDs to the same color"""
        for i in range(config.NUM_LEDS):
            self.np[i] = color
        self.np.write()

    def startup_cycle(self):
        """Cycle through the colors in the Power Zones"""
        print("Starting LED cycle...")
        for _, _, color in config.POWER_ZONES:
            self.fill(color)
            time.sleep_ms(500)
        self.clear()

    def flash_color(self, color, duration_ms=3000, interval_ms=500):
        """Flash a specific color for a duration"""
        end_time = time.ticks_add(time.ticks_ms(), duration_ms)
        on = True
        while time.ticks_diff(end_time, time.ticks_ms()) > 0:
            if on:
                self.fill(color)
            else:
                self.clear()
            time.sleep_ms(interval_ms)
            on = not on
        self.clear() # Ensure off at end

    def update_from_power(self, power):
        """Update LEDs based on power wattage"""
        ftp = config.FTP
        percentage = (power / ftp) * 100
        
        target_color = (0, 0, 0) # Default off
        
        for min_p, max_p, color in config.POWER_ZONES:
            if min_p <= percentage < max_p:
                target_color = color
                break
        
        # Special case for max zone (last one usually)
        if percentage >= config.POWER_ZONES[-1][0]:
             target_color = config.POWER_ZONES[-1][2]
             
        self.fill(target_color)
        return target_color
