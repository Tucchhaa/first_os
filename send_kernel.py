import argparse
import struct
import sys
import time
import serial

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("--device", required=True)
arg_parser.add_argument("--baud", type=int, default=115200)
args = arg_parser.parse_args()
device = args.device

ACK_MAGIC = 0x544F4F42

with open('./build/kernel.bin', 'rb') as f:  # 'rb' = read binary
    kernel_data = f.read()

header = struct.pack('<II',
    ACK_MAGIC,
    len(kernel_data),
)

with serial.Serial(
    device,
    baudrate=args.baud,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    rtscts=False,   # disable hardware flow control
    dsrdtr=False,   # disable DSR/DTR flow control
    timeout=10,
) as tty:
    tty.write(header)
    tty.write(kernel_data)
    tty.flush()

    ack_bytes = struct.pack('<I', ACK_MAGIC)
    timeout_s = 10
    deadline = time.monotonic() + timeout_s
    tty.timeout = 0.1

    buffer = b""
    ok = False
    while time.monotonic() < deadline:
        chunk = tty.read(64)
        if not chunk:
            continue
        sys.stdout.write(chunk.decode("ascii", errors="replace"))
        sys.stdout.flush()
        buffer += chunk
        if ack_bytes in buffer:
            ok = True
            break
        if len(buffer) > 4096:
            buffer = buffer[-4:]

    if ok:
        print("\nSuccess: kernel received by bootloader")
    else:
        print(f"\nTimeout: did not receive ack 0x{ACK_MAGIC:08X} within {timeout_s}s")
        sys.exit(1)
