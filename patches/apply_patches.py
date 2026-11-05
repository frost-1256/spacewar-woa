#!/usr/bin/env python3
"""Apply the spacewar patch specifications to your own copy of a driver binary.

Usage:
    python3 apply_patches.py <driver.sys> <variant>
    python3 apply_patches.py qcdxkm7325.sys dsc2
    python3 apply_patches.py qcwlan7325.sys sarv2-allow

Safety properties:
- The target file is identified by SHA-256 against the pristine hash recorded
  in patches.json; any other file is refused.
- Every patch site's original bytes are re-verified before anything is
  written; if any site does not match, nothing is written.
- The original file is backed up to <file>.bak-<timestamp> first.
- Variants are cumulative: choosing dsc2 also applies capw and dsc1 sites.

After patching you must re-sign the driver with your own test certificate
(signtool /s My /n "<your cert>" /fd SHA256) and run Windows with test
signing enabled; a patched binary with the original signature will not load.
"""
import hashlib
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    path, want = sys.argv[1], sys.argv[2]
    spec = json.load(open(os.path.join(HERE, "patches.json")))

    data = open(path, "rb").read()
    digest = hashlib.sha256(data).hexdigest()

    target = None
    for t in spec["targets"]:
        if t["pristine_sha256"] == digest:
            target = t
            break
    if target is None:
        print(f"REFUSED: {path} (sha256 {digest}) does not match any pristine "
              "target in patches.json. Patch specs are offset-exact; a different "
              "driver build needs different offsets.")
        return 1
    if os.path.basename(path) != target["file"]:
        print(f"note: file content matches {target['file']} although the name differs")

    variants = {v["name"]: v for v in target["variants"]}
    if want not in variants:
        print(f"REFUSED: variant '{want}' not defined for {target['file']}. "
              f"Available: {', '.join(variants)}")
        return 1

    # Resolve cumulative chain in order.
    chain = []
    seen = set()

    def add(name):
        if name in seen:
            return
        for dep in variants[name]["cumulative"]:
            add(dep)
        seen.add(name)
        chain.append(variants[name])

    add(want)
    sites = [s for v in chain for s in v["sites"]]

    buf = bytearray(data)
    for s in sites:
        off = int(s["file_offset"], 16)
        orig = bytes.fromhex(s["orig"])
        new = bytes.fromhex(s["new"])
        cur = bytes(buf[off:off + len(orig)])
        if cur != orig:
            print(f"REFUSED: site '{s['name']}' at {s['file_offset']}: expected "
                  f"original bytes {s['orig']}, found {cur.hex()}. Nothing written.")
            return 1
    for s in sites:
        off = int(s["file_offset"], 16)
        new = bytes.fromhex(s["new"])
        buf[off:off + len(new)] = new
        print(f"patched {s['name']} @ {s['file_offset']}: {s['orig']} -> {s['new']}")

    backup = f"{path}.bak-{time.strftime('%Y%m%d%H%M%S')}"
    with open(backup, "wb") as f:
        f.write(data)
    with open(path, "wb") as f:
        f.write(buf)
    print(f"backup: {backup}")
    print(f"done: {path} now carries variant '{want}' "
          f"(sha256 {hashlib.sha256(bytes(buf)).hexdigest()})")
    print("NEXT: re-sign with your own test certificate before installing; "
          "the original signature no longer matches the file.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
