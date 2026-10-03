#include "platform/win32/CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
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

LONG WINAPI onCrash(EXCEPTION_POINTERS* info) {
    FILE* out = _wfopen(besideExe(L"openwheels_crash.txt").c_str(), L"w");
    if (!out) return EXCEPTION_CONTINUE_SEARCH;

    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    std::fprintf(out, "exception 0x%08lx at %p", rec->ExceptionCode, rec->ExceptionAddress);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
        std::fprintf(out, " (%s 0x%p)", rec->ExceptionInformation[0] ? "write" : "read",
                     reinterpret_cast<void*>(rec->ExceptionInformation[1]));
    std::fprintf(out, "\n\n");

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT ctx = *info->ContextRecord;
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
    }
    std::fclose(out);
    SymCleanup(process);
    writeMinidump(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void installCrashHandler() {
    // Room for the handler itself when the crash is a stack overflow.
    ULONG guarantee = 128 * 1024;
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(onCrash);
}

}  // namespace pc
}  // namespace openwheels
