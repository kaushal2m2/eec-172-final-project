import re
import os
import argparse

def extract_arrays_from_header(header_file):
    """Extract bitmap arrays from a C/C++ header file."""
    with open(header_file, 'r') as f:
        content = f.read()
    
    # Find array name and dimensions
    array_match = re.search(r'const\s+uint8_t\s+(\w+)\s*\[(\d+)\]\s*\[(\d+)\]', content)
    if not array_match:
        # Try alternative pattern (single-dimensional array)
        array_match = re.search(r'const\s+uint8_t\s+(\w+)\s*\[(\d+)\]', content)
        if array_match:
            array_name = array_match.group(1)
            num_frames = 1
            frame_size = int(array_match.group(2))
            is_2d = False
        else:
            raise ValueError("Could not find bitmap array declaration in header file")
    else:
        array_name = array_match.group(1)
        num_frames = int(array_match.group(2))
        frame_size = int(array_match.group(3))
        is_2d = True
    
    print(f"Found array: {array_name} with {num_frames} frames of {frame_size} bytes each")
    
    # Extract array data
    if is_2d:
        # Find the full array declaration
        array_start_pattern = rf"{array_name}\s*\[\d+\]\[\d+\]\s*=\s*\{{" 
        array_start_match = re.search(array_start_pattern, content)
        if not array_start_match:
            raise ValueError(f"Could not find start of array {array_name}")
        
        array_start_pos = array_start_match.end()
        
        # Find the end of the array (matching closing brace)
        brace_level = 1  # We're already inside one brace
        array_end_pos = array_start_pos
        for i in range(array_start_pos, len(content)):
            if content[i] == '{':
                brace_level += 1
            elif content[i] == '}':
                brace_level -= 1
                if brace_level == 0:
                    array_end_pos = i
                    break
        
        array_content = content[array_start_pos:array_end_pos]
        
        # Extract each frame by tracking brace levels
        frames = []
        current_frame = ""
        brace_level = 0
        in_frame = False
        
        for char in array_content:
            if char == '{':
                brace_level += 1
                if brace_level == 1:  # Start of a new frame
                    in_frame = True
                    current_frame = ""
            elif char == '}':
                if in_frame and brace_level == 1:  # End of a frame
                    in_frame = False
                    
                    # Process the collected frame data
                    hex_values = re.findall(r'0x[0-9A-Fa-f]+', current_frame)
                    if not hex_values:
                        # Try decimal values
                        hex_values = re.findall(r'\b\d+\b', current_frame)
                    
                    # Convert to bytes
                    try:
                        frame_bytes = bytes([int(val, 0) if val.startswith('0x') else int(val) for val in hex_values])
                        frames.append(frame_bytes)
                        print(f"Extracted frame {len(frames)}, byte length: {len(frame_bytes)}")
                    except Exception as e:
                        print(f"Error processing frame: {e}")
                
                brace_level -= 1
            
            if in_frame:
                current_frame += char
        
        return array_name, frames
    else:
        # Single frame pattern - same as before
        array_pattern = r'\{([^}]*)\}'
        match = re.search(array_pattern, content)
        if not match:
            raise ValueError("Could not find array data in header file")
            
        frame_data = match.group(1)
        # Extract hex values
        hex_values = re.findall(r'0x[0-9A-Fa-f]+', frame_data)
        if not hex_values:
            # Try decimal values
            hex_values = re.findall(r'\b\d+\b', frame_data)
            
        # Convert to bytes
        frame_bytes = bytes([int(val, 0) if val.startswith('0x') else int(val) for val in hex_values])
        return array_name, [frame_bytes]

def save_frames_to_binary(array_name, frames, output_dir):
    """Save extracted frames to binary files."""
    os.makedirs(output_dir, exist_ok=True)
    
    # Save each frame to a separate file
    for i, frame in enumerate(frames):
        filename = f"{array_name}_{i}.bin"
        filepath = os.path.join(output_dir, filename)
        
        with open(filepath, 'wb') as f:
            f.write(frame)
        
        print(f"Saved frame {i} to {filepath} ({len(frame)} bytes)")
    
    # Also save all frames to a single file
    all_frames_file = os.path.join(output_dir, f"{array_name}_all.bin")
    with open(all_frames_file, 'wb') as f:
        for frame in frames:
            f.write(frame)
    
    print(f"Saved all frames to {all_frames_file} ({sum(len(frame) for frame in frames)} bytes)")

def main():
    parser = argparse.ArgumentParser(description='Convert bitmap arrays in C/C++ headers to binary files')
    parser.add_argument('header_file', help='Path to header file containing bitmap arrays')
    parser.add_argument('--output', '-o', default='output', help='Output directory for binary files')
    
    args = parser.parse_args()
    
    try:
        array_name, frames = extract_arrays_from_header(args.header_file)
        save_frames_to_binary(array_name, frames, args.output)
        print("Conversion completed successfully!")
        
        # Print instructions for using with CC3200
        basename = os.path.basename(args.header_file).replace('.h', '')
        print("\nCC3200 Usage Instructions:")
        print("1. Use Uniflash to upload the binary files to your CC3200")
        print("2. Access the files in your code using:")
        print(f"   char filename[32];")
        print(f"   sprintf(filename, \"/{array_name}_%d.bin\", frame_index);")
        print(f"   // Then open the file with sl_FsOpen() and read it")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()