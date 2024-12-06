import os
import random
import struct

def generate_random_data(num_integers):
    """生成指定数量的随机64位有符号整数"""
    return [random.randint(-2**63, 2**63 - 1) for _ in range(num_integers)]

def write_file(file_path, data):
    """将数据写入指定文件"""
    with open(file_path, 'wb') as f:
        packed_data = struct.pack(f'{len(data)}q', *data)  # 'q'表示64位有符号整数
        f.write(packed_data)  # 一次性写入所有数据
        
def generate_file_sizes(total_size, total_files):
    """生成文件大小数组，确保总大小为目标值且每个文件大小不一样"""
    base_size_kb = 16  # 每个文件初始为16KB
    sizes = [base_size_kb * 1024 for _ in range(total_files)]  # 初始化为16KB

    # 计算目标总字节数
    target_size_bytes = total_size
    
    # 随机调整每个文件的大小
    current_total_size = sum(sizes)
    remaining_size = target_size_bytes - current_total_size
    
    # 随机分配剩余的字节数
    while remaining_size > 0:
        for i in range(total_files):
            if remaining_size <= 0:
                break
            
            # 随机增加文件大小，确保不超过一定上限（例如2MB），并保持为8的倍数
            additional_size = min(remaining_size, random.randint(0, 2 * (1024 ** 2)))
            additional_size -= additional_size % 8  # 确保增加的大小是8的倍数
            
            sizes[i] += additional_size
            remaining_size -= additional_size

    # 打乱文件大小顺序以确保每个文件大小不一样
    random.shuffle(sizes)

    assert remaining_size >= 0, "Remaining size should not be negative!"
    
    # 确保所有文件大小都是8的倍数
    sizes = [size - (size % 8) for size in sizes]

    return sizes

def generate_files(directory, total_size, total_files):
    """在指定目录中生成固定数量的文件，确保总大小达到目标且每个文件大小不一样"""
    sizes = generate_file_sizes(total_size, total_files)

    # 创建文件并写入数据
    for file_count in range(total_files):
        num_integers = sizes[file_count] // struct.calcsize('q')  # 确保整数数量正确
        assert sizes[file_count] % struct.calcsize('q') == 0, "整除失败"
        data = generate_random_data(num_integers)

        # 创建文件路径
        file_name = f"data_file_{file_count + 1}.bin"
        file_path = os.path.join(directory, file_name)

        write_file(file_path, data)

        print(f"生成文件: {file_name}, 大小: {os.path.getsize(file_path)} 字节")

if __name__ == "__main__":
    output_directory = "./data"  # 指定输出目录
    os.makedirs(output_directory, exist_ok=True)
    
    target_size = 128 * 1024 * 1024 * 1024  # 目标大小
    total_files = 100000  # 文件数量
    
    generate_files(output_directory, target_size, total_files)