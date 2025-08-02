#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ConsoleColor {
    // ANSI颜色码 (Linux/macOS/现代Windows终端均支持)
    constexpr auto RESET = "\033[0m";
    constexpr auto GREEN = "\033[32m";
    constexpr auto RED = "\033[31m";
    constexpr auto BOLD = "\033[1m";
    constexpr auto CYAN = "\033[36m";

    // 初始化Windows控制台颜色支持
    void init() {
        #ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        #endif
    }
}

void printColoredResult(bool success, const std::string& message = "") {
    ConsoleColor::init();  

    if (success) {
        std::cout << ConsoleColor::BOLD << ConsoleColor::GREEN 
                  << std::endl << ConsoleColor::RESET;
    } else {
        std::cout << ConsoleColor::BOLD << ConsoleColor::RED 
                   << std::endl << ConsoleColor::RESET;
    }

    if (!message.empty()) {
        std::cout << " " << ConsoleColor::CYAN << message << ConsoleColor::RESET;
    }
    std::cout << std::endl;
}

// // 使用示例
// int main() {
//     printColoredResult(true, "Connection established");  // 绿色✓ + 青色附加信息
//     printColoredResult(false, "Timeout exceeded");       // 红色✗ + 青色附加信息
//     return 0;
// }