"""Reproduce the reviewed Sensors 1.1.2 oracle from the historical oracle.

This reads versioned policy-review data and the original golden. It never runs
or reads output from the classifier. Default operation checks the committed
oracle; --write recreates only the already reviewed expected bytes.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent


def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def derive():
    review = json.loads((HERE / "reviewed-sensors-1.1.2.json").read_text(encoding="utf-8"))
    original = (HERE / "golden_894b508.txt").read_text(encoding="utf-8")
    if digest(original) != review["historicalOutputSha256"]:
        raise ValueError("Historical oracle changed; a new policy review is required")
    header = (HERE / "corpus_894b508.h").read_text(encoding="utf-8")
    uuid_block = header.split("kBleUuidSignatures[] = {", 1)[1].split("};", 1)[0]
    oui_block = header.split("kOuiSignatures[] = {", 1)[1].split("};", 1)[0]
    uuids = [int(x, 16) for x in re.findall(r"\{(0x[0-9A-Fa-f]+)\}", uuid_block)]
    ouis = [":".join(f"{int(x.strip(), 16):02X}" for x in row.split(","))
            for row in re.findall(r"\{\{([^}]+)\}\}", oui_block)]
    if {"uuidKeys": uuids, "ouiKeys": ouis} != review["frozenCorpus"]:
        raise ValueError("Historical corpus keys changed; a new input review is required")
    vendors = review["vendorOuiChanges"]
    if len(vendors) != 19 or "B4:1E:52" in vendors:
        raise ValueError("Unexpected reviewed vendor-prefix set")
    for before_after in vendors.values():
        before, after = before_after["from"], before_after["to"]
        if before[:2] != ["Flock", "High"] or after[:2] != ["Flock", "Low"] or before[2] != after[2]:
            raise ValueError("Unexpected policy translation")
    lines = original.splitlines()
    result, counts, mode, i = [], Counter(), "", 0
    while i < len(lines):
        line = lines[i]
        if line.startswith(("BLE ", "WIFI ")):
            mode = "BLE" if line.startswith("BLE ") else "WIFI"
            result.append(line)
            i += 1
            continue
        if not line.startswith((" advert ", " frame ")):
            raise ValueError("Unexpected historical trace format")
        header = line
        i += 1
        events = []
        while i < len(lines) and lines[i].startswith("  -> "):
            events.append(lines[i])
            i += 1
        if mode == "BLE":
            advert = int(header.split()[1])
            translated = []
            for event in events:
                if " det=10 " in event:
                    if advert == 21:
                        counts["removed_false_subtype_0x1e"] += 1
                        continue
                    if advert not in (20, 31) or "detail=Find My tracker beacon" not in event:
                        raise ValueError("Unreviewed Find My case")
                    event = event.replace("conf=2", "conf=0").replace(
                        "detail=Find My tracker beacon", "detail=Find My beacon, type unknown")
                    counts["short_find_my_low"] += 1
                translated.append(event)
            events = translated
            flock = [j for j, event in enumerate(events) if " det=8 " in event]
            # The one OUI result follows manufacturer records. Preserve the
            # earlier, independently name-gated manufacturer evidence.
            if flock:
                j = flock[-1]
                prefix = re.search(r"addr=([0-9A-F:]+)", events[j]).group(1)[:8]
                if prefix in vendors and " conf=2 " in events[j]:
                    events[j] = events[j].replace("conf=2", "conf=0")
                    counts["ble_vendor_oui_low"] += 1
        else:
            for j, event in enumerate(events):
                if " det=8 " in event:
                    prefix = re.search(r"addr=([0-9A-F:]+)", event).group(1)[:8]
                    if prefix in vendors and " conf=2 " in event:
                        events[j] = event.replace("conf=2", "conf=0")
                        counts["wifi_vendor_oui_low"] += 1
        if events:
            result.append(header)
            result.extend(events)
    expected = "\n".join(result) + "\n"
    if dict(counts) != review["reviewedExpectedChanges"] or digest(expected) != review["expectedOutputSha256"]:
        raise ValueError("Derived changes differ from the reviewed policy record")
    return expected, counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected, counts = derive()
    target = HERE / "golden_sensors_1_1_2.txt"
    if args.write:
        target.write_text(expected, encoding="utf-8", newline="\n")
    elif target.read_text(encoding="utf-8") != expected:
        raise SystemExit("Committed oracle differs from the reviewed derivation")
    print("Reviewed oracle verified: " + json.dumps(dict(counts), sort_keys=True))


if __name__ == "__main__":
    main()
