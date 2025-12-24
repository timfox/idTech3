#!/bin/bash

# rename_md_files.sh: Rename .md files from ALL_CAPS_WITH_UNDERSCORES to lowercase-with-dashes

set -e

DOCS_DIR="/home/tim/Desktop/idtech3/docs"

echo "Renaming .md files in $DOCS_DIR to lowercase-with-dashes format..."

cd "$DOCS_DIR"

for file in *.md; do
    # Skip if already in lowercase-with-dashes format
    if [[ "$file" =~ [a-z]+(-[a-z]+)*\.md$ ]]; then
        echo "Skipping: $file (already lowercase)"
        continue
    fi

    # Convert to lowercase and replace underscores with dashes
    new_name=$(echo "$file" | tr '[:upper:]' '[:lower:]' | sed 's/_/-/g')

    if [ "$file" != "$new_name" ]; then
        echo "Renaming: $file → $new_name"
        mv "$file" "$new_name"
    fi
done

echo "Done! All .md files renamed."
echo ""
echo "Now regenerate documentation:"
echo "  cd ../tools && ./md2web.sh --all"
