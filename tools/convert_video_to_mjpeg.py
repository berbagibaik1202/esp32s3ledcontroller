#!/usr/bin/env python3
"""
Convert any video (MP4, MKV, MOV, WEBM, GIF) to 128x64 MJPEG for ESP32 HUB75 Controller.
Usage:
    py tools/convert_video_to_mjpeg.py <input_video> [output_file.mjpeg] [--fps 15] [--quality 5]
"""
import sys
import os
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(description="Convert video to 128x64 MJPEG for ESP32 LED Controller")
    parser.add_argument("input", help="Path to input video file (.mp4, .mov, .avi, .gif, .webm, dll)")
    parser.add_argument("output", nargs="?", help="Path to output .mjpeg file (opsional)")
    parser.add_argument("--fps", type=int, default=15, help="Target FPS (default: 15)")
    parser.add_argument("--quality", type=int, default=5, help="JPEG quality 2-31, 2 terbaik (default: 5)")
    args = parser.parse_args()

    input_file = args.input
    if not os.path.exists(input_file):
        print(f"[ERROR] File '{input_file}' tidak ditemukan!")
        sys.exit(1)

    if args.output:
        output_file = args.output
    else:
        base, _ = os.path.splitext(input_file)
        output_file = f"{base}_128x64.mjpeg"

    # Scale maintaining aspect ratio, then pad to 128x64 with black bars
    vf_filter = (
        "scale=128:64:force_original_aspect_ratio=decrease,"
        "pad=128:64:(ow-iw)/2:(oh-ih)/2:black"
    )

    cmd = [
        "ffmpeg", "-y",
        "-i", input_file,
        "-vf", vf_filter,
        "-r", str(args.fps),
        "-c:v", "mjpeg",
        "-q:v", str(args.quality),
        "-an", # No audio
        output_file
    ]

    print(f"[INFO] Memproses '{input_file}' -> '{output_file}'...")
    print(f"[INFO] Resolusi: 128x64 | FPS: {args.fps} | Kualitas JPEG: {args.quality}")

    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print("[FAIL] FFmpeg error:")
            print(res.stderr.decode('utf-8', errors='ignore'))
            sys.exit(1)
        
        size_bytes = os.path.getsize(output_file)
        size_kb = size_bytes / 1024.0
        print(f"[SUCCESS] File video MJPEG berhasil dibuat:")
        print(f"  Lokasi: {os.path.abspath(output_file)}")
        print(f"  Ukuran: {size_kb:.1f} KiB ({size_bytes} byte)")
        print("\nCara Memutar di Kontroller LED:")
        print("  1. Buka Web UI ESP32 di browser HP: http://192.168.1.10 (atau http://192.168.4.1)")
        print("  2. Scroll ke bagian 'Media LittleFS'")
        print(f"  3. Pilih file '{os.path.basename(output_file)}' dan klik 'Upload gambar/video'")
        print("  4. Setelah ter-upload, tekan tombol '▶ Putar Video' di daftar media!")
    except FileNotFoundError:
        print("[ERROR] FFmpeg tidak ditemukan di PATH sistem!")
        sys.exit(1)

if __name__ == "__main__":
    main()
