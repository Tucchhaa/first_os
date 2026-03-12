from glob import glob
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

def main():
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--platform", choices=get_platforms(), required=True)
    args = arg_parser.parse_args()

    # Remove old build files
    execute(["rm", "-rf", OBJ_DIR])

    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(OBJ_DIR, exist_ok=True)

    for pattern in ["*.elf", "*.bin", "*.fit"]:
        for f in glob(os.path.join(BUILD_DIR, pattern)):
            os.remove(f)

    # Build
    bootloader = BootLoader(args.platform)
    bootloader.build()
    bootloader.create_fit_image()

    kernel = Kernel(args.platform)
    kernel.build()

    print("\n\nDone!")


def get_platforms():
    platforms = []

    for entry in os.listdir(PLATFORM_DIR):
        path = os.path.join(PLATFORM_DIR, entry)

        if os.path.isdir(path):
            platforms.append(entry)

    return platforms

def execute(cmd, cleanup=False):
    result = subprocess.run(cmd)

    if cleanup:
        cleanup()

    if result.returncode != 0:
        print(f"Error during: {' '.join(cmd)}")
        sys.exit(1)

class Project:
    def __init__(self, platform):
        self.name = None
        self.platform = platform
        self.platform_dir = os.path.join(PLATFORM_DIR, self.platform)
        self.linker_script = None
        self.source_files = []
    
    def build(self):
        print(f"\n\n[Building {self.name}]")

        # Compilation
        print("=> Compilation")

        obj_files = []

        for c_file in self.source_files:
            out_file = os.path.relpath(c_file, SRC_DIR).replace(".c", ".o").replace(".S", ".o")
            out_path = os.path.join(OBJ_DIR, out_file)

            print(f"Compiling: {c_file} -> {out_path}")

            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            obj_files.append(out_path)

            cmd = [COMPILER] + ["-c", "-mcmodel=medany", f"-DPLATFORM_{self.platform.upper()}"] + [c_file, "-o", out_path]
            
            execute(cmd)

        # Linking
        print("=> Linking")

        linker_path = os.path.join(self.platform_dir, self.linker_script)
        elf_path = os.path.join(BUILD_DIR, f"{self.name}.elf")
        cmd = [LINKER] + ["-T", linker_path] + ["-o", elf_path] + obj_files 

        execute(cmd)

        # Create binary
        print("=> Creating binary")

        bin_path = os.path.join(BUILD_DIR, f"{self.name}.bin")
        cmd = [OBJ_COPY, "-O", "binary", elf_path, bin_path]

        execute(cmd)

class BootLoader(Project):
    def __init__(self, platform):
        super().__init__(platform)
        self.name = "bootloader"
        self.linker_script = "linker_bootloader.ld"
        self.source_files = [
            "src/bootloader/entry.S",
            "src/bootloader/bootloader.c",
            "src/uart.c",
            "src/string.c"
        ]

    def create_fit_image(self):
        print("=> Creating FIT image")
        its_path = os.path.join(self.platform_dir, "bootloader.its")

        if not os.path.exists(its_path):
            print(f"{its_path} not found, skipping FIT image creation")
            return

        # Copy bin to platform dir for mkimage to find it
        bin_path = os.path.join(BUILD_DIR, f"{self.name}.bin")
        bin_copy_path = os.path.join(self.platform_dir, f"{self.name}.bin")
        
        os.system(f"cp {bin_path} {bin_copy_path}")

        # Create FIT image

        fit_path = os.path.join(BUILD_DIR, f"{self.name}.fit")
        cmd = ["mkimage", "-f", its_path, fit_path]

        execute(cmd, cleanup=lambda: os.remove(bin_copy_path))

class Kernel(Project):
    def __init__(self, platform):
        super().__init__(platform)
        self.name = "kernel"
        self.linker_script = "linker.ld"
        self.source_files = [
            "src/kernel/entry.S",
            "src/kernel/kmain.c",
            "src/uart.c",
            "src/sbi.c",
            "src/string.c"
        ]

main()
