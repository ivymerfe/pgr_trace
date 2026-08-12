from collections import defaultdict
import os
import struct


def check_latest_trace(folder_path):
    with os.scandir(folder_path) as entries:
        files = [
            entry
            for entry in entries
            if entry.is_file() and entry.stat().st_size > 0
        ]

    if not files:
        print("no file.")
        return

    latest_entry = max(files, key=lambda entry: entry.stat().st_mtime)
    print(f"Trace file: {latest_entry.path}\n")

    totals = defaultdict(int)

    struct_fmt = "<IIIb"
    with open(latest_entry.path, "rb") as file:
        data = file.read()
        for user_id, index, duration_us, event_type in struct.iter_unpack(struct_fmt, data):
            totals[user_id] += duration_us

    for user_id, total_sum in sorted(totals.items(), key=lambda x: x[0]):
        print(f"ID {user_id}: {total_sum}")


check_latest_trace("data/pgr_trace")
