#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
TMP = ROOT / "tmp"
EIGEN_VERSION = "3.4.0"
EIGEN_ARCHIVE = TMP / f"eigen-{EIGEN_VERSION}.tar.gz"
EIGEN_URL = f"https://gitlab.com/libeigen/eigen/-/archive/{EIGEN_VERSION}/eigen-{EIGEN_VERSION}.tar.gz"
PKGCONFIG_DIR = TMP / "lib" / "pkgconfig"


def ensure_eigen_headers() -> pathlib.Path:
    if not EIGEN_ARCHIVE.exists():
        TMP.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(EIGEN_URL, EIGEN_ARCHIVE)

    with tarfile.open(EIGEN_ARCHIVE, "r:gz") as tf:
        tf.extractall(TMP)

    extracted_root = TMP / f"eigen-{EIGEN_VERSION}"
    return extracted_root


def write_pc_file(include_root: pathlib.Path) -> None:
    PKGCONFIG_DIR.mkdir(parents=True, exist_ok=True)
    content = "\n".join(
        [
            "prefix=/",
            "exec_prefix=${prefix}",
            f"includedir={include_root.as_posix()}",
            "",
            "Name: eigen3",
            "Description: C++ template library for linear algebra",
            "Version: 3.4.0",
            "Cflags: -I${includedir}",
            "",
        ]
    )
    (PKGCONFIG_DIR / "eigen3.pc").write_text(content, encoding="utf-8")


if __name__ == "__main__":
    include_root = ensure_eigen_headers()
    write_pc_file(include_root)
    print(f"Prepared Eigen pkg-config metadata at {(PKGCONFIG_DIR / 'eigen3.pc')}")
