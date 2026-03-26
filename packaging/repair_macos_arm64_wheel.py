#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def _run(cmd):
    subprocess.run(cmd, check=True)


def _lipo_info(path: Path) -> str:
    result = subprocess.run(
        ["lipo", "-info", str(path)],
        check=True,
        text=True,
        capture_output=True,
    )
    return result.stdout.strip()


def _thin_universal_binaries_to_arm64(root: Path) -> None:
    for ext in ("*.so", "*.dylib"):
        for binary in root.rglob(ext):
            info = _lipo_info(binary)
            if "x86_64" in info and "arm64" in info:
                tmp_out = binary.with_suffix(binary.suffix + ".arm64")
                _run(["lipo", str(binary), "-thin", "arm64", "-output", str(tmp_out)])
                tmp_out.replace(binary)


def main() -> int:
    if len(sys.argv) < 3:
        print(
            "Usage: repair_macos_arm64_wheel.py <wheel> <dest_dir> [require_archs]",
            file=sys.stderr,
        )
        return 2

    wheel = Path(sys.argv[1]).resolve()
    dest_dir = Path(sys.argv[2]).resolve()
    require_archs = sys.argv[3] if len(sys.argv) >= 4 and sys.argv[3] else "arm64"

    with tempfile.TemporaryDirectory(prefix="essentia-arm64-wheel-") as tmp:
        tmp_path = Path(tmp)
        unpack_dir = tmp_path / "unpacked"
        repacked_dir = tmp_path / "repacked"
        unpack_dir.mkdir(parents=True, exist_ok=True)
        repacked_dir.mkdir(parents=True, exist_ok=True)

        _run([sys.executable, "-m", "wheel", "unpack", str(wheel), "-d", str(unpack_dir)])
        unpacked_wheels = [p for p in unpack_dir.iterdir() if p.is_dir()]
        if len(unpacked_wheels) != 1:
            raise RuntimeError(f"Expected exactly 1 unpacked wheel directory, got {len(unpacked_wheels)}")

        wheel_root = unpacked_wheels[0]
        _thin_universal_binaries_to_arm64(wheel_root)

        _run([sys.executable, "-m", "wheel", "pack", str(wheel_root), "-d", str(repacked_dir)])
        repacked_wheels = list(repacked_dir.glob("*.whl"))
        if len(repacked_wheels) != 1:
            raise RuntimeError(f"Expected exactly 1 repacked wheel, got {len(repacked_wheels)}")

        repaired_input = repacked_wheels[0]
        _run(
            [
                "delocate-wheel",
                "--require-archs",
                require_archs,
                "-w",
                str(dest_dir),
                "-v",
                str(repaired_input),
            ]
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
