#include "platform/win32/CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace openwheels {
namespace pc {
namespace {

std::wstring besideExe(const wchar_t* name) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path, n);
    s.resize(s.find_last_of(L"\\/") + 1);
    return s + name;
}

void writeMinidump(EXCEPTION_POINTERS* info) {
    HANDLE f = CreateFileW(besideExe(L"openwheels_crash.dmp").c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), info, FALSE};
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, &mei, nullptr, nullptr);
    CloseHandle(f);
}

// Writes `header` and a symbolized stack walked from `context` (of `thread`) to `file`.
void writeReport(const char* header, CONTEXT ctx, HANDLE thread = GetCurrentThread(),
                 const wchar_t* file = L"openwheels_crash.txt") {
    FILE* out = _wfopen(besideExe(file).c_str(), L"w");
    if (!out) return;
    std::fprintf(out, "%s\n\n", header);
    std::fflush(out);

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    STACKFRAME64 frame{};
#if defined(_M_IX86)
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrStack.Offset = ctx.Esp;
#else
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrStack.Offset = ctx.Rsp;
#endif
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

    char symBuf[sizeof(SYMBOL_INFO) + 512];
    for (int i = 0; i < 64; ++i) {
        if (!StackWalk64(machine, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset)
            break;
        const DWORD64 pc = frame.AddrPC.Offset;
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 symOff = 0;
        const bool haveSym = SymFromAddr(process, pc, &symOff, sym) != FALSE;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD lineOff = 0;
        const bool haveLine = SymGetLineFromAddr64(process, pc, &lineOff, &line) != FALSE;
        std::fprintf(out, "#%-2d %p %s+0x%llx", i, reinterpret_cast<void*>(pc), haveSym ? sym->Name : "?",
                     static_cast<unsigned long long>(symOff));
        if (haveLine) std::fprintf(out, "  %s:%lu", line.FileName, line.LineNumber);
        std::fprintf(out, "\n");
        std::fflush(out);
    }
    std::fclose(out);
    SymCleanup(process);
}

LONG WINAPI onCrash(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    char header[160];
    int n = std::snprintf(header, sizeof header, "exception 0x%08lx at %p", rec->ExceptionCode, rec->ExceptionAddress);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
        std::snprintf(header + n, sizeof header - n, " (%s 0x%p)", rec->ExceptionInformation[0] ? "write" : "read",
                      reinterpret_cast<void*>(rec->ExceptionInformation[1]));
    writeReport(header, *info->ContextRecord);
    writeMinidump(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

// CRT failure paths that never reach the unhandled-exception filter (they fail fast).
void reportHere(const char* header) {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&ctx);
    writeReport(header, ctx);
}

void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    reportHere("CRT invalid parameter");
    TerminateProcess(GetCurrentProcess(), 3);
}

void onPureCall() {
    reportHere("pure virtual function call");
    TerminateProcess(GetCurrentProcess(), 3);
}

void onTerminate() {
    reportHere("std::terminate (unhandled C++ exception?)");
    TerminateProcess(GetCurrentProcess(), 3);
}

void onAbort(int) {
    reportHere("abort()");
    TerminateProcess(GetCurrentProcess(), 3);
}

// ---- hang watchdog --------------------------------------------------------------------------------
volatile LONG g_heartbeat = 0;
HANDLE g_mainThread = nullptr;

DWORD WINAPI watchdog(void*) {
    LONG last = g_heartbeat;
    int stalledSeconds = 0;
    for (;;) {
        Sleep(1000);
        const LONG now = g_heartbeat;
        if (now != last) {
            last = now;
            stalledSeconds = 0;
            continue;
        }
        if (now == 0 || ++stalledSeconds != 10) continue;  // not started yet, or already reported
        // The main loop made no progress for 10 s: record where the main thread is (once per stall).
        if (SuspendThread(g_mainThread) == (DWORD)-1) continue;
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(g_mainThread, &ctx))
            writeReport("hang: the main loop made no progress for 10 seconds; main thread stack:", ctx,
                        g_mainThread, L"openwheels_hang.txt");
        ResumeThread(g_mainThread);
    }
}

}  // namespace

void heartbeat() { InterlockedIncrement(&g_heartbeat); }

void installHangWatchdog() {
    if (g_mainThread) return;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    CloseHandle(CreateThread(nullptr, 64 * 1024, watchdog, nullptr, 0, nullptr));
}

void installCrashHandler() {
    // Room for the handler itself when the crash is a stack overflow.
    ULONG guarantee = 128 * 1024;
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(onCrash);
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_purecall_handler(onPureCall);
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onAbort);
}

}  // namespace pc
}  // namespace openwheels
