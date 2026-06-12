/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.utils.crashdump;
import std.file : write, thisExePath;
//import std.stdio;
import std.path;
import std.process : environment;
import std.traits;
import std.array;
import i18n;

private string serializeCrashDumpState(T)(auto ref T value) {
    import std.conv : text;
    import std.string : replace;
    static if (is(T == string) || is(T == immutable(char)[]) || is(T == const(char)[])) {
        return `"` ~ text(value).replace(`\`, `\\`).replace(`"`, `\"`) ~ `"`;
    } else static if (is(T == typeof(null))) {
        return "null";
    } else {
        return text(value);
    }
}

version(Posix) {
    import core.stdc.signal : raise;
    import core.stdc.stdlib : malloc;
    import core.sys.posix.fcntl : O_CREAT, O_TRUNC, O_WRONLY, open;
    import core.sys.posix.signal : SA_ONSTACK, SA_RESETHAND, SA_SIGINFO, SIGSEGV, SIGSTKSZ,
        sigaction, sigaction_t, sigaltstack, sigemptyset, siginfo_t, stack_t;
    import core.sys.posix.sys.types : mode_t;
    import core.sys.posix.unistd : _exit, close, execv, fork, getpid, write;

    version(OSX) {
        import core.sys.darwin.execinfo : backtrace, backtrace_symbols_fd;
    } else version(linux) {
        import core.sys.linux.execinfo : backtrace, backtrace_symbols_fd;
    }
}

string genCrashDump(T...)(Throwable t, T state) {
    string[] args;
    static foreach(i; 0 .. state.length) {
        args ~= serializeCrashDumpState(state[i]);
    }
    Appender!string str;
    str.put("=== Args State ===\n");
    str.put(args.join(",\n"));
    str.put("\n\n=== Exception ===\n");
    str.put(t.toString());
    return str.data;
}

version(Windows) {
    pragma(lib, "user32.lib");
    pragma(lib, "shell32.lib");
    pragma(lib, "dbghelp.lib");
    import core.sys.windows.winuser : MessageBoxW;
    import std.utf : toUTF16z, toUTF8;
    import std.string : fromStringz;

    private string getDesktopDir() {
        import core.sys.windows.windows : HWND_DESKTOP, MAX_PATH;
        import core.sys.windows.shlobj : CSIDL_DESKTOP, SHGetSpecialFolderPath;
        wstring desktopDir = new wstring(MAX_PATH);
        SHGetSpecialFolderPath(HWND_DESKTOP, cast(wchar*)desktopDir.ptr, CSIDL_DESKTOP, FALSE);
        return (cast(wstring)fromStringz!wchar(desktopDir.ptr)).toUTF8;
    }

    private void ShowMessageBox(string message, string title) {
        MessageBoxW(null, toUTF16z(message), toUTF16z(title), 0);
    }

    private enum size_t nativeCrashPathMax = 4096;
    private alias DWORD = uint;
    private alias BOOL = int;
    private alias LONG = int;
    private alias HANDLE = void*;
    private alias MINIDUMP_TYPE = int;

    private enum DWORD GENERIC_WRITE = 0x40000000;
    private enum DWORD FILE_SHARE_READ = 0x00000001;
    private enum DWORD CREATE_ALWAYS = 2;
    private enum DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080;
    private enum BOOL FALSE = 0;
    private enum LONG EXCEPTION_EXECUTE_HANDLER = 1;

    private struct EXCEPTION_RECORD {
        DWORD ExceptionCode;
        DWORD ExceptionFlags;
        EXCEPTION_RECORD* ExceptionRecord;
        void* ExceptionAddress;
        DWORD NumberParameters;
        size_t[15] ExceptionInformation;
    }

    private struct EXCEPTION_POINTERS {
        EXCEPTION_RECORD* ExceptionRecord;
        void* ContextRecord;
    }

    private enum MINIDUMP_TYPE MiniDumpNormal = 0x00000000;
    private enum MINIDUMP_TYPE MiniDumpWithDataSegs = 0x00000001;
    private enum MINIDUMP_TYPE MiniDumpWithHandleData = 0x00000004;
    private enum MINIDUMP_TYPE MiniDumpWithIndirectlyReferencedMemory = 0x00000040;
    private enum MINIDUMP_TYPE MiniDumpWithThreadInfo = 0x00001000;

    private struct MINIDUMP_EXCEPTION_INFORMATION {
        DWORD ThreadId;
        EXCEPTION_POINTERS* ExceptionPointers;
        BOOL ClientPointers;
    }

    private alias TopLevelExceptionFilter = extern(Windows) LONG function(EXCEPTION_POINTERS*) nothrow @nogc;

    private extern(Windows) {
        HANDLE CreateFileW(const(wchar)* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
            void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
            HANDLE hTemplateFile) nothrow @nogc;
        BOOL WriteFile(HANDLE hFile, const(void)* lpBuffer, DWORD nNumberOfBytesToWrite,
            DWORD* lpNumberOfBytesWritten, void* lpOverlapped) nothrow @nogc;
        BOOL CloseHandle(HANDLE hObject) nothrow @nogc;
        HANDLE GetCurrentProcess() nothrow @nogc;
        DWORD GetCurrentProcessId() nothrow @nogc;
        DWORD GetCurrentThreadId() nothrow @nogc;
        TopLevelExceptionFilter SetUnhandledExceptionFilter(TopLevelExceptionFilter lpTopLevelExceptionFilter) nothrow @nogc;
        BOOL MiniDumpWriteDump(HANDLE hProcess, DWORD processId, HANDLE hFile, MINIDUMP_TYPE dumpType,
            MINIDUMP_EXCEPTION_INFORMATION* exceptionParam, void* userStreamParam,
            void* callbackParam) nothrow @nogc;
    }

    __gshared wchar[nativeCrashPathMax] nativeCrashDumpPathW;
    __gshared size_t nativeCrashDumpPathWLen;
    __gshared wchar[nativeCrashPathMax] nativeCrashMiniDumpPathW;
    __gshared size_t nativeCrashMiniDumpPathWLen;
    __gshared int nativeCrashHandlerActive;

    private void copyNativeCrashPath(ref wchar[nativeCrashPathMax] target, ref size_t targetLen, string path) {
        import std.utf : toUTF16;
        auto encoded = path.toUTF16;
        auto n = encoded.length < nativeCrashPathMax - 1 ? encoded.length : nativeCrashPathMax - 1;
        target[0 .. n] = encoded[0 .. n];
        target[n] = 0;
        targetLen = n;
    }

    private bool validNativeHandle(HANDLE handle) nothrow @nogc {
        return handle !is null && handle != cast(HANDLE)size_t.max;
    }

    private void writeAll(HANDLE file, const(char)[] text) nothrow @nogc {
        if (!validNativeHandle(file) || text.length == 0) return;
        size_t pos;
        while (pos < text.length) {
            auto remaining = text.length - pos;
            auto chunk = remaining < uint.max ? cast(DWORD)remaining : uint.max;
            DWORD written;
            if (!WriteFile(file, text.ptr + pos, chunk, &written, null) || written == 0)
                return;
            pos += written;
        }
    }

    private void writeInt(HANDLE file, ulong value) nothrow @nogc {
        char[32] buf;
        size_t pos = buf.length;
        do {
            buf[--pos] = cast(char)('0' + (value % 10));
            value /= 10;
        } while (value);
        writeAll(file, buf[pos .. $]);
    }

    private void writeHex(HANDLE file, size_t value) nothrow @nogc {
        enum digits = "0123456789ABCDEF";
        char[2 + size_t.sizeof * 2] buf;
        buf[0] = '0';
        buf[1] = 'x';
        foreach (i; 0 .. size_t.sizeof * 2) {
            auto shift = cast(uint)((size_t.sizeof * 2 - 1 - i) * 4);
            buf[2 + i] = digits[(value >> shift) & 0xF];
        }
        writeAll(file, buf[]);
    }

    private HANDLE createCrashFile(const(wchar)* path) nothrow @nogc {
        return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, null, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, null);
    }

    private extern(Windows) LONG nativeCrashExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) nothrow @nogc {
        if (nativeCrashHandlerActive)
            return EXCEPTION_EXECUTE_HANDLER;
        nativeCrashHandlerActive = 1;

        HANDLE textFile = nativeCrashDumpPathWLen > 0 ? createCrashFile(nativeCrashDumpPathW.ptr) : null;
        HANDLE dumpFile = nativeCrashMiniDumpPathWLen > 0 ? createCrashFile(nativeCrashMiniDumpPathW.ptr) : null;

        if (validNativeHandle(textFile)) {
            writeAll(textFile, "=== nijigenerate native crashdump ===\r\n");
            writeAll(textFile, "pid: ");
            writeInt(textFile, GetCurrentProcessId());
            writeAll(textFile, "\r\nthread: ");
            writeInt(textFile, GetCurrentThreadId());
            if (exceptionInfo !is null && exceptionInfo.ExceptionRecord !is null) {
                writeAll(textFile, "\r\nexception code: ");
                writeHex(textFile, exceptionInfo.ExceptionRecord.ExceptionCode);
                writeAll(textFile, "\r\nexception address: ");
                writeHex(textFile, cast(size_t)exceptionInfo.ExceptionRecord.ExceptionAddress);
            }
            writeAll(textFile, "\r\nminidump: ");
            if (nativeCrashMiniDumpPathWLen > 0) {
                foreach (ch; nativeCrashMiniDumpPathW[0 .. nativeCrashMiniDumpPathWLen]) {
                    char c = ch < 128 ? cast(char)ch : '?';
                    writeAll(textFile, (&c)[0 .. 1]);
                }
            } else {
                writeAll(textFile, "(unavailable)");
            }
            writeAll(textFile, "\r\n\r\nAttach both this text file and the .dmp file when filing an issue.\r\n");
        }

        if (validNativeHandle(dumpFile)) {
            MINIDUMP_EXCEPTION_INFORMATION dumpException;
            dumpException.ThreadId = GetCurrentThreadId();
            dumpException.ExceptionPointers = exceptionInfo;
            dumpException.ClientPointers = FALSE;
            auto dumpType = cast(MINIDUMP_TYPE)(
                MiniDumpNormal |
                MiniDumpWithDataSegs |
                MiniDumpWithHandleData |
                MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpWithThreadInfo);
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, dumpType,
                &dumpException, null, null);
        }

        if (validNativeHandle(dumpFile)) CloseHandle(dumpFile);
        if (validNativeHandle(textFile)) CloseHandle(textFile);
        return EXCEPTION_EXECUTE_HANDLER;
    }
}

version(Posix) {
    private void ShowMessageBox(string dumpPath) {
        import tinyfiledialogs;
        import std.format : format;
        import std.string : toStringz;
        import std.stdio : writeln;
        auto title = __("nijigenerate Crashdump");
        auto message = _("The application has unexpectedly crashed.\n\nIf autosave is enabled, open File → Recent → Autosaves to recover your work.\n\nA crash report was written to:\n%s\n\nPlease attach it when filing an issue:\nhttps://github.com/nijigenerate/nijigenerate/issues").format(
            dumpPath.length ? dumpPath : getCrashDumpDir());

        tinyfd_messageBox(title, message.toStringz, "ok", "error", 1);
    }
}

string linuxStateHome() {
    // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
    return environment.get("XDG_STATE_HOME", buildPath(environment["HOME"], ".local", "state"));
}

string getCrashDumpDir() {
    version(Windows) return getDesktopDir();
    else version(OSX) return expandTilde("~/Library/Logs/");
    else version(linux) return expandTilde(linuxStateHome() ~ "/");
    else return expandTilde("~");
}

string genCrashDumpPath(string filename) {
    import std.datetime;
    auto timestamp = Clock.currTime.toISOString();
    version(Windows) {
        import std.string : tr;
        timestamp = timestamp.tr(`<>:"/\|?*`, "---------");
    }
    return buildPath(getCrashDumpDir(), filename ~ "-" ~ timestamp ~ ".txt");
}

version(Posix) {
    enum size_t nativeCrashPathMax = 4096;
    enum int nativeBacktraceMaxFrames = 128;
    __gshared char[nativeCrashPathMax] nativeCrashDumpPath;
    __gshared size_t nativeCrashDumpPathLen;
    __gshared char[nativeCrashPathMax] nativeCrashExePath;
    __gshared size_t nativeCrashExePathLen;
    __gshared int nativeCrashHandlerActive;
    void* nativeSignalStack;

    private void copyNativeCrashDumpPath(string path) {
        auto n = path.length < nativeCrashPathMax - 1 ? path.length : nativeCrashPathMax - 1;
        nativeCrashDumpPath[0 .. n] = path[0 .. n];
        nativeCrashDumpPath[n] = '\0';
        nativeCrashDumpPathLen = n;
    }

    private void writeAll(int fd, const(char)[] text) nothrow @nogc {
        if (fd < 0 || text.length == 0) return;
        size_t pos;
        while (pos < text.length) {
            auto wrote = write(fd, text.ptr + pos, text.length - pos);
            if (wrote <= 0) return;
            pos += cast(size_t)wrote;
        }
    }

    private void writeInt(int fd, long value) nothrow @nogc {
        char[32] buf;
        size_t pos = buf.length;
        bool neg = value < 0;
        ulong v = neg ? cast(ulong)(-value) : cast(ulong)value;
        do {
            buf[--pos] = cast(char)('0' + (v % 10));
            v /= 10;
        } while (v);
        if (neg) buf[--pos] = '-';
        writeAll(fd, buf[pos .. $]);
    }

    private void writeNativeCrashDumpFromSignal(int sig) nothrow @nogc {
        int fd = -1;
        if (nativeCrashDumpPathLen > 0) {
            fd = open(nativeCrashDumpPath.ptr, O_WRONLY | O_CREAT | O_TRUNC, cast(mode_t)384); // 0600
        }
        if (fd >= 0) {
            writeAll(fd, "=== nijigenerate native crashdump ===\n");
            writeAll(fd, "signal: ");
            writeInt(fd, sig);
            writeAll(fd, "\npid: ");
            writeInt(fd, getpid());
            writeAll(fd, "\npath: ");
            writeAll(fd, nativeCrashDumpPath[0 .. nativeCrashDumpPathLen]);
            writeAll(fd, "\n\n=== Backtrace ===\n");

            static if (__traits(compiles, backtrace((void**).init, int.init))) {
                void*[nativeBacktraceMaxFrames] frames;
                auto count = backtrace(frames.ptr, nativeBacktraceMaxFrames);
                backtrace_symbols_fd(cast(const(void*)*)frames.ptr, count, fd);
            } else {
                writeAll(fd, "backtrace unavailable on this platform\n");
            }

            writeAll(fd, "\n\nSymbolication hint:\n");
            writeAll(fd, "  atos -o <nijigenerate-binary> -arch arm64 <address>\n");
            close(fd);
        }
    }

    private void notifyNativeCrashUserAfterSignal() nothrow @nogc {
        if (nativeCrashExePathLen == 0) return;
        if (fork() != 0) return;
        const(char)*[4] argv = [
            nativeCrashExePath.ptr, "--crash-notify", nativeCrashDumpPath.ptr, null,
        ];
        execv(nativeCrashExePath.ptr, cast(char**)argv.ptr);
        _exit(127);
    }

    extern(C) private void nativeCrashSignalHandler(int sig, siginfo_t* info, void* context) nothrow @nogc {
        if (nativeCrashHandlerActive) {
            _exit(128 + sig);
        }
        nativeCrashHandlerActive = 1;

        writeNativeCrashDumpFromSignal(sig);
        notifyNativeCrashUserAfterSignal();

        // SA_RESETHAND has already restored the default handler; re-raise so
        // the process still terminates as a native crash and OS reports remain useful.
        raise(sig);
        _exit(128 + sig);
    }

    private void installNativeCrashHandlerFor(int sig) {
        sigaction_t action = void;
        (cast(ubyte*)&action)[0 .. sigaction_t.sizeof] = 0;
        action.sa_sigaction = &nativeCrashSignalHandler;
        action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        sigaction(sig, &action, null);
    }

    void installNativeCrashDumpThreadHandler() {
        if (nativeSignalStack is null) {
            nativeSignalStack = malloc(SIGSTKSZ);
            if (nativeSignalStack !is null) {
                stack_t stack;
                stack.ss_sp = nativeSignalStack;
                stack.ss_size = SIGSTKSZ;
                stack.ss_flags = 0;
                sigaltstack(&stack, null);
            }
        }
    }

    void installNativeCrashDumpHandler() {
        copyNativeCrashDumpPath(thisExePath());
        nativeCrashExePath = nativeCrashDumpPath;
        nativeCrashExePathLen = nativeCrashDumpPathLen;
        mkdirCrashDumpDir();
        copyNativeCrashDumpPath(genCrashDumpPath("nijigenerate-native-crashdump"));
        installNativeCrashDumpThreadHandler();
        installNativeCrashHandlerFor(SIGSEGV);
    }
} else version(Windows) {
    void installNativeCrashDumpHandler() {
        mkdirCrashDumpDir();
        auto textPath = genCrashDumpPath("nijigenerate-native-crashdump");
        auto dumpPath = textPath.setExtension("dmp");
        copyNativeCrashPath(nativeCrashDumpPathW, nativeCrashDumpPathWLen, textPath);
        copyNativeCrashPath(nativeCrashMiniDumpPathW, nativeCrashMiniDumpPathWLen, dumpPath);
        SetUnhandledExceptionFilter(&nativeCrashExceptionHandler);
    }

    void installNativeCrashDumpThreadHandler() {}
} else {
    void installNativeCrashDumpHandler() {}
    void installNativeCrashDumpThreadHandler() {}
}

void mkdirCrashDumpDir() {
    import std.file : mkdir, exists, setAttributes;
    auto dir = getCrashDumpDir();
    if (exists(dir))
        return;
    
    // Should we set recursively make the directory or not?
    mkdir(dir);
    version(linux) {
        import std.conv : octal;
        // https://specifications.freedesktop.org/basedir-spec/latest/#referencing
        // TODO: Should we set permissions recursively?
        setAttributes(dir, octal!700);
    }
}

string writeCrashDump(T...)(string filename, Throwable throwable, T state) {
    mkdirCrashDumpDir();
    string path = genCrashDumpPath(filename);
    write(path, genCrashDump(throwable, state));
    return path;
}

void crashdump(T...)(Throwable throwable, T state) {
    // Write crash dump to disk
    string dumpPath;
    try {
        dumpPath = writeCrashDump("nijigenerate-crashdump", throwable, state);
    } catch (Exception ex) {
        version (Windows) {
        } else{
            import std.stdio;
            writeln("Failed to write crash dump" ~ ex.msg);
        }
    }

    // Use appropriate system method to notify user where crash dump is.
    notifyCrashUser(dumpPath);
}

void notifyCrashUser(string dumpPath) {
    version(OSX) {
        import std.stdio;
        writeln(_("\n\n\n===   nijigenerate has crashed   ===\nPlease send us the nijigenerate-crashdump.txt file in ~/Library/Logs\nAttach the file as a git issue @ https://github.com/nijigenerate/nijigenerate/issues"));
        ShowMessageBox(dumpPath);
    }
    else version(linux) {
        import std.stdio;
        writeln(_("\n\n\n===   nijigenerate has crashed   ===\nPlease send us the nijigenerate-crashdump.txt file in your log directory, XDG_STATE_HOME. For Flatpak, this is in ~/.var/app/nijigenerate.nijigenerate.\nAttach the file as a git issue @ https://github.com/nijigenerate/nijigenerate/issues"));
        ShowMessageBox(dumpPath);
    }
    else version(Windows) ShowMessageBox(
        _("The application has unexpectedly crashed\nPlease send the developers the nijigenerate-crashdump.txt which has been put on your desktop\nVia https://github.com/nijigenerate/nijigenerate/issues"),
        _("nijigenerate Crashdump")
    );
}
