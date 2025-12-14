#!/usr/bin/env python3
"""
Save Game Migration Tool

Migrates save games from older formats to newer formats, ensuring
backwards compatibility across engine versions.

Usage:
    python3 tools/migrate_save.py <savefile> [target_version]
    python3 tools/migrate_save.py --batch <directory> [target_version]
"""

import argparse
import os
import sys
import struct
import shutil
from pathlib import Path
from typing import Optional, Tuple

# Save file magic
SAVE_MAGIC = b"Q3SV"

# Current save format version
CURRENT_SAVE_VERSION = 2

def read_save_header(filepath: Path) -> Optional[Tuple[int, int, int]]:
    """Read save file header and return (version, engine_version, checksum)"""
    try:
        with open(filepath, 'rb') as f:
            magic = f.read(4)
            if magic != SAVE_MAGIC:
                return None
            
            version = struct.unpack('<I', f.read(4))[0]
            engine_version = struct.unpack('<I', f.read(4))[0]
            checksum = struct.unpack('<I', f.read(4))[0]
            data_size = struct.unpack('<I', f.read(4))[0]
            
            return (version, engine_version, checksum)
    except Exception as e:
        print(f"Error reading save file: {e}", file=sys.stderr)
        return None

def detect_save_version(filepath: Path) -> Optional[int]:
    """Detect save file version"""
    header = read_save_header(filepath)
    if header:
        return header[0]
    
    # Try to detect legacy format (text-based)
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            first_line = f.readline().strip()
            if first_line.startswith('version'):
                # Legacy text format
                parts = first_line.split()
                if len(parts) >= 2:
                    return int(parts[1])
    except Exception:
        pass
    
    return None

def migrate_v1_to_v2(input_path: Path, output_path: Path) -> bool:
    """Migrate save file from version 1 to version 2"""
    try:
        # Read v1 format (example - adjust based on actual format)
        with open(input_path, 'rb') as f:
            data = f.read()
        
        # Create v2 format header
        with open(output_path, 'wb') as f:
            f.write(SAVE_MAGIC)
            f.write(struct.pack('<I', 2))  # Version 2
            f.write(struct.pack('<I', 1))  # Engine version (placeholder)
            
            # Calculate checksum
            checksum = sum(data) & 0xFFFFFFFF
            f.write(struct.pack('<I', checksum))
            
            # Write data size
            f.write(struct.pack('<I', len(data)))
            
            # Write timestamp
            import time
            f.write(struct.pack('<Q', int(time.time())))
            
            # Write migrated data
            f.write(data)
        
        return True
    except Exception as e:
        print(f"Error migrating v1 to v2: {e}", file=sys.stderr)
        return False

def migrate_save(input_path: Path, target_version: int = CURRENT_SAVE_VERSION, 
                 backup: bool = True) -> bool:
    """Migrate a save file to target version"""
    if not input_path.exists():
        print(f"Error: Save file not found: {input_path}", file=sys.stderr)
        return False
    
    # Detect current version
    current_version = detect_save_version(input_path)
    if current_version is None:
        print(f"Error: Could not detect save file version: {input_path}", file=sys.stderr)
        return False
    
    if current_version == target_version:
        print(f"Save file already at version {target_version}: {input_path}")
        return True
    
    if current_version > target_version:
        print(f"Warning: Save file version {current_version} is newer than target {target_version}")
        print("Downgrading is not supported")
        return False
    
    print(f"Migrating {input_path} from version {current_version} to {target_version}")
    
    # Create backup
    if backup:
        backup_path = input_path.with_suffix(input_path.suffix + '.bak')
        shutil.copy2(input_path, backup_path)
        print(f"Created backup: {backup_path}")
    
    # Temporary output file
    temp_path = input_path.with_suffix(input_path.suffix + '.tmp')
    
    # Perform migration
    success = False
    if current_version == 1 and target_version == 2:
        success = migrate_v1_to_v2(input_path, temp_path)
    else:
        print(f"Error: Migration from version {current_version} to {target_version} not implemented", file=sys.stderr)
        return False
    
    if success:
        # Atomic replace
        temp_path.replace(input_path)
        print(f"Successfully migrated to version {target_version}")
        return True
    else:
        # Cleanup temp file
        if temp_path.exists():
            temp_path.unlink()
        return False

def migrate_batch(directory: Path, target_version: int = CURRENT_SAVE_VERSION) -> int:
    """Migrate all save files in a directory"""
    migrated = 0
    failed = 0
    
    # Find save files (common extensions)
    save_extensions = ['.save', '.sav', '.dat']
    for ext in save_extensions:
        for save_file in directory.rglob(f'*{ext}'):
            if migrate_save(save_file, target_version):
                migrated += 1
            else:
                failed += 1
    
    print(f"\nBatch migration complete:")
    print(f"  Migrated: {migrated}")
    print(f"  Failed: {failed}")
    
    return failed

def main():
    parser = argparse.ArgumentParser(
        description='Migrate save games to newer formats',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Migrate single save file
  python3 tools/migrate_save.py player.save
  
  # Migrate to specific version
  python3 tools/migrate_save.py player.save 2
  
  # Batch migrate all saves in directory
  python3 tools/migrate_save.py --batch ~/.q3a/baseq3/saves/
        """
    )
    
    parser.add_argument('savefile', nargs='?', help='Save file to migrate')
    parser.add_argument('target_version', nargs='?', type=int, default=CURRENT_SAVE_VERSION,
                       help=f'Target version (default: {CURRENT_SAVE_VERSION})')
    parser.add_argument('--batch', '-b', metavar='DIR', help='Batch migrate all saves in directory')
    parser.add_argument('--no-backup', action='store_true', help='Do not create backup files')
    parser.add_argument('--check', action='store_true', help='Check version without migrating')
    
    args = parser.parse_args()
    
    if args.batch:
        # Batch mode
        directory = Path(args.batch)
        if not directory.is_dir():
            print(f"Error: Not a directory: {directory}", file=sys.stderr)
            sys.exit(1)
        
        failed = migrate_batch(directory, args.target_version)
        sys.exit(0 if failed == 0 else 1)
    
    elif args.savefile:
        # Single file mode
        save_path = Path(args.savefile)
        
        if args.check:
            # Check version only
            version = detect_save_version(save_path)
            if version is not None:
                print(f"Save file version: {version}")
                if version < CURRENT_SAVE_VERSION:
                    print(f"Migration available to version {CURRENT_SAVE_VERSION}")
                else:
                    print("Save file is up to date")
                sys.exit(0)
            else:
                print("Error: Could not detect save file version", file=sys.stderr)
                sys.exit(1)
        else:
            # Migrate
            success = migrate_save(save_path, args.target_version, backup=not args.no_backup)
            sys.exit(0 if success else 1)
    
    else:
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()
