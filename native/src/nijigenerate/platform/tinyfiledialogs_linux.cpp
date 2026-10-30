/*
    Altered C++ port of the independent tinyfiledialogs 0.10.1 D package,
    itself a custom port of tinyfiledialogs 3.3.9 (April 14, 2019).
    Copyright (c) 2014-2019 Guillaume Vareille
    Copyright (c) 2019 dayllenger

    zlib License

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/
#include "tinyfiledialogs.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <utility>

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char** environ;
#endif

namespace nijigenerate::platform::tinyfiledialogs {
const char tinyfd_version[8] = "3.3.9";
const char tinyfd_needs[] = R"tinyfd( ___________
/           \
| tiny file |
|  dialogs  |
\_____  ____/
      \|
tiny file dialogs on UNIX needs:
   applescript
or kdialog
or zenity (or matedialog or qarma)
or python (2 or 3)
 + tkinter + python-dbus (optional)
or dialog (opens console if needed)
or xterm + bash
   (opens console for basic input)
or existing console for basic input)tinyfd";
bool tinyfd_verbose = false;
bool tinyfd_silent = true;
bool tinyfd_forceConsole = false;
char tinyfd_response[1024]{};

namespace {
char** processEnvironment() {
#if defined(__APPLE__)
    return *_NSGetEnviron();
#else
    return environ;
#endif
}
constexpr int MAX_PATH_OR_CMD = 1024;
constexpr int MAX_MULTIPLE_FILES = 32;
constexpr int MAX_PATTERNS = 8;
constexpr char gTitle[] = "missing software! (we will try basic console input)";
bool gWarningDisplayed = false;
char gPython2Name[16]{};
char gPython3Name[16]{};
char gPythonName[16]{};

bool some(const char* value) {
    return value && value[0] != '\0';
}
bool eq(const char* left, const char* right) {
    if (left == right) return true;
    if (!left || !right) return false;
    return strcmp(left, right) == 0;
}
char lastch(const char* value) {
    if (value) {
        const auto length = strlen(value);
        if (length) return value[length - 1];
    }
    return '\0';
}
void removeLastNL(char* value) {
    const auto length = strlen(value);
    if (length && value[length - 1] == '\n') value[length - 1] = '\0';
}
void response(const char* message) {
    strcpy(tinyfd_response, message);
}
unsigned getValidPatterns(const TFD_Filter& filter, bool& isMime,
        std::array<const char*, MAX_PATTERNS>& output) {
    isMime = false;
    output.fill(nullptr);
    unsigned length = 0;
    for (const auto pattern : filter.patterns) {
        if (some(pattern)) {
            output[length++] = pattern;
            if (length == MAX_PATTERNS) break;
        }
    }
    if (!length) return 0;
    unsigned mime = 0;
    for (unsigned i = 0; i < length; ++i) {
        if (strchr(output[i], '/')) ++mime;
        else break;
    }
    if (mime) {
        if (mime != length) {
            for (unsigned i = 0; i < length; ++i) output[i] = nullptr;
            return 0;
        }
        isMime = true;
    }
    return length;
}
void Hex2RGB(const char* hex, std::array<unsigned char, 3>& result) {
    char channel[8]{};
    if (hex) {
        strncpy(channel, hex, sizeof(channel) - 1);
        result[2] = static_cast<unsigned char>(strtoul(channel + 5, nullptr, 16));
        channel[5] = '\0';
        result[1] = static_cast<unsigned char>(strtoul(channel + 3, nullptr, 16));
        channel[3] = '\0';
        result[0] = static_cast<unsigned char>(strtoul(channel + 1, nullptr, 16));
    } else result = {};
}
void RGB2Hex(const std::array<unsigned char, 3>& rgb, char* result) {
    if (result) sprintf(result, "#%02hhx%02hhx%02hhx", rgb[0], rgb[1], rgb[2]);
}
bool filenameValid(const char* name) {
    return some(name) && !strpbrk(name, "\\/:*?\"<>|");
}
bool fileExists(const char* filename) {
    if (!some(filename)) return false;
    FILE* input = fopen(filename, "r");
    if (!input) return false;
    fclose(input);
    return true;
}
int isDarwin() {
    static thread_local int result = -1;
    utsname value;
    if (result < 0) result = !uname(&value) && eq(value.sysname, "Darwin");
    return result;
}
bool dirExists(const char* path) {
    if (!some(path)) return false;
    DIR* directory = opendir(path);
    if (!directory) return false;
    closedir(directory);
    return true;
}
bool detectPresence(const char* executable) {
    char buffer[MAX_PATH_OR_CMD]{};
    char command[MAX_PATH_OR_CMD] = "which ";
    strcat(command, executable);
    strcat(command, " 2>/dev/null ");
    FILE* input = popen(command, "r");
    const bool present = input && fgets(buffer, sizeof(buffer), input) &&
        !strchr(buffer, ':') && strncmp(buffer, "no ", 3);
    if (input) pclose(input);
    if (tinyfd_verbose) printf("detectPresence %s %d\n", executable, present ? 1 : 0);
    return present;
}
const char* getVersion(const char* executable) {
    static thread_local char buffer[MAX_PATH_OR_CMD]{};
    char command[MAX_PATH_OR_CMD]{};
    strcpy(command, executable);
    strcat(command, " --version");
    FILE* input = popen(command, "r");
    char* result = input ? fgets(buffer, sizeof(buffer), input) : nullptr;
    if (input) pclose(input);
    if (!result) return nullptr;
    result += strcspn(result, "0123456789");
    return result;
}
const int* getMajorMinorPatch(const char* executable) {
    static thread_local int result[3]{};
    char* value = const_cast<char*>(getVersion(executable));
    if (!value) return nullptr;
    for (int i = 0; i < 3; ++i) {
        const auto component = strtok(i ? nullptr : value, " ,.-");
        result[i] = component ? atoi(component) : 0;
    }
    return !result[0] && !result[1] && !result[2] ? nullptr : result;
}
bool tryCommand(const char* command) {
    char buffer[MAX_PATH_OR_CMD]{};
    FILE* input = popen(command, "r");
    const bool present = input && fgets(buffer, sizeof(buffer), input) == nullptr;
    if (input) pclose(input);
    return present;
}
int isTerminalRunning() {
    static thread_local int result = -1;
    if (result < 0) {
        result = isatty(1);
        if (tinyfd_verbose) printf("isTerminalRunning %d\n", result);
    }
    return result;
}
const char* dialogNameOnly() {
    static thread_local char result[128] = "*";
    if (result[0] == '*') {
        if (isDarwin() && strcpy(result, "/opt/local/bin/dialog") && detectPresence(result)) {}
        else if (strcpy(result, "dialog") && detectPresence(result)) {}
        else strcpy(result, "");
    }
    return result;
}
bool isDialogVersionBetter09b() {
    const char* name = dialogNameOnly();
    if (!some(name)) return false;
    char* version = const_cast<char*>(getVersion(name));
    if (!version) return false;
    char buffer[128]{};
    if (strlen(version) >= sizeof(buffer)) return false;
    strcpy(buffer, version);
    const char* majorText = strtok(version, " ,.-");
    if (!majorText) { strcpy(version, buffer); return false; }
    const int major = atoi(majorText);
    char* minorText = strtok(nullptr, " ,.-abcdefghijklmnopqrstuvxyz");
    if (!minorText) { strcpy(version, buffer); return major > 0; }
    const int minor = atoi(minorText);
    const char* dateText = strtok(nullptr, " ,.-");
    int date = dateText ? atoi(dateText) : 0;
    if (date < 0) date = -date;
    char* letter = minorText + strlen(minorText);
    strcpy(version, buffer);
    strtok(letter, " ,.-");
    return major > 0 || (minor == 9 && *letter == 'b' && date >= 20031126);
}
int whiptailPresentOnly() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("whiptail");
    return result;
}
const char* terminalName() {
    static thread_local char result[128] = "*";
    char shell[64] = "*";
    if (result[0] == '*') {
        if (detectPresence("bash")) strcpy(shell, "bash -c ");
        else if (some(dialogNameOnly()) || whiptailPresentOnly()) strcpy(shell, "sh -c ");
        else { strcpy(result, ""); return nullptr; }
        if (isDarwin()) {
            if (strcpy(result, "/opt/X11/bin/xterm") && detectPresence(result)) {
                strcat(result, " -fa 'DejaVu Sans Mono' -fs 10 -title tinyfiledialogs -e ");
                strcat(result, shell);
            } else strcpy(result, "");
        } else if (strcpy(result, "xterm") && detectPresence(result)) {
            strcat(result, " -fa 'DejaVu Sans Mono' -fs 10 -title tinyfiledialogs -e ");
            strcat(result, shell);
        } else if (strcpy(result, "terminator") && detectPresence(result)) {
            strcat(result, " -x "); strcat(result, shell);
        } else if (strcpy(result, "lxterminal") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "konsole") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "kterm") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "tilix") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "xfce4-terminal") && detectPresence(result)) {
            strcat(result, " -x "); strcat(result, shell);
        } else if (strcpy(result, "mate-terminal") && detectPresence(result)) {
            strcat(result, " -x "); strcat(result, shell);
        } else if (strcpy(result, "Eterm") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "evilvte") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "pterm") && detectPresence(result)) {
            strcat(result, " -e "); strcat(result, shell);
        } else if (strcpy(result, "gnome-terminal") && detectPresence(result)) {
            const auto version = getMajorMinorPatch(result);
            if (version && (version[0] < 3 || (version[0] == 3 && version[1] <= 6))) {
                strcat(result, " --disable-factory -x "); strcat(result, shell);
            } else strcpy(result, "");
        } else strcpy(result, "");
    }
    return some(result) ? result : nullptr;
}
const char* dialogName() {
    const auto result = dialogNameOnly();
    return some(result) && (isTerminalRunning() || terminalName()) ? result : nullptr;
}
int whiptailPresent() {
    const auto result = whiptailPresentOnly();
    return result && (isTerminalRunning() || terminalName()) ? result : 0;
}
int graphicMode() {
    return !(tinyfd_forceConsole && (isTerminalRunning() || terminalName())) &&
        (getenv("DISPLAY") || (isDarwin() && (!getenv("SSH_TTY") || getenv("DISPLAY"))));
}
int pactlPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("pactl");
    return result;
}
int speakertestPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("speaker-test");
    return result;
}
int beepexePresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("beep.exe");
    return result;
}
int xmessagePresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("xmessage");
    return result && graphicMode();
}
int gxmessagePresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("gxmessage");
    return result && graphicMode();
}
int gmessagePresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("gmessage");
    return result && graphicMode();
}
int notifysendPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("notify-send");
    return result && graphicMode();
}
int perlPresent() {
    static thread_local int result = -1;
    char buffer[MAX_PATH_OR_CMD]{};
    if (result < 0) {
        result = detectPresence("perl");
        if (result) {
            FILE* input = popen(
                "perl -MNet::DBus -e \"Net::DBus->session->get_service('org.freedesktop.Notifications')\" 2>&1", "r");
            if (input && !fgets(buffer, sizeof(buffer), input)) result = 2;
            if (input) pclose(input);
            if (tinyfd_verbose) printf("perl-dbus %d\n", result);
        }
    }
    return graphicMode() ? result : 0;
}
int afplayPresent() {
    static thread_local int result = -1;
    char buffer[MAX_PATH_OR_CMD]{};
    if (result < 0) {
        result = detectPresence("afplay");
        if (result) {
            FILE* input = popen("test -e /System/Library/Sounds/Ping.aiff || echo Ping", "r");
            if (input && !fgets(buffer, sizeof(buffer), input)) result = 2;
            if (input) pclose(input);
            if (tinyfd_verbose) printf("afplay %d\n", result);
        }
    }
    return graphicMode() ? result : 0;
}
int xdialogPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("Xdialog");
    return result && graphicMode();
}
int gdialogPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("gdialog");
    return result && graphicMode();
}
int osascriptPresent() {
    static thread_local int result = -1;
    if (result < 0) {
        gWarningDisplayed |= !!getenv("SSH_TTY");
        result = detectPresence("osascript");
    }
    return result && graphicMode() && !getenv("SSH_TTY");
}
int qarmaPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("qarma");
    return result && graphicMode();
}
int matedialogPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("matedialog");
    return result && graphicMode();
}
int shellementaryPresent() {
    static thread_local int result = -1;
    // The independent source deliberately disables this unfinished backend.
    if (result < 0) result = 0;
    return result && graphicMode();
}
int zenityPresent() {
    static thread_local int result = -1;
    if (result < 0) result = detectPresence("zenity");
    return result && graphicMode();
}
int zenity3Present() {
    static thread_local int result = -1;
    char buffer[MAX_PATH_OR_CMD]{};
    if (result < 0) {
        result = 0;
        if (zenityPresent()) {
            FILE* input = popen("zenity --version", "r");
            if (input && fgets(buffer, sizeof(buffer), input)) {
                const auto separator = strchr(buffer, '.');
                const int minor = separator ? atoi(separator + 1) : 0;
                if (atoi(buffer) >= 3) {
                    result = 3;
                    if (minor >= 18) result = 5;
                    else if (minor >= 10) result = 4;
                } else if (atoi(buffer) == 2 && minor >= 32) result = 2;
                if (tinyfd_verbose) printf("zenity %d\n", result);
            }
            if (input) pclose(input);
        }
    }
    return graphicMode() ? result : 0;
}
int kdialogPresent() {
    static thread_local int result = -1;
    char buffer[MAX_PATH_OR_CMD]{};
    if (result < 0) {
        if (zenityPresent()) {
            const char* desktop = getenv("XDG_SESSION_DESKTOP");
            if (!eq(desktop, "KDE") && !eq(desktop, "lxqt")) { result = 0; return result; }
        }
        result = detectPresence("kdialog");
        if (result && !getenv("SSH_TTY")) {
            FILE* input = popen("kdialog --attach 2>&1", "r");
            if (input && fgets(buffer, sizeof(buffer), input) && !strstr("Unknown", buffer)) {
                result = 2;
                if (tinyfd_verbose) printf("kdialog-attach %d\n", result);
            }
            if (input) pclose(input);
            if (result == 2) {
                result = 1;
                input = popen("kdialog --passivepopup 2>&1", "r");
                if (input && fgets(buffer, sizeof(buffer), input) && !strstr("Unknown", buffer)) {
                    result = 2;
                    if (tinyfd_verbose) printf("kdialog-popup %d\n", result);
                }
                if (input) pclose(input);
            }
        }
    }
    return graphicMode() ? result : 0;
}
int osx9orBetter() {
    static thread_local int result = -1;
    char buffer[MAX_PATH_OR_CMD]{};
    int major = 0, minor = 0;
    if (result < 0) {
        result = 0;
        FILE* input = popen("osascript -e 'set osver to system version of (system info)'", "r");
        if (input && fgets(buffer, sizeof(buffer), input) && sscanf(buffer, "%d.%d", &major, &minor) == 2) {
            major = major * 100 + minor;
            if (major >= 1009) result = 1;
        }
        if (input) pclose(input);
        if (tinyfd_verbose) printf("Osx10 = %d, %d = %s\n", result, major, buffer);
    }
    return result;
}
int python2Present() {
    static thread_local int result = -1;
    if (result < 0) {
        result = 0;
        strcpy(gPython2Name, "python2");
        if (detectPresence(gPython2Name)) result = 1;
        else for (int i = 9; i >= 0; --i) {
            sprintf(gPython2Name, "python2.%d", i);
            if (detectPresence(gPython2Name)) { result = 1; break; }
        }
        if (tinyfd_verbose) printf("python2Present %d\n", result);
        if (tinyfd_verbose) printf("gPython2Name %s\n", gPython2Name);
    }
    return result;
}
int python3Present() {
    static thread_local int result = -1;
    if (result < 0) {
        result = 0;
        strcpy(gPython3Name, "python3");
        if (detectPresence(gPython3Name)) result = 1;
        else for (int i = 9; i >= 0; --i) {
            sprintf(gPython3Name, "python3.%d", i);
            if (detectPresence(gPython3Name)) { result = 1; break; }
        }
        if (tinyfd_verbose) printf("python3Present %d\n", result);
        if (tinyfd_verbose) printf("gPython3Name %s\n", gPython3Name);
    }
    return result;
}
int tkinter2Present() {
    static thread_local int result = -1;
    char command[256]{};
    const char parameters[] = "-S -c \"try:\n\timport Tkinter;\nexcept:\n\tprint 0;\"";
    if (result < 0) {
        result = 0;
        if (python2Present()) {
            sprintf(command, "%s %s", gPython2Name, parameters);
            result = tryCommand(command);
        }
        if (tinyfd_verbose) printf("tkinter2Present %d\n", result);
    }
    return result && graphicMode() && !(isDarwin() && getenv("SSH_TTY"));
}
int tkinter3Present() {
    static thread_local int result = -1;
    char command[256]{};
    const char parameters[] = "-S -c \"try:\n\timport tkinter;\nexcept:\n\tprint(0);\"";
    if (result < 0) {
        result = 0;
        if (python3Present()) {
            sprintf(command, "%s %s", gPython3Name, parameters);
            result = tryCommand(command);
        }
        if (tinyfd_verbose) printf("tkinter3Present %d\n", result);
    }
    return result && graphicMode() && !(isDarwin() && getenv("SSH_TTY"));
}
int pythonDbusPresent() {
    static thread_local int result = -1;
    char command[256]{};
    const char parameters[] = R"tinyfd(-c "try:
    import dbus
    bus=dbus.SessionBus()
    notif=bus.get_object('org.freedesktop.Notifications','/org/freedesktop/Notifications')
    notify=dbus.Interface(notif,'org.freedesktop.Notifications')
except:
    print(0)")tinyfd";
    if (result < 0) {
        result = 0;
        if (python2Present()) {
            strcpy(gPythonName, gPython2Name);
            sprintf(command, "%s %s", gPythonName, parameters);
            result = tryCommand(command);
        }
        if (!result && python3Present()) {
            strcpy(gPythonName, gPython3Name);
            sprintf(command, "%s %s", gPythonName, parameters);
            result = tryCommand(command);
        }
        if (tinyfd_verbose) printf("dbusPresent %d\n", result);
        if (tinyfd_verbose) printf("gPythonName %s\n", gPythonName);
    }
    return result && graphicMode() && !(isDarwin() && getenv("SSH_TTY"));
}
void sigHandler(int) {
    FILE* input = popen("pactl unload-module module-sine", "r");
    if (input) pclose(input);
}
void beep() {
    char command[256]{};
    if (osascriptPresent()) {
        if (afplayPresent() >= 2) strcpy(command, "afplay /System/Library/Sounds/Ping.aiff");
        else strcpy(command, "osascript -e 'tell application \"System Events\" to beep'");
    } else if (pactlPresent()) {
        signal(SIGINT, &sigHandler);
        strcpy(command, "thnum=$(pactl load-module module-sine frequency=440);sleep .3;pactl unload-module $thnum");
    } else if (speakertestPresent()) {
        strcpy(command, "( speaker-test -t sine -f 440 > /dev/tty )& pid=$!;sleep .3; kill -9 $pid");
    } else if (beepexePresent()) strcpy(command, "beep.exe 440 300");
    else strcpy(command, "printf '\a' > /dev/tty");
    if (tinyfd_verbose) printf("str: %s\n", command);
    FILE* input = popen(command, "r");
    if (input) pclose(input);
    if (pactlPresent()) signal(SIGINT, SIG_DFL);
}
struct CommandResult {
    bool started = false;
    int status = -1;
    std::string output;
};
class ReadDescriptor {
    int value_;
public:
    explicit ReadDescriptor(int value) : value_(value) {}
    ~ReadDescriptor() { if (value_ >= 0) close(value_); }
    ReadDescriptor(const ReadDescriptor&) = delete;
    ReadDescriptor& operator=(const ReadDescriptor&) = delete;
    int get() const { return value_; }
    void reset() {
        if (value_ >= 0) close(value_);
        value_ = -1;
    }
};
CommandResult executeArguments(const std::vector<std::string>& arguments,
        int captureDescriptor = STDOUT_FILENO, bool terminalOutput = false, bool silentErrors = false,
        bool mergeErrors = false) {
    CommandResult result;
    if (arguments.empty()) return result;
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    int descriptors[2];
#if defined(__linux__)
    if (pipe2(descriptors, O_CLOEXEC) != 0) return result;
#else
    if (pipe(descriptors) != 0) return result;
    if (fcntl(descriptors[0], F_SETFD, FD_CLOEXEC) < 0 ||
            fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return result;
    }
#endif
    ReadDescriptor input(descriptors[0]);
    if (descriptors[1] <= STDERR_FILENO) {
        const auto original = descriptors[1];
        descriptors[1] = fcntl(original, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        close(original);
        if (descriptors[1] < 0) return result;
    }
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        close(descriptors[1]);
        return result;
    }
    int error = posix_spawn_file_actions_addclose(&actions, descriptors[0]);
    if (!error && descriptors[1] != captureDescriptor)
        error = posix_spawn_file_actions_adddup2(&actions, descriptors[1], captureDescriptor);
    const int otherDescriptor = captureDescriptor == STDOUT_FILENO ? STDERR_FILENO : STDOUT_FILENO;
    if (!error && mergeErrors && descriptors[1] != otherDescriptor)
        error = posix_spawn_file_actions_adddup2(&actions, descriptors[1], otherDescriptor);
    if (!error && descriptors[1] != captureDescriptor && (!mergeErrors || descriptors[1] != otherDescriptor))
        error = posix_spawn_file_actions_addclose(&actions, descriptors[1]);
    if (!error && terminalOutput && captureDescriptor != STDOUT_FILENO)
        error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/tty", O_WRONLY, 0);
    if (!error && silentErrors && captureDescriptor != STDERR_FILENO)
        error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t process = -1;
    if (!error) error = posix_spawnp(&process, argv[0], &actions, nullptr, argv.data(), processEnvironment());
    posix_spawn_file_actions_destroy(&actions);
    close(descriptors[1]);
    if (error) return result;
    result.started = true;
    try {
        char buffer[1024];
        for (;;) {
            const auto count = read(input.get(), buffer, sizeof(buffer));
            if (count > 0) result.output.append(buffer, static_cast<std::size_t>(count));
            else if (count < 0 && errno == EINTR) continue;
            else break;
        }
    } catch (...) {
        input.reset();
        int status;
        while (waitpid(process, &status, 0) < 0 && errno == EINTR) {}
        throw;
    }
    input.reset();
    int status = 0;
    pid_t waited;
    do { waited = waitpid(process, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited >= 0) {
        if (WIFEXITED(status)) result.status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) result.status = 128 + WTERMSIG(status);
    }
    return result;
}
class TemporaryResult {
    char path_[64] = "/tmp/tinyfdXXXXXX";
    int descriptor_ = -1;
public:
    TemporaryResult() {
#if defined(__linux__)
        descriptor_ = mkostemp(path_, O_CLOEXEC);
#else
        descriptor_ = mkstemp(path_);
        if (descriptor_ >= 0 && fcntl(descriptor_, F_SETFD, FD_CLOEXEC) < 0) {
            close(descriptor_);
            unlink(path_);
            descriptor_ = -1;
        }
#endif
    }
    ~TemporaryResult() {
        if (descriptor_ >= 0) { close(descriptor_); unlink(path_); }
    }
    TemporaryResult(const TemporaryResult&) = delete;
    TemporaryResult& operator=(const TemporaryResult&) = delete;
    bool valid() const { return descriptor_ >= 0; }
    const char* path() const { return path_; }
    std::string contents() const {
        std::string result;
        if (lseek(descriptor_, 0, SEEK_SET) < 0) return result;
        char buffer[1024];
        for (;;) {
            const auto count = read(descriptor_, buffer, sizeof(buffer));
            if (count > 0) result.append(buffer, static_cast<std::size_t>(count));
            else if (count < 0 && errno == EINTR) continue;
            else return result;
        }
    }
};
std::vector<std::string> terminalArguments() {
    const char* command = terminalName();
    if (!command) return {};
    const std::string_view value(command);
    if (value.starts_with("/opt/X11/bin/xterm "))
        return {"/opt/X11/bin/xterm", "-fa", "DejaVu Sans Mono", "-fs", "10", "-title", "tinyfiledialogs", "-e"};
    if (value.starts_with("xterm "))
        return {"xterm", "-fa", "DejaVu Sans Mono", "-fs", "10", "-title", "tinyfiledialogs", "-e"};
    if (value.starts_with("gnome-terminal ")) return {"gnome-terminal", "--disable-factory", "-x"};
    for (const char* name : {"terminator", "lxterminal", "konsole", "kterm", "tilix", "xfce4-terminal",
            "mate-terminal", "Eterm", "evilvte", "pterm"}) {
        const std::string prefix = std::string(name) + " ";
        if (value.starts_with(prefix)) {
            const bool useX = eq(name, "terminator") || eq(name, "xfce4-terminal") || eq(name, "mate-terminal");
            return {name, useX ? "-x" : "-e"};
        }
    }
    return {};
}
CommandResult executeTerminalArguments(const std::vector<std::string>& command) {
    CommandResult result;
    auto arguments = terminalArguments();
    if (arguments.empty()) return result;
    TemporaryResult output;
    TemporaryResult status;
    if (!output.valid() || !status.valid()) return result;
    // The shell program is constant. Backend names and every input value remain
    // separate positional arguments; no input is interpreted as shell source.
    const char script[] =
        "output=$1; status=$2; shift 2; \"$@\" >/dev/tty 2>\"$output\"; "
        "result=$?; printf '%s' \"$result\" >\"$status\"";
    arguments.emplace_back(strstr(terminalName(), "bash -c ") ? "bash" : "sh");
    arguments.emplace_back("-c");
    arguments.emplace_back(script);
    arguments.emplace_back("tinyfiledialogs");
    arguments.emplace_back(output.path());
    arguments.emplace_back(status.path());
    arguments.insert(arguments.end(), command.begin(), command.end());
    result = executeArguments(arguments);
    const auto exitStatus = status.contents();
    result.status = exitStatus.empty() ? -1 : atoi(exitStatus.c_str());
    result.output = output.contents();
    return result;
}
std::string lastOutputLine(std::string value) {
    if (!value.empty() && value.back() == '\n') value.pop_back();
    const auto newline = value.find_last_of('\n');
    if (newline != std::string::npos) value.erase(0, newline + 1);
    return value;
}
std::string activeWindow(bool delay) {
    if (delay) usleep(10000);
    auto result = executeArguments({"xprop", "-root", "32x", "\t$0", "_NET_ACTIVE_WINDOW"});
    auto text = lastOutputLine(std::move(result.output));
    const auto tab = text.find('\t');
    if (tab != std::string::npos) {
        text.erase(0, tab + 1);
        const auto end = text.find('\t');
        if (end != std::string::npos) text.resize(end);
    }
    return text;
}
int appleScriptMessageBox(const char* title, const char* message, const char* type,
        const char* icon, int defaultButton) {
    const bool modern = osx9orBetter();
    std::string script = "on run argv\n";
    if (!modern) script += "tell application \"System Events\"\nActivate\n";
    script += "try\nset {vButton} to {button returned} of (display dialog (item 2 of argv) ";
    if (some(title)) script += "with title (item 1 of argv) ";
    script += "with icon ";
    script += eq(icon, "error") ? "stop " : eq(icon, "warning") ? "caution " : "note ";
    if (eq(type, "okcancel")) {
        if (!defaultButton) script += "default button \"Cancel\" ";
    } else if (eq(type, "yesno")) {
        script += "buttons {\"No\",\"Yes\"} default button ";
        script += defaultButton ? "\"Yes\" " : "\"No\" ";
        script += "cancel button \"No\" ";
    } else if (eq(type, "yesnocancel")) {
        script += "buttons {\"No\",\"Yes\",\"Cancel\"} ";
        if (defaultButton == 1) script += "default button \"Yes\" ";
        else if (defaultButton == 2) script += "default button \"No\" ";
        else if (!defaultButton) script += "default button \"Cancel\" ";
        script += "cancel button \"Cancel\" ";
    } else script += "buttons {\"OK\"} default button \"OK\" ";
    script += ")\nif vButton is \"Yes\" then\nreturn 1\nelse if vButton is \"OK\" then\nreturn 1\n"
        "else if vButton is \"No\" then\nreturn 2\nelse\nreturn 0\nend if\n"
        "on error number -128\n0\nend try\n";
    if (!modern) script += "end tell\n";
    script += "end run";
    const auto result = executeArguments({"osascript", "-e", script, "--", title ? title : "", message ? message : ""});
    const auto value = lastOutputLine(result.output);
    return value == "2" ? 2 : value == "1" ? 1 : 0;
}
int tkinterMessageBox(bool python3, const char* title, const char* message,
        const char* type, const char* icon, int defaultButton) {
    std::string script = python3 ?
        "import sys,tkinter;from tkinter import messagebox;root=tkinter.Tk();root.withdraw();" :
        "import sys,Tkinter,tkMessageBox;root=Tkinter.Tk();root.withdraw();";
    if (!python3 && isDarwin())
        script += "import subprocess;subprocess.call(['/usr/bin/osascript','-e',"
            "'tell app \"Finder\" to set frontmost of process \"Python\" to true']);";
    const std::string box = python3 ? "messagebox" : "tkMessageBox";
    script += "res=" + box + ".";
    std::string defaultValue;
    if (eq(type, "okcancel")) {
        script += "askokcancel(";
        defaultValue = defaultButton ? "OK" : "CANCEL";
    } else if (eq(type, "yesno")) {
        script += "askyesno(";
        defaultValue = defaultButton ? "YES" : "NO";
    } else if (eq(type, "yesnocancel")) {
        script += "askyesnocancel(";
        if (defaultButton == 1) defaultValue = "YES";
        else if (defaultButton == 2) defaultValue = "NO";
        else if (!defaultButton) defaultValue = "CANCEL";
    } else script += "showinfo(";
    if (!defaultValue.empty()) script += "default=" + box + "." + defaultValue + ",";
    const char* selectedIcon = eq(icon, "question") || eq(icon, "error") || eq(icon, "warning") ? icon : "info";
    script += "icon=sys.argv[3]";
    if (some(title)) script += ",title=sys.argv[1]";
    if (some(message)) script += ",message=sys.argv[2]";
    script += ");\n";
    if (eq(type, "yesnocancel")) {
        script += python3 ?
            "if res is None:\n    print(0)\nelif res is False:\n    print(2)\nelse:\n    print(1)\n" :
            "if res is None:\n    print 0\nelif res is False:\n    print 2\nelse:\n    print 1\n";
    } else {
        script += python3 ? "if res is False:\n    print(0)\nelse:\n    print(1)\n" :
            "if res is False:\n    print 0\nelse:\n    print 1\n";
    }
    std::vector<std::string> arguments{python3 ? gPython3Name : gPython2Name};
    if (!python3 && !isTerminalRunning() && isDarwin()) arguments.emplace_back("-i");
    arguments.insert(arguments.end(), {"-S", "-c", script, title ? title : "", message ? message : "", selectedIcon});
    const auto result = executeArguments(arguments);
    const auto value = lastOutputLine(result.output);
    return value == "2" ? 2 : value == "1" ? 1 : 0;
}
int basicMessageBox(const char* title, const char* message, const char* type) {
    if (!gWarningDisplayed && !tinyfd_forceConsole) {
        gWarningDisplayed = true;
        printf("\n\n%s\n", gTitle);
        printf("%s\n\n", tinyfd_needs);
    }
    if (some(title)) printf("\n%s\n", title);
    termios original{};
    termios settings{};
    tcgetattr(0, &original);
    tcgetattr(0, &settings);
    settings.c_lflag &= ~ICANON;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &settings);
    int result;
    if (eq(type, "yesno") || eq(type, "okcancel") || eq(type, "yesnocancel")) {
        const bool yesNo = eq(type, "yesno");
        const bool okCancel = eq(type, "okcancel");
        char value;
        do {
            if (some(message)) printf("\n%s\n", message);
            printf("%s", yesNo ? "y/n: " : okCancel ? "[O]kay/[C]ancel: " : "[Y]es/[N]o/[C]ancel: ");
            fflush(stdout);
            value = static_cast<char>(tolower(getchar()));
            printf("\n\n");
        } while (yesNo ? value != 'y' && value != 'n' : okCancel ? value != 'o' && value != 'c' :
            value != 'y' && value != 'n' && value != 'c');
        result = yesNo ? (value == 'y' ? 1 : 0) : okCancel ? (value == 'o' ? 1 : 0) :
            value == 'y' ? 1 : value == 'n' ? 2 : 0;
    } else {
        if (some(message)) printf("\n%s\n\n", message);
        printf("press enter to continue ");
        fflush(stdout);
        getchar();
        printf("\n\n");
        result = 1;
    }
    tcsetattr(0, TCSANOW, &original);
    return result;
}
int terminalMessageBox(const char* title, const char* message, const char* type) {
    auto arguments = terminalArguments();
    if (arguments.empty()) return 0;
    TemporaryResult output;
    if (!output.valid()) return 0;
    const bool warning = !gWarningDisplayed && !tinyfd_forceConsole;
    if (warning) gWarningDisplayed = true;
    const char script[] = R"tinyfd_script(exec >/dev/tty
if [ "$5" = 1 ]; then printf '%s\n%s\n\n\n' "$6" "$7"; fi
if [ -n "$2" ]; then printf '%s\n\n' "$2"; fi
if [ -n "$3" ]; then printf '%s\n' "$3"; fi
case "$4" in
yesno)
    printf 'y/n: '; stty sane -echo
    answer=$(while ! head -c 1 | grep -i '[ny]'; do true; done)
    if printf '%s\n' "$answer" | grep -iq '^y'; then printf 1 >"$1"; else printf 0 >"$1"; fi
    ;;
okcancel)
    printf '[O]kay/[C]ancel: '; stty sane -echo
    answer=$(while ! head -c 1 | grep -i '[oc]'; do true; done)
    if printf '%s\n' "$answer" | grep -iq '^o'; then printf 1 >"$1"; else printf 0 >"$1"; fi
    ;;
yesnocancel)
    printf '[Y]es/[N]o/[C]ancel: '; stty sane -echo
    answer=$(while ! head -c 1 | grep -i '[nyc]'; do true; done)
    if printf '%s\n' "$answer" | grep -iq '^y'; then printf 1 >"$1"
    elif printf '%s\n' "$answer" | grep -iq '^n'; then printf 2 >"$1"
    else printf 0 >"$1"; fi
    ;;
*)
    printf 'press enter to continue '; stty sane -echo
    answer=$(while ! head -c 1; do true; done)
    printf 1 >"$1"
    ;;
esac)tinyfd_script";
    arguments.emplace_back(strstr(terminalName(), "bash -c ") ? "bash" : "sh");
    arguments.insert(arguments.end(), {"-c", script, "tinyfiledialogs", output.path(), title ? title : "",
        message ? message : "", type ? type : "", warning ? "1" : "0", gTitle, tinyfd_needs});
    executeArguments(arguments);
    const auto value = lastOutputLine(output.contents());
    return value == "2" ? 2 : value == "1" ? 1 : 0;
}
enum class NotificationBackend { Perl, Python, NotifySend };
std::vector<std::string> notificationArguments(NotificationBackend backend,
        const char* title, const char* message, const char* icon) {
    const std::string titleValue = title ? title : "";
    const std::string messageValue = message ? message : "";
    const std::string iconValue = icon ? icon : "";
    if (backend == NotificationBackend::Perl) {
        const std::string script =
            "use Net::DBus;my $bus=Net::DBus->session;"
            "my $service=$bus->get_service('org.freedesktop.Notifications');"
            "my $object=$service->get_object('/org/freedesktop/Notifications','org.freedesktop.Notifications');"
            "$object->Notify(undef,0,$ARGV[0],$ARGV[1],$ARGV[2],[],{},-1);";
        return {"perl", "-e", script, "--", iconValue, titleValue, messageValue};
    }
    if (backend == NotificationBackend::Python) {
        const std::string script =
            "import dbus,sys;bus=dbus.SessionBus();"
            "notif=bus.get_object('org.freedesktop.Notifications','/org/freedesktop/Notifications');"
            "notify=dbus.Interface(notif,'org.freedesktop.Notifications');"
            "notify.Notify('',0,sys.argv[1],sys.argv[2],sys.argv[3],'','','',5000)";
        return {gPythonName, "-c", script, iconValue, titleValue, messageValue};
    }
    std::vector<std::string> arguments{"notify-send"};
    if (some(icon)) { arguments.emplace_back("-i"); arguments.push_back(iconValue); }
    // The source's final replacement overwrites its previous newline passes.
    std::string text = some(title) ? titleValue + " | " : "";
    for (const auto value : messageValue) {
        if (value == '\t') text += "  ";
        else text += value;
    }
    arguments.push_back(std::move(text));
    return arguments;
}
int _messageBox(const char* title, const char* message, const char* type, const char* icon, int defaultButton) {
    const bool query = eq(title, "tinyfd_query");
    const bool okCancel = eq(type, "okcancel");
    const bool yesNo = eq(type, "yesno");
    const bool threeButtons = eq(type, "yesnocancel");
    if (osascriptPresent()) {
        if (query) { response("applescript"); return 1; }
        return appleScriptMessageBox(title, message, type, icon, defaultButton);
    }
    if (kdialogPresent()) {
        if (query) { response("kdialog"); return 1; }
        std::vector<std::string> arguments{"kdialog"};
        if (kdialogPresent() == 2) arguments.push_back("--attach=" + activeWindow(false));
        std::string operation = "--";
        if (okCancel || yesNo || threeButtons) {
            if (eq(icon, "warning") || eq(icon, "error")) operation += "warning";
            operation += threeButtons ? "yesnocancel" : "yesno";
        } else operation += eq(icon, "error") ? "error" : eq(icon, "warning") ? "sorry" : "msgbox";
        arguments.push_back(operation);
        arguments.emplace_back(message ? message : "");
        if (okCancel) arguments.insert(arguments.end(), {"--yes-label", "Ok", "--no-label", "Cancel"});
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        const auto result = executeArguments(arguments);
        if (threeButtons) return result.status == 0 ? 1 : result.status == 1 ? 2 : 0;
        return result.status == 0 ? 1 : 0;
    }
    if (zenityPresent() || matedialogPresent() || shellementaryPresent() || qarmaPresent()) {
        const char* backend = zenityPresent() ? "zenity" : matedialogPresent() ? "matedialog" :
            shellementaryPresent() ? "shellementary" : "qarma";
        if (query) { response(backend); return 1; }
        std::vector<std::string> arguments{backend};
        if (eq(backend, "zenity") && zenity3Present() >= 4 && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(true));
        else if (eq(backend, "qarma") && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(false));
        if (okCancel) arguments.insert(arguments.end(), {"--question", "--ok-label=Ok", "--cancel-label=Cancel"});
        else if (yesNo) arguments.emplace_back("--question");
        else if (threeButtons) arguments.insert(arguments.end(), {"--list", "--column", "", "--hide-header", "Yes", "No"});
        else arguments.emplace_back(eq(icon, "error") ? "--error" : eq(icon, "warning") ? "--warning" : "--info");
        if (some(title)) arguments.push_back(std::string("--title=") + title);
        if (some(message)) { arguments.emplace_back("--no-wrap"); arguments.push_back(std::string("--text=") + message); }
        if (zenity3Present() >= 3 || (!zenityPresent() && (shellementaryPresent() || qarmaPresent()))) {
            const char* selected = eq(icon, "question") || eq(icon, "error") || eq(icon, "warning") ? icon : "information";
            arguments.push_back(std::string("--icon-name=dialog-") + selected);
        }
        const auto result = executeArguments(arguments, STDOUT_FILENO, false, tinyfd_silent);
        if (threeButtons) return result.status == 1 ? 0 : lastOutputLine(result.output) == "No" ? 2 : 1;
        return result.status == 0 ? 1 : 0;
    }
    if (!gxmessagePresent() && !gmessagePresent() && !gdialogPresent() && !xdialogPresent() && tkinter2Present()) {
        if (query) { response("python2-tkinter"); return 1; }
        return tkinterMessageBox(false, title, message, type, icon, defaultButton);
    }
    if (!gxmessagePresent() && !gmessagePresent() && !gdialogPresent() && !xdialogPresent() && tkinter3Present()) {
        if (query) { response("python3-tkinter"); return 1; }
        return tkinterMessageBox(true, title, message, type, icon, defaultButton);
    }
    if (gxmessagePresent() || gmessagePresent() || (!gdialogPresent() && !xdialogPresent() && xmessagePresent())) {
        const char* backend = gxmessagePresent() ? "gxmessage" : gmessagePresent() ? "gmessage" : "xmessage";
        if (query) { response(backend); return 1; }
        std::vector<std::string> arguments{backend, "-buttons"};
        if (okCancel) {
            arguments.emplace_back("Ok:1,Cancel:0");
            if (defaultButton == 1) arguments.insert(arguments.end(), {"-default", "Ok"});
            else if (!defaultButton) arguments.insert(arguments.end(), {"-default", "Cancel"});
        } else if (yesNo) {
            arguments.emplace_back("Yes:1,No:0");
            if (defaultButton == 1) arguments.insert(arguments.end(), {"-default", "Yes"});
            else if (!defaultButton) arguments.insert(arguments.end(), {"-default", "No"});
        } else if (threeButtons) {
            arguments.emplace_back("Yes:1,No:2,Cancel:0");
            if (defaultButton == 1) arguments.insert(arguments.end(), {"-default", "Yes"});
            else if (defaultButton == 2) arguments.insert(arguments.end(), {"-default", "No"});
            else if (!defaultButton) arguments.insert(arguments.end(), {"-default", "Cancel"});
        } else arguments.insert(arguments.end(), {"Ok:1", "-default", "Ok"});
        arguments.insert(arguments.end(), {"-center", message ? message : ""});
        if (some(title)) arguments.insert(arguments.end(), {"-title", title});
        const auto result = executeArguments(arguments);
        return result.status == 2 ? 2 : result.status == 1 ? 1 : 0;
    }
    if (xdialogPresent() || gdialogPresent() || dialogName() || whiptailPresent()) {
        const bool graphic = gdialogPresent() || xdialogPresent();
        const char* backend = gdialogPresent() ? "gdialog" : xdialogPresent() ? "Xdialog" :
            dialogName() ? dialogName() : "whiptail";
        if (query) {
            response(eq(backend, "Xdialog") ? "xdialog" : graphic ? backend : dialogName() ? "dialog" : "whiptail");
            return graphic ? 1 : 0;
        }
        std::vector<std::string> arguments{backend};
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        if (!xdialogPresent() && !gdialogPresent() && (okCancel || yesNo || threeButtons))
            arguments.insert(arguments.end(), {"--backtitle", "tab: move focus"});
        if (okCancel) {
            if (!defaultButton) arguments.emplace_back("--defaultno");
            arguments.insert(arguments.end(), {"--yes-label", "Ok", "--no-label", "Cancel", "--yesno"});
        } else if (yesNo) {
            if (!defaultButton) arguments.emplace_back("--defaultno");
            arguments.emplace_back("--yesno");
        } else if (threeButtons) {
            if (!defaultButton) arguments.emplace_back("--defaultno");
            arguments.emplace_back("--menu");
        } else arguments.emplace_back("--msgbox");
        arguments.emplace_back(message ? message : "");
        if (threeButtons) arguments.insert(arguments.end(), {"0", "60", "0", "Yes", "", "No", ""});
        else arguments.insert(arguments.end(), {"10", "60"});
        const bool newTerminal = !graphic && !isTerminalRunning();
        const auto result = newTerminal ? executeTerminalArguments(arguments) :
            executeArguments(arguments, STDERR_FILENO, !graphic);
        if (!graphic && !newTerminal) executeArguments({"clear"}, STDERR_FILENO, true);
        if (threeButtons) {
            if (result.status != 0) return 0;
            const auto value = lastOutputLine(result.output);
            return value == "Yes" ? 1 : value == "No" ? 2 : 0;
        }
        return result.status == 0 ? 1 : 0;
    }
    if (!isTerminalRunning() && terminalName()) {
        if (query) { response("basicinput"); return 0; }
        return terminalMessageBox(title, message, type);
    }
    std::vector<std::string> arguments;
    if (!isTerminalRunning() && pythonDbusPresent() && eq(type, "ok")) {
        if (query) { response("python-dbus"); return 1; }
        arguments = notificationArguments(NotificationBackend::Python, title, message, icon);
    } else if (!isTerminalRunning() && perlPresent() >= 2 && eq(type, "ok")) {
        if (query) { response("perl-dbus"); return 1; }
        arguments = notificationArguments(NotificationBackend::Perl, title, message, icon);
    } else if (!isTerminalRunning() && notifysendPresent() && eq(type, "ok")) {
        if (query) { response("notifysend"); return 1; }
        arguments = notificationArguments(NotificationBackend::NotifySend, title, message, icon);
    } else {
        if (query) { response("basicinput"); return 0; }
        return basicMessageBox(title, message, type);
    }
    const auto result = executeArguments(arguments);
    const auto value = lastOutputLine(result.output);
    return value == "2" ? 2 : value == "1" ? 1 : 0;
}
int notifyPopup(const char* title, const char* message, const char* icon) {
    if (getenv("SSH_TTY")) return _messageBox(title, message, "ok", icon, 0);
    const bool query = eq(title, "tinyfd_query");
    const std::string titleValue = title ? title : "";
    const std::string messageValue = message ? message : "";
    const std::string iconValue = icon ? icon : "";
    std::vector<std::string> arguments;
    if (osascriptPresent()) {
        if (query) { response("applescript"); return 1; }
        const bool modern = osx9orBetter();
        std::string script = "on run argv\n";
        if (!modern) script += "tell application \"System Events\"\nActivate\n";
        script += "try\ndisplay notification ((item 2 of argv) & \" \")";
        if (some(title)) script += " with title (item 1 of argv)";
        script += "\nend try\n";
        if (!modern) script += "end tell\n";
        script += "end run";
        arguments = {"osascript", "-e", script, "--", titleValue, messageValue};
    } else if (kdialogPresent()) {
        if (query) { response("kdialog"); return 1; }
        arguments = {"kdialog"};
        if (some(icon)) { arguments.emplace_back("--icon"); arguments.push_back(iconValue); }
        if (some(title)) { arguments.emplace_back("--title"); arguments.push_back(titleValue); }
        arguments.emplace_back("--passivepopup");
        arguments.push_back(messageValue + " ");
        arguments.emplace_back("5");
    } else if (zenity3Present() >= 5 || matedialogPresent() || shellementaryPresent() || qarmaPresent()) {
        const char* backend = zenity3Present() >= 5 ? "zenity" :
            matedialogPresent() ? "matedialog" : shellementaryPresent() ? "shellementary" : "qarma";
        if (query) { response(backend); return 1; }
        arguments = {backend, "--notification"};
        if (some(icon)) { arguments.emplace_back("--window-icon"); arguments.push_back(iconValue); }
        arguments.emplace_back("--text");
        arguments.push_back((some(title) ? titleValue + "\n" : "") + messageValue + " ");
    } else if (perlPresent() >= 2) {
        if (query) { response("perl-dbus"); return 1; }
        const std::string script =
            "use Net::DBus;"
            "my $bus=Net::DBus->session;"
            "my $service=$bus->get_service('org.freedesktop.Notifications');"
            "my $object=$service->get_object('/org/freedesktop/Notifications','org.freedesktop.Notifications');"
            "$object->Notify(undef,0,$ARGV[0],$ARGV[1],$ARGV[2],[],{},-1);";
        arguments = {"perl", "-e", script, "--", iconValue, titleValue, messageValue};
    } else if (pythonDbusPresent()) {
        if (query) { response("python-dbus"); return 1; }
        const std::string script =
            "import dbus,sys;bus=dbus.SessionBus();"
            "notif=bus.get_object('org.freedesktop.Notifications','/org/freedesktop/Notifications');"
            "notify=dbus.Interface(notif,'org.freedesktop.Notifications');"
            "notify.Notify('',0,sys.argv[1],sys.argv[2],sys.argv[3],'','','',5000)";
        arguments = {gPythonName, "-c", script, iconValue, titleValue, messageValue};
    } else if (notifysendPresent()) {
        if (query) { response("notifysend"); return 1; }
        arguments = {"notify-send"};
        if (some(icon)) { arguments.emplace_back("-i"); arguments.push_back(iconValue); }
        // The source's final replacement overwrites its previous newline passes.
        std::string text = some(title) ? titleValue + " | " : "";
        for (const auto value : messageValue) {
            if (value == '\t') text += "  ";
            else text += value;
        }
        arguments.push_back(std::move(text));
    } else return _messageBox(title, message, "ok", icon, 0);
    return executeArguments(arguments).started ? 1 : 0;
}
std::string finalOutputChunk(std::string_view output, std::size_t capacity) {
    std::string result;
    for (std::size_t position = 0; position < output.size();) {
        auto count = std::min(capacity - 1, output.size() - position);
        const auto newline = output.find('\n', position);
        if (newline != std::string_view::npos && newline - position < count) count = newline - position + 1;
        result.assign(output.substr(position, count));
        position += count;
    }
    const auto nul = result.find('\0');
    if (nul != std::string::npos) result.resize(nul);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}
std::string shellEchoValue(std::string value) {
    // Preserve the original unquoted echo's IFS whitespace joining, without
    // evaluating input as code or expanding it into filesystem pathnames.
    while (!value.empty() && value.back() == '\n') value.pop_back();
    std::string result;
    bool separator = false;
    for (const auto byte : value) {
        if (byte == ' ' || byte == '\t' || byte == '\n') separator = !result.empty();
        else {
            if (separator) result += ' ';
            separator = false;
            result += byte;
        }
    }
    return result;
}
CommandResult tkinterInputBox(bool python3, const char* title, const char* message, const char* initial) {
    std::string script = python3 ?
        "import sys,tkinter;from tkinter import simpledialog;root=tkinter.Tk();root.withdraw();" :
        "import sys,Tkinter,tkSimpleDialog;root=Tkinter.Tk();root.withdraw();";
    if (!python3 && isDarwin())
        script += "import subprocess;subprocess.call(['/usr/bin/osascript','-e',"
            "'tell app \"Finder\" to set frontmost of process \"Python\" to true']);";
    script += python3 ? "res=simpledialog.askstring(" : "res=tkSimpleDialog.askstring(";
    std::string separator;
    const auto argument = [&](const char* key, const char* expression) {
        script += separator + key + "=" + expression;
        separator = ",";
    };
    if (some(title)) argument("title", "sys.argv[1]");
    if (some(message)) argument("prompt", "sys.argv[2]");
    if (initial) { if (some(initial)) argument("initialvalue", "sys.argv[3]"); }
    else argument("show", "'*'");
    script += ");\n";
    script += python3 ? "if res is None:\n    print(0)\nelse:\n    print('1'+res)\n" :
        "if res is None:\n    print 0\nelse:\n    print '1'+res\n";
    std::vector<std::string> arguments{python3 ? gPython3Name : gPython2Name};
    if (!python3 && !isTerminalRunning() && isDarwin()) arguments.emplace_back("-i");
    arguments.insert(arguments.end(), {"-S", "-c", script, title ? title : "", message ? message : "", initial ? initial : ""});
    return executeArguments(arguments);
}
CommandResult terminalInputBox(const char* title, const char* message, const char* initial) {
    CommandResult result;
    auto arguments = terminalArguments();
    if (arguments.empty()) return result;
    TemporaryResult output;
    if (!output.valid()) return result;
    if (!gWarningDisplayed && !tinyfd_forceConsole) {
        gWarningDisplayed = true;
        _messageBox(gTitle, tinyfd_needs, "ok", "warning", 0);
    }
    const char script[] = R"tinyfd_script(exec >/dev/tty
if [ "$5" = 1 ] && [ -n "$2" ]; then printf '%s\n\n' "$2"; fi
printf '%s\n' "$3"
if [ "$4" = 1 ]; then read -s -p '(esc+enter to cancel): ' ANSWER
else read -p '(esc+enter to cancel): ' ANSWER; fi
echo 1$ANSWER >"$1")tinyfd_script";
    arguments.emplace_back(strstr(terminalName(), "bash -c ") ? "bash" : "sh");
    arguments.insert(arguments.end(), {"-c", script, "tinyfiledialogs", output.path(), title ? title : "",
        message ? message : "", initial ? "0" : "1", tinyfd_forceConsole ? "0" : "1"});
    result = executeArguments(arguments);
    result.output = executeArguments({"cat", "-v", output.path()}).output;
    return result;
}
const char* inputBox(const char* title, const char* message, const char* initial) {
    static thread_local char buffer[MAX_PATH_OR_CMD]{};
    buffer[0] = '\0';
    const bool query = eq(title, "tinyfd_query");
    const auto graphicalQuery = [](const char* backend) {
        response(backend);
        return reinterpret_cast<const char*>(1);
    };
    CommandResult result;
    bool prefixed = false;
    bool basicTerminal = false;
    if (osascriptPresent()) {
        if (query) return graphicalQuery("applescript");
        const bool modern = osx9orBetter();
        std::string script = "on run argv\n";
        if (!modern) script += "tell application \"System Events\"\nActivate\n";
        script += "try\ndisplay dialog (item 2 of argv) default answer (item 3 of argv) ";
        if (!initial) script += "hidden answer true ";
        if (some(title)) script += "with title (item 1 of argv) ";
        script += "with icon note\n\"1\" & text returned of result\non error number -128\n0\nend try\n";
        if (!modern) script += "end tell\n";
        script += "end run";
        result = executeArguments({"osascript", "-e", script, "--", title ? title : "", message ? message : "", initial ? initial : ""});
        prefixed = true;
    } else if (kdialogPresent()) {
        if (query) return graphicalQuery("kdialog");
        std::vector<std::string> arguments{"kdialog"};
        if (kdialogPresent() == 2) arguments.push_back("--attach=" + activeWindow(false));
        arguments.emplace_back(initial ? "--inputbox" : "--password");
        arguments.emplace_back(message ? message : "");
        arguments.emplace_back(initial ? initial : "");
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        result = executeArguments(arguments);
    } else if (zenityPresent() || matedialogPresent() || shellementaryPresent() || qarmaPresent()) {
        const char* backend = zenityPresent() ? "zenity" : matedialogPresent() ? "matedialog" :
            shellementaryPresent() ? "shellementary" : "qarma";
        if (query) return graphicalQuery(backend);
        std::vector<std::string> arguments{backend};
        if (eq(backend, "zenity") && zenity3Present() >= 4 && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(true));
        else if (eq(backend, "qarma") && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(false));
        arguments.emplace_back("--entry");
        if (some(title)) arguments.push_back(std::string("--title=") + title);
        if (some(message)) arguments.push_back(std::string("--text=") + message);
        if (some(initial)) arguments.push_back(std::string("--entry-text=") + initial);
        else arguments.emplace_back("--hide-text");
        result = executeArguments(arguments, STDOUT_FILENO, false, tinyfd_silent);
    } else if (gxmessagePresent() || gmessagePresent()) {
        const char* backend = gxmessagePresent() ? "gxmessage" : "gmessage";
        if (query) return graphicalQuery(backend);
        std::vector<std::string> arguments{backend, "-buttons", "Ok:1,Cancel:0", "-center", message ? message : ""};
        if (some(title)) arguments.insert(arguments.end(), {"-title", title});
        arguments.insert(arguments.end(), {"-entrytext", initial ? initial : ""});
        result = executeArguments(arguments);
        result.status = result.status == 1 ? 0 : 1;
    } else if (!gdialogPresent() && !xdialogPresent() && tkinter2Present()) {
        if (query) return graphicalQuery("python2-tkinter");
        result = tkinterInputBox(false, title, message, initial);
        prefixed = true;
    } else if (!gdialogPresent() && !xdialogPresent() && tkinter3Present()) {
        if (query) return graphicalQuery("python3-tkinter");
        result = tkinterInputBox(true, title, message, initial);
        prefixed = true;
    } else if (gdialogPresent() || xdialogPresent() || dialogName() || whiptailPresent()) {
        const bool gdialog = gdialogPresent();
        const bool graphic = gdialog || xdialogPresent();
        const char* backend = gdialog ? "gdialog" : xdialogPresent() ? "Xdialog" : dialogName() ? dialogName() : "whiptail";
        if (query) {
            response(eq(backend, "Xdialog") ? "xdialog" : graphic ? backend : dialogName() ? "dialog" : "whiptail");
            return graphic ? reinterpret_cast<const char*>(1) : nullptr;
        }
        std::vector<std::string> arguments{backend};
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        if (!xdialogPresent() && !gdialogPresent()) {
            std::string text = "tab: move focus";
            if (!initial && !gdialog) text += " (sometimes nothing, no blink nor star, is shown in text field)";
            arguments.insert(arguments.end(), {"--backtitle", text});
        }
        if (initial || gdialog) arguments.emplace_back("--inputbox");
        else {
            if (!graphic && dialogName() && isDialogVersionBetter09b()) arguments.emplace_back("--insecure");
            arguments.emplace_back("--passwordbox");
        }
        arguments.insert(arguments.end(), {message ? message : "", "10", "60"});
        if (some(initial)) arguments.emplace_back(initial);
        const bool newTerminal = !graphic && !isTerminalRunning();
        result = newTerminal ? executeTerminalArguments(arguments) : executeArguments(arguments, STDERR_FILENO, !graphic);
        if (!graphic && !newTerminal) executeArguments({"clear"}, STDERR_FILENO, true);
    } else if (!isTerminalRunning() && terminalName()) {
        if (query) { response("basicinput"); return nullptr; }
        result = terminalInputBox(title, message, initial);
        prefixed = true;
        basicTerminal = true;
    } else if (!gWarningDisplayed && !isTerminalRunning() && !terminalName()) {
        gWarningDisplayed = true;
        _messageBox(gTitle, tinyfd_needs, "ok", "warning", 0);
        if (query) response("no_solution");
        return nullptr;
    } else {
        if (query) { response("basicinput"); return nullptr; }
        if (!gWarningDisplayed && !tinyfd_forceConsole) {
            gWarningDisplayed = true;
            _messageBox(gTitle, tinyfd_needs, "ok", "warning", 0);
        }
        if (some(title)) printf("\n%s\n", title);
        if (some(message)) printf("\n%s\n", message);
        printf("(esc+enter to cancel): ");
        fflush(stdout);
        termios original{};
        if (!initial) {
            tcgetattr(STDIN_FILENO, &original);
            auto settings = original;
            settings.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &settings);
        }
        char* readValue = fgets(buffer, MAX_PATH_OR_CMD, stdin);
        if (!readValue || !buffer[0]) return nullptr;
        if (buffer[0] == '\n') {
            readValue = fgets(buffer, MAX_PATH_OR_CMD, stdin);
            if (!readValue || !buffer[0]) return nullptr;
        }
        if (!initial) { tcsetattr(STDIN_FILENO, TCSANOW, &original); printf("\n"); }
        printf("\n");
        if (strchr(buffer, 27)) return nullptr;
        removeLastNL(buffer);
        return buffer;
    }
    if (!result.started) return nullptr;
    if (!prefixed)
        result.output = shellEchoValue(std::string(result.status == 0 ? "1" : "0") + result.output) + "\n";
    const auto value = finalOutputChunk(result.output, sizeof(buffer));
    if (basicTerminal && (value.find("^[") != std::string::npos || value.find('\x1b') != std::string::npos)) return nullptr;
    if (value.empty() || value[0] != '1') return nullptr;
    memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    return buffer + 1;
}
std::string pathDirectory(const char* source) {
    if (!source) return {};
    const std::string_view value(source);
    auto position = value.find_last_of('/');
    if (position == std::string_view::npos) position = value.find_last_of('\\');
    return position == std::string_view::npos ? std::string{} : std::string(value.substr(0, position));
}
std::string pathName(const char* source) {
    if (!source) return {};
    const std::string_view value(source);
    auto position = value.find_last_of('/');
    if (position == std::string_view::npos) position = value.find_last_of('\\');
    return std::string(position == std::string_view::npos ? value : value.substr(position + 1));
}
std::string defaultKdialogPath(const char* source) {
    if (some(source) && source[0] == '/') return source;
    std::string directory;
    if (const char* pwd = getenv("PWD")) directory = pwd;
    else if (char* pwd = getcwd(nullptr, 0)) { directory = pwd; free(pwd); }
    return directory + "/" + (source ? source : "");
}
std::string kdialogFilters(std::span<const TFD_Filter> filters) {
    std::string result;
    bool added = false;
    for (const auto& filter : filters) {
        bool mime;
        std::array<const char*, MAX_PATTERNS> patterns;
        const auto count = getValidPatterns(filter, mime, patterns);
        if (!count) continue;
        std::string joined;
        for (unsigned i = 0; i < count; ++i) {
            if (i) joined += ' ';
            joined += patterns[i];
        }
        if (added) result += " | ";
        added = true;
        if (!mime) result += (some(filter.description) ? std::string(filter.description) : joined) + "(";
        result += joined;
        if (!mime) result += ')';
    }
    return result;
}
void appendZenityFilters(std::vector<std::string>& arguments, std::span<const TFD_Filter> filters) {
    if (filters.empty()) return;
    for (const auto& filter : filters) {
        bool mime;
        std::array<const char*, MAX_PATTERNS> patterns;
        const auto count = getValidPatterns(filter, mime, patterns);
        if (!count || mime) continue;
        std::string value = "--file-filter=";
        if (some(filter.description)) value += std::string(filter.description) + " | ";
        for (unsigned i = 0; i < count; ++i) value += std::string(patterns[i]) + " ";
        arguments.push_back(std::move(value));
    }
    arguments.emplace_back("--file-filter=All files | *");
}
void appendTkinterFilterData(std::vector<std::string>& arguments, std::span<const TFD_Filter> filters) {
    if (filters.empty()) return;
    unsigned char placeholder = 'a';
    for (const auto& filter : filters) {
        bool mime;
        std::array<const char*, MAX_PATTERNS> patterns;
        const auto count = getValidPatterns(filter, mime, patterns);
        if (!count || mime || (count == 1 && lastch(patterns[0]) == '*')) continue;
        arguments.push_back(some(filter.description) ? std::string(filter.description) :
            std::string(1, static_cast<char>(placeholder++)));
        arguments.push_back(std::to_string(count));
        for (unsigned i = 0; i < count; ++i) arguments.emplace_back(patterns[i]);
    }
    arguments.insert(arguments.end(), {"All files", "1", "*"});
}
std::vector<std::string> appleScriptFilterData(std::span<const TFD_Filter> filters) {
    for (const auto& filter : filters) {
        bool mime;
        std::array<const char*, MAX_PATTERNS> patterns;
        const auto count = getValidPatterns(filter, mime, patterns);
        if (!count || mime) continue;
        std::vector<std::string> result(1);
        for (unsigned i = 0; i < count; ++i) {
            const char* pattern = patterns[i];
            if (strstr(pattern, "*.")) pattern += 2;
            if (!*pattern) continue;
            if (i) result.emplace_back(pattern);
            else result[0] = pattern;
        }
        return result;
    }
    return {};
}
enum class FileDialogKind { Save, Open, Folder };
CommandResult tkinterFileDialog(bool python3, FileDialogKind kind, const char* title,
        const char* initial, std::span<const TFD_Filter> filters, bool multiple) {
    const auto directory = kind == FileDialogKind::Folder ? std::string(initial ? initial : "") : pathDirectory(initial);
    const auto filename = kind == FileDialogKind::Folder ? std::string{} : pathName(initial);
    std::string script = python3 ?
        "import sys,tkinter;from tkinter import filedialog;root=tkinter.Tk();root.withdraw();\n" :
        "import sys,Tkinter,tkFileDialog;root=Tkinter.Tk();root.withdraw();\n";
    if (!python3 && isDarwin())
        script += "import subprocess;subprocess.call(['/usr/bin/osascript','-e',"
            "'tell app \"Finder\" to set frontmost of process \"Python\" to true'])\n";
    script += "kwargs={}\nif sys.argv[1]: kwargs['title']=sys.argv[1]\n"
        "if sys.argv[2]: kwargs['initialdir']=sys.argv[2]\n";
    if (kind != FileDialogKind::Folder) script += "if sys.argv[3]: kwargs['initialfile']=sys.argv[3]\n";
    if (kind == FileDialogKind::Open && multiple) script += "kwargs['multiple']=1\n";
    script += "filetypes=[]\ni=5\nwhile i<len(sys.argv):\n"
        "    count=int(sys.argv[i+1])\n    patterns=sys.argv[i+2:i+2+count]\n"
        "    if count==1: patterns=patterns[0]\n"
        "    filetypes.append((sys.argv[i],patterns))\n    i+=2+count\n"
        "if filetypes: kwargs['filetypes']=filetypes\n";
    script += python3 ? "value=filedialog." : "value=tkFileDialog.";
    script += kind == FileDialogKind::Save ? "asksaveasfilename(**kwargs)\n" :
        kind == FileDialogKind::Open ? "askopenfilename(**kwargs)\n" : "askdirectory(**kwargs)\n";
    if (kind == FileDialogKind::Open) {
        script += python3 ?
            "if not isinstance(value,tuple):\n    print(value)\nelse:\n"
            "    text=''\n    for name in value: text+=str(name)+'|'\n    print(text[:-1])\n" :
            "if not isinstance(value,tuple):\n    print value\nelse:\n"
            "    text=''\n    for name in value: text+=str(name)+'|'\n    print text[:-1]\n";
    } else script += python3 ? "print(value)\n" : "print value\n";
    std::vector<std::string> arguments{python3 ? gPython3Name : gPython2Name};
    if (!python3 && !isTerminalRunning() && isDarwin()) arguments.emplace_back("-i");
    arguments.insert(arguments.end(), {"-S", "-c", script, title ? title : "", directory, filename, multiple ? "1" : "0"});
    appendTkinterFilterData(arguments, filters);
    return executeArguments(arguments);
}
struct FileDialogResult {
    CommandResult command;
    bool graphical = false;
    bool fallback = false;
    bool kdialog = false;
};
FileDialogResult runFileDialog(FileDialogKind kind, const char* title, const char* initial,
        std::span<const TFD_Filter> filters, bool multiple, bool query) {
    FileDialogResult result;
    if (osascriptPresent()) {
        result.graphical = true;
        if (query) { response("applescript"); return result; }
        const bool modern = osx9orBetter();
        const auto directory = kind == FileDialogKind::Folder ? std::string(initial ? initial : "") : pathDirectory(initial);
        const auto filename = kind == FileDialogKind::Save ? pathName(initial) : std::string{};
        const auto types = kind == FileDialogKind::Open ? appleScriptFilterData(filters) : std::vector<std::string>{};
        std::string script = "on run argv\n";
        if (!modern) script += kind == FileDialogKind::Save ?
            "tell application \"Finder\"\nActivate\n" : "tell application \"System Events\"\nActivate\n";
        script += "try\n";
        const bool many = kind == FileDialogKind::Open && multiple;
        script += many ? "set mylist to choose file " : "POSIX path of (choose ";
        if (!many) script += kind == FileDialogKind::Save ? "file name " :
            kind == FileDialogKind::Open ? "file " : "folder ";
        if (some(title)) script += "with prompt (item 1 of argv) ";
        if (!directory.empty()) script += "default location (item 2 of argv) ";
        if (!filename.empty()) script += "default name (item 3 of argv) ";
        if (!types.empty()) script += "of type (items 5 thru -1 of argv) ";
        if (many) script += "multiple selections allowed true\n"
            "set mystring to POSIX path of item 1 of mylist\n"
            "repeat with i from 2 to the count of mylist\n"
            "set mystring to mystring & \"|\"\n"
            "set mystring to mystring & POSIX path of item i of mylist\nend repeat\nmystring\n";
        else script += ")\n";
        script += "on error number -128\nend try\n";
        if (!modern) script += "end tell\n";
        script += "end run";
        std::vector<std::string> arguments{"osascript", "-e", script, "--", title ? title : "", directory, filename, multiple ? "1" : "0"};
        arguments.insert(arguments.end(), types.begin(), types.end());
        result.command = executeArguments(arguments);
    } else if (kdialogPresent()) {
        result.graphical = result.kdialog = true;
        if (query) { response("kdialog"); return result; }
        std::vector<std::string> arguments{"kdialog"};
        if (kdialogPresent() == 2) arguments.push_back("--attach=" + activeWindow(false));
        arguments.emplace_back(kind == FileDialogKind::Save ? "--getsavefilename" :
            kind == FileDialogKind::Open ? "--getopenfilename" : "--getexistingdirectory");
        arguments.push_back(defaultKdialogPath(initial));
        if (!filters.empty()) arguments.push_back(kdialogFilters(filters));
        if (multiple && kind == FileDialogKind::Open) arguments.insert(arguments.end(), {"--multiple", "--separate-output"});
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        result.command = executeArguments(arguments);
    } else if (zenityPresent() || matedialogPresent() || shellementaryPresent() || qarmaPresent()) {
        result.graphical = true;
        const char* backend = zenityPresent() ? "zenity" : matedialogPresent() ? "matedialog" :
            shellementaryPresent() ? "shellementary" : "qarma";
        if (query) { response(backend); return result; }
        std::vector<std::string> arguments{backend};
        if (eq(backend, "zenity") && zenity3Present() >= 4 && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(true));
        else if (eq(backend, "qarma") && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(false));
        arguments.emplace_back("--file-selection");
        if (kind == FileDialogKind::Save) arguments.insert(arguments.end(), {"--save", "--confirm-overwrite"});
        else if (kind == FileDialogKind::Folder) arguments.emplace_back("--directory");
        else if (multiple) arguments.emplace_back("--multiple");
        if (some(title)) arguments.push_back(std::string("--title=") + title);
        if (some(initial)) arguments.push_back(std::string("--filename=") + initial);
        appendZenityFilters(arguments, filters);
        result.command = executeArguments(arguments, STDOUT_FILENO, false, tinyfd_silent);
    } else if ((kind == FileDialogKind::Open || !xdialogPresent()) && tkinter2Present()) {
        result.graphical = true;
        if (query) { response("python2-tkinter"); return result; }
        result.command = tkinterFileDialog(false, kind, title, initial, filters, multiple);
    } else if ((kind == FileDialogKind::Open || !xdialogPresent()) && tkinter3Present()) {
        result.graphical = true;
        if (query) { response("python3-tkinter"); return result; }
        result.command = tkinterFileDialog(true, kind, title, initial, filters, multiple);
    } else if (xdialogPresent() || dialogName()) {
        result.graphical = xdialogPresent();
        const char* backend = result.graphical ? "Xdialog" : dialogName();
        if (query) { response(result.graphical ? "xdialog" : "dialog"); return result; }
        std::vector<std::string> arguments{backend};
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        if (!xdialogPresent() && !gdialogPresent()) arguments.insert(arguments.end(), {"--backtitle",
            "tab: focus | /: populate | spacebar: fill text field | ok: TEXT FIELD ONLY"});
        arguments.emplace_back(kind == FileDialogKind::Folder ? "--dselect" : "--fselect");
        std::string selected;
        if (some(initial)) {
            if (kind != FileDialogKind::Folder && !strchr(initial, '/')) selected = "./";
            selected += initial;
            if (kind == FileDialogKind::Folder && selected.back() != '/') selected += '/';
        } else if (!isTerminalRunning() && !result.graphical) {
            const char* home = getenv("HOME");
            selected = std::string(home ? home : "") + "/";
        } else selected = "./";
        arguments.insert(arguments.end(), {selected, "0", "60"});
        const bool newTerminal = !result.graphical && !isTerminalRunning();
        result.command = newTerminal ? executeTerminalArguments(arguments) : result.graphical ?
            executeArguments(arguments, STDOUT_FILENO, false, false, true) :
            executeArguments(arguments, STDERR_FILENO, true);
        if (!result.graphical && !newTerminal) executeArguments({"clear"}, STDERR_FILENO, true);
    } else result.fallback = true;
    return result;
}
const char* saveFileDialog(const char* title, const char* initial, std::span<const TFD_Filter> filters) {
    static thread_local char buffer[MAX_PATH_OR_CMD]{};
    buffer[0] = '\0';
    const bool query = eq(title, "tinyfd_query");
    const auto choice = runFileDialog(FileDialogKind::Save, title, initial, filters, false, query);
    if (choice.fallback) {
        if (query) return inputBox(title, nullptr, nullptr);
        const char* value = inputBox(title, "Save file", "");
        if (!dirExists(pathDirectory(value).c_str()) || pathName(value).empty()) return nullptr;
        return value;
    }
    if (query) return choice.graphical ? reinterpret_cast<const char*>(1) : nullptr;
    if (!choice.command.started) return nullptr;
    const auto value = finalOutputChunk(choice.command.output, sizeof(buffer));
    if (value.empty() || !dirExists(pathDirectory(value.c_str()).c_str()) || !filenameValid(pathName(value.c_str()).c_str()))
        return nullptr;
    memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    return buffer;
}
const char* openFileDialog(const char* title, const char* initial,
        std::span<const TFD_Filter> filters, bool multiple) {
    static thread_local char buffer[MAX_MULTIPLE_FILES * MAX_PATH_OR_CMD]{};
    buffer[0] = '\0';
    const bool query = eq(title, "tinyfd_query");
    const auto choice = runFileDialog(FileDialogKind::Open, title, initial, filters, multiple, query);
    if (choice.fallback) {
        if (query) return inputBox(title, nullptr, nullptr);
        const char* value = inputBox(title, "Open file", "");
        return fileExists(value) ? value : nullptr;
    }
    if (query) return choice.graphical ? reinterpret_cast<const char*>(1) : nullptr;
    if (!choice.command.started) return nullptr;
    std::string value = choice.command.output;
    const auto nul = value.find('\0');
    if (nul != std::string::npos) value.resize(nul);
    if (!value.empty() && value.back() == '\n') value.pop_back();
    if (choice.kdialog && multiple) std::replace(value.begin(), value.end(), '\n', '|');
    if (value.empty()) return nullptr;
    if (multiple && value.find('|') != std::string::npos) {
        std::string existing;
        for (std::size_t begin = 0;;) {
            const auto end = value.find('|', begin);
            const auto path = value.substr(begin, end == std::string::npos ? end : end - begin);
            if (fileExists(path.c_str())) {
                if (!existing.empty()) existing += '|';
                existing += path;
            }
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        value = std::move(existing);
    } else if (!fileExists(value.c_str())) return nullptr;
    if (value.empty() || value.size() >= sizeof(buffer)) return nullptr;
    memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    return buffer;
}
const char* selectFolderDialog(const char* title, const char* initial) {
    static thread_local char buffer[MAX_PATH_OR_CMD]{};
    buffer[0] = '\0';
    const bool query = eq(title, "tinyfd_query");
    const auto choice = runFileDialog(FileDialogKind::Folder, title, initial, {}, false, query);
    if (choice.fallback) {
        if (query) return inputBox(title, nullptr, nullptr);
        const char* value = inputBox(title, "Select folder", "");
        return dirExists(value) ? value : nullptr;
    }
    if (query) return choice.graphical ? reinterpret_cast<const char*>(1) : nullptr;
    if (!choice.command.started) return nullptr;
    const auto value = finalOutputChunk(choice.command.output, sizeof(buffer));
    if (!dirExists(value.c_str())) return nullptr;
    memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    return buffer;
}
const char* colorChooser(const char* title, const char* initialHex,
        const std::array<unsigned char, 3>& initialRGB, std::array<unsigned char, 3>& rgb) {
    static thread_local char buffer[128]{};
    buffer[0] = '\0';
    const bool query = eq(title, "tinyfd_query");
    std::array<unsigned char, 3> defaults{};
    char defaultHex[8]{};
    if (initialHex) Hex2RGB(initialHex, defaults);
    else { defaults = initialRGB; RGB2Hex(initialRGB, defaultHex); initialHex = defaultHex; }
    const auto graphicalQuery = [](const char* backend) {
        response(backend);
        return reinterpret_cast<const char*>(1);
    };
    CommandResult result;
    bool zenity = false, apple = false, xdialog = false;
    if (osascriptPresent()) {
        if (query) return graphicalQuery("applescript");
        apple = true;
        const bool modern = osx9orBetter();
        std::string script = "on run argv\n";
        if (!modern) script += "tell application \"System Events\"\nActivate\ntry\n";
        else script += "try\ntell app (path to frontmost application as Unicode text) to ";
        script += "set mycolor to choose color default color "
            "{(item 1 of argv as integer),(item 2 of argv as integer),(item 3 of argv as integer)}\n"
            "set mystring to ((item 1 of mycolor) div 256 as integer) as string\n"
            "repeat with i from 2 to the count of mycolor\n"
            "set mystring to mystring & \" \" & ((item i of mycolor) div 256 as integer) as string\n"
            "end repeat\nmystring\non error number -128\nend try\n";
        if (!modern) script += "end tell\n";
        script += "end run";
        result = executeArguments({"osascript", "-e", script, "--", std::to_string(256 * defaults[0]),
            std::to_string(256 * defaults[1]), std::to_string(256 * defaults[2])});
    } else if (kdialogPresent()) {
        if (query) return graphicalQuery("kdialog");
        std::vector<std::string> arguments{"kdialog"};
        if (kdialogPresent() == 2) arguments.push_back("--attach=" + activeWindow(false));
        arguments.insert(arguments.end(), {"--getcolor", "--default", initialHex});
        if (some(title)) arguments.insert(arguments.end(), {"--title", title});
        result = executeArguments(arguments);
    } else if (zenity3Present() || matedialogPresent() || shellementaryPresent() || qarmaPresent()) {
        zenity = true;
        const char* backend = zenity3Present() ? "zenity" : matedialogPresent() ? "matedialog" :
            shellementaryPresent() ? "shellementary" : "qarma";
        if (query) return graphicalQuery(eq(backend, "zenity") ? "zenity3" : backend);
        std::vector<std::string> arguments{backend};
        if (eq(backend, "zenity") && zenity3Present() >= 4 && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(true));
        else if (eq(backend, "qarma") && !getenv("SSH_TTY"))
            arguments.push_back("--attach=" + activeWindow(false));
        arguments.insert(arguments.end(), {"--color-selection", "--show-palette"});
        arguments.push_back(std::string("--color=") + initialHex);
        if (some(title)) arguments.push_back(std::string("--title=") + title);
        result = executeArguments(arguments, STDOUT_FILENO, false, tinyfd_silent);
    } else if (xdialogPresent()) {
        if (query) return graphicalQuery("xdialog");
        xdialog = true;
        result = executeArguments({"Xdialog", "--colorsel", title ? title : "", "0", "60",
            std::to_string(defaults[0]), std::to_string(defaults[1]), std::to_string(defaults[2])},
            STDOUT_FILENO, false, false, true);
    } else if (tkinter2Present() || tkinter3Present()) {
        const bool python3 = !tkinter2Present();
        if (query) return graphicalQuery(python3 ? "python3-tkinter" : "python2-tkinter");
        std::string script = python3 ?
            "import sys,tkinter;from tkinter import colorchooser;root=tkinter.Tk();root.withdraw();" :
            "import sys,Tkinter,tkColorChooser;root=Tkinter.Tk();root.withdraw();";
        if (!python3 && isDarwin())
            script += "import subprocess;subprocess.call(['osascript','-e',"
                "'tell app \"Finder\" to set frontmost of process \"Python\" to true']);";
        script += python3 ? "res=colorchooser.askcolor(color=sys.argv[1]" : "res=tkColorChooser.askcolor(color=sys.argv[1]";
        if (some(title)) script += ",title=sys.argv[2]";
        script += ");\n";
        script += python3 ? "if res[1] is not None:\n    print(res[1])\n" :
            "if res[1] is not None:\n    print res[1]\n";
        std::vector<std::string> arguments{python3 ? gPython3Name : gPython2Name};
        if (!python3 && !isTerminalRunning() && isDarwin()) arguments.emplace_back("-i");
        arguments.insert(arguments.end(), {"-S", "-c", script, initialHex, title ? title : ""});
        result = executeArguments(arguments);
    } else {
        if (query) return inputBox(title, nullptr, nullptr);
        const char* value = inputBox(title, "Enter hex rgb color (i.e. #f5ca20)", initialHex);
        if (!value || strlen(value) != 7 || value[0] != '#') return nullptr;
        for (unsigned i = 1; i < 7; ++i)
            if (!isxdigit(static_cast<unsigned char>(value[i]))) return nullptr;
        Hex2RGB(value, rgb);
        return value;
    }
    if (!result.started) return nullptr;
    const auto value = finalOutputChunk(result.output, sizeof(buffer));
    if (value.empty()) return nullptr;
    memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    if (zenity) {
        if (buffer[0] == '#') {
            if (strlen(buffer) > 7) {
                buffer[3] = buffer[5]; buffer[4] = buffer[6];
                buffer[5] = buffer[9]; buffer[6] = buffer[10]; buffer[7] = '\0';
            }
            Hex2RGB(buffer, rgb);
        } else if (buffer[3] == '(') {
            sscanf(buffer, "rgb(%hhu,%hhu,%hhu", &rgb[0], &rgb[1], &rgb[2]);
            RGB2Hex(rgb, buffer);
        } else if (buffer[4] == '(') {
            sscanf(buffer, "rgba(%hhu,%hhu,%hhu", &rgb[0], &rgb[1], &rgb[2]);
            RGB2Hex(rgb, buffer);
        }
    } else if (apple || xdialog) {
        sscanf(buffer, "%hhu %hhu %hhu", &rgb[0], &rgb[1], &rgb[2]);
        RGB2Hex(rgb, buffer);
    } else Hex2RGB(buffer, rgb);
    return buffer;
}
} // namespace
void tinyfd_beep() { beep(); }
int tinyfd_notifyPopup(const char* title, const char* message, const char* iconType) {
    return notifyPopup(title, message, iconType);
}
int tinyfd_messageBox(const char* title, const char* message, const char* dialogType,
        const char* iconType, int defaultButton) {
    return _messageBox(title, message, dialogType, iconType, defaultButton);
}
const char* tinyfd_inputBox(const char* title, const char* message, const char* defaultInput) {
    return inputBox(title, message, defaultInput);
}
const char* tinyfd_saveFileDialog(const char* title, const char* defaultPathAndFile,
        std::span<const TFD_Filter> filters) {
    return saveFileDialog(title, defaultPathAndFile, filters);
}
const char* tinyfd_openFileDialog(const char* title, const char* defaultPathAndFile,
        std::span<const TFD_Filter> filters, bool allowMultipleSelects) {
    return openFileDialog(title, defaultPathAndFile, filters, allowMultipleSelects);
}
const char* tinyfd_selectFolderDialog(const char* title, const char* defaultPath) {
    return selectFolderDialog(title, defaultPath);
}
const char* tinyfd_colorChooser(const char* title, const char* defaultHexRGB,
        const std::array<unsigned char, 3>& defaultRGB, std::array<unsigned char, 3>& resultRGB) {
    return colorChooser(title, defaultHexRGB, defaultRGB, resultRGB);
}
} // namespace nijigenerate::platform::tinyfiledialogs
#endif
