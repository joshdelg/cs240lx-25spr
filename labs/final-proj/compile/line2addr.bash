#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 binary_name (without extension)"
    exit 1
fi

BINARY=$1
CODE_END=$(arm-none-eabi-nm ./${BINARY}.elf | grep __code_end__ | cut -d' ' -f1)

# Convert hex to decimal for comparison
CODE_END_DEC=$((16#$CODE_END))
START_ADDR=0x50000

# Output file
OUT_FILE="${BINARY}.line_map"
rm -f $OUT_FILE

# Track the current line number and start address
current_line=""
start_addr=""

# Iterate through addresses
for ((addr=$START_ADDR; addr<=$CODE_END_DEC; addr+=4)); do
    # Convert decimal to hex for addr2line
    hex_addr=$(printf "0x%x" $addr)
    
    # Get line info for this address
    line_info=$(arm-none-eabi-addr2line -e ${BINARY}.elf $hex_addr)
    
    # Skip if it's not a valid line
    if [[ $line_info == "??:0" ]]; then
        continue
    fi
    
    # Extract filename, function name and line number
    func_info=$(arm-none-eabi-addr2line -f -e ${BINARY}.elf $hex_addr)
    func_name=$(echo "$func_info" | head -n1)
    line_num=$(echo "$func_info" | tail -n1 | sed 's/\(.*\):\(.*\)/\1 \2/')
    
    if [ "$current_line" = "" ]; then
        # First valid line encountered
        current_line="$line_num"
        start_addr=$hex_addr
    elif [ "$line_num" != "$current_line" ]; then
        # Line number changed - write the previous mapping with end address being last instruction
        prev_addr=$(printf "0x%x" $((addr-4)))
        echo "$current_line $func_name $start_addr $prev_addr" >> $OUT_FILE
        current_line="$line_num"
        start_addr=$hex_addr
    fi
done

# Write the final mapping
if [ "$current_line" != "" ]; then
    # Use CODE_END as inclusive end address for final line
    echo "$current_line $start_addr $CODE_END" >> $OUT_FILE
fi
