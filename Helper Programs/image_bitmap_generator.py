from PIL import Image, ImageSequence
import numpy as np
import os

def gif_to_monochrome_bitmaps(gif_path, output_width=128, output_height=128, threshold=128, output_c_file="bitmap_animation.h"):
    """
    Convert an animated GIF to a series of monochrome bitmap frames
    
    Args:
        gif_path: Path to the input GIF file
        output_width: Desired width of each frame bitmap (default: 128)
        output_height: Desired height of each frame bitmap (default: 128)
        threshold: Brightness threshold for black/white conversion (0-255)
        output_c_file: Path to save the C array declaration
        
    Returns:
        list: List of bitmap data for each frame
        str: C array declaration
    """
    try:
        # Open the GIF file
        img = Image.open(gif_path)
        
        # Calculate bytes per row (each byte holds 8 pixels)
        bytes_per_row = (output_width + 7) // 8
        bitmap_size = bytes_per_row * output_height
        
        # Extract filename without extension for variable name
        var_name = os.path.splitext(os.path.basename(gif_path))[0]
        var_name = ''.join(c if c.isalnum() else '_' for c in var_name)
        
        # List to store all frame bitmaps
        all_frames = []
        
        # Count frames
        frame_count = 0
        for frame in ImageSequence.Iterator(img):
            frame_count += 1
        
        # Generate C array declaration header
        c_array = f"// Monochrome bitmap animation data for {os.path.basename(gif_path)}\n"
        c_array += f"// Size: {output_width}x{output_height} pixels, {frame_count} frames\n"
        c_array += f"#define {var_name.upper()}_WIDTH {output_width}\n"
        c_array += f"#define {var_name.upper()}_HEIGHT {output_height}\n"
        c_array += f"#define {var_name.upper()}_FRAME_COUNT {frame_count}\n\n"
        
        # Create a 2D array for all frames
        c_array += f"const uint8_t {var_name}Frames[{frame_count}][{bitmap_size}] = {{\n"
        
        # Process each frame
        frame_index = 0
        for frame in ImageSequence.Iterator(img):
            # Resize and convert to grayscale
            frame = frame.convert('L').resize((output_width, output_height), Image.LANCZOS)
            # frame = frame.point(lambda p: 0 if p > threshold else 255)
            # Convert to 1-bit monochrome
            frame = frame.point(lambda p: p > threshold and 255)
            
            # Create byte array for this frame
            frame_bitmap = bytearray(bitmap_size)
            
            # Convert frame data to bitmap format
            for y in range(output_height):
                for x in range(output_width):
                    # If the pixel is white (255), set the corresponding bit to 1
                    if frame.getpixel((x, y)) > 0:
                        byte_index = y * bytes_per_row + x // 8
                        bit_position = 7 - (x % 8)  # MSB first
                        frame_bitmap[byte_index] |= (1 << bit_position)
            
            # Add this frame to our collection
            all_frames.append(bytes(frame_bitmap))
            
            # Add this frame's data to the C array
            c_array += f"    {{  // Frame {frame_index}\n        "
            for i, byte in enumerate(frame_bitmap):
                c_array += f"0x{byte:02X}, "
                if (i + 1) % 12 == 0:  # 12 values per line
                    c_array += "\n        "
            c_array += "\n    },\n"
            
            frame_index += 1
        
        # Close the array
        c_array += "};\n\n"
        
        # Add a function to get a specific frame
        c_array += f"// Function to get a pointer to a specific frame\n"
        c_array += f"const uint8_t* get_{var_name}_frame(uint16_t frame_index) {{\n"
        c_array += f"    if (frame_index >= {var_name.upper()}_FRAME_COUNT) {{\n"
        c_array += f"        frame_index = 0;  // Wrap around if index is out of bounds\n"
        c_array += f"    }}\n"
        c_array += f"    return {var_name}Frames[frame_index];\n"
        c_array += f"}}\n"
        
        # Write to file if a path was provided
        if output_c_file:
            with open(output_c_file, 'w') as f:
                f.write(c_array)
        
        return all_frames, c_array
    
    except Exception as e:
        print(f"Error processing GIF: {e}")
        return None, None

def image_to_monochrome_bitmap(image_path, output_width=128, output_height=128, threshold=128, output_c_file=None):
    """
    Convert a static image to a monochrome bitmap format
    
    Args:
        image_path: Path to the input image
        output_width: Desired width of the output bitmap (default: 128)
        output_height: Desired height of the output bitmap (default: 128)
        threshold: Brightness threshold for black/white conversion (0-255)
        output_c_file: Optional path to save the C array declaration
        
    Returns:
        bytes: The bitmap data as bytes
        str: C array declaration if requested
    """
    try:
        # Load the image and convert to grayscale
        img = Image.open(image_path)
        
        # Check if this is a GIF with multiple frames
        if img.format == 'GIF' and hasattr(img, 'n_frames') and img.n_frames > 1:
            print(f"Detected animated GIF with {img.n_frames} frames, processing as animation...")
            return gif_to_monochrome_bitmaps(image_path, output_width, output_height, threshold, output_c_file)
        
        # Process as a single image
        img = img.convert('L').resize((output_width, output_height), Image.LANCZOS)
        
        # Convert to 1-bit monochrome
        img = img.point(lambda p: p > threshold and 255)
        
        # Calculate bytes per row (each byte holds 8 pixels)
        bytes_per_row = (output_width + 7) // 8
        
        # Create byte array to hold the bitmap data
        bitmap_size = bytes_per_row * output_height
        bitmap_data = bytearray(bitmap_size)
        
        # Convert image data to bitmap format
        for y in range(output_height):
            for x in range(output_width):
                # If the pixel is white (255), set the corresponding bit to 1
                if img.getpixel((x, y)) > 0:
                    byte_index = y * bytes_per_row + x // 8
                    bit_position = 7 - (x % 8)  # MSB first (0x80 >> (i & 7))
                    bitmap_data[byte_index] |= (1 << bit_position)
        
        # Generate C array declaration if requested
        c_array = None
        if output_c_file or output_c_file == "":
            # Extract filename without extension for variable name
            var_name = os.path.splitext(os.path.basename(image_path))[0]
            var_name = ''.join(c if c.isalnum() else '_' for c in var_name)
            
            # Generate the C array declaration
            c_array = f"// Monochrome bitmap data for {os.path.basename(image_path)}\n"
            c_array += f"// Size: {output_width}x{output_height} pixels\n"
            c_array += f"#define {var_name.upper()}_WIDTH {output_width}\n"
            c_array += f"#define {var_name.upper()}_HEIGHT {output_height}\n\n"
            c_array += f"const uint8_t {var_name}Bitmap[] = {{\n    "
            
            # Add the bitmap data in hex format
            for i, byte in enumerate(bitmap_data):
                c_array += f"0x{byte:02X}, "
                if (i + 1) % 12 == 0:  # 12 values per line
                    c_array += "\n    "
            
            c_array += "\n};"
            
            # Write to file if a path was provided
            if output_c_file != "":
                with open(output_c_file, 'w') as f:
                    f.write(c_array)
        
        return bytes(bitmap_data), c_array
    
    except Exception as e:
        print(f"Error processing image: {e}")
        return None, None

def preview_bitmap(bitmap_data, width, height):
    """
    Generate a simple ASCII preview of the bitmap
    """
    bytes_per_row = (width + 7) // 8
    preview = ""
    
    for y in range(height):
        for x in range(width):
            byte_index = y * bytes_per_row + x // 8
            bit_position = 7 - (x % 8)
            bit_value = (bitmap_data[byte_index] >> bit_position) & 1
            preview += "█" if bit_value else " "
        preview += "\n"
    
    return preview

# Main execution with hardcoded parameters
def convert_image():
    # HARDCODED PARAMETERS
    image_path = "C:/Users/lfiel/Desktop/Final_172_LAB/loading_screen.gif"  # Path to your input image or GIF
    width = 128                   # Output width in pixels
    height = 128                 # Output height in pixels
    threshold = 100               # Threshold for black/white conversion (0-255)
    output_c_file = "C:/Users/lfiel/Desktop/Final_172_LAB/loading_screen_bitmap.h"    # Output C header file
    
    result, c_array = image_to_monochrome_bitmap(image_path, width, height, threshold, output_c_file)
    
    if result:
        if isinstance(result, list):
            # We processed an animated GIF
            print(f"Successfully converted {image_path} to {len(result)} frames at {width}x{height} pixels")
            if output_c_file:
                print(f"C array with {len(result)} frames written to {output_c_file}")
            
            # Preview the first frame
            print("\nPreview of first frame:")
            preview = preview_bitmap(result[0], width, height)
            if height > 40:
                preview_lines = preview.split('\n')
                print('\n'.join(preview_lines[:40]) + "\n... (truncated)")
            else:
                print(preview)
        else:
            # We processed a static image
            print(f"Successfully converted {image_path} to a {width}x{height} monochrome bitmap")
            if output_c_file:
                print(f"C array written to {output_c_file}")
            
            # Print a preview of the bitmap
            print("\nBitmap preview:")
            preview = preview_bitmap(result, width, height)
            if height > 40:
                preview_lines = preview.split('\n')
                print('\n'.join(preview_lines[:40]) + "\n... (truncated)")
            else:
                print(preview)

# Run the converter with hardcoded parameters
if __name__ == "__main__":
    convert_image()