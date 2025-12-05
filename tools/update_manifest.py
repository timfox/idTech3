import argparse
import os
import json
import hashlib
import re

def compute_file_hash(path):
    """Compute the SHA1 hash of a file."""
    sha1 = hashlib.sha1()
    with open(path, 'rb') as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            sha1.update(chunk)
    return sha1.hexdigest()

def find_assets(mod_dir):
    """Find all asset files to add to the manifest, excluding gamesrc."""
    assets = []
    for root, dirs, files in os.walk(mod_dir):
        # Prevent descent into gamesrc explicitly
        if 'gamesrc' in dirs:
            dirs.remove('gamesrc')
        for filename in files:
            relpath = os.path.relpath(os.path.join(root, filename), mod_dir)
            # Ignore manifest itself
            if relpath == "manifest.json":
                continue
            # Assets usually should not include .pyc, build files, etc.
            if relpath.startswith('.') or relpath.endswith('~'):
                continue
            # Store manifest paths with forward slashes
            assets.append(relpath.replace(os.sep, '/'))
    return assets

def increment_version(version_str):
    """Increment semantic version, e.g. 1.2.3 -> 1.2.4"""
    match = re.match(r'^(\d+)\.(\d+)\.(\d+)', version_str)
    if not match:
        # fallback: if not a version, just return "1.0.1"
        return "1.0.1"
    major, minor, patch = map(int, match.groups())
    patch += 1
    return f"{major}.{minor}.{patch}"

def main():
    parser = argparse.ArgumentParser(description="Update a mod manifest.")
    parser.add_argument("mod", help="The mod directory, e.g. mymod or /mymod")
    args = parser.parse_args()

    mod_name = args.mod.strip("/\\")
    mod_dir = os.path.join(os.getcwd(), mod_name)
    manifest_path = os.path.join(mod_dir, "manifest.json")

    # Load existing manifest or create a new one
    if os.path.isfile(manifest_path):
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    else:
        # Start a new manifest if none exists
        manifest = {
            "manifestVersion": 1,
            "bundleVersion": "1.0.0",
            "gameName": mod_name,
            "assets": [],
            "generatedAt": "",
        }

    # Bump the patch version
    old_version = manifest.get("bundleVersion", "1.0.0")
    manifest["bundleVersion"] = increment_version(old_version)

    # Gather assets and compute their hashes
    assets = []
    for asset_path in sorted(find_assets(mod_dir)):
        # Normalize mixed separators (especially on Windows) before isfile/hash
        full_asset_path = os.path.join(mod_dir, *asset_path.split('/'))
        if os.path.isfile(full_asset_path):
            file_hash = compute_file_hash(full_asset_path)
            assets.append({
                "path": asset_path,
                "hash": file_hash
            })

    manifest["assets"] = assets

    # Set generatedAt to current UTC ISO8601
    from datetime import datetime, timezone
    manifest["generatedAt"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    # Save the manifest
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print(f"Updated manifest at {manifest_path}. Version: {manifest['bundleVersion']}, {len(assets)} assets.")

if __name__ == "__main__":
    main()
