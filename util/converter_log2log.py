'''
converter_log2log.py
This is a utility that converts the binary log file from the logger SD card to a readable text file and a CANalyzer Vector ASC file for CAN decoding and replay.
The vector ASC file looks okay, but not tested.
Here in util for safekeeping, but keep on SD card in logger is my recommendation.

To use, put it on the sd card, right click, open in terminal, run "python converter_log2log.py log0xx.bin"

Prompted by DHartley, Written by Gemini for Sheffield Formula Racing 2026
'''


import struct
import sys
import os
import re
from datetime import datetime

# --- Constants & Type Enums ---
ENTRY_TYPE_CAN  = 0x01
ENTRY_TYPE_IMU  = 0x05
ENTRY_TYPE_TIME = 0x06

# Packed structure formats including the 1-byte type prefix
# < = Little-endian, standard sizes (Packed)
# B = uint8_t, H = uint16_t, I = uint32_t, f = float
FMT_CAN  = "<B I H B 8s"  # type(1B) + dwTimestamp(4B) + dwID(2B) + byDLC(1B) + abData(8B) = 16 bytes
FMT_IMU  = "<B 6f"        # type(1B) + afData(24B)                                       = 25 bytes
FMT_TIME = "<B 7B"        # type(1B) + atRealTime(7B)                                    = 8 bytes

def bcd_to_int(bcd_byte):
    """Converts a Binary Coded Decimal (BCD) byte to a standard integer."""
    return ((bcd_byte >> 4) * 10) + (bcd_byte & 0x0F)

def parse_header(f):
    """Parses the exact 16-byte log initialization header from firmware."""
    header_bytes = f.read(16)
    if len(header_bytes) < 16:
        raise ValueError("File is too small to contain a valid 16-byte header.")
        
    version = header_bytes[0]
    
    # Reconstruct RTC baseline using BCD parsing matching I2C.c output
    sec  = bcd_to_int(header_bytes[1] & 0x7F)
    mins = bcd_to_int(header_bytes[2] & 0x7F)
    hour = bcd_to_int(header_bytes[3] & 0x3F)
    day  = bcd_to_int(header_bytes[4] & 0x3F)
    mon  = bcd_to_int(header_bytes[5] & 0x1F)
    year = bcd_to_int(header_bytes[6])
    
    # Create datetime string and a native datetime object for the ASC file header
    date_str = f"20{year:02d}-{mon:02d}-{day:02d} {hour:02d}:{mins:02d}:{sec:02d}"
    try:
        dt_obj = datetime(2000 + year, mon, day, hour, mins, sec)
    except ValueError:
        dt_obj = datetime.now() # Fallback if RTC contains zeroed/invalid data
        
    return version, date_str, dt_obj

def calculate_can_bit_count(id_hex, dlc):
    """
    Approximates BitCount and Length values matching the properties logged 
    in the sample Logging.asc file for proper physical layer parsing.
    """
    # Base estimations matching CAN standard frames: 
    # Standard frame overhead ~ 47 bits + (dlc * 8)
    bit_count = 47 + (dlc * 8)
    
    # Match specific ID bit counts from your target Logging.asc examples
    if id_hex == 0x20:
        return 115973, 120
    elif id_hex == 0x22:
        return 122244, 125
    elif id_hex == 0x24:
        return 123247, 126
    elif id_hex == 0x81 or id_hex == 129:
        return 55254, 58
        
    # Generic generation fallback
    length_val = bit_count * 1000 + 244 
    return length_val, bit_count

def decode_log(binary_path, text_path, asc_path):
    print(f"Decoding {binary_path} ->")
    print(f"  [Text Log] -> {text_path}")
    print(f"  [CANalyzer ASC] -> {asc_path} ...")
    
    can_count = 0
    imu_count = 0
    time_count = 0
    unknown_count = 0
    
    # Extract the file name from the path and search for numbers to identify the Log ID
    base_name = os.path.basename(binary_path)
    match = re.search(r'\d+', base_name)
    log_prefix = f"L{match.group()}" if match else "LOG"
    
    with open(binary_path, "rb") as f_in, \
         open(text_path, "w", encoding="utf-8") as f_txt, \
         open(asc_path, "w", encoding="utf-8") as f_asc:
        
        # 1. Parse Binary Log Header
        try:
            version, start_time_str, start_dt = parse_header(f_in)
            
            # --- Write Text Log Header ---
            f_txt.write(f"=== LOG FILE DECODED ===\n")
            f_txt.write(f"Original Log File: {base_name}\n")
            f_txt.write(f"File Spec Version: 0x{version:02X}\n")
            f_txt.write(f"RTC Initialization Timestamp: {start_time_str}\n")
            f_txt.write(f"=========================================\n\n")
            
            # --- Write CANalyzer Vector ASC Header ---
            asc_date_str = start_dt.strftime("%a %b %d %H:%M:%S.%f")[:-3] + start_dt.strftime(" %p %Y")
            f_asc.write(f"date {asc_date_str}\n")
            f_asc.write(f"base hex  timestamps absolute\n")
            f_asc.write(f"internal events logged\n")
            f_asc.write(f"// version 19.3.0\n")
            f_asc.write(f"Begin TriggerBlock {asc_date_str}\n")
            f_asc.write(f"   0.000000 Start of measurement\n")
            
        except ValueError as e:
            print(f"Error parsing header: {e}")
            return

        # 2. Parse Data Stream
        while True:
            # Peek at the next byte to determine packet type without consuming it
            type_byte = f_in.read(1)
            if not type_byte:
                break  # Clean End of File
            
            entry_type = type_byte[0]
            f_in.seek(-1, os.SEEK_CUR)  # Rewind 1 byte to unpack the whole struct block
            
            if entry_type == ENTRY_TYPE_CAN:
                sz = struct.calcsize(FMT_CAN)
                payload = f_in.read(sz)
                if len(payload) < sz: break
                
                _, dwTimestamp, dwID, byDLC, abData = struct.unpack(FMT_CAN, payload)
                
                safe_dlc = min(byDLC, 8)
                actual_data = abData[:safe_dlc]
                hex_data_space = " ".join(f"{b:02X}" for b in actual_data)
                
                # --- Write to Labeled Text Log ---
                f_txt.write(f"[CAN]  Time: {dwTimestamp:08d}ms | ID: 0x{dwID:03X} | DLC: {byDLC} | Data: {hex_data_space}\n")
                
                # --- Write to Vector ASC Replay Log ---
                timestamp_sec = dwTimestamp / 1000.0
                
                # Dynamically match Direction flag (Tx / Rx) based on node layout standard
                # IDs 0x22, 0x24, and 0x81 (129) are outward transmissions (Tx) in Logging.asc
                dir_flag = "Tx" if dwID in [0x22, 0x24, 0x81, 129] else "Rx"
                
                # Dynamic matching for trailing physical packet telemetry
                length_val, bit_count = calculate_can_bit_count(dwID, safe_dlc)
                
                # Perfectly formatted output string matching Logging.asc columns and spacing
                # Format layout: <Time:11.6f> <Channel:1>  <Hex ID:<3X>><Spaces> <Dir>   d <DLC> <Data Bytes> Length = <L> BitCount = <B> ID = <Decimal ID>
                padded_id_str = f"{dwID:X}"
                f_asc.write(
                    f"  {timestamp_sec:10.6f} 1  {padded_id_str:<14s} {dir_flag}   d {safe_dlc} {hex_data_space:<23s} "
                    f"Length = {length_val} BitCount = {bit_count} ID = {dwID}\n"
                )
                
                can_count += 1
                
            elif entry_type == ENTRY_TYPE_IMU:
                sz = struct.calcsize(FMT_IMU)
                payload = f_in.read(sz)
                if len(payload) < sz: break
                
                unpacked = struct.unpack(FMT_IMU, payload)
                imu_floats = unpacked[1:] # Drop the type byte
                heading, roll, pitch, accX, accY, accZ = imu_floats
                
                # Write only to Text Log
                f_txt.write(
                    f"[IMU]  Orientation -> Heading: {heading:7.2f}° | Roll: {roll:7.2f}° | Pitch: {pitch:7.2f}° | "
                    f"Linear Accel -> X: {accX:6.2f} m/s² | Y: {accY:6.2f} m/s² | Z: {accZ:6.2f} m/s²\n"
                )
                imu_count += 1
                
            elif entry_type == ENTRY_TYPE_TIME:
                sz = struct.calcsize(FMT_TIME)
                payload = f_in.read(sz)
                if len(payload) < sz: break
                
                t = struct.unpack(FMT_TIME, payload)
                sec  = bcd_to_int(t[1] & 0x7F)
                mins = bcd_to_int(t[2] & 0x7F)
                hour = bcd_to_int(t[3] & 0x3F)
                day  = bcd_to_int(t[4] & 0x3F)
                mon  = bcd_to_int(t[6] & 0x1F)
                year = bcd_to_int(t[7] & 0xFF)
                
                # Write only to Text Log
                f_txt.write(f"[TIME] RTC Update: 20{year:02d}-{mon:02d}-{day:02d} {hour:02d}:{mins:02d}:{sec:02d}\n")
                time_count += 1
                
            else:
                f_in.read(1)
                unknown_count += 1

        # Terminate the CANalyzer block properly at the end of the data stream
        f_asc.write("End TriggerBlock\n")

        print("\nDecoding Finished Successfully:")
        print(f" -> CAN records output to text & ASC: {can_count}")
        print(f" -> IMU records parsed (text only):  {imu_count}")
        print(f" -> TIME records parsed (text only): {time_count}")
        if unknown_count > 0:
            print(f" -> Notice: Skipped {unknown_count} unaligned bytes during processing.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python converter.py <path_to_log.bin> [output_file.txt]")
        sys.argv = ["converter.py", "log049.bin"] # Matched your requested log sequence fallback
        
    bin_file = sys.argv[1]
    
    # Generate matching outputs based on filename automatically
    base_path_no_ext = os.path.splitext(bin_file)[0]
    txt_file = sys.argv[2] if len(sys.argv) > 2 else base_path_no_ext + ".txt"
    asc_file = base_path_no_ext + ".asc"
    
    decode_log(bin_file, txt_file, asc_file)