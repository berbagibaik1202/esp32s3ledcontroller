#!/usr/bin/env python3
"""
YouTube & Video to MJPEG Stream Bridge for ESP32-S3 LED Matrix Controller (128x64)

Script ini bertindak sebagai jembatan (relay):
1. Mengambil video dari YouTube (atau file video / webcam lokal)
2. Mengonversi dan me-resize frame ke resolusi 128x64 piksel
3. Menyediakan HTTP MJPEG stream (misal: http://<IP_PC>:8080/stream.mjpg)
   yang dapat langsung diputar secara real-time oleh ESP32-S3 HUB75 controller.

Persyaratan:
  pip install yt-dlp opencv-python numpy

Cara Penggunaan:
  # 1. Dari YouTube URL:
  py tools/youtube_stream_bridge.py --url "https://www.youtube.com/watch?v=VIDEO_ID" --port 8080

  # 2. Dari File Video Lokal:
  py tools/youtube_stream_bridge.py --file "video.mp4" --port 8080

  # 3. Dari Webcam:
  py tools/youtube_stream_bridge.py --cam 0 --port 8080
"""

import argparse
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
import cv2

# Global frame buffer for HTTP stream
current_jpeg_frame = None
frame_lock = threading.Lock()
running = True

def get_local_ip():
    """Mendeteksi IP lokal PC di jaringan LAN/Wi-Fi."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

class MjpegStreamHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Mute normal request logging to keep console clean
        pass

    def do_GET(self):
        global current_jpeg_frame
        if self.path == "/" or self.path.startswith("/stream"):
            self.send_response(200)
            self.send_header("Age", "0")
            self.send_header("Cache-Control", "no-cache, private")
            self.send_header("Pragma", "no-cache")
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=FRAME")
            self.end_headers()
            try:
                while running:
                    with frame_lock:
                        frame = current_jpeg_frame
                    if frame is not None:
                        self.wfile.write(b"--FRAME\r\n")
                        self.wfile.write(b"Content-Type: image/jpeg\r\n")
                        self.wfile.write(f"Content-Length: {len(frame)}\r\n\r\n".encode("utf-8"))
                        self.wfile.write(frame)
                        self.wfile.write(b"\r\n")
                    time.sleep(0.05)  # ~20 FPS stream rate
            except (ConnectionResetError, BrokenPipeError):
                pass
        elif self.path == "/snapshot.jpg":
            with frame_lock:
                frame = current_jpeg_frame
            if frame is not None:
                self.send_response(200)
                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", str(len(frame)))
                self.end_headers()
                self.wfile.write(frame)
            else:
                self.send_response(503)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

def video_capture_loop(source, width=128, height=64, target_fps=15, is_youtube=False):
    global current_jpeg_frame, running

    while running:
        actual_source = source
        if is_youtube:
            print(f"[INFO] Mengekstrak stream YouTube dari: {source} ...")
            try:
                import yt_dlp
                ydl_opts = {
                    "format": "best[height<=720]/best",
                    "quiet": True,
                    "no_warnings": True,
                }
                with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                    info = ydl.extract_info(source, download=False)
                    actual_source = info.get("url", None)
                    title = info.get("title", "Video")
                    print(f"[PASS] Judul: {title}")
            except ImportError:
                print("[ERROR] Modul 'yt-dlp' belum terpasang. Jalankan: pip install yt-dlp")
                return
            except Exception as e:
                print(f"[ERROR] Gagal mengekstrak YouTube: {e}")
                time.sleep(5)
                continue

        cap = cv2.VideoCapture(actual_source)
        if not cap.isOpened():
            print(f"[FAIL] Tidak dapat membuka video source: {actual_source}")
            time.sleep(3)
            continue

        print(f"[PASS] Video terhubung. Me-resize ke {width}x{height} @ {target_fps} FPS...")
        delay = 1.0 / target_fps

        while running:
            t0 = time.time()
            ret, frame = cap.read()
            if not ret:
                print("[INFO] Video selesai / loop ulang.")
                break

            # Resize frame to panel dimensions (128x64)
            resized = cv2.resize(frame, (width, height), interpolation=cv2.INTER_AREA)

            # Encode frame to JPEG
            encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 75]
            _, enc = cv2.imencode(".jpg", resized, encode_param)

            with frame_lock:
                current_jpeg_frame = enc.tobytes()

            elapsed = time.time() - t0
            time_to_wait = delay - elapsed
            if time_to_wait > 0:
                time.sleep(time_to_wait)

        cap.release()
        time.sleep(1)

def main():
    parser = argparse.ArgumentParser(description="YouTube / Video to MJPEG Bridge untuk ESP32 LED Controller")
    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument("--url", help="URL Video YouTube")
    group.add_argument("--file", help="Path file video lokal (.mp4, .mkv, .avi)")
    group.add_argument("--cam", type=int, default=None, help="Index kamera webcam lokal (0, 1, ...)")
    parser.add_argument("--port", type=int, default=8080, help="Port HTTP server (default: 8080)")
    parser.add_argument("--fps", type=int, default=15, help="Target frame rate (default: 15)")
    args = parser.parse_args()

    local_ip = get_local_ip()
    stream_url = f"http://{local_ip}:{args.port}/stream.mjpg"

    is_yt = False
    source = None
    if args.url:
        is_yt = True
        source = args.url
    elif args.file:
        source = args.file
    elif args.cam is not None:
        source = args.cam
    else:
        # Default demo test bars pattern or prompt
        print("=" * 65)
        print("  YouTube & Video MJPEG Bridge untuk ESP32-S3 LED Controller")
        print("=" * 65)
        print("Gunakan argumen berikut:")
        print(f"  py tools/youtube_stream_bridge.py --url \"https://www.youtube.com/watch?v=...\" --port {args.port}")
        print(f"  py tools/youtube_stream_bridge.py --file \"sample.mp4\" --port {args.port}")
        print("=" * 65)
        sys.exit(1)

    print("-" * 65)
    print(f"  LOCAL IP     : {local_ip}")
    print(f"  PORT         : {args.port}")
    print(f"  STREAM URL   : {stream_url}")
    print("-" * 65)
    print(f">> Masukkan URL berikut di Web UI LED Controller: {stream_url}")
    print("-" * 65)

    # Start video capture in background thread
    t = threading.Thread(target=video_capture_loop, args=(source, 128, 64, args.fps, is_yt), daemon=True)
    t.start()

    # Start HTTP server
    server = HTTPServer(("0.0.0.0", args.port), MjpegStreamHandler)
    print(f"[PASS] HTTP Server aktif di port {args.port}. Tekan Ctrl+C untuk berhenti.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[INFO] Menutup server...")
        global running
        running = False
        server.server_close()

if __name__ == "__main__":
    main()
