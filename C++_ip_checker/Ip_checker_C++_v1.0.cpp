#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <locale>
#include <codecvt>
#include <vector>

//#include <eh.h>
//#pragma comment(lib, "libcmt.lib")
//#pragma comment(lib, "advapi32.lib") // Если используются функции безопасности
//#pragma comment(lib, "user32.lib")
//#pragma comment(lib, "kernel32.lib")
//#pragma comment(lib, "libcmt.lib")

// Функция для получения текущего времени в виде строки
std::wstring GetCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm tm;
    localtime_s(&tm, &in_time_t);

    std::wstringstream ss;
    ss << std::put_time(&tm, L"%Y-%m-%d %X");
    return ss.str();
}

// Унифицированная функция логирования с обработкой кодировок
void Log(const std::wstring& message) {
    std::wofstream logFile(L"ip_txt_C++_v1.0.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}


// Преобразование ANSI (CP866) -> Unicode
std::wstring AnsiToWide(const std::string& str) {
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(866, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(866, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
    return result;
}

// Преобразование Unicode -> UTF-8
std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);
    return result;
}

// Функция для завершения дубликатов процессов
void TerminateDuplicateProcesses() {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        Log(L"Failed to create process snapshot.");
        return;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        Log(L"Failed to retrieve process information.");
        CloseHandle(hProcessSnap);
        return;
    }

    DWORD currentProcessId = GetCurrentProcessId();

    do {
        if (_wcsicmp(pe32.szExeFile, L"ip_checker_C++_v1.0.exe") == 0) {
            if (pe32.th32ProcessID != currentProcessId) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    Log(L"Terminated duplicate process with PID: " + std::to_wstring(pe32.th32ProcessID));
                }
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
}

std::string executeCommand(const std::wstring& command) {
    std::string result;

    // Создаем анонимный канал для чтения вывода команды
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        Log(L"Failed to create pipe.");
        return "";
    }

    // Настройка структуры STARTUPINFO для процесса
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    // Запускаем процесс
    if (!CreateProcess(
        NULL, // Имя приложения (используем command)
        const_cast<LPWSTR>(command.c_str()), // Команда
        NULL, // Атрибуты безопасности процесса
        NULL, // Атрибуты безопасности потока
        TRUE, // Наследование дескрипторов
        CREATE_NO_WINDOW, // Флаг создания без окна
        NULL, // Окружение
        NULL, // Текущий каталог
        &si, // STARTUPINFO
        &pi // PROCESS_INFORMATION
    )) {
        Log(L"Failed to create process.");
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return "";
    }

    // Закрываем ненужный дескриптор записи
    CloseHandle(hWritePipe);

    // Чтение вывода с обработкой кодировки
    std::string raw_output;
    char buffer[4096];
    DWORD bytes_read;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        raw_output += buffer;
    }

    // Преобразование и логирование с проверкой
    std::wstring converted = AnsiToWide(raw_output);
    if (converted.empty() && !raw_output.empty()) {
        Log(L"Failed to convert command output");
    }

    return raw_output;
}

// Улучшенная проверка статуса ping
bool isNodeUnavailable(const std::string& response) {
    const std::wstring converted = AnsiToWide(response);

    // Ключевые фразы для определения недоступности
    const std::vector<std::wstring> patterns = {
        L"недоступен",
        L"Превышен интервал",
        L"сбой передачи",
        L"unreachable",
        L"Timed out",
        L"TTL expired",
    };

    for (const auto& pattern : patterns) {
        if (converted.find(pattern) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

// Точка входа для Windows-приложения
int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
) {
    TerminateDuplicateProcesses();

    const std::wstring configPath = L"C:\\554\\ip.txt";

    while (true) {
        std::wifstream file(configPath);
        if (file.is_open()) {
            std::wstring ip, cmd;
            if (std::getline(file, ip) && std::getline(file, cmd)) {
                std::wstring command = L"ping -n 6 " + ip; // 2 попытки для стабильности
                std::string response = executeCommand(command);

                if (!isNodeUnavailable(response)) {
                    // Сохранение текущего времени
                    const std::wstring time = GetCurrentTimeString();

                    // Преобразование response (wstring) в UTF-8
                    std::string command_converted_utf = WideToUTF8(AnsiToWide(response));

                    if (cmd == L"0") {
                        // Сохранения прчины и времени дейтсвия
                        Log(time + L" - INITIATING SHUTDOWN\n");

                        // Сохранение сообщения от консоли в файл
                        std::ofstream rawFile("ip_txt_C++_v1.0.txt", std::ios::app);
                        if (rawFile.is_open()) {
                            rawFile << "command: " << std::endl;
                            rawFile << command_converted_utf << std::endl;
                            rawFile.close();
                        }

                        // само действие
                        system("shutdown /p");
                    }
                    else if (cmd == L"1") {
                        // Сохранения причины и времени дейтсвия
                        Log(time + L" - INITIATING REBOOT\n");

                        // Сохранение сообщения от консоли в файл
                        std::ofstream rawFile("ip_txt_C++_v1.0.txt", std::ios::app);
                        if (rawFile.is_open()) {
                            rawFile << "command: " << std::endl;
                            rawFile << command_converted_utf << std::endl;
                            rawFile.close();
                        }

                        // само действие
                        system("shutdown /r /t 0");
                    }
                }
            }
        }

        Sleep(5000);
    }

    return 0;
}