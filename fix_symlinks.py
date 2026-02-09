import os
import glob

# Directory containing the JSON files
json_dir = 'mavis/json'

# Get all JSON files in the directory
json_files = glob.glob(os.path.join(json_dir, '*.json'))

print(f"Found {len(json_files)} JSON files in {json_dir}")

fixed_count = 0
for file_path in json_files:
    try:
        # Read the content of the file
        with open(file_path, 'r') as f:
            content = f.read().strip()
        
        # Check if the content is a path to another existing JSON file in the same directory
        # The content of broken symlinks is usually just the filename it points to
        target_path = os.path.join(json_dir, content)
        
        if len(content) < 100 and os.path.exists(target_path) and os.path.isfile(target_path) and content.endswith('.json') and content != os.path.basename(file_path):
            print(f"Fixing broken symlink: {file_path} -> {target_path}")
            
            # Read the target file content
            with open(target_path, 'r') as target_file:
                target_content = target_file.read()
            
            # Write the target content to the symlink file
            with open(file_path, 'w') as f:
                f.write(target_content)
                
            fixed_count += 1
            
    except Exception as e:
        print(f"Error processing {file_path}: {e}")

print(f"Fixed {fixed_count} broken symlinks.")
