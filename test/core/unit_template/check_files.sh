#!/bin/bash

# Check files w/ clang-format and clang-tidy
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
  "Dut.cpp"
  "Dut.hpp"
  "Sink.hpp"
  "Src.cpp"
  "Src.hpp"
  "Testbench.cpp"
)

#for file in "${files[@]}"; do
#  echo "Checking $file for formatting issues..."
#  if [[ -f "$file" ]]; then
#    clang-format --dry-run --Werror "$file"
#    if [[ $? -eq 0 ]]; then
#      echo "$file is properly formatted."
#    else
#      echo "$file needs formatting."
#      clang-format  "$file" > "$file.formatted"
#    fi
#  else
#    echo "Warning: File $file does not exist, skipping."
#  fi
#done

# Remove previous results
rm -f not_so_tidy.txt
#    clang-tidy --quiet -p "$(dirname "$compile_commands")" "$file" >> not_so_tidy.txt 2>&1

# clang-tidy 
for file in "${files[@]}"; do
  if [[ -f "$file" ]]; then
    echo "Running clang-tidy on $file..."
    clang-tidy --quiet -p "$(dirname "$compile_commands")" "$file" --extra-arg="-I../../.." >> not_so_tidy.txt 2>&1
  else
    echo "Warning: File $file does not exist, skipping." >> not_so_tidy.txt
  fi
done

echo "Clang-tidy results saved to not_so_tidy.txt"
