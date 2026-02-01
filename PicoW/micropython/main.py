import time
import config
from leds import LEDController
from ble_client import BLEClient
from display import Display

def main():
    print("ZwiftPowerLighting Starting...")
    
    # 1. Initialize LEDs and Display
    leds = LEDController()
    display = Display()
    
    display.text("ZwiftPowerLighting\nStarting...")
    
    # 2. Startup Sequence: Cycle through colors
    leds.startup_cycle()
    
    # 3. Turn LEDs White
    print("Set White, scanning for trainer...")
    leds.fill((255, 255, 255))
    display.text(f"Scanning for:\n{config.BLE_NAME}")
    
    # 4. Scan and Connect
    client = BLEClient()
    connected = False
    
    # Simple retry loop for connection
    while not connected:
        connected = client.scan_and_connect(config.BLE_NAME)
        if not connected:
            print("Retrying connection in 5 seconds...")
            display.text("Not Found.\nRetrying...")
            time.sleep(5)
            display.text(f"Scanning for:\n{config.BLE_NAME}")
            
    # 5. Connected -> Flash Green
    if connected:
        print("Connected! Flashing Green.")
        leds.flash_color((0, 255, 0), duration_ms=3000, interval_ms=500)
        display.update_status(True, 0, (0, 0, 0))
    
    # Callback to update both LEDs and Display
    def on_power_update(power):
        color = leds.update_from_power(power)
        display.update_status(True, power, color)

    # Setup callback
    client.set_power_callback(on_power_update)
    
    # Main Loop
    print("Entering Main Loop...")
    try:
        while True:
            if not client.is_connected():
                print("Connection lost. Resetting...")
                display.text("Disconnected.\nReconnecting...")
                leds.fill((255, 255, 255))
                connected = client.scan_and_connect(config.BLE_NAME)
                if connected:
                    leds.flash_color((0, 255, 0))
                    display.update_status(True, 0, (0, 0, 0))
                    
            time.sleep(1)
    except KeyboardInterrupt:
        print("Stopping...")
        leds.clear()
        display.clear()
        display.text("Stopped.")

if __name__ == "__main__":
    main()
