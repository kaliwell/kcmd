#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <thread>
#include <iostream>
#include <mutex>
#include <sstream>

std::vector<DWORD> pid_list;
std::mutex console_mutex;

BOOL kill_process(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL)
        return FALSE;
    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result;
}

BOOL kill_process_and_children(DWORD pid, bool killChildren, bool killParent) {
    // First, kill all child processes
    if (killChildren) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(hSnapshot, &pe32)) {
                do {
                    if (pe32.th32ParentProcessID == pid)
                        kill_process_and_children(pe32.th32ProcessID, killChildren, killParent);
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
    }

    // Then, kill the process itself
    if (killParent) {
        return kill_process(pid);
    }
    return TRUE;
}

void get_processes(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName) == 0)
                    pid_list.push_back(pe32.th32ProcessID);
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
}

void print_help() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    std::wcout << L"[+]Usage: kcmd [options]\n";
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset to white
    std::wcout << L"Options:\n"
        << L"  -f    Only kill parent processes\n"
        << L"  -s    Only kill child processes\n"
        << L"  -n    Specify the name of the processes to kill\n"
        << L"  -wt   Kill all WindowsTerminal.exe processes\n"
        << L"  -help Display this help message\n";
}

int wmain(int argc, wchar_t* argv[]) {
    const wchar_t* processName = L"cmd.exe";
    bool invalidOption = false;

    if (argc >= 2) {
        if (wcscmp(argv[1], L"-help") == 0) {
            print_help();
            return 0;
        }
        else if (wcscmp(argv[1], L"-f") == 0 || wcscmp(argv[1], L"-s") == 0 || wcscmp(argv[1], L"-a") == 0) {
            // valid options, do nothing
        }
        else if (wcscmp(argv[1], L"-n") == 0 && argc >= 3) {
            processName = argv[2];
        }
        else if (wcscmp(argv[1], L"-wt") == 0) {
            processName = L"WindowsTerminal.exe";
        }
        else {
            invalidOption = true;
        }
    }

    if (invalidOption) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::wcout << L"[-]Invalid option\n";
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset to white
        print_help();
        return 1;
    }

    get_processes(processName);

    std::vector<std::thread> threads;
for (DWORD pid : pid_list) {
    threads.push_back(std::thread([pid, argc, argv]() {
        bool killChildren = true;
        bool killParent = true;
        if (argc >= 2) {
            if (wcscmp(argv[1], L"-f") == 0) {
                killChildren = false;
            }
            if (wcscmp(argv[1], L"-s") == 0) {
                killParent = false;
            }
            if (wcscmp(argv[1], L"-a") == 0) {
                killChildren = true;
                killParent = true;
            }
            }
            if (kill_process_and_children(pid, killChildren, killParent)) {
                std::lock_guard<std::mutex> lock(console_mutex);
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                std::wstringstream ss;
                ss << L"[+]";
                SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                std::wcout << ss.str();
                SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Set to green
                std::wcout << L"Successfully ";
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY); // Set to red
                std::wcout << L"killed";
                SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                std::wcout << L" PID: ";
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Set to yellow
                std::wcout << pid;
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset to white
                std::wcout << std::endl;
            }
            }));
    }

    for (std::thread& t : threads) {
        if (t.joinable())
            t.join();
    }

    return 0;
}