# GitHub Actions workflows

Active workflows:

- `build-vcpkg.yml`: cross-platform wheel build (cibuildwheel) using vcpkg-managed dependencies on Linux/macOS/Windows, with wheel artifacts uploaded.
- `build-wheels-cibuildwheel.yml`: wheel builds for Linux/macOS/Windows.
- `build-docs.yml`: documentation build validation.

Removed workflows:

- `build-wheels.yml`: legacy manylinux/travis-based wheel flow.
