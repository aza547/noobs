#!/usr/bin/env python3
"""Give bundled libs a unique SONAME so they can neither shadow a host lib nor
be shadowed by one. Run against the prebuilt binaries after refreshing them."""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SUFFIX = "-noobs"

ISOLATED = [
    "libavcodec.so.62",
    "libavdevice.so.62",
    "libavfilter.so.11",
    "libavformat.so.62",
    "libavutil.so.60",
    "libswresample.so.6",
    "libswscale.so.9",
    "libbz2.so.1.0",
    "librnnoise.so.0",
    "libspeexdsp.so.1",
    "libx264.so.165",
]

BIN = Path("bin/native/linux")
PLUGINS = Path("bin/obs-plugins/linux")
BIN_RPATH = "$ORIGIN"
PLUGIN_RPATH = "$ORIGIN/../../bin/linux"


def isolated_name(name: str) -> str:
    head, sep, tail = name.partition(".so")
    if not sep:
        raise ValueError(f"not a shared library name: {name}")
    return f"{head}{SUFFIX}{sep}{tail}"


def patchelf(*args: str) -> str:
    return subprocess.run(
        ["patchelf", *args], check=True, capture_output=True, text=True
    ).stdout


def needed(path: Path) -> list[str]:
    return patchelf("--print-needed", str(path)).split()


def is_elf(path: Path) -> bool:
    with path.open("rb") as fh:
        return fh.read(4) == b"\x7fELF"


def elves(directory: Path) -> list[Path]:
    if not directory.is_dir():
        return []
    return sorted(
        p for p in directory.iterdir() if p.is_file() and not p.is_symlink() and is_elf(p)
    )


def rename(bin_dir: Path) -> dict[str, str]:
    renames = {}
    for original in ISOLATED:
        source = bin_dir / original
        if not source.is_file():
            continue
        new = isolated_name(original)
        source.rename(bin_dir / new)
        patchelf("--set-soname", new, str(bin_dir / new))
        renames[original] = new
        print(f"soname   {original} -> {new}")
    return renames


def repoint(targets: list[Path], renames: dict[str, str], root: Path) -> None:
    for target in targets:
        args = []
        for dep in needed(target):
            if dep in renames:
                args += ["--replace-needed", dep, renames[dep]]
        if args:
            patchelf(*args, str(target))
            print(f"needed   {target.relative_to(root)}")


def set_rpaths(bin_dir: Path, plugin_dir: Path, root: Path) -> None:
    for directory, rpath in ((bin_dir, BIN_RPATH), (plugin_dir, PLUGIN_RPATH)):
        for target in elves(directory):
            existing = patchelf("--print-rpath", str(target)).strip()
            if existing and existing != rpath:
                raise SystemExit(
                    f"{target.relative_to(root)} already has rpath {existing!r}, "
                    f"refusing to replace it with {rpath!r}"
                )
            patchelf("--set-rpath", rpath, str(target))
    print(f"rpath    {bin_dir.relative_to(root)}, {plugin_dir.relative_to(root)}")


def relink(bin_dir: Path, renames: dict[str, str]) -> None:
    for link in sorted(p for p in bin_dir.iterdir() if p.is_symlink()):
        target = link.readlink().name
        if target not in renames:
            continue
        new_link = bin_dir / isolated_name(link.name)
        link.unlink()
        new_link.symlink_to(renames[target])
        print(f"symlink  {link.name} -> {new_link.name} -> {renames[target]}")


def verify(targets: list[Path], root: Path) -> int:
    stale = [
        f"{t.relative_to(root)} -> {dep}"
        for t in targets
        for dep in needed(t)
        if dep in ISOLATED
    ]
    for entry in stale:
        print(f"STALE    {entry}", file=sys.stderr)
    return 1 if stale else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        type=Path,
        nargs="?",
        default=Path(__file__).resolve().parent.parent,
        help="noobs repository root",
    )
    parser.add_argument("--verify", action="store_true", help="check only")
    args = parser.parse_args()

    if not sys.platform.startswith("linux"):
        parser.error(f"ELF binaries can only be patched on Linux, not {sys.platform}")

    root = args.root.resolve()
    bin_dir = root / BIN
    plugin_dir = root / PLUGINS

    if not bin_dir.is_dir():
        parser.error(f"no such directory: {bin_dir}")

    if shutil.which("patchelf") is None:
        parser.error("patchelf not found on PATH")

    if args.verify:
        return verify(elves(bin_dir) + elves(plugin_dir), root)

    renames = rename(bin_dir)

    if not renames:
        print("nothing to rename, verifying")
        return verify(elves(bin_dir) + elves(plugin_dir), root)

    targets = elves(bin_dir) + elves(plugin_dir)
    repoint(targets, renames, root)
    set_rpaths(bin_dir, plugin_dir, root)
    relink(bin_dir, renames)

    return verify(elves(bin_dir) + elves(plugin_dir), root)


if __name__ == "__main__":
    sys.exit(main())
