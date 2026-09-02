# Pico W Integrated Keyboard WiFi Bridge

This project combines USB keyboard input via PIO-USB with WiFi transmission on the Raspberry Pi Pico W.

## Hardware Setup

### Wiring Configuration
Connect your keyboard to the Pico W using the following GPIO pins:

| Connection | Pico W Pin |
|-----------|-----------|
| Power (VBUS) | VBUS (Pin 40) |
| Ground | GND (Pin 38) |
| D+ (USB Data+) | GPIO 0 |
| D- (USB Data-) | GPIO 1 |

### Power Considerations
- Ensure adequate power supply for both the Pico W and the keyboard
- USB keyboard typically draws 100mA at 5V
- Use a quality USB power supply rated for at least 1-2A

## Features

### Dual WiFi Modes

#### 1. **AP Mode (Default - Recommended for Testing)**
- Pico W broadcasts its own WiFi network
- **SSID:** `PicoW_Keyboard_AP`
- **Password:** `123456789`
- **Access:** Connect to the network and open browser to `http://192.168.4.1`
- **Advantage:** Works without existing WiFi network

#### 2. **Station Mode**
- Pico W connects to your existing WiFi network
- Sends keyboard data via UDP to your PC
- **Configuration:** Edit these lines in `main_integrated.c`:
  ```c
  #define WIFI_SSID "YOUR_SSID"           // Your network SSID
  #define WIFI_PASSWORD "YOUR_PASSWORD"   // Your network password
  #define SERVER_IP "192.168.1.100"       // Your PC's IP address
  #define UDP_PORT 4444
  ```
- Change `wifi_mode = 0;` to `wifi_mode = 1;` in main()

## Features

- ✅ **Real-time keyboard input capture** via USB HID
- ✅ **Live web interface** to view keyboard input as you type
- ✅ **Dual WiFi modes** (AP or Station)
- ✅ **DHCP server** for automatic client network configuration (AP mode)
- ✅ **Multi-core processing** - Core 1 handles USB, Core 0 handles WiFi
- ✅ **Thread-safe** keyboard data handling with mutexes
- ✅ **Auto-refresh** web page every 2 seconds

## Building

### Prerequisites
- Raspberry Pi Pico W SDK
- CMake 3.12+
- Arm GCC toolchain
- TinyUSB library
- Pico PIO USB library

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

### Upload to Pico W

```bash
# Hold BOOTSEL button while plugging in USB
cp main_integrated.uf2 /media/your-user/RPI-RP2/
```

## Usage

### AP Mode (Easiest)
1. Build and upload firmware
2. Power on Pico W
3. On your PC/phone, connect to WiFi network `PicoW_Keyboard_AP`
4. Open web browser to `http://192.168.4.1`
5. Plugin USB keyboard to Pico W
6. Type on keyboard - characters appear live on web page

### Station Mode
1. Configure WiFi credentials and PC IP in code
2. Build and upload firmware
3. Pico W connects to your network automatically
4. Keyboard data is sent via UDP to your PC on port 4444
5. Setup a listener on your PC to receive UDP packets

## PC UDP Listener Example (Python)

For Station mode, receive keyboard data on your PC:

```python
import socket

UDP_IP = "0.0.0.0"
UDP_PORT = 4444

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening for keyboard data on port {UDP_PORT}...")

while True:
    data, addr = sock.recvfrom(1024)
    # Each keyboard report is 8 bytes
    if len(data) >= 8:
        modifier = data[0]
        keycode = data[2:8]
        print(f"Modifier: {modifier:02x}, Keys: {keycode.hex()}")
```

## File Structure

- `main_integrated.c` - Main integrated firmware
- `CMakeLists.txt` - Build configuration
- `pico_sdk_import.cmake` - SDK import
- `tusb_config.h` - TinyUSB configuration
- `usb_descriptors.c/h` - USB device descriptors
- `dhcp_dns_helpers.h` - DHCP/DNS utilities
- `lwipopts.h` - lwIP configuration

## Keyboard Key Codes

The firmware converts HID key codes to ASCII characters where applicable:
- Letters A-Z (with Shift for uppercase)
- Numbers 0-9
- Space (keycode 44)
- Enter/Return (keycode 40)

Full HID key codes are logged for other keys.

## Troubleshooting

### No WiFi network appears
- Check if Pico W is powered correctly
- Look for LED indicator on Pico W
- Try reprogramming the firmware

### Can't connect to `192.168.4.1`
- Ensure you're connected to `PicoW_Keyboard_AP` network
- Check browser URL spelling
- Try `http://192.168.4.20` if DHCP gave different IP

### Keyboard not recognized
- Verify GPIO 0 and 1 are correctly wired (D+ and D-)
- Ensure USB power is connected to VBUS
- Check ground connection at Pin 38
- USB keyboard may require more power - use external supply

### Keyboard data not appearing
- Check Pico W serial output for debug messages
- Verify keyboard is properly detected by Pico W
- Try different keyboard to rule out hardware incompatibility

## Serial Debug Output

Connect via USB serial (115200 baud) to see debug messages:

```
=== Pico W Integrated Keyboard WiFi Bridge ===
GPIO Pins: D+ = GPIO0, D- = GPIO1, Power = VBUS, GND = Pin 38

Choose WiFi mode:
0 = AP Mode (broadcast its own network)
1 = Station Mode (connect to existing network)
Using AP mode by default...

Starting WiFi AP Mode...
AP SSID: PicoW_Keyboard_AP
AP Password: 123456789
Connect to this network and visit http://192.168.4.1 to see keyboard inputs

Core 1: USB Host initialized (GPIO0=D+, GPIO1=D-)
Main loop starting...
Waiting for keyboard to be connected...

HID device mounted at addr 1, instance 0
```

## Performance Notes

- USB reports processed at ~125Hz
- Web server refreshes every 2 seconds
- UDP transmission rate limited by WiFi speed
- Log buffer holds ~4KB of recent keystrokes

## License

MIT License - See LICENSE file for details

## References

- [Raspberry Pi Pico W Documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [Pico PIO USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
- [lwIP Documentation](https://savannah.nongnu.org/projects/lwip/)

## Next Steps

Consider adding:
- Bluetooth support for wireless keyboard
- MQTT protocol for IoT integration
- Multi-keyboard support
- Key filtering and encryption
- Remote keyboard control (send keys back to Pico)
- Database logging of all keystrokes
