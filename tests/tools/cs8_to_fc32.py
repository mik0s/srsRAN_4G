#!/usr/bin/env python3

import argparse
import array
import sys
from pathlib import Path

SCALE = 100.0
CHUNK_BYTES = 1024 * 1024


def convert(src: Path, dst: Path) -> None:
    size = src.stat().st_size

    if size % 2 != 0:
        raise ValueError(
            f"invalid CS8 IQ file size {size}: expected an even number of bytes"
        )

    samples = 0

    with src.open("rb") as fin, dst.open("wb") as fout:
        while True:
            data = fin.read(CHUNK_BYTES)
            if not data:
                break

            iq = array.array("b")
            iq.frombytes(data)

            fc32 = array.array(
                "f",
                (sample / SCALE for sample in iq),
            )

            if sys.byteorder != "little":
                fc32.byteswap()

            fc32.tofile(fout)
            samples += len(iq) // 2

    print(
        f"Converted {samples} complex samples: "
        f"{src} -> {dst}",
        file=sys.stderr,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert normalized interleaved CS8 IQ to little-endian FC32."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    try:
        convert(args.input, args.output)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
