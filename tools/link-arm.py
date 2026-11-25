#!/usr/bin/env python3
"""Link a GTK2 ARM executable against the Kindle's public dynamic imports.

The temporary libraries only supply symbol names to the linker. The output
resolves all imports on the device and never contains or ships those stubs.
For builds with a firmware sysroot, prefer the normal Meson SDK target.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, capture=False):
    result = subprocess.run(args, check=True, text=True, capture_output=capture)
    return result.stdout if capture else None


def library(symbol):
    if symbol.startswith("cairo_"):
        return "libcairo.so.2"
    if symbol.startswith("atk_"):
        return "libatk-1.0.so.0"
    if symbol.startswith("pango_"):
        return "libpango-1.0.so.0"
    if symbol.startswith("gtk_"):
        return "libgtk-x11-2.0.so.0"
    if symbol.startswith("gdk_"):
        return "libgdk-x11-2.0.so.0"
    if symbol.startswith(("g_object_", "g_signal_")):
        return "libgobject-2.0.so.0"
    if symbol.startswith("g_"):
        return "libglib-2.0.so.0"
    if symbol in {"__libc_start_main", "memset", "memcpy", "open", "ioctl"}:
        return "libc.so.6"
    raise ValueError(f"Unexpected ARM import {symbol}")


def build(mode, output):
    compiler = os.environ.get("ARM_CLANG", "clang")
    target = "arm-linux-gnueabihf" if mode == "hard" else "arm-linux-gnueabi"
    compiler_flags = (
        f"--target={target}", "-march=armv7-a", "-mfpu=vfpv3-d16",
        f"-mfloat-abi={mode}", "-marm",
    )
    interpreter = "/lib/ld-linux-armhf.so.3" if mode == "hard" else "/lib/ld-linux.so.3"
    prefix = os.environ.get("ARM_BINUTILS_PREFIX", "arm-none-eabi-")
    with tempfile.TemporaryDirectory(prefix="hnreader-arm-") as tmp:
        tmp = Path(tmp)
        main = str(tmp / "main.o")
        start = str(tmp / "start.o")
        run(compiler, *compiler_flags, "-ffreestanding", "-fno-stack-protector", "-fno-pie",
            "-DHNREADER_ARM_IMPORTS", "-Werror", "-Wall", "-Wextra", "-O2", "-I", str(ROOT / "tools"),
            "-c", str(ROOT / "src/main.c"), "-o", main)
        run(compiler, *compiler_flags, "-c", str(ROOT / "tools/arm-start.S"), "-o", start)
        inputs = (main, start)
        used = set()
        provided = set()
        for obj in inputs:
            for line in run(prefix + "nm", obj, capture=True).splitlines():
                fields = line.split()
                if fields[0] == "U":
                    used.add(fields[1])
                elif fields[-2] in {"T", "W", "D", "B"}:
                    provided.add(fields[-1])

        libs = {}
        for symbol in sorted(used - provided):
            libs.setdefault(library(symbol), []).append(symbol)

        for soname, symbols in libs.items():
            asm = tmp / (soname + ".s")
            asm.write_text(".syntax unified\n.arm\n.text\n" + "".join(
                f".global {name}\n.type {name}, %function\n{name}: bx lr\n" for name in symbols
            ) + '.section .note.GNU-stack,"",%progbits\n')
            stub = str(asm.with_suffix(".o"))
            run(compiler, *compiler_flags, "-c", str(asm), "-o", stub)
            run(prefix + "ld", "-shared", "-soname", soname, "-o", str(tmp / soname), stub)

        output.parent.mkdir(parents=True, exist_ok=True)
        run(prefix + "ld", "-z", "noexecstack", "-Ttext-segment=0x10000", "--dynamic-linker", interpreter,
            "--entry", "_start", "-o", str(output), *inputs,
            *(str(tmp / soname) for soname in libs))
        run(prefix + "strip", "--strip-unneeded", str(output))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("softfp", "hard"))
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    build(arguments.mode, arguments.output)
