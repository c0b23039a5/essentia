#!/usr/bin/env python3
import base64
import csv
import hashlib
import subprocess
import sys
import tempfile
import zipfile
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


def _find_dist_info_dir(root: Path) -> Path:
    dist_info_dirs = sorted(p for p in root.iterdir() if p.is_dir() and p.name.endswith(".dist-info"))
    if len(dist_info_dirs) != 1:
        raise RuntimeError(f"Expected exactly 1 .dist-info directory, got {len(dist_info_dirs)}")
    return dist_info_dirs[0]


def _hash_file(path: Path) -> tuple[str, str]:
    data = path.read_bytes()
    digest = hashlib.sha256(data).digest()
    b64 = base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")
    return f"sha256={b64}", str(len(data))


def _rewrite_record(root: Path) -> None:
    dist_info = _find_dist_info_dir(root)
    record_path = dist_info / "RECORD"
    record_rel = record_path.relative_to(root).as_posix()

    rows = []
    for file_path in sorted(p for p in root.rglob("*") if p.is_file()):
        rel = file_path.relative_to(root).as_posix()
        if rel == record_rel:
            continue
        digest, size = _hash_file(file_path)
        rows.append((rel, digest, size))

    rows.append((record_rel, "", ""))
    with record_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerows(rows)


def _unpack_wheel(wheel: Path, dest: Path) -> None:
    with zipfile.ZipFile(wheel) as zf:
        zf.extractall(dest)


def _pack_wheel(src_dir: Path, out_wheel: Path) -> None:
    with zipfile.ZipFile(out_wheel, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for file_path in sorted(p for p in src_dir.rglob("*") if p.is_file()):
            zf.write(file_path, arcname=file_path.relative_to(src_dir).as_posix())


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
        wheel_root = tmp_path / "wheel"
        wheel_root.mkdir(parents=True, exist_ok=True)
        _unpack_wheel(wheel, wheel_root)
        _thin_universal_binaries_to_arm64(wheel_root)
        _rewrite_record(wheel_root)

        repaired_input = tmp_path / wheel.name
        _pack_wheel(wheel_root, repaired_input)
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
