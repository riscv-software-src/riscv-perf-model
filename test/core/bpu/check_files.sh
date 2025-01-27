#!/bin/bash

# Check files w/ clang-format
# This does not modify the source files

# The argument is required
if [[ -z "$1" ]]; then
  echo "Usage: $0 <path/to/compile_commands.json>"
  exit 1
fi

compile_commands="$1"

# Make sure the json exists
if [[ ! -f "$compile_commands" ]]; then
  echo "Error: File '$compile_commands' does not exist."
  exit 1
fi

# List of files
files=(
  "../../../core/fetch/BPU.cpp"
  "../../../core/fetch/BPU.hpp"
  "BPUSink.hpp"
  "BPUSource.hpp"
  "BPU_testbench.cpp"
)

for file in "${files[@]}"; do
  echo "Checking $file for formatting issues..."
  if [[ -f "$file" ]]; then
    clang-format --dry-run --Werror "$file"
    if [[ $? -eq 0 ]]; then
      echo "$file is properly formatted."
    else
      echo "$file needs formatting."
      clang-format  "$file" > "$file.formatted"
    fi
  else
    echo "Warning: File $file does not exist, skipping."
  fi
done
