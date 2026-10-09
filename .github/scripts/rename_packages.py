"""Rename final packages without changing upstream executable identities."""

from pathlib import Path
import sys


PACKAGE_SUFFIXES = {".apk", ".exe", ".dmg", ".deb", ".rpm", ".AppImage", ".zip"}


def rename_packages(directory: Path) -> list[Path]:
    packages = sorted(
        path for path in directory.iterdir()
        if path.is_file() and path.suffix in PACKAGE_SUFFIXES
    )
    if not packages:
        raise ValueError(f"No installation packages found in {directory}")
    renamed = []
    for path in packages:
        if path.name.startswith("Bettbox-"):
            destination = path.with_name("Betterbox-" + path.name[len("Bettbox-"):])
            if destination.exists():
                raise FileExistsError(destination)
            path.rename(destination)
            path = destination
        if not path.name.startswith("Betterbox-"):
            raise ValueError(f"Unexpected package name: {path.name}")
        renamed.append(path)
    return renamed


if __name__ == "__main__":
    for package in rename_packages(Path(sys.argv[1] if len(sys.argv) > 1 else "dist")):
        print(package.name)
