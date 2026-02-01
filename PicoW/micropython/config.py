# Configuration for ZwiftPowerLighting

# Hardware Configuration
PIN_NUM = 28      # The GPIO Pin used for the data line of the LED strip
NUM_LEDS = 30     # The number of LEDs in the strip (Configurable)

# Bluetooth Configuration
BLE_NAME = "KICKR CORE 5D21" # The name of the trainer to search for

# Rider Configuration
FTP = 250         # Functional Threshold Power (Watts) - UPDATE THIS VALUE!

# Power Zones (Percentage range, (R, G, B))
# Percentages are based on FTP.
# (Min %, Max %, Color)
# Colors are (Red, Green, Blue) 0-255
POWER_ZONES = [
    (0,   60,  (255, 255, 255)),  # Zone 1: Recovery (White)
    (60,  76,  (0,   0,   255)),  # Zone 2: Endurance (Blue)
    (76,  90,  (0,   255, 0)),    # Zone 3: Tempo (Green)
    (90,  105, (255, 255, 0)),    # Zone 4: Threshold (Yellow)
    (105, 119, (255, 165, 0)),    # Zone 5: VO2 Max (Orange)
    (119, 999, (255, 0,   0)),    # Zone 6: Anaerobic (Red)
]
