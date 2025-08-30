#include <iostream>   // 用于 std::cout、std::endl
#include <string>     // 用于 std::string
#include <sstream>    // 用于 std::stringstream
#include <iomanip>    // 关键：包含 std::setw、std::setfill
#include <cstdint>    // 用于 uint8_t、int32_t 等类型

// 模板函数：将任意类型变量转换为二进制字符串
template <typename T>
std::string get_binary_string(const T& var) {
	std::string binary_str;
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&var);
	size_t size = sizeof(var);

	// 从高位字节到低位字节处理
	for (size_t i = 0; i < size; ++i) {
		// 每个字节转换为8位二进制，不足补前导0
		binary_str += std::bitset<8>(bytes[size - 1 - i]).to_string();
		// 字节之间添加空格分隔（可选）
		if (i != size - 1) {
			binary_str += " ";
		}
	}

	return binary_str;
}

// 模板函数：将任意类型变量转换为十六进制字符串（修复后）
template <typename T>
std::string get_hex_string(const T& var) {
	std::stringstream ss;
	// 强制按字节读取变量内存（const 确保不修改原变量）
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&var);
	size_t size = sizeof(var);  // 获取变量的字节数

	// 从高位字节到低位字节处理（符合人类阅读习惯，如 0x1234 显示为 "12 34"）
	for (size_t i = 0; i < size; ++i) {
		// 每个字节转 2 位十六进制（不足补 0），static_cast<int> 避免符号扩展问题
		ss << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(bytes[size - 1 - i]);
		// 字节间加空格分隔（可选，删除则紧凑显示，如 "1234"）
		if (i != size - 1) {
			ss << " ";
		}
	}

	return ss.str();
}


struct Info
{
	uint8_t u8;
	int8_t i8;

	uint16_t u16;
	int16_t i16;

	uint32_t u32;
	int32_t i32;

	uint64_t u64;
	int64_t i64;

	float f32;
	double f64;

	char name[10];
	Info()
	{
		name[10 - 1] = '\0';
	}

	void print()
	{
        std::cout << "u8: " << (int)u8 << " [" << get_hex_string(u8) << "]" << std::endl;
        std::cout << "i8: " << (int)i8 << " [" << get_hex_string(i8) << "]" << std::endl;
        std::cout << "u16: " << u16 << " [" << get_hex_string(u16) << "]" << std::endl;
        std::cout << "i16: " << i16 << " [" << get_hex_string(i16) << "]" << std::endl;
        std::cout << "u32: " << u32 << " [" << get_hex_string(u32) << "]" << std::endl;
        std::cout << "i32: " << i32 << " [" << get_hex_string(i32) << "]" << std::endl;
        std::cout << "u64: " << u64 << " [" << get_hex_string(u64) << "]" << std::endl;
        std::cout << "i64: " << i64 << " [" << get_hex_string(i64) << "]" << std::endl;
        std::cout << "f32: " << f32 << " [" << get_hex_string(f32) << "]" << std::endl;
        std::cout << "f64: " << f64 << " [" << get_hex_string(f64) << "]" << std::endl;
        std::cout << "name: " << name << " [" << get_hex_string(name) << "]" << std::endl;
	}
};

int main()
{
	{
		Info info;
		std::cout << "----------------------------------------------------\n";
		std::cout << "struct is on stack. default value of data members:\n";
		info.print();
	}

	{
		Info* info = new Info();
		std::cout << "----------------------------------------------------\n";
		std::cout << "struct is on heap. default value of data members:\n";
		info->print();
		delete info;
	}

	return 0;
}