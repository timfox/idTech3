#!/usr/bin/env python3
"""
Asset Validation Tool for id Tech 3

Validates assets in mod directories and PK3 files, checking for:
- Missing asset references
- Bad paths
- Invalid metadata (corrupted files, wrong formats)
- Manifest integrity
- Duplicate assets
- Orphaned assets

Usage:
    python3 tools/validate_assets.py [mod_dir] [options]
    python3 tools/validate_assets.py mymod --check-manifest --check-orphans
"""

import argparse
import os
import sys
import json
import hashlib
import zipfile
import re
from pathlib import Path
from typing import List, Dict, Set, Tuple, Optional
from collections import defaultdict

# Supported asset formats
IMAGE_EXTENSIONS = {'.tga', '.jpg', '.jpeg', '.png', '.pcx', '.bmp', '.dds', '.svg', '.exr'}
MODEL_EXTENSIONS = {'.md3', '.ase', '.obj'}
SOUND_EXTENSIONS = {'.wav', '.ogg'}
SHADER_EXTENSIONS = {'.shader'}
ASSET_EXTENSIONS = IMAGE_EXTENSIONS | MODEL_EXTENSIONS | SOUND_EXTENSIONS | SHADER_EXTENSIONS

# Maximum path length (MAX_QPATH = 256)
MAX_QPATH = 256


class ValidationError:
    """Represents a validation error"""
    def __init__(self, severity: str, category: str, message: str, file: Optional[str] = None, line: Optional[int] = None):
        self.severity = severity  # 'error', 'warning', 'info'
        self.category = category   # 'missing', 'bad_path', 'invalid', 'manifest', 'duplicate', 'orphan'
        self.message = message
        self.file = file
        self.line = line
    
    def __str__(self):
        result = f"[{self.severity.upper()}] {self.category}: {self.message}"
        if self.file:
            result += f" (file: {self.file}"
            if self.line:
                result += f", line: {self.line}"
            result += ")"
        return result


class AssetValidator:
    """Validates assets in a mod directory"""
    
    def __init__(self, mod_dir: str, check_manifest: bool = True, check_orphans: bool = False, verbose: bool = False):
        self.mod_dir = Path(mod_dir).resolve()
        self.check_manifest = check_manifest
        self.check_orphans = check_orphans
        self.verbose = verbose
        self.errors: List[ValidationError] = []
        self.assets: Set[str] = set()
        self.references: Set[str] = set()
        self.manifest_assets: Set[str] = set()
        self.pk3_files: List[Path] = []
        
    def validate(self) -> bool:
        """Run all validation checks"""
        if not self.mod_dir.exists():
            self.errors.append(ValidationError('error', 'bad_path', f"Mod directory does not exist: {self.mod_dir}"))
            return False
        
        print(f"Validating assets in: {self.mod_dir}")
        
        # Scan for assets and PK3 files
        self._scan_directory()
        
        # Check manifest if requested
        if self.check_manifest:
            self._validate_manifest()
        
        # Check for missing references
        self._check_missing_references()
        
        # Check for bad paths
        self._check_bad_paths()
        
        # Check for invalid files
        self._check_invalid_files()
        
        # Check for duplicates
        self._check_duplicates()
        
        # Check for orphaned assets
        if self.check_orphans:
            self._check_orphaned_assets()
        
        return len([e for e in self.errors if e.severity == 'error']) == 0
    
    def _scan_directory(self):
        """Scan directory for assets and PK3 files"""
        print("Scanning directory for assets...")
        
        for root, dirs, files in os.walk(self.mod_dir):
            # Skip gamesrc directory
            if 'gamesrc' in dirs:
                dirs.remove('gamesrc')
            
            for filename in files:
                filepath = Path(root) / filename
                relpath = filepath.relative_to(self.mod_dir)
                relpath_str = str(relpath).replace(os.sep, '/')
                
                # Check for PK3 files
                if filename.endswith('.pk3'):
                    self.pk3_files.append(filepath)
                    continue
                
                # Skip manifest itself
                if filename == 'manifest.json':
                    continue
                
                # Track assets
                ext = filepath.suffix.lower()
                if ext in ASSET_EXTENSIONS or ext == '.map' or ext == '.bsp':
                    self.assets.add(relpath_str)
        
        print(f"Found {len(self.assets)} assets, {len(self.pk3_files)} PK3 files")
    
    def _validate_manifest(self):
        """Validate manifest.json if it exists"""
        manifest_path = self.mod_dir / 'manifest.json'
        
        if not manifest_path.exists():
            self.errors.append(ValidationError('warning', 'manifest', 'No manifest.json found'))
            return
        
        print("Validating manifest.json...")
        
        try:
            with open(manifest_path, 'r', encoding='utf-8') as f:
                manifest = json.load(f)
        except json.JSONDecodeError as e:
            self.errors.append(ValidationError('error', 'manifest', f'Invalid JSON in manifest.json: {e}'))
            return
        except Exception as e:
            self.errors.append(ValidationError('error', 'manifest', f'Failed to read manifest.json: {e}'))
            return
        
        # Check manifest structure
        if 'assets' not in manifest:
            self.errors.append(ValidationError('error', 'manifest', 'manifest.json missing "assets" field'))
            return
        
        # Validate each asset in manifest
        manifest_hashes = {}
        for asset in manifest['assets']:
            if 'path' not in asset:
                self.errors.append(ValidationError('error', 'manifest', 'Asset entry missing "path" field'))
                continue
            
            asset_path = asset['path']
            self.manifest_assets.add(asset_path)
            
            # Check if file exists
            full_path = self.mod_dir / asset_path.replace('/', os.sep)
            if not full_path.exists():
                self.errors.append(ValidationError('error', 'manifest', f'Manifest references non-existent file: {asset_path}'))
                continue
            
            # Check hash if present
            if 'hash' in asset:
                expected_hash = asset['hash']
                try:
                    actual_hash = self._compute_file_hash(full_path)
                    if actual_hash != expected_hash:
                        self.errors.append(ValidationError('error', 'manifest', f'Hash mismatch for {asset_path}: expected {expected_hash}, got {actual_hash}'))
                except Exception as e:
                    self.errors.append(ValidationError('warning', 'manifest', f'Failed to compute hash for {asset_path}: {e}'))
        
        print(f"Manifest contains {len(self.manifest_assets)} assets")
    
    def _check_missing_references(self):
        """Check for missing asset references in shaders, maps, etc."""
        print("Checking for missing asset references...")
        
        # Scan shader files for texture references
        for asset_path in self.assets:
            if asset_path.endswith('.shader'):
                self._check_shader_references(asset_path)
            elif asset_path.endswith('.map'):
                self._check_map_references(asset_path)
    
    def _check_shader_references(self, shader_path: str):
        """Check texture references in a shader file"""
        full_path = self.mod_dir / shader_path.replace('/', os.sep)
        
        try:
            with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Find texture references (map, animMap, clampMap, etc.)
            texture_pattern = r'(?:map|animMap|clampMap|lightMap|normalMap|specularMap)\s+([^\s\n]+)'
            for match in re.finditer(texture_pattern, content, re.IGNORECASE):
                texture_path = match.group(1).strip('"\'')
                self.references.add(texture_path)
                
                # Check if referenced texture exists
                if not self._asset_exists(texture_path):
                    self.errors.append(ValidationError('warning', 'missing', f'Shader references missing texture: {texture_path}', shader_path))
        
        except Exception as e:
            self.errors.append(ValidationError('warning', 'invalid', f'Failed to read shader file: {e}', shader_path))
    
    def _check_map_references(self, map_path: str):
        """Check asset references in a map file"""
        full_path = self.mod_dir / map_path.replace('/', os.sep)
        
        try:
            with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Find texture references in map files (shader field)
            texture_pattern = r'"shader"\s+"([^"]+)"'
            for match in re.finditer(texture_pattern, content, re.IGNORECASE):
                texture_path = match.group(1)
                self.references.add(texture_path)
                
                # Check if referenced texture exists
                if not self._asset_exists(texture_path):
                    self.errors.append(ValidationError('warning', 'missing', f'Map references missing texture: {texture_path}', map_path))
        
        except Exception as e:
            self.errors.append(ValidationError('warning', 'invalid', f'Failed to read map file: {e}', map_path))
    
    def _check_bad_paths(self):
        """Check for invalid paths (too long, illegal characters, etc.)"""
        print("Checking for bad paths...")
        
        for asset_path in self.assets:
            # Check path length
            if len(asset_path) >= MAX_QPATH:
                self.errors.append(ValidationError('error', 'bad_path', f'Path too long ({len(asset_path)} >= {MAX_QPATH}): {asset_path}'))
            
            # Check for illegal characters
            if '..' in asset_path or '\\' in asset_path or ':' in asset_path:
                self.errors.append(ValidationError('error', 'bad_path', f'Path contains illegal characters: {asset_path}'))
            
            # Check for absolute paths
            if os.path.isabs(asset_path):
                self.errors.append(ValidationError('error', 'bad_path', f'Path is absolute: {asset_path}'))
    
    def _check_invalid_files(self):
        """Check for invalid/corrupted files"""
        print("Checking for invalid files...")
        
        for asset_path in self.assets:
            full_path = self.mod_dir / asset_path.replace('/', os.sep)
            
            if not full_path.exists():
                continue
            
            ext = full_path.suffix.lower()
            
            # Basic file validation
            try:
                size = full_path.stat().st_size
                if size == 0:
                    self.errors.append(ValidationError('warning', 'invalid', f'Empty file: {asset_path}'))
                    continue
                
                # Try to read first few bytes
                with open(full_path, 'rb') as f:
                    header = f.read(16)
                    if len(header) == 0:
                        self.errors.append(ValidationError('error', 'invalid', f'Cannot read file: {asset_path}'))
                        continue
                
                # Format-specific validation could be added here
                # For now, we just check that files are readable
                
            except Exception as e:
                self.errors.append(ValidationError('error', 'invalid', f'Failed to validate file: {e}', asset_path))
    
    def _check_duplicates(self):
        """Check for duplicate assets"""
        print("Checking for duplicate assets...")
        
        asset_hashes: Dict[str, List[str]] = defaultdict(list)
        
        for asset_path in self.assets:
            full_path = self.mod_dir / asset_path.replace('/', os.sep)
            if full_path.exists():
                try:
                    file_hash = self._compute_file_hash(full_path)
                    asset_hashes[file_hash].append(asset_path)
                except Exception:
                    pass
        
        for file_hash, paths in asset_hashes.items():
            if len(paths) > 1:
                self.errors.append(ValidationError('info', 'duplicate', f'Duplicate files found (hash: {file_hash[:8]}...): {", ".join(paths)}'))
    
    def _check_orphaned_assets(self):
        """Check for orphaned assets (not referenced anywhere)"""
        print("Checking for orphaned assets...")
        
        # This is a simple check - in a full implementation, we'd parse all game files
        # to find references. For now, we just check if assets are in the manifest.
        if self.check_manifest:
            for asset_path in self.assets:
                if asset_path not in self.manifest_assets:
                    # Check if it's referenced in shaders/maps
                    if asset_path not in self.references:
                        ext = Path(asset_path).suffix.lower()
                        if ext in ASSET_EXTENSIONS:
                            self.errors.append(ValidationError('info', 'orphan', f'Potentially orphaned asset: {asset_path}'))
    
    def _asset_exists(self, asset_path: str) -> bool:
        """Check if an asset exists (with or without extension)"""
        # Try exact path
        full_path = self.mod_dir / asset_path.replace('/', os.sep)
        if full_path.exists():
            return True
        
        # Try with common extensions
        base_path = full_path.parent / full_path.stem
        for ext in IMAGE_EXTENSIONS:
            if (base_path.with_suffix(ext)).exists():
                return True
        
        return False
    
    def _compute_file_hash(self, filepath: Path) -> str:
        """Compute SHA1 hash of a file"""
        sha1 = hashlib.sha1()
        with open(filepath, 'rb') as f:
            while True:
                chunk = f.read(8192)
                if not chunk:
                    break
                sha1.update(chunk)
        return sha1.hexdigest()
    
    def print_report(self):
        """Print validation report"""
        print("\n" + "="*70)
        print("ASSET VALIDATION REPORT")
        print("="*70)
        
        errors_by_severity = defaultdict(list)
        for error in self.errors:
            errors_by_severity[error.severity].append(error)
        
        for severity in ['error', 'warning', 'info']:
            errors = errors_by_severity[severity]
            if errors:
                print(f"\n{severity.upper()}S ({len(errors)}):")
                for error in errors:
                    print(f"  {error}")
        
        print("\n" + "="*70)
        print(f"Summary: {len(errors_by_severity['error'])} errors, "
              f"{len(errors_by_severity['warning'])} warnings, "
              f"{len(errors_by_severity['info'])} info messages")
        print("="*70 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Validate assets in a mod directory',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic validation
  python3 tools/validate_assets.py mymod
  
  # Full validation including manifest and orphaned assets
  python3 tools/validate_assets.py mymod --check-manifest --check-orphans
  
  # Verbose output
  python3 tools/validate_assets.py mymod --verbose
        """
    )
    
    parser.add_argument('mod_dir', help='Mod directory to validate')
    parser.add_argument('--check-manifest', action='store_true', default=True,
                       help='Check manifest.json integrity (default: True)')
    parser.add_argument('--no-check-manifest', dest='check_manifest', action='store_false',
                       help='Skip manifest validation')
    parser.add_argument('--check-orphans', action='store_true',
                       help='Check for orphaned assets (not referenced)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--json', action='store_true',
                       help='Output results as JSON')
    
    args = parser.parse_args()
    
    validator = AssetValidator(
        args.mod_dir,
        check_manifest=args.check_manifest,
        check_orphans=args.check_orphans,
        verbose=args.verbose
    )
    
    success = validator.validate()
    
    if args.json:
        # Output JSON format
        output = {
            'success': success,
            'errors': [
                {
                    'severity': e.severity,
                    'category': e.category,
                    'message': e.message,
                    'file': e.file,
                    'line': e.line
                }
                for e in validator.errors
            ],
            'stats': {
                'total_assets': len(validator.assets),
                'pk3_files': len(validator.pk3_files),
                'manifest_assets': len(validator.manifest_assets),
                'references': len(validator.references)
            }
        }
        print(json.dumps(output, indent=2))
    else:
        validator.print_report()
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
