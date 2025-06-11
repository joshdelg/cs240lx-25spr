#!/bin/bash

# Check if input file provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <input.list>"
    exit 1
fi

input=$1
output="${input%.*}.map"

# Process file line by line
while IFS= read -r line; do
    # Skip empty lines and section headers (lines starting at column 0)
    if [[ -z "$line" ]] || [[ "$line" =~ ^[^[:space:]] ]]; then
        continue
    fi

    # Extract address and assembly text
    if [[ $line =~ ^[[:space:]]+([0-9a-f]+):[[:space:]]+([0-9a-f]+)[[:space:]]+(.+)$ ]]; then
        addr="${BASH_REMATCH[1]}"
        asm="${BASH_REMATCH[3]}"
        echo "$addr:   $asm" >> "$output"
    fi
done < "$input"
