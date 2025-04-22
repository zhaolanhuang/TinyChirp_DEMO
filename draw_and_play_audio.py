import numpy as np
import matplotlib.pyplot as plt
import sounddevice as sd


def read_hex_file_to_array(filepath):
    with open(filepath, 'r', encoding='utf-8') as file:
        content = file.read()

    hex_values = content.split()
    # 将 16 位十六进制值转换为 int16（带符号）
    int_array = np.array([int(h, 16) for h in hex_values], dtype=np.int16)

    # 转为有符号（int16）数据
    signed_array = int_array.astype(np.int16)

    return signed_array

def plot_waveform(data, sample_rate=16000):
    duration = len(data) / sample_rate
    time = np.linspace(0, duration, len(data))
    plt.figure(figsize=(10, 4))
    plt.plot(time, data)
    plt.title("Waveform")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

def play_audio(data, sample_rate=16000):
    print("Playing audio...")
    sd.play(data, samplerate=sample_rate)
    sd.wait()
    print("Playback finished.")

# ==== 主流程 ====
filepath = 'rec_clap.txt'  # 替换为你的txt文件路径
audio_data = (read_hex_file_to_array(filepath)  / 4096 - 0.39) * 2.5
# breakpoint()
plot_waveform(audio_data)
play_audio(audio_data)
