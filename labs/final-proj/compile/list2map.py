#!/usr/bin/env python3

import sys
import re

def process_list_file(filename):
    with open(filename) as f:
        for line in f:
            # Skip empty lines
            if not line.strip():
                continue
                
            # Try to match lines with format: addr: hex instruction
            match = re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]+\s+(.*)', line)
            if match:
                addr = match.group(1)
                instr = match.group(2).strip()
                
                # Skip lines without an instruction
                if not instr or instr.startswith('.word'):
                    continue
                    
                print(f"{addr}: {instr}")

def main():
    if len(sys.argv) != 2:
        print("Usage: list2map.py <list-file>")
        sys.exit(1)
        
    process_list_file(sys.argv[1])

if __name__ == "__main__":
    main()
