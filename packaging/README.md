# Packaging scripts

## Entry points

- `build_3rdparty_static_debian.sh`: builds static third-party dependencies for Linux/debian environments.
- `build_3rdparty_static_win32.sh`: builds static third-party dependencies for Windows (MinGW) environments.
- `make_debian_package.sh`: creates a debian package.

## Directory layout

- `debian_3rdparty/`: per-library Linux/debian dependency builders.
- `win32_3rdparty/`: per-library Windows dependency builders.
- `darwin/`: macOS packaging utilities.
- `win32/`: NSIS installer resources.

## Notes

- The legacy Travis-based wheel build flow has been removed in favor of GitHub Actions + cibuildwheel.
- Both static build entry points use the same ordered library build list for easier maintenance.
