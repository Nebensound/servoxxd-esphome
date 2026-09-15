#!/usr/bin/env python3
"""
Script to remove excessive debug logs (ESP_LOGD and ESP_LOGV) from C++ files.
Keeps ESP_LOGE, ESP_LOGW, ESP_LOGI, and ESP_LOGCONFIG.
"""

import re
import sys

def remove_debug_logs(file_path):
    """Remove ESP_LOGD and ESP_LOGV lines from a C++ file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    result = []
    skip_next = False
    i = 0
    
    while i < len(lines):
        line = lines[i]
        
        # Check if line contains ESP_LOGD or ESP_LOGV
        if re.search(r'\bESP_LOG[DV]\s*\(', line):
            # Check if it's a multi-line statement (ends with comma or doesn't have closing paren + semicolon)
            if not line.rstrip().endswith(');'):
                # Multi-line log statement - skip until we find the closing );
                while i < len(lines) and not lines[i].rstrip().endswith(');'):
                    i += 1
                # Skip the closing line too
                i += 1
                continue
            else:
                # Single line - just skip it
                i += 1
                continue
        
        # Keep this line
        result.append(line)
        i += 1
    
    # Write result back
    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(result)
    
    return len(lines) - len(result)

if __name__ == "__main__":
    files = [
        "components/servoxxd/stepper/servoxxd.cpp",
        "components/servoxxd/stepper/servoxxd_command_queue.cpp",
        "components/servoxxd/stepper/servoxxd_modbus.cpp",
        "components/servoxxd/stepper/servoxxd_stepper_engine.cpp",
        "components/servoxxd/stepper/servoxxd_speed.cpp",
        "components/servoxxd/stepper/servoxxd_acceleration.cpp",
    ]
    
    total_removed = 0
    for file_path in files:
        try:
            removed = remove_debug_logs(file_path)
            if removed > 0:
                print(f"{file_path}: Removed {removed} debug log line(s)")
                total_removed += removed
        except FileNotFoundError:
            print(f"Warning: {file_path} not found", file=sys.stderr)
        except Exception as e:
            print(f"Error processing {file_path}: {e}", file=sys.stderr)
    
    print(f"\nTotal: Removed {total_removed} debug log line(s)")
