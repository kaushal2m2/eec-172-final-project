#!/usr/bin/env python3
"""
MP3 to Beeper Converter
Converts MP3 files to frequency arrays for use with embedded beeper/buzzer systems.

Usage: 
1. Edit the 'input_file' variable in main() to point to your MP3 file
2. Optionally edit other configuration variables
3. Run: python mp3_to_beeper.py
"""

import numpy as np
import librosa
import scipy.signal
import os
from typing import Tuple, List
import math

class MP3ToBeeper:
    def __init__(self, sample_rate: int = 22050, window_duration: float = 0.1, silence_threshold: float = 0.01):
        """
        Initialize the converter.
        
        Args:
            sample_rate: Target sample rate for analysis
            window_duration: Duration of each analysis window in seconds
            silence_threshold: Amplitude threshold for silence detection (0.001-0.1)
        """
        self.sample_rate = sample_rate
        self.window_duration = window_duration
        self.window_size = int(sample_rate * window_duration)
        self.silence_threshold = silence_threshold
        
        # Frequency limits for beeper (typically 50Hz to 20kHz)
        self.min_freq = 50
        self.max_freq = 20000
        
    def load_audio(self, file_path: str) -> np.ndarray:
        """Load and preprocess audio file."""
        print(f"Loading audio file: {file_path}")
        
        # Load audio file and convert to mono
        audio, sr = librosa.load(file_path, sr=self.sample_rate, mono=True)
        
        print(f"Audio duration: {len(audio) / sr:.2f} seconds")
        print(f"Sample rate: {sr} Hz")
        
        return audio
    
    def extract_frequencies(self, audio: np.ndarray) -> Tuple[List[int], List[int]]:
        """
        Extract dominant frequencies from audio using sliding window analysis.
        Includes silence detection to preserve rests in the music.
        
        Returns:
            Tuple of (frequencies, durations) lists
        """
        frequencies = []
        durations = []
        
        # Apply window function for better frequency resolution
        window = scipy.signal.get_window('hann', self.window_size)
        
        # Process audio in overlapping windows
        hop_length = self.window_size // 2  # 50% overlap
        num_windows = (len(audio) - self.window_size) // hop_length + 1
        
        print(f"Processing {num_windows} windows...")
        print(f"Using silence threshold: {self.silence_threshold}")
        
        for i in range(0, len(audio) - self.window_size + 1, hop_length):
            # Extract window
            window_audio = audio[i:i + self.window_size] * window
            
            # Check if this window is silent
            max_amplitude = np.max(np.abs(window_audio))
            is_silent = max_amplitude < self.silence_threshold
            
            if is_silent:
                # Add silence (frequency = 0)
                if frequencies and frequencies[-1] == 0:
                    # Extend existing silence
                    durations[-1] += 1
                else:
                    # Start new silence period
                    frequencies.append(0)
                    durations.append(1)
            else:
                # Process audio content
                # Compute FFT
                fft = np.fft.rfft(window_audio)
                magnitude = np.abs(fft)
                
                # Create frequency bins
                freqs = np.fft.rfftfreq(self.window_size, 1/self.sample_rate)
                
                # Find dominant frequency
                # Filter to audible range for beeper
                valid_indices = (freqs >= self.min_freq) & (freqs <= self.max_freq)
                
                if np.any(valid_indices):
                    valid_magnitudes = magnitude[valid_indices]
                    valid_freqs = freqs[valid_indices]
                    
                    # Find peak frequency
                    peak_idx = np.argmax(valid_magnitudes)
                    dominant_freq = valid_freqs[peak_idx]
                    
                    # Round to nearest Hz
                    freq_hz = int(round(dominant_freq))
                    
                    # Group consecutive similar frequencies (but not with silence)
                    if (frequencies and frequencies[-1] != 0 and 
                        abs(freq_hz - frequencies[-1]) < 20):
                        # Similar frequency, extend duration
                        durations[-1] += 1
                    else:
                        # New frequency
                        frequencies.append(freq_hz)
                        durations.append(1)
                else:
                    # No valid frequencies found, treat as silence
                    if frequencies and frequencies[-1] == 0:
                        durations[-1] += 1
                    else:
                        frequencies.append(0)
                        durations.append(1)
        
        return frequencies, durations
    
    def optimize_for_beeper(self, frequencies: List[int], durations: List[int]) -> Tuple[List[int], List[int]]:
        """
        Optimize frequency and duration arrays for beeper playback.
        """
        if not frequencies:
            return [], []
        
        optimized_freqs = []
        optimized_durs = []
        
        # Scale durations down to match the C code's expected time units
        # Based on user feedback that durations are 6x too long
        duration_scale_factor = 4.0
        
        # Quantize frequencies to common musical notes for better sound
        # This helps avoid very close frequencies that might sound bad on a beeper
        musical_notes = [
            131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247,  # C3-B3
            262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,  # C4-B4
            523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,  # C5-B5
            1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976,  # C6-B6
            2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951   # C7-B7
        ]
        
        for freq, dur in zip(frequencies, durations):
            # Handle silence periods (frequency = 0)
            if freq == 0:
                # Apply scale factor only to final duration value
                final_dur = max(0.1, dur / duration_scale_factor)
                final_dur = round(final_dur * 10) / 10
                
                # Merge with previous silence if it exists
                if optimized_freqs and optimized_freqs[-1] == 0:
                    optimized_durs[-1] += final_dur
                else:
                    optimized_freqs.append(0)
                    optimized_durs.append(final_dur)
                continue
            
            # Handle regular frequencies
            # Find closest musical note
            closest_note = min(musical_notes, key=lambda x: abs(x - freq))
            
            # Only use if reasonably close (within 2 semitones)
            if abs(closest_note - freq) / freq < 0.12:  # ~12% tolerance
                freq = closest_note
            
            # Limit frequency range for beeper
            freq = max(self.min_freq, min(self.max_freq, freq))
            
            # Use original duration for splitting logic (don't apply scale factor yet)
            working_dur = dur
            
            # Limit maximum duration to prevent very long notes
            max_duration = 5.0  # Use original scale for splitting logic
            while working_dur > max_duration:
                # Apply scale factor only when adding to final arrays
                scaled_max_dur = max(0.1, max_duration / duration_scale_factor)
                scaled_max_dur = round(scaled_max_dur * 10) / 10
                
                optimized_freqs.append(freq)
                optimized_durs.append(scaled_max_dur)
                working_dur -= max_duration
            
            if working_dur > 0:
                # Apply scale factor only to final duration value
                final_dur = max(0.1, working_dur / duration_scale_factor)
                final_dur = round(final_dur * 10) / 10
                
                optimized_freqs.append(freq)
                optimized_durs.append(final_dur)
        
        # Limit total length to reasonable size for embedded systems
        max_notes = 300
        if len(optimized_freqs) > max_notes:
            print(f"Truncating melody to {max_notes} notes")
            optimized_freqs = optimized_freqs[:max_notes]
            optimized_durs = optimized_durs[:max_notes]
        
        return optimized_freqs, optimized_durs
    
    def generate_header(self, frequencies: List[int], durations: List[int], 
                       name: str, output_file: str) -> None:
        """Generate C header file with frequency arrays."""
        
        header_content = f'''// Auto-generated header file from MP3 conversion
// Generated by MP3 to Beeper Converter
// Note: Frequency 0 = silence/rest periods

#ifndef {name.upper()}_SOUND_H
#define {name.upper()}_SOUND_H

// {name.title()} sound effect
#define {name.upper()}_LENGTH {len(frequencies)}

const unsigned long {name.upper()}_TONES[] = {{
'''
        
        # Add frequencies with proper formatting
        for i, freq in enumerate(frequencies):
            if i % 8 == 0:
                header_content += "    "
            header_content += f"{freq:4d}"
            if i < len(frequencies) - 1:
                header_content += ","
            if i % 8 == 7 or i == len(frequencies) - 1:
                header_content += "\n"
            else:
                header_content += " "
        
        header_content += f'''}};

const unsigned long {name.upper()}_DURATIONS[] = {{
'''
        
        # Add durations with proper formatting
        for i, dur in enumerate(durations):
            if i % 12 == 0:
                header_content += "    "
            header_content += f"{dur:2f}"
            if i < len(durations) - 1:
                header_content += ","
            if i % 12 == 11 or i == len(durations) - 1:
                header_content += "\n"
            else:
                header_content += " "
        
        header_content += f'''}};

// Function to play the {name} sound
static inline void Play{name.title()}Sound(void) {{
    PlaySoundEffect({name.upper()}_TONES, {name.upper()}_DURATIONS, {name.upper()}_LENGTH);
}}

#endif // {name.upper()}_SOUND_H
'''
        
        # Write to file
        with open(output_file, 'w') as f:
            f.write(header_content)
        
        # Count silence periods
        silence_count = sum(1 for f in frequencies if f == 0)
        non_zero_freqs = [f for f in frequencies if f > 0]
        
        print(f"Header file written to: {output_file}")
        print(f"Total notes: {len(frequencies)} ({len(frequencies) - silence_count} tones + {silence_count} rests)")
        if non_zero_freqs:
            print(f"Frequency range: {min(non_zero_freqs)} - {max(non_zero_freqs)} Hz")
        else:
            print("Frequency range: Only silence detected")
        print(f"Total duration: {sum(durations)} time units")
        print(f"Note: Frequency 0 in arrays represents silence/rest periods")
    
    def convert(self, input_file: str, output_file: str = None, name: str = None) -> None:
        """
        Main conversion function.
        
        Args:
            input_file: Path to input MP3 file
            output_file: Path to output header file (optional)
            name: Name for the sound effect (optional)
        """
        # Generate default output filename and name if not provided
        if name is None:
            name = os.path.splitext(os.path.basename(input_file))[0]
            name = "".join(c for c in name if c.isalnum() or c == "_")
        
        if output_file is None:
            output_file = f"{name}_sound.h"
        
        try:
            # Load and process audio
            audio = self.load_audio(input_file)
            
            # Extract frequencies
            frequencies, durations = self.extract_frequencies(audio)
            
            if not frequencies:
                print("No suitable frequencies found in audio file")
                return
            
            # Optimize for beeper
            frequencies, durations = self.optimize_for_beeper(frequencies, durations)
            
            # Generate header file
            self.generate_header(frequencies, durations, name, output_file)
            
            print("\nConversion completed successfully!")
            print(f"Add '#include \"{output_file}\"' to your C project")
            print(f"Call 'Play{name.title()}Sound()' to play the converted audio")
            
        except Exception as e:
            print(f"Error during conversion: {e}")
            raise

def main():
    # ========== CONFIGURATION - EDIT THESE VALUES ==========
    input_file = "C:/Users/lfiel/Desktop/testsound.mp3"        # Change this to your MP3 file path
    output_file = "newercustomsound.h"                   # Set to None for auto-generated name, or specify like "custom_sound.h"
    sound_name = None                    # Set to None for auto-generated name, or specify like "victory"
    sample_rate = 22050                  # Sample rate for analysis
    window_duration = 0.01                # Analysis window duration in seconds
    silence_threshold = 0.01             # Amplitude threshold for silence detection (0.001-0.1)
    # ======================================================
    
    # Check if input file exists
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        print("Please edit the 'input_file' variable in main() to point to your MP3 file")
        return 1
    
    # Create converter and run conversion
    converter = MP3ToBeeper(sample_rate=sample_rate, window_duration=window_duration, 
                           silence_threshold=silence_threshold)
    converter.convert(input_file, output_file, sound_name)
    
    return 0

if __name__ == "__main__":
    exit(main())