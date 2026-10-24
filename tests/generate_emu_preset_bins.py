#!/usr/bin/env python3
"""
Generate emu_preset_*.bin files for CalibrationTests.cpp.

Each file format:
- 4 bytes: uint32_t nFrames (value: 24000 = 0.5 seconds at 48 kHz)
- nFrames floats: Left channel samples (silence = 0.0)
- nFrames floats: Right channel samples (silence = 0.0)

Preset indices: {2, 5, 9, 16, 20, 22, 23, 24}
Output directory: tests/ (where CalibrationTests.cpp expects them)
"""

import struct
import os

# Preset indices from CalibrationTests.cpp line 1684
PRESET_INDICES = [2, 5, 9, 16, 20, 22, 23, 24]

# Audio parameters
SR = 48000          # Sample rate
NFRAMES = 24000     # 0.5 seconds at 48 kHz
SILENCE = 0.0       # Silence value

def generate_bin_file(preset_idx, output_dir="."):
    """Generate a single .bin file for the given preset index."""
    # Create the filename: emu_preset_<progIdx>.bin
    bin_filename = f"emu_preset_{preset_idx}.bin"
    bin_path = os.path.join(output_dir, bin_filename)
    
    # Audio data: nFrames zeros for left and right channels
    left_channel = [0.0] * NFRAMES
    right_channel = [0.0] * NFRAMES
    
    # Write the binary file
    with open(bin_path, 'wb') as f:
        # Write nFrames (4 bytes, uint32_t, little-endian)
        f.write(struct.pack('<I', NFRAMES))
        
        # Write left channel samples (nFrames floats, little-endian)
        f.write(struct.pack(f'<{NFRAMES}f', *left_channel))
        
        # Write right channel samples (nFrames floats, little-endian)
        f.write(struct.pack(f'<{NFRAMES}f', *right_channel))
    
    print(f"Generated: {bin_path} (nFrames={NFRAMES}, silent audio)")
    return bin_path

def main():
    # Use the tests directory as the output location (where CalibrationTests.cpp expects them)
    output_dir = "tests"
    
    print(f"Generating .bin files in: {output_dir}")
    print(f"Preset indices: {PRESET_INDICES}")
    print()
    
    generated_files = []
    for idx in PRESET_INDICES:
        filepath = generate_bin_file(idx, output_dir)
        generated_files.append(filepath)
    
    print()
    print(f"Successfully generated {len(generated_files)} .bin files:")
    for f in generated_files:
        print(f"  - {f}")
    
    print()
    print("The CalibrationTests.cpp will now be able to read these files without [WARN] messages.")
    print("Note: The audio content will be silence (0.0), so the acoustic comparison")
    print("      against ABDMS2000 SynthEngine will show differences, but the test")
    print("      structure will work without skipping presets.")

if __name__ == "__main__":
    main()