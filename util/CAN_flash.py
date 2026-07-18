import sys
import os
import time
import struct

###
# SFR ESP32 CAN Flasher using Vector CAN Interface
# This script sends a binary firmware file to an SFR ESP32 device over CAN bus
# using a Vector CAN device. 
# Make sure whatever binary you send supports CAN reflash!!!!
# The sequence for sending a binay is as follows:
# 1. Send "Enter Reflash Mode" command with size of binary
#    - ID: 0x010, Data: [0x04, TargetID, Size3, Size2, Size1, Size0, 0, 0]
#    - TargetID is the ID that the ESP sends status messages from, a list is in the CAN Spec.
#    - Size is a 4-byte big-endian integer representing the size of the binary in bytes.
#
# 2. Stream binary data in 8-byte CAN frames with crc8 error detection
#    - ID: TargetID, Data: [Data0, Data1, Data2, Data3, Data4, Data5, Data6, CRC8]
#    - Each payload is 7 bytes of data + 1 byte CRC8 checksum.
#    - The ESP returns a ACK or NACK, if is a NACK the same frame is resent.
#    - NACK frames also contain the error count.
#    - ACK frame: ID: DeviceID, Data: [0xFF]
#    - NACK frame: ID: DeviceID, Data: [0x00, ErrorCount0, ErrorCount1]
#
# 3. The ESP will validate the binary after all data is sent and restart running the new firmware.
#   

vector_lib_path = r"C:\Users\Public\Documents\Vector\XL Driver Library\bin"
if os.path.exists(vector_lib_path):
    os.environ['PATH'] += os.pathsep + vector_lib_path

# Try to import python-can, provide instructions if missing
try:
    import can
except ImportError:
    print("Error: 'python-can' library is required.")
    print("Please install it using: pip install python-can")
    sys.exit(1)

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------
CAN_INTERFACE = 'vector'
CAN_CHANNEL = 0          # 0 = First channel assigned in Vector Hardware Config
BITRATE = 1000000        # 1 Mbps
APP_NAME = "CAN_Flash"   # Name to appear in Vector Hardware Config

# CAN Protocol Definitions
CAN_CMD_ID = 0x010
DEVICE_ID = 0xFF
CRC8_POLYNOMIAL = 0x12F

# Command Codes (Must match firmware eCAN_CMD_t)
CMD_REFLASH_MODE = 0b00001000
CMD_NORMAL_MODE  = 0b00010000  # Reused for Size packet in firmware logic

# Timing
TIMEOUT_RX = 1.0              # Timeout for receiving messages (if needed)
ESP32_REFLASH_DELAY = 1.0   # Delay after sending reflash command
TIMEOUT_COMMS = 5.0         # Timeout if no ACK/NACK received
RESEND_INTERVAL = 0.5       # Resend frame if no ACK/NACK received for this long (s)

# File Paths
# Assumes script is in 'util/' and build is in 'build/' relative to project root
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(PROJECT_ROOT, 'build')
BIN_FILENAME = 'SFRESP32.bin'
BIN_PATH = os.path.join(BUILD_DIR, BIN_FILENAME)

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
def send_frame(bus, arbitration_id, data):
    """Sends a single CAN frame."""
    msg = can.Message(arbitration_id=arbitration_id, data=data, is_extended_id=False)
    try:
        bus.send(msg)
    except can.CanError as e:
        print(f"TX Error: {e}")
        return False
    return True

def print_progress(iteration, total, prefix='', suffix='', decimals=1, length=50, fill='█'):
    """Call in a loop to create terminal progress bar"""
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    filled_length = int(length * iteration // total)
    bar = fill * filled_length + '-' * (length - filled_length)
    sys.stdout.write(f'\r{prefix} |{bar}| {percent}% {suffix}')
    if iteration == total:
        sys.stdout.write('\n')
    sys.stdout.flush()

def crc8(data_bytes):
    """Calculates CRC8-AUTOSTAR (Polynomial 0x12F)"""
    val = int.from_bytes(data_bytes, 'little')
    # If len < 8, we need to shift it up? 
    # C code: memcpy into qword (64 bits). 
    # If 8 bytes, it fills the qword.
    # We always send 8 bytes (7 data + 1 crc).
    
    val_64 = val # assuming 8 bytes
    
    for offset in range(63, 7, -1):
        if (val_64 & (1 << offset)):
            val_64 ^= (CRC8_POLYNOMIAL << (offset - 8))
            
    return val_64 & 0xFF

def calculate_crc_byte(seven_bytes):
    """Finds the 8th byte that makes crc8() return 0"""
    for i in range(256):
        candidate = seven_bytes + bytes([i])
        if crc8(candidate) == 0:
            return i
    raise ValueError("Could not find valid CRC byte")

# -----------------------------------------------------------------------------
# Main Execution
# -----------------------------------------------------------------------------
def main():
    print(f"\n=== SFR ESP32 CAN Flasher (Vector) ===")
    print(f"Target Binary: {BIN_PATH}")

    # 1. Validate Binary File
    if not os.path.exists(BIN_PATH):
        print(f"Error: Binary file not found at {BIN_PATH}")
        print("Please build the project first.")
        sys.exit(1)

    with open(BIN_PATH, 'rb') as f:
        firmware_data = f.read()
    
    firmware_size = len(firmware_data)
    print(f"Firmware Size: {firmware_size} bytes ({firmware_size/1024:.2f} KB)")

    # 2. Initialize CAN Bus (Vector)
    print(f"Initializing Vector CAN Interface (Channel {CAN_CHANNEL})...")
    try:
        # 'app_name' is important for Vector hardware to recognize the application
        bus = can.Bus(interface=CAN_INTERFACE, channel=CAN_CHANNEL, bitrate=BITRATE, app_name=APP_NAME)
        print("CAN Bus Connected.")
    except Exception as e:
        print(f"\nCRITICAL ERROR: Could not connect to Vector CAN hardware.")
        print(f"Details: {e}")
        print("\nTroubleshooting:")
        print("1. Ensure Vector hardware (VN16xx, etc.) is connected via USB.")
        print("2. Ensure Vector drivers are installed.")
        print("3. Check 'Vector Hardware Config' tool to see if channels are assigned.")
        sys.exit(1)

    try:
        # 3. Enter Reflash Mode & Send Size
        print("\nSending 'Enter Reflash Mode' command with Size...")
        # Protocol: ID=0x010, Data=[CMD_REFLASH_MODE, DEVICE_ID, Size3, Size2, Size1, Size0, 0, 0]
        size_bytes = struct.pack('>I', firmware_size)
        data_packet = [CMD_REFLASH_MODE, DEVICE_ID] + list(size_bytes) + [0, 0]
        send_frame(bus, CAN_CMD_ID, data_packet)
        
        # Give the ESP32 time to switch tasks/modes and erase flash
        time.sleep(ESP32_REFLASH_DELAY)

        # 4. Stream Firmware Data
        print("Flashing Firmware...")
        start_time = time.time()
        
        # Chunk data into 7-byte blocks (leaving 1 byte for CRC)
        chunks = [firmware_data[i:i+7] for i in range(0, firmware_size, 7)]
        total_chunks = len(chunks)
        
        total_errors = 0

        for i, chunk in enumerate(chunks):
            # Pad chunk if less than 7 bytes
            if len(chunk) < 7:
                chunk += b'\xFF' * (7 - len(chunk))
                
            # Calculate CRC byte
            crc_byte = calculate_crc_byte(chunk)
            
            # Construct 8-byte payload
            payload = list(chunk) + [crc_byte]
            
            # Track overall wait and time since last ACK/NACK for this payload
            start_wait_time = time.time()
            last_response_time = start_wait_time  # last ACK/NACK (or initial send anchor)

            def send_with_retry():
                nonlocal last_response_time
                while not send_frame(bus, DEVICE_ID, payload):
                    if (time.time() - start_wait_time) > TIMEOUT_COMMS:
                        raise TimeoutError(f"Communication Timeout: Unable to send frame for {TIMEOUT_COMMS}s")
                    time.sleep(0.1)
                # Anchor the "no response" timer to when we sent
                last_response_time = time.time()

            # Initial send: block/retry on TX errors but don't spam
            send_with_retry()

            # Wait for ACK/NACK. Ignore other bus traffic; only ACK/NACK
            # affects resend timing.
            while True:
                now = time.time()
                if (now - start_wait_time) > TIMEOUT_COMMS:
                    raise TimeoutError(f"Communication Timeout: No ACK or NACK received for {TIMEOUT_COMMS}s")

                # Short receive window so we can periodically check resend condition
                recv_timeout = min(0.1, max(0.0, TIMEOUT_COMMS - (now - start_wait_time)))

                try:
                    msg = bus.recv(timeout=recv_timeout)
                except can.CanError:
                    msg = None

                if msg and msg.arbitration_id == DEVICE_ID:
                    # Some frames may have no or too few data bytes; guard against that.
                    data_len = len(msg.data) if msg.data is not None else 0
                    if data_len == 0:
                        # Empty payload from our device, ignore
                        pass
                    else:
                        first_byte = msg.data[0]

                        if first_byte == 0xFF:
                            # ACK
                            break
                        elif first_byte == 0x00:
                            # NACK - Error Count in [1:3] (Little Endian)
                            if data_len >= 3:
                                error_count = msg.data[1] | (msg.data[2] << 8)
                                total_errors += 1
                                print_progress(i, total_chunks, prefix='Progress:', suffix=f'Complete (Err: {total_errors})', length=40)

                            # Resend immediately on explicit NACK and reset response timer
                            send_with_retry()

                    # Either way, after handling our own device message, check resend timer below

                # If we haven't seen an ACK/NACK for RESEND_INTERVAL seconds,
                # resend the frame regardless of other bus traffic.
                if (time.time() - last_response_time) >= RESEND_INTERVAL:
                    send_with_retry()

            # Update Progress Bar every 10 chunks to reduce console I/O overhead
            if i % 10 == 0 or i == total_chunks - 1:
                print_progress(i + 1, total_chunks, prefix='Progress:', suffix=f'Complete (Err: {total_errors})', length=40)

        end_time = time.time()
        duration = end_time - start_time
        speed_kbs = (firmware_size / 1024) / duration

        print(f"\nFlash Complete!")
        print(f"Time Elapsed: {duration:.2f}s")
        print(f"Average Speed: {speed_kbs:.2f} KB/s")
        print("The device should now validate and restart.")

    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    except Exception as e:
        print(f"\nAn error occurred during flashing: {e}")
    finally:
        bus.shutdown()
        print("CAN Bus Shutdown.")

if __name__ == "__main__":
    main()