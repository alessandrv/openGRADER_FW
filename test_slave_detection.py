import serial
import time

def test_i2c_slave_detection():
    """
    Test script to monitor CDC output from STM32G4 running in forced slave mode.
    Connect your board via USB and check the CDC output for I2C debug messages.
    """
    
    # Adjust COM port as needed (Windows: COM1, COM2, etc.; Linux/Mac: /dev/ttyACM0, etc.)
    COM_PORT = "COM3"  # Change this to match your board's COM port
    BAUD_RATE = 115200
    
    try:
        # Open serial connection
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {COM_PORT} at {BAUD_RATE} baud")
        print("Monitoring I2C debug messages...")
        print("=" * 60)
        
        # Monitor for debug messages
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    timestamp = time.strftime("%H:%M:%S.%f")[:-3]
                    print(f"[{timestamp}] {line}")
                    
                    # Highlight I2C related messages
                    if "I2C" in line or "SLAVE" in line or "slave" in line:
                        print("  ^^ I2C RELATED MESSAGE ^^")
                        
    except serial.SerialException as e:
        print(f"Error: Could not open {COM_PORT}")
        print(f"Make sure the board is connected and the COM port is correct")
        print(f"Error details: {e}")
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial connection closed")

if __name__ == "__main__":
    test_i2c_slave_detection()