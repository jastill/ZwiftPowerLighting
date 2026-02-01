import bluetooth
import time
import struct
import micropython
import config

# UUIDs
_IRQ_SCAN_RESULT = 5
_IRQ_SCAN_DONE = 6
_IRQ_PERIPHERAL_CONNECT = 7
_IRQ_PERIPHERAL_DISCONNECT = 8
_IRQ_GATTC_SERVICE_RESULT = 9
_IRQ_GATTC_SERVICE_DONE = 10
_IRQ_GATTC_CHARACTERISTIC_RESULT = 11
_IRQ_GATTC_CHARACTERISTIC_DONE = 12
_IRQ_GATTC_DESCRIPTOR_RESULT = 13
_IRQ_GATTC_DESCRIPTOR_DONE = 14
_IRQ_GATTC_READ_RESULT = 15
_IRQ_GATTC_WRITE_DONE = 17
_IRQ_GATTC_NOTIFY = 18
_IRQ_GATTC_INDICATE = 19

_CYCLING_POWER_SERVICE_UUID = bluetooth.UUID(0x1818)
_CYCLING_POWER_MEASUREMENT_CHAR_UUID = bluetooth.UUID(0x2A63)
_CCCD_UUID = bluetooth.UUID(0x2902)

class BLEClient:
    def __init__(self):
        self._ble = bluetooth.BLE()
        self._ble.active(True)
        self._ble.irq(self._irq)
        
        self.reset()
        self._power_callback = None

    def reset(self):
        self._addr_type = None
        self._addr = None
        self._conn_handle = None
        self._start_handle = None
        self._end_handle = None
        self._value_handle = None
        self._cccd_handle = None
        self._is_connected = False
        self._scan_callback = None

    def set_power_callback(self, callback):
        self._power_callback = callback

    def _irq(self, event, data):
        if event == _IRQ_SCAN_RESULT:
            addr_type, addr, adv_type, rssi, adv_data = data
            if self._scan_callback:
                self._scan_callback(addr_type, addr, adv_data)

        elif event == _IRQ_PERIPHERAL_CONNECT:
            conn_handle, addr_type, addr = data
            if addr_type == self._addr_type and addr == self._addr:
                self._conn_handle = conn_handle
                self._is_connected = True
                print(f"Connected to {config.BLE_NAME}")

        elif event == _IRQ_PERIPHERAL_DISCONNECT:
            conn_handle, _, _ = data
            if conn_handle == self._conn_handle:
                self._is_connected = False
                self._conn_handle = None
                print("Disconnected")

        elif event == _IRQ_GATTC_SERVICE_RESULT:
            conn_handle, start_handle, end_handle, uuid = data
            if conn_handle == self._conn_handle and uuid == _CYCLING_POWER_SERVICE_UUID:
                self._start_handle = start_handle
                self._end_handle = end_handle

        elif event == _IRQ_GATTC_CHARACTERISTIC_RESULT:
            conn_handle, def_handle, value_handle, properties, uuid = data
            if conn_handle == self._conn_handle and uuid == _CYCLING_POWER_MEASUREMENT_CHAR_UUID:
                self._value_handle = value_handle

        elif event == _IRQ_GATTC_DESCRIPTOR_RESULT:
            conn_handle, dsc_handle, uuid = data
            if conn_handle == self._conn_handle and uuid == _CCCD_UUID:
               self._cccd_handle = dsc_handle

        elif event == _IRQ_GATTC_NOTIFY:
            conn_handle, value_handle, notify_data = data
            if conn_handle == self._conn_handle and value_handle == self._value_handle:
                self._parse_power(notify_data)

    def _decode_name(self, payload):
        i = 0
        while i < len(payload):
            if i + 1 >= len(payload): break
            length = payload[i]
            if length == 0: break
            type = payload[i + 1]
            if type == 0x09: # Complete Local Name
                return payload[i + 2 : i + length + 1].decode('utf-8')
            i += length + 1
        return None

    def scan_and_connect(self, target_name):
        print(f"Scanning for {target_name}...")
        found = False
        
        def on_scan(addr_type, addr, adv_data):
            nonlocal found
            name = self._decode_name(adv_data)
            if name == target_name:
                self._addr_type = addr_type
                self._addr = bytes(addr)
                found = True
                self._ble.gap_scan(None) # Stop scanning

        self._scan_callback = on_scan
        self._ble.gap_scan(10000, 30000, 30000) # Run for 10s

        start = time.time()
        while not found and time.time() - start < 10:
            time.sleep_ms(100)
            
        if not found:
            print("Target not found.")
            return False
            
        print("Target found. Connecting...")
        self._ble.gap_connect(self._addr_type, self._addr)
        
        # Wait for connection
        start = time.time()
        while not self._is_connected and time.time() - start < 5:
            time.sleep_ms(100)
            
        if not self._is_connected:
            print("Failed to connect.")
            return False

        print("Discovering Services...")
        self._ble.gattc_discover_services(self._conn_handle)
        start = time.time()
        while self._start_handle is None and time.time() - start < 5:
            time.sleep_ms(100)
            
        if self._start_handle is None:
            print("Service not found.")
            return False

        print("Discovering Characteristics...")
        self._ble.gattc_discover_characteristics(self._conn_handle, self._start_handle, self._end_handle)
        start = time.time()
        while self._value_handle is None and time.time() - start < 5:
            time.sleep_ms(100)
            
        if self._value_handle is None:
            print("Characteristic not found.")
            return False
            
        # Discover Descriptors to find CCCD
        # Note: Often CCCD is value_handle + 1, but we can search to be sure or just assume.
        # To keep it simple and standard, let's try to discover in range.
        # Or better yet, we can try writing to handle + 1 if we want to save code, but let's do it right.
        print("Discovering Descriptors...")
        self._ble.gattc_discover_descriptors(self._conn_handle, self._value_handle, self._end_handle)
        start = time.time()
        while self._cccd_handle is None and time.time() - start < 3:
            time.sleep_ms(100)
            
        if self._cccd_handle is None:
            print("CCCD not found, trying handle + 1 logic as fallback")
            self._cccd_handle = self._value_handle + 1
        
        print(f"Subscribing to notifications on handle {self._value_handle}, CCCD {self._cccd_handle}")
        # Write 0x0100 to CCCD to enable notifications
        try:
            self._ble.gattc_write(self._conn_handle, self._cccd_handle, b'\x01\x00', 1)
        except Exception as e:
            print(f"Error writing to CCCD: {e}")
            return False
            
        return True

    def _parse_power(self, data):
        # Data format for Cycling Power Measurement
        # Flags (16 bit)
        # Instantaneous Power (Sint16)
        try:
            flags = struct.unpack("<H", data[0:2])[0]
            # Bit 0 is unused in standard generic cycling power? 
            # Actually Check GATT spec.
            # 0x2A63:
            # Flags: 
            # Bit 0: 0 = Paddle Power Balance Present ? No.
            # ISO/IEEE 11073-20601 ?
            # Standard:
            # Flags (2 bytes)
            # Instantaneous Power (2 bytes, sint16)
            
            # If standard, power is bytes 2 and 3.
            power = struct.unpack("<h", data[2:4])[0]
            
            if self._power_callback:
                try:
                    micropython.schedule(self._power_callback, power)
                except RuntimeError:
                    # Schedule queue full, ignore this update
                    pass
        except Exception as e:
            print(f"Error parsing power: {e}")

    def is_connected(self):
        return self._is_connected
