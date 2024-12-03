files=(
  "Dut.cpp"
  "Dut.hpp"
  "Sink.hpp"
  "Src.cpp"
  "Src.hpp"
  "Testbench.cpp"
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
