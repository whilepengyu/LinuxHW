import struct
import os

def read_file(file_path):
    """读取指定文件并输出其中的64位有符号整数"""
    if not os.path.isfile(file_path):
        print("文件不存在，请检查路径。")
        return

    with open(file_path, 'rb') as f:
        while True:
            # 读取8个字节（64位有符号整数）
            bytes_data = f.read(8)
            if not bytes_data:
                break  # 到达文件末尾
            
            # 解包数据
            number = struct.unpack('q', bytes_data)[0]  # 'q'表示64位有符号整数
            print(number, end=' ')

if __name__ == "__main__":
    read_file("../data/data_file_1.bin");