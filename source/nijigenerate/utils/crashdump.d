/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.utils.crashdump;
import std.file : write;
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
    import core.sys.posix.unistd : _exit, close, fork, getpid, write;

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
    import core.sys.windows.winuser : MessageBoxW;
    import std.utf : toUTF16z, toUTF8;
    import std.string : fromStringz;

    private string getDesktopDir() {
        import core.sys.windows.windows;
        import core.sys.windows.shlobj;
        wstring desktopDir = new wstring(MAX_PATH);
        SHGetSpecialFolderPath(HWND_DESKTOP, cast(wchar*)desktopDir.ptr, CSIDL_DESKTOP, FALSE);
        return (cast(wstring)fromStringz!wchar(desktopDir.ptr)).toUTF8;
    }

    private void ShowMessageBox(string message, string title) {
        MessageBoxW(null, toUTF16z(message), toUTF16z(title), 0);
    }
}

version(Posix) {
    private void ShowMessageBox(string message, string title) {
        import tinyfiledialogs;
        import std.string : toStringz;
        import std.stdio : writeln;
        // Native dialog (Cocoa / GTK / Zenity). Do not use SDL/ImGui here; the GL
        // frame may be broken after an exception and SDL message boxes often fail silently.
        tinyfd_messageBox(toStringz(title), toStringz(message), "ok", "error", 1);
        writeln(title);
        writeln(message);
    }

    private void ShowMessageBox(string dumpPath) {
        import std.format : format;
        ShowMessageBox(
            _("The application has unexpectedly crashed.\n\nIf autosave is enabled, open File → Recent → Autosaves to recover your work.\n\nA crash report was written to:\n%s\n\nPlease attach it when filing an issue:\nhttps://github.com/nijigenerate/nijigenerate/issues").format(
                dumpPath.length ? dumpPath : getCrashDumpDir()),
            _("nijigenerate Crashdump")
        );
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
    return buildPath(getCrashDumpDir(), filename ~ "-" ~ Clock.currTime.toISOString() ~ ".txt");
}

version(Posix) {
    enum size_t nativeCrashPathMax = 4096;
    enum int nativeBacktraceMaxFrames = 128;
    __gshared char[nativeCrashPathMax] nativeCrashDumpPath;
    __gshared size_t nativeCrashDumpPathLen;
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

    extern(C) private void nativeCrashSignalHandler(int sig, siginfo_t* info, void* context) {
        if (nativeCrashHandlerActive) {
            _exit(128 + sig);
        }
        nativeCrashHandlerActive = 1;

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

        // crashdump() is not reached on SIGSEGV; fork a child to notify the user.
        auto pid = fork();
        if (pid == 0) {
            try {
                string dumpPath = nativeCrashDumpPathLen > 0
                    ? nativeCrashDumpPath[0 .. nativeCrashDumpPathLen].idup : "";
                notifyCrashUser(dumpPath);
            } catch (Throwable) {}
            _exit(0);
        }

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
        mkdirCrashDumpDir();
        copyNativeCrashDumpPath(genCrashDumpPath("nijigenerate-native-crashdump"));
        installNativeCrashDumpThreadHandler();
        installNativeCrashHandlerFor(SIGSEGV);
    }
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
