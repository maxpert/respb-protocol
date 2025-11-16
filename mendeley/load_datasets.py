#!/usr/bin/env python3
"""
Load Mendeley datasets into Redis with appropriate prefixes
"""
import redis
import json
import csv
from pathlib import Path
import sys
import time

# Dataset prefixes
PREFIXES = {
    'KVData1': 'synth:kv1:',
    'KVData2': 'synth:kv2:',
    'KVData3': 'synth:kv3:',
    'KVData4': 'synth:kv4:',
    'ID-Geo': 'real:geo:',
    'ID-Hashtag': 'real:hashtag:',
    'ID-Tweet': 'real:tweet:',
    'User-Followers': 'real:followers:',
}

def load_synthetic_dataset(r, file_path, prefix):
    """Load synthetic dataset (key-value pairs)"""
    print(f"Loading {file_path.name} with prefix '{prefix}'...")
    
    count = 0
    # Synthetic datasets are typically CSV or text files with key,value format
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            # Try CSV first
            try:
                reader = csv.reader(f)
                for row in reader:
                    if len(row) >= 2:
                        key = f"{prefix}{row[0]}"
                        value = row[1]
                        r.set(key, value)
                        count += 1
                        if count % 10000 == 0:
                            print(f"  Loaded {count:,} keys...", end='\r')
            except:
                # Try line-by-line format: key,value or key\tvalue
                f.seek(0)
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    
                    # Try comma or tab separator
                    if ',' in line:
                        parts = line.split(',', 1)
                    elif '\t' in line:
                        parts = line.split('\t', 1)
                    else:
                        continue
                    
                    if len(parts) == 2:
                        key = f"{prefix}{parts[0].strip()}"
                        value = parts[1].strip()
                        r.set(key, value)
                        count += 1
                        if count % 10000 == 0:
                            print(f"  Loaded {count:,} keys...", end='\r')
    except Exception as e:
        print(f"  Error loading {file_path.name}: {e}")
        return 0
    
    print(f"  Loaded {count:,} keys")
    return count

def load_real_dataset(r, file_path, prefix, dataset_type):
    """Load real Twitter dataset"""
    print(f"Loading {file_path.name} ({dataset_type}) with prefix '{prefix}'...")
    
    count = 0
    try:
        # Real datasets might be JSON, CSV, or text format
        if file_path.suffix == '.json':
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                if isinstance(data, list):
                    for item in data:
                        if isinstance(item, dict):
                            # Try common key names
                            key_field = None
                            value_field = None
                            for k in ['id', 'ID', 'tweet_id', 'user_id', 'key']:
                                if k in item:
                                    key_field = k
                                    break
                            for v in ['value', 'text', 'tweet', 'hashtag', 'location', 'followers', 'geo']:
                                if v in item:
                                    value_field = v
                                    break
                            
                            if key_field and value_field:
                                key = f"{prefix}{item[key_field]}"
                                value = str(item[value_field])
                                r.set(key, value)
                                count += 1
                                if count % 10000 == 0:
                                    print(f"  Loaded {count:,} keys...", end='\r')
        else:
            # Try CSV or text format
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                reader = csv.DictReader(f) if file_path.suffix == '.csv' else None
                if reader:
                    for row in reader:
                        # Find ID and value columns
                        id_col = None
                        val_col = None
                        for col in row.keys():
                            if 'id' in col.lower() or col.lower() in ['key', 'tweet_id', 'user_id']:
                                id_col = col
                            elif col.lower() in ['value', 'text', 'tweet', 'hashtag', 'location', 'followers', 'geo']:
                                val_col = col
                        
                        if id_col and val_col and row[id_col] and row[val_col]:
                            key = f"{prefix}{row[id_col]}"
                            value = row[val_col]
                            r.set(key, value)
                            count += 1
                            if count % 10000 == 0:
                                print(f"  Loaded {count:,} keys...", end='\r')
                else:
                    # Line-by-line format
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        # Try JSON lines
                        try:
                            item = json.loads(line)
                            if isinstance(item, dict):
                                # Similar logic as above
                                key_val = None
                                value_val = None
                                for k, v in item.items():
                                    if 'id' in k.lower() or k.lower() in ['key', 'tweet_id', 'user_id']:
                                        key_val = str(v)
                                    elif k.lower() in ['value', 'text', 'tweet', 'hashtag', 'location', 'followers', 'geo']:
                                        value_val = str(v)
                                if key_val and value_val:
                                    key = f"{prefix}{key_val}"
                                    r.set(key, value_val)
                                    count += 1
                                    if count % 10000 == 0:
                                        print(f"  Loaded {count:,} keys...", end='\r')
                        except:
                            # Fallback to CSV-like parsing
                            if ',' in line or '\t' in line:
                                parts = line.split(',') if ',' in line else line.split('\t')
                                if len(parts) >= 2:
                                    key = f"{prefix}{parts[0].strip()}"
                                    value = parts[1].strip()
                                    r.set(key, value)
                                    count += 1
                                    if count % 10000 == 0:
                                        print(f"  Loaded {count:,} keys...", end='\r')
    except Exception as e:
        print(f"  Error loading {file_path.name}: {e}")
        import traceback
        traceback.print_exc()
        return 0
    
    print(f"  Loaded {count:,} keys")
    return count

def main():
    data_dir = Path(__file__).parent / "data"
    
    if not data_dir.exists():
        print(f"ERROR: Data directory not found: {data_dir}")
        print("Please run download_datasets.py first")
        sys.exit(1)
    
    # Connect to Redis
    try:
        r = redis.Redis(host='127.0.0.1', port=6379, db=0, decode_responses=False)
        r.ping()
    except redis.ConnectionError:
        print("ERROR: Cannot connect to Redis. Is it running?")
        sys.exit(1)
    
    # Flush database to start fresh
    print("Flushing Redis database...")
    r.flushdb()
    
    total_keys = 0
    
    # Find and load datasets - search recursively in subdirectories
    # EXCLUDE synthetic data - only load real Twitter datasets
    files = list(data_dir.rglob("*"))
    files = [f for f in files if f.is_file() and not f.name.startswith('.')]
    
    # Filter out synthetic datasets
    real_files = []
    for f in files:
        file_path_str = str(f).lower()
        file_name = f.name.lower()
        # Skip synthetic data directories and files
        if ('synthetic' in file_path_str or 
            'data_output' in file_name or 
            'kvdata' in file_name):
            continue
        # Only include real data sets
        if ('real' in file_path_str or 
            'tweet' in file_path_str or 
            'geo' in file_path_str or 
            'hashtag' in file_path_str or 
            'followers' in file_path_str):
            real_files.append(f)
    
    if not real_files:
        print(f"ERROR: No real dataset files found in {data_dir}")
        print("Please extract the zip file first")
        sys.exit(1)
    
    print(f"\nFound {len(real_files)} real dataset file(s) to process (synthetic data excluded)\n")
    
    # Try to match files to datasets - only real datasets
    for file_path in sorted(real_files):
        file_name = file_path.name.lower()
        file_path_str = str(file_path).lower()
        
        # Try to match dataset name in filename or path - only real datasets
        matched = False
        for dataset_name, prefix in PREFIXES.items():
            # Skip synthetic datasets
            if 'kvdata' in dataset_name.lower() or 'synth' in dataset_name.lower():
                continue
                
            dataset_lower = dataset_name.lower()
            # Check if dataset name appears in filename or path
            if (dataset_lower in file_name or 
                dataset_lower.replace('-', '_') in file_name or
                dataset_lower in file_path_str):
                count = load_real_dataset(r, file_path, prefix, dataset_name)
                total_keys += count
                matched = True
                break
        
        if not matched:
            # Try to infer from directory structure - only real datasets
            if 'tweet-geo' in file_path_str or ('geo' in file_name and 'tweet' in file_path_str):
                count = load_real_dataset(r, file_path, PREFIXES['ID-Geo'], 'ID-Geo')
                total_keys += count
                matched = True
            elif 'tweet-hashtag' in file_path_str or ('hashtag' in file_name and 'tweet' in file_path_str):
                count = load_real_dataset(r, file_path, PREFIXES['ID-Hashtag'], 'ID-Hashtag')
                total_keys += count
                matched = True
            elif 'tweet-tweet' in file_path_str or ('tweet' in file_name and 'hashtag' not in file_name and 'geo' not in file_name and 'followers' not in file_name):
                count = load_real_dataset(r, file_path, PREFIXES['ID-Tweet'], 'ID-Tweet')
                total_keys += count
                matched = True
            elif 'user-followers' in file_path_str or ('followers' in file_name and 'user' in file_path_str):
                count = load_real_dataset(r, file_path, PREFIXES['User-Followers'], 'User-Followers')
                total_keys += count
                matched = True
        
        if not matched:
            print(f"Skipping {file_path.relative_to(data_dir)} (unknown dataset type)")
    
    # Force AOF sync and wait for it to complete
    print("\nSyncing AOF to disk...")
    
    # Verify Redis config (dir cannot be changed at runtime - it's protected)
    redis_dir = r.config_get('dir')['dir']
    expected_dir = str(Path(__file__).parent.absolute())
    if redis_dir != expected_dir:
        print(f"  WARNING: Redis dir is '{redis_dir}', expected '{expected_dir}'")
        print(f"  NOTE: 'dir' is a PROTECTED config and must be set in redis.conf at startup")
    
    # Check AOF status
    try:
        info = r.info('persistence')
        aof_enabled = info.get('aof_enabled', 0)
        if aof_enabled != 1:
            print("  WARNING: AOF is not enabled! Enable it in redis.conf")
        else:
            print("  ✓ AOF is enabled")
    except Exception as e:
        print(f"  Warning checking persistence info: {e}")
    
    # Trigger BGREWRITEAOF to compact the AOF file
    try:
        print("  Triggering BGREWRITEAOF to compact AOF...")
        r.bgrewriteaof()
        # Wait for rewrite to complete
        for i in range(60):
            try:
                info = r.info('persistence')
                if info.get('aof_rewrite_in_progress', 0) == 0:
                    break
            except:
                pass
            time.sleep(1)
        print("  ✓ AOF rewrite completed")
    except Exception as e:
        print(f"  Warning: Could not trigger AOF rewrite: {e}")
    
    # Get final stats
    db_size = r.dbsize()
    print(f"\n=== Load Complete ===")
    print(f"Total keys in database: {db_size:,}")
    print(f"Keys loaded in this session: {total_keys:,}")

if __name__ == "__main__":
    main()

