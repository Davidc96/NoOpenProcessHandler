#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "ntdll.lib")

using fnNtGetNextProcess = NTSTATUS(NTAPI*)(HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);
using fnNtGetNextThread  = NTSTATUS(NTAPI*)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);

typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS  ExitStatus;
    PTEB      TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

#define STATUS_NO_MORE_ENTRIES ((NTSTATUS)0x8000001A)

fnNtGetNextProcess _NtGetNextProcess = nullptr;
fnNtGetNextThread  _NtGetNextThread  = nullptr;

struct ProcessEntry {
    DWORD        pid;
    HANDLE       handle;
    std::wstring name;
};

void EnumThreads(HANDLE hProcess, DWORD pid) {
    std::cout << "\nThreads of PID " << pid << ":\n";

    HANDLE hThread = nullptr;

    while (true) {
        HANDLE hNextThread = nullptr;
        NTSTATUS st = _NtGetNextThread(hProcess, hThread, THREAD_QUERY_INFORMATION, 0, 0, &hNextThread);

        if (hThread) CloseHandle(hThread);
        hThread = hNextThread;

        if (st == STATUS_NO_MORE_ENTRIES || !NT_SUCCESS(st))
            break;

        THREAD_BASIC_INFORMATION tbi = {};
        NtQueryInformationThread(hThread, (THREADINFOCLASS)0, &tbi, sizeof(tbi), nullptr);

        std::cout << "    TID: " << (ULONG_PTR)tbi.ClientId.UniqueThread << "\n";
    }
}

int main() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    _NtGetNextProcess = (fnNtGetNextProcess)GetProcAddress(ntdll, "NtGetNextProcess");
    _NtGetNextThread  = (fnNtGetNextThread)GetProcAddress(ntdll, "NtGetNextThread");

    std::vector<ProcessEntry> processes;
    HANDLE hProc = nullptr;

    std::cout << "Processes\n";

    while (true) {
        NTSTATUS st = _NtGetNextProcess(hProc, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, 0, &hProc);

        if (st == STATUS_NO_MORE_ENTRIES || !NT_SUCCESS(st))
            break;

        PROCESS_BASIC_INFORMATION pbi = {};
        NtQueryInformationProcess(hProc, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

        WCHAR name[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        QueryFullProcessImageNameW(hProc, 0, name, &size);

        DWORD pid = (DWORD)(ULONG_PTR)pbi.UniqueProcessId;
        std::wcout << L"  [" << pid << L"] " << name << L"\n";

        processes.push_back({ pid, hProc, name });
    }

    std::cout << "\nEnter a PID to list its threads: ";
    DWORD targetPid;
    std::cin >> targetPid;

    bool found = false;
    for (auto& p : processes) {
        if (p.pid == targetPid) {
            EnumThreads(p.handle, p.pid);
            found = true;
        }
        CloseHandle(p.handle);
    }

    if (!found)
        std::cout << "PID not found.\n";

    return 0;
}
