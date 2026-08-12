import os
import glob
from collections import defaultdict

def process_latest_csv(folder_path):
    csv_files = glob.glob(os.path.join(folder_path, "*.csv"))
    
    if not csv_files:
        print("no file.")
        return

    latest_file = max(csv_files, key=os.path.getmtime)
    print(f"Trace file: {latest_file}\n")

    totals = defaultdict(int)

    with open(latest_file, 'r', encoding='utf-8') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            user_id = parts[0].strip()
            value = int(parts[3].strip())
            
            totals[user_id] += value

    for user_id, total_sum in sorted(totals.items(), key=lambda x: int(x[0])):
        print(f"ID {user_id}: {total_sum}")

process_latest_csv('data/pgr_trace')
