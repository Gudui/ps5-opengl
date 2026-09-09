#!/usr/bin/env python3
"""Write a complete, atomic SHA-256 manifest for one native title folder."""

import argparse
import hashlib
import os
import re
import sys
import tempfile
from pathlib import Path


TITLE_ID = re.compile(r"PPSA99[0-9]{3}\Z")


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def write_manifest(root: Path, title_id: str) -> Path:
    if not TITLE_ID.fullmatch(title_id):
        raise ValueError(f"invalid title id: {title_id!r}")
    if not root.is_dir() or root.is_symlink():
        raise ValueError(f"manifest root is not a real directory: {root}")

    title = root / title_id
    if not title.is_dir() or title.is_symlink():
        raise ValueError(f"title folder is not a real directory: {title}")

    files = []
    for path in title.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"refusing symlink in title folder: {path}")
        if path.is_file():
            files.append(path)
        elif not path.is_dir():
            raise ValueError(f"refusing non-regular title entry: {path}")
    if not files:
        raise ValueError(f"title folder contains no files: {title}")

    lines = [f"{digest(path)}  {path.relative_to(root).as_posix()}\n"
             for path in sorted(files, key=lambda item: item.relative_to(root).as_posix())]
    manifest = root / "folder-manifest.sha256"
    descriptor, temporary = tempfile.mkstemp(prefix=".folder-manifest.", dir=root, text=True)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.writelines(lines)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, manifest)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--title-id", required=True)
    args = parser.parse_args()
    try:
        manifest = write_manifest(args.root, args.title_id)
    except (OSError, ValueError) as error:
        print(f"folder manifest: {error}", file=sys.stderr)
        return 2
    print(f"Folder manifest: {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
