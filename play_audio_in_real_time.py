import serial
import sounddevice as sd
import numpy as np
import threading
import queue
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import wave
import os
import time
from datetime import datetime

# 串口参数
SERIAL_PORT = '/dev/ttyACM0'       # 替换为你的串口号
BAUDRATE = 1000000          # 串口波特率
SAMPLE_RATE = 16000        # 音频采样率
CHUNK_SIZE = 1024          # 每次读取的数据点数量（按样本数）
DISPLAY_BUFFER_SIZE = 2048  # 波形显示样本数量

audio_queue = queue.Queue()
plot_buffer = np.zeros(DISPLAY_BUFFER_SIZE, dtype=np.float32)

# === 自动生成文件名（按日期时间） ===
timestamp_str = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
OUTPUT_WAV = f'record_{timestamp_str}.wav'

# === 初始化 WAV 文件 ===
wav_file = wave.open(OUTPUT_WAV, 'wb')
wav_file.setnchannels(1)        # 单声道
wav_file.setsampwidth(2)        # 16位 = 2字节
wav_file.setframerate(SAMPLE_RATE)


def serial_reader():
    with serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1) as ser:
        print(f"Connected to {SERIAL_PORT}")
        buffer = bytearray()
        while True:
            # 持续读取串口数据
            data = ser.read(CHUNK_SIZE * 2)  # 16-bit = 2 bytes per sample
            buffer.extend(data)

            # 保证处理整块的样本
            while len(buffer) >= CHUNK_SIZE * 2:
                chunk = buffer[:CHUNK_SIZE * 2]
                buffer = buffer[CHUNK_SIZE * 2:]

                # 转换为 numpy int16 数组
                samples = (np.frombuffer(chunk, dtype=np.int16) / 4096 - 0.39) * 2.5
                samples = samples.astype(np.float32)
                audio_queue.put(samples)

                legalized_chunk = (samples * 32767).astype(np.int16).tobytes()

                wav_file.writeframes(legalized_chunk)

                global plot_buffer
                plot_buffer = np.roll(plot_buffer, -len(samples))
                plot_buffer[-len(samples):] = samples

def audio_stream_callback(outdata, frames, time, status):
    if status:
        print(f"Audio stream status: {status}")
    try:
        # 从队列中获取数据，填满音频缓冲
        data = audio_queue.get_nowait()
    except queue.Empty:
        outdata[:] = np.zeros((frames, 1), dtype=np.float32)
    else:
        if len(data) < frames:
            padded = np.zeros((frames,), dtype=np.float32)
            padded[:len(data)] = data
            outdata[:, 0] = padded
        else:
            outdata[:, 0] = data[:frames]

# 启动串口读取线程
threading.Thread(target=serial_reader, daemon=True).start()

def animate(i):
    line.set_ydata(plot_buffer)
    return line,


fig, ax = plt.subplots()
x = np.arange(DISPLAY_BUFFER_SIZE)
line, = ax.plot(x, plot_buffer)
ax.set_ylim(-1, 1)
ax.set_xlim(0, DISPLAY_BUFFER_SIZE)
ax.set_title("Waveform")
ax.set_xlabel("sample")
ax.set_ylabel("amp")

# 动画更新器
ani = animation.FuncAnimation(fig, animate, interval=30, blit=True)

# 启动音频播放流
with sd.OutputStream(channels=1, dtype='float32', samplerate=SAMPLE_RATE,
                     callback=audio_stream_callback, blocksize=CHUNK_SIZE):
    print("Audio stream started. Press Ctrl+C to stop.")
    try:
        while True:
            plt.show()
    except KeyboardInterrupt:
        print("\nStopped by user.")

    finally:
        wav_file.close()
        print(f"\n音频保存至: {os.path.abspath(OUTPUT_WAV)}")