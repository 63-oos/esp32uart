# ESP32 UART PROJECT

## Description
This project demonstrates UART (Universal Asynchronous Receiver/Transmitter) serial communication between an ESP32 and a Raspberry Pi 5.

The ESP32 firmware is developed using ESP-IDF, while Python and the PySerial library are used on the Raspberry Pi to transmit and receive serial data.

The project covers basic UART communication, including data transmission, data reception, and full-duplex communication, where both devices can send and receive data.

It serves as a practical introduction to embedded systems communication and provides a foundation for integrating microcontrollers with Linux-based systems in robotics and IoT applications.


## Features

- UART communication between ESP32 and Raspberry Pi 5
- ESP32 firmware development using ESP-IDF
- Serial data transmission from Raspberry Pi to ESP32
- Serial data reception from ESP32 to Raspberry Pi
- Full-duplex communication using Python threading
- Configurable UART baud rate


## Requirements

## Hardware Connection
- ESP32 development board
- Raspberry Pi
- Jumper Wires

## Software Connection
- ESP-IDF installed and configured
- Python 3 installed on the Raspberry Pi
- Python pyserial library

## Hardware Connections
Connect the ESP32 and Raspberry Pi UART pins as follows:



|ESP32      |	Raspberry Pi        |
:-----------|-----------------------
|TX	        |RX(GPIO 15)  (PIN-10)  |
|RX	        |TX(GPIO 14)  (PIN-08)  |
|GND        |GND      (PIN 6)       |

## Build and Flash
Set the target according to your ESP32 chip model:
```bash
idf.py set-target esp32
idf.py build
idf.py -p PORT flash
idf.py -p PORT monitor
```

Replace `PORT` with your serial port connected to your ESP32

For example:
- Linux: /dev/ttyUSB0 or /dev/ttyACM0
- Windows: COM3, COM4, etc.

### Finding the Serial Port

#### Linux:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```
You can also check available devices using:
```bash
ls /dev/tty*
```

#### Windows
- Press Win + X.
- Open Device Manager.
- Expand Ports (COM & LPT).
- Find the COM port assigned to your ESP32.

## Raspberry Pi Setup
Enable the UART on the Pi
```bash
sudo raspi-config
# Interface Options -> Serial Port
#   Login shell over serial?  -> No
#   Enable serial hardware?   -> Yes
sudo reboot
```

Install the Python serial library:
```bash
sudo apt update
python3 -m pip install pyserial
```

Then verify it:
```bash
python3 -c "import serial; print(serial.__version__)"
```
Ensure the UART interface is enabled on the Raspberry Pi and that the selected UART is not being used by a serial login console.

## Test Data Receveing 
Run the following Python program on the Raspberry Pi to receive data sent by the ESP32.
Save it as receive_uart.py:

```python
import serial

ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=1)
print("Listening...")
while True:
    line = ser.readline()
    if line:
       print("From ESP32:", line.decode(errors='ignore').strip())
```
Run the program:
```bash
python3 receive_uart.py

```
Press Ctrl+C to exit.

## Test Data Transmission
Run the following Python program on the Raspberry Pi to send data sent by the ESP32.
Save it as send_uart.py:

```python

import serial

ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=1)
while True:
    text = input("Send to ESP32: ")
    ser.write((text + "\n").encode())
```
Run the program:
```bash
python3 send_uart.py

```
Press Ctrl+C to exit.

## Test Data Transmission and Receving simultaneously 
Run the following Python program on the Raspberry Pi to send data sent by the ESP32.
Save it as fullduplex_uart.py:

```python

import serial
import threading

# UART configuration
ser = serial.Serial('/dev/ttyAMA0', 9600, timeout=1)

def receive_data():
    while True:
        try:
            line = ser.readline()
            if line:
                print(f"\nFrom ESP32: {line.decode(errors='ignore').strip()}")
                print("Send to ESP32: ", end="", flush=True)
        except serial.SerialException as e:
            print(f"\nUART receive error: {e}")
            break

# Start receiving in a separate thread
rx_thread = threading.Thread(target=receive_data, daemon=True)
rx_thread.start()

print("UART communication started.")
print("Type a message and press Enter to send.")
print("Type 'exit' to quit.")

try:
    while True:
        text = input("Send to ESP32: ")

        if text.lower() == "exit":
            break

        ser.write((text + "\n").encode())

except KeyboardInterrupt:
    print("\nProgram stopped.")

finally:
    ser.close()
    print("UART closed.")

```
Run the program:
```bash
python3 fullduplex_uart.py

```
Press Ctrl+C to exit.

## Test Data Transmission and Receving simultaneously 
Run the following Python program on the Raspberry Pi to send data sent by the ESP32.
Save it as testfd.py:

```python
import serial

ser = serial.Serial('/dev/ttyAMA0', 115200, timeout=1)
ser.reset_input_buffer()     # discard anything left over, like the old READY line

print("Commands: on, off, toggle, status   (Ctrl+C to quit)")
while True:
    cmd = input("> ").strip()
    if not cmd:
        continue
    ser.write((cmd + "\n").encode())
    reply = ser.readline().decode(errors='ignore').strip()
    print("ESP32:", reply if reply else "(no reply)")
```

Run the program:
```bash
python3 testfd.py

```

## Experiments
- Break the baud rate on purpose. Change the Pi script to 9600 while the ESP32 stays at 115200. Polling gave you garbage text. Here you should see [EVENT] Frame error lines. Seeing the error event is the point of this version.
- Send a long message (about 200 characters) from the Pi. Watch how the data arrives, possibly across more than one UART_DATA event.
- Add a measurement after the printf in UART_DATA: printf("stack free: %u\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));. Then use the sizing method from earlier to choose the final stack size for uart_event_task.
- Disconnect the Pi’s GND while it sends. Events and garbage often show up.

## Future Improvements

- GPIO Control via UART: Control the ESP32 onboard LED from the Raspberry Pi by sending commands over UART.

- Button Input: Connect a push button to the ESP32 and detect button presses.

- Interrupt-Based Button Handling: Use GPIO interrupts to detect button presses without continuously polling the button state.

- Event Notification via UART: Send a message from the ESP32 to the Raspberry Pi whenever the button is pressed.

- Bidirectional Interaction: Allow the Raspberry Pi to control the LED and receive button events from the ESP32 in real time.
