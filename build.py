import os
import subprocess
import sys
import argparse

# Config
COMPILER = "riscv64-unknown-elf-gcc"
LINKER = "riscv64-unknown-elf-ld"
OBJ_COPY = "riscv64-unknown-elf-objcopy"

SRC_DIR = "./src"
BUILD_DIR = "./build"
OBJ_DIR = os.path.join(BUILD_DIR, "obj")
PLATFORM_DIR = "./platform"

def get_platforms():
    platforms = []
    for entry in os.listdir(PLATFORM_DIR):
        path = os.path.join(PLATFORM_DIR, entry)
        if os.path.isdir(path):
            platforms.append(entry)
    return platforms

def execute(stage_name, cmd, cleanup=False):
    print(' '.join(cmd))
    result = subprocess.run(cmd)

    if result.returncode != 0:
        print(f"Error during {stage_name.lower()}")

        if cleanup:
            cleanup()

        sys.exit(1)
    
    if cleanup:
        cleanup()

# Parse command-line arguments
arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("--platform", choices=get_platforms(), required=True)
args = arg_parser.parse_args()

platform_dir = os.path.join(PLATFORM_DIR, args.platform)

# Create build directory if it doesn't exist
os.makedirs(BUILD_DIR, exist_ok=True)
os.makedirs(OBJ_DIR, exist_ok=True)

# Clean up old files
for root, dirs, files in os.walk(OBJ_DIR):
    for file in files:
        os.remove(os.path.join(root, file))

os.remove(os.path.join(BUILD_DIR, "kernel.elf")) if os.path.exists(os.path.join(BUILD_DIR, "kernel.elf")) else None
os.remove(os.path.join(BUILD_DIR, "kernel.bin")) if os.path.exists(os.path.join(BUILD_DIR, "kernel.bin")) else None
os.remove(os.path.join(BUILD_DIR, "kernel.fit")) if os.path.exists(os.path.join(BUILD_DIR, "kernel.fit")) else None

# Find source files
c_files = [
    os.path.join(root, file)
    for root, _, files in os.walk(SRC_DIR)
    for file in files
    if file.endswith(".c") or file.endswith(".S")
]

if not c_files:
    print("No source files found!")
    sys.exit(1)

# Compile each file
obj_files = []

for c_file in c_files:
    src_path = c_file
    out_path = os.path.join(OBJ_DIR, os.path.relpath(c_file, SRC_DIR).replace(".c", ".o").replace(".S", ".o"))

    cmd = [COMPILER] + ["-c", "-mcmodel=medany", f"-DPLATFORM_{args.platform.upper()}"] + [src_path, "-o", out_path]
    execute(f"Compiling: {c_file} -> {out_path}", cmd)

    obj_files.append(out_path)

# Link object files
linker_script = os.path.join(platform_dir, "linker.ld")

cmd = [LINKER] + ["-T", linker_script] + ["-o", os.path.join(BUILD_DIR, "kernel.elf")] + obj_files 
execute("Linking", cmd)

# Create binary
cmd = [OBJ_COPY, "-O", "binary", os.path.join(BUILD_DIR, "kernel.elf"), os.path.join(BUILD_DIR, "kernel.bin")]
execute("Creating binary", cmd)

# Create FIT image
its_path = os.path.join(platform_dir, "kernel.its")

if os.path.exists(its_path):
    os.system(f"cp {os.path.join(BUILD_DIR, 'kernel.bin')} {os.path.join(platform_dir, 'kernel.bin')}")

    cmd = ["mkimage", "-f", os.path.join(platform_dir, "kernel.its"), os.path.join(BUILD_DIR, "kernel.fit")]
    execute("Creating FIT image", cmd, lambda: os.remove(os.path.join(platform_dir, "kernel.bin")))

print("Done!")
