import argparse
import struct
import serial

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("--device", required=True)
arg_parser.add_argument("--baud", type=int, default=115200)
args = arg_parser.parse_args()
device = args.device

with open('./build/kernel.bin', 'rb') as f:  # 'rb' = read binary
    kernel_data = f.read()

header = struct.pack('<II',
    0x544F4F42,
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
