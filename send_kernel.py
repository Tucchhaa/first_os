import argparse
import struct

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("--device", required=True)
args = arg_parser.parse_args()
device = args.device

with open('./build/kernel.bin', 'rb') as f:  # 'rb' = read binary
    kernel_data = f.read()

header = struct.pack('<II',
    0x544F4F42,
    len(kernel_data),
)

with open(device, "wb", buffering = 0) as tty:
    tty.write(header)
    tty.write(kernel_data)
