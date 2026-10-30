// Windows port of the independently installed tinyfiledialogs 0.10.1 D implementation.
// Original copyright (c) 2014-2019 Guillaume Vareille; (c) 2019 dayllenger.
// Altered C++ version; see tinyfiledialogs.LICENSE.
#include "tinyfiledialogs.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <conio.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include "tinyfiledialogs_windows_process.hpp"

namespace nijigenerate::platform::tinyfiledialogs {
const char tinyfd_version[8] = "3.3.9";
const char tinyfd_needs[] = R"TFD( ___________
/           \
| tiny file |
|  dialogs  |
\_____  ____/
      \|
tiny file dialogs on Windows needs:
   a graphic display
or dialog.exe (enhanced console mode)
or a console for basic input)TFD";
bool tinyfd_verbose = false;
bool tinyfd_silent = true;
bool tinyfd_forceConsole = false;
char tinyfd_response[1024]{};
namespace {
constexpr int MAX_PATH_OR_CMD = 1024;
constexpr int MAX_MULTIPLE_FILES = 32;
constexpr int MAX_PATTERNS = 8;
constexpr const char* gTitle = "missing software! (we will try basic console input)";
bool gWarningDisplayed = false;
int dialogPresent();
bool fileExists(const char*);
wchar_t* utf8to16(const char*);
int _messageBox(const char*, const char*, const char*, const char*, int);
using WideAllocation = std::unique_ptr<wchar_t, decltype(&std::free)>;
class ComInitialization {
    bool active_;
public:
    const HRESULT status;
    explicit ComInitialization(DWORD flags) : active_(false), status(CoInitializeEx(nullptr, flags)) {
        active_ = status == S_OK || status == S_FALSE;
    }
    ComInitialization(const ComInitialization&) = delete;
    ComInitialization& operator=(const ComInitialization&) = delete;
    ~ComInitialization() { close(); }
    void close() {
        if (active_) { active_ = false; CoUninitialize(); }
    }
};
bool some(const char* str)
{
    return str && str[0] != '\0';
}

bool wsome(const wchar_t* str)
{
    return str && str[0] != '\0';
}

bool eq(const char* s1, const char* s2)
{
    if (s1 == s2)
        return true;
    if (s1 == nullptr || s2 == nullptr)
        return false;
    return strcmp(s1, s2) == 0;
}

char lastch(const char* str)
{
    if (str)
    {
        size_t len = strlen(str);
        if (len > 0)
            return str[len - 1];
    }
    return '\0';
}

void removeLastNL(char* str)
{
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
    {
        str[len - 1] = '\0';
    }
}

void response(const char* message)
{
    strcpy(tinyfd_response, message);
}

char* getPathWithoutFinalSlash(char* output, const char* source) {
    const char* separator = source ? strrchr(source, '/') : nullptr;
    if (source && !separator) separator = strrchr(source, '\\');
    const auto length = separator ? static_cast<std::size_t>(separator - source) : 0;
    if (length >= MAX_PATH_OR_CMD) { output[0] = '\0'; return output; }
    if (length) memmove(output, source, length);
    output[length] = '\0';
    return output;
}

char* getLastName(char* output, const char* source) {
    const char* separator = source ? strrchr(source, '/') : nullptr;
    if (source && !separator) separator = strrchr(source, '\\');
    const auto* name = separator ? separator + 1 : source;
    const auto length = name ? strlen(name) : 0;
    if (length >= MAX_PATH_OR_CMD) { output[0] = '\0'; return output; }
    if (length) memmove(output, name, length);
    output[length] = '\0';
    return output;
}

unsigned int getValidPatterns(const TFD_Filter& filter, bool& isMime, std::array<const char*, MAX_PATTERNS>& output)
{
    isMime = false;
    output.fill(nullptr);
    unsigned int len{};
    for (auto pt : filter.patterns)
    {
        if (some(pt))
        {
            output[len] = pt;
            len++;
            if (len == MAX_PATTERNS)
                break;
        }
    }
    if (len == 0)
        return 0;

    unsigned int mime{};
    for (auto pt : std::span(output.data(), len))
    {
        if (strchr(pt, '/'))
            mime++;
        else
            break;
    }
    if (mime > 0)
    {
        // forbid mixing of MIME and usual patterns
        if (mime != len)
        {
            isMime = false;
            std::fill_n(output.begin(), len, nullptr);
            return 0;
        }
        isMime = true;
    }
    return len;
}

void Hex2RGB(const char* aHexRGB, std::array<unsigned char, 3>& aoResultRGB)
{
    char lColorChannel[8]{};
    if (aHexRGB)
    {
        strncpy(lColorChannel, aHexRGB, sizeof(lColorChannel) - 1);
        aoResultRGB[2] = (unsigned char)strtoul(lColorChannel + 5, nullptr, 16);
        lColorChannel[5] = '\0';
        aoResultRGB[1] = (unsigned char)strtoul(lColorChannel + 3, nullptr, 16);
        lColorChannel[3] = '\0';
        aoResultRGB[0] = (unsigned char)strtoul(lColorChannel + 1, nullptr, 16);
        /* printf("%d %d %d\n", aoResultRGB[0], aoResultRGB[1], aoResultRGB[2]); */
    }
    else
    {
        aoResultRGB[0] = 0;
        aoResultRGB[1] = 0;
        aoResultRGB[2] = 0;
    }
}

void RGB2Hex(const std::array<unsigned char, 3> aRGB, char* aoResultHexRGB)
{
    if (aoResultHexRGB)
    {
        // NOTE: here was compiler ifdef
        sprintf(aoResultHexRGB, "#%02hhx%02hhx%02hhx", aRGB[0], aRGB[1], aRGB[2]);
        /* printf("aoResultHexRGB %s\n", aoResultHexRGB); */
    }
}

bool filenameValid(const char* aFileNameWithoutPath)
{
    return some(aFileNameWithoutPath) && !strpbrk(aFileNameWithoutPath, "\\/:*?\"<>|");
}

/* source and destination can be the same or ovelap*/
const char* ensureFilesExist(char* output, const char* paths) {
    if (!some(paths)) return nullptr;
    const std::string input(paths);
    std::string result;
    for (std::size_t begin = 0;;) {
        const auto end = input.find('|', begin);
        const auto path = input.substr(begin, end == std::string::npos ? end : end - begin);
        if (fileExists(path.c_str())) {
            if (!result.empty()) result += '|';
            result += path;
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    memcpy(output, result.data(), result.size());
    output[result.size()] = '\0';
    return output;
}

    bool fileExists(const char* aFilePathAndName)
    {
        if (!some(aFilePathAndName))
            return false;

        wchar_t* lTmpWChar = utf8to16(aFilePathAndName);
        DWORD attribs = GetFileAttributesW(lTmpWChar);
        free(lTmpWChar);
        return !(attribs & FILE_ATTRIBUTE_DEVICE) && !(attribs & FILE_ATTRIBUTE_DIRECTORY);
    }

bool replaceChr(char* aString, const char aOldChr, const char aNewChr)
{
    if (!aString)
        return false;
    if (aOldChr == aNewChr)
        return false;

    bool ret{};
    char* p = aString;
    while ((p = strchr(p, aOldChr)) != nullptr)
    {
        *p = aNewChr;
        p++;
        ret = true;
    }
    return ret;
}

int sizeUtf16(const char* aUtf8string)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               aUtf8string, -1, nullptr, 0);
}

wchar_t* utf8to16(const char* str)
{
    if (!some(str))
        return nullptr;

    int lSize = sizeUtf16(str);
    wchar_t* ret = (wchar_t*)malloc(lSize * sizeof(wchar_t));
    lSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                str, -1, ret, lSize);
    if (lSize == 0)
    {
        free(ret);
        return nullptr;
    }
    return ret;
}

std::wstring wideArgument(const char* text) {
    std::unique_ptr<wchar_t, decltype(&std::free)> value(utf8to16(text), &std::free);
    return value ? std::wstring(value.get()) : std::wstring{};
}

bool dirExists(const char* aDirPath)
{
    if (!some(aDirPath))
        return false;
    size_t lDirLen = strlen(aDirPath);
    if (lDirLen == 2 && aDirPath[1] == ':')
        return true;

    wchar_t* lTmpWChar = utf8to16(aDirPath);
    DWORD attribs = GetFileAttributesW(lTmpWChar);
    free(lTmpWChar);
    return (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

 // TINYFD_NOLIB

void _beep()
{
    Beep(440, 300);
}

wchar_t* getPathWithoutFinalSlashW(wchar_t* output, const wchar_t* source) {
    const auto* separator = source ? wcsrchr(source, L'/') : nullptr;
    if (source && !separator) separator = wcsrchr(source, L'\\');
    const auto length = separator ? static_cast<std::size_t>(separator - source) : 0;
    if (length >= MAX_PATH_OR_CMD) { output[0] = L'\0'; return output; }
    if (length) memmove(output, source, length * sizeof(wchar_t));
    output[length] = L'\0';
    return output;
}

wchar_t* getLastNameW(wchar_t* output, const wchar_t* source) {
    const auto* separator = source ? wcsrchr(source, L'/') : nullptr;
    if (source && !separator) separator = wcsrchr(source, L'\\');
    const auto* name = separator ? separator + 1 : source;
    const auto length = name ? wcslen(name) : 0;
    if (length >= MAX_PATH_OR_CMD) { output[0] = L'\0'; return output; }
    if (length) memmove(output, name, length * sizeof(wchar_t));
    output[length] = L'\0';
    return output;
}

#ifndef WC_ERR_INVALID_CHARS
constexpr DWORD WC_ERR_INVALID_CHARS = 0x00000080;
#endif

int sizeUtf8(const wchar_t* aUtf16string)
{
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                               aUtf16string, -1, nullptr, 0, nullptr, nullptr);
}

char* utf16to8(const wchar_t* str)
{
    if (!wsome(str))
        return nullptr;

    int lSize = sizeUtf8(str);
    char* ret = (char*)malloc(lSize);
    lSize = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                str, -1, ret, lSize, nullptr, nullptr);
    if (lSize == 0)
    {
        free(ret);
        return nullptr;
    }
    return ret;
}

const char* copyUTF8(char* output, std::size_t capacity, const wchar_t* value) {
    if (!value || !capacity) return nullptr;
    if (!*value) { output[0] = '\0'; return output; }
    std::unique_ptr<char, decltype(&std::free)> converted(utf16to8(value), &std::free);
    if (!converted) return nullptr;
    const auto length = strlen(converted.get());
    if (length >= capacity) return nullptr;
    memcpy(output, converted.get(), length + 1);
    return output;
}

bool willBeGui()
{
    return (!tinyfd_forceConsole || !(GetConsoleWindow() || dialogPresent())) &&
           (!getenv("SSH_CLIENT") || getenv("DISPLAY"));
}

int messageBoxWinGui(
    const char* aTitle,
    const char* aMessage,
    const char* aDialogType,
    const char* aIconType,
    const int aDefaultButton)
{
    wchar_t* lTitle = utf8to16(aTitle);
    WideAllocation titleOwner(lTitle, &std::free);
    wchar_t* lMessage = utf8to16(aMessage);
    WideAllocation messageOwner(lMessage, &std::free);

    UINT aCode{};

    if (eq("warning", aIconType))
    {
        aCode = MB_ICONWARNING;
    }
    else if (eq("error", aIconType))
    {
        aCode = MB_ICONERROR;
    }
    else if (eq("question", aIconType))
    {
        aCode = MB_ICONQUESTION;
    }
    else
    {
        aCode = MB_ICONINFORMATION;
    }

    if (eq("okcancel", aDialogType))
    {
        aCode += MB_OKCANCEL;
        if (!aDefaultButton)
        {
            aCode += MB_DEFBUTTON2;
        }
    }
    else if (eq("yesno", aDialogType))
    {
        aCode += MB_YESNO;
        if (!aDefaultButton)
        {
            aCode += MB_DEFBUTTON2;
        }
    }
    else
    {
        aCode += MB_OK;
    }

    aCode += MB_TOPMOST;

    int lBoxReturnValue = MessageBoxW(GetForegroundWindow(), lMessage, lTitle, aCode);
    titleOwner.reset();
    messageOwner.reset();

    if (eq("ok", aDialogType) || lBoxReturnValue == IDOK || lBoxReturnValue == IDYES)
        return 1;
    else
        return 0;
}

int notifyWinGui(const char* title, const char* message, const char* iconType) {
    const std::wstring script = LR"TFD(function Show-BalloonTip {
[cmdletbinding()]
param([string]$Title=' ', [string]$Message=' ',
[ValidateSet('info','warning','error')][string]$IconType='info')
[System.Reflection.Assembly]::LoadWithPartialName('System.Windows.Forms') | Out-Null
$balloon=New-Object System.Windows.Forms.NotifyIcon
$path=Get-Process -id $pid | Select-Object -ExpandProperty Path
$icon=[System.Drawing.Icon]::ExtractAssociatedIcon($path)
$balloon.Icon=$icon
$balloon.BalloonTipIcon=$IconType
$balloon.BalloonTipText=$Message
$balloon.BalloonTipTitle=$Title
$balloon.Text='lalala'
$balloon.Visible=$true
$balloon.ShowBalloonTip(5000)
}
$title=$env:TINYFD_TITLE
$message=$env:TINYFD_MESSAGE
$icon=$env:TINYFD_ICON
if([string]::IsNullOrEmpty($title)) {$title=' '}
if([string]::IsNullOrEmpty($message)) {$message=' '}
if([string]::IsNullOrEmpty($icon)) {$icon='info'}
Show-BalloonTip -Title $title -Message $message -IconType $icon
)TFD";
    const auto result = process_detail::run({L"powershell.exe", L"-EncodedCommand",
        process_detail::encodedCommand(script)}, {{L"TINYFD_TITLE", wideArgument(title)},
        {L"TINYFD_MESSAGE", wideArgument(message)}, {L"TINYFD_ICON", wideArgument(iconType)}}, true);
    return result.started ? 1 : 0;
}

const wchar_t* _inputBoxW(const wchar_t* title, const wchar_t* message, const wchar_t* initial) {
    static thread_local wchar_t buffer[MAX_PATH_OR_CMD]{};
    buffer[0] = L'\0';
    process_detail::TemporaryFile scriptFile(initial ? L".vbs" : L".hta");
    process_detail::TemporaryFile resultFile(L".txt");
    if (!scriptFile.valid() || !resultFile.valid()) return nullptr;
    const std::wstring_view normalScript = LR"TFD(Dim env, result, output
Set env=CreateObject("WScript.Shell").Environment("PROCESS")
result=InputBox(Replace(env("TINYFD_MESSAGE"),vbLf," "),"tinyfiledialogsTopWindow",Replace(env("TINYFD_DEFAULT"),vbLf," "))
Set output=CreateObject("Scripting.FileSystemObject").CreateTextFile(env("TINYFD_RESULT"),True,True)
If IsEmpty(result) Then
output.Write "0"
Else
output.Write "1" & result
End If
output.Close
)TFD";
    const std::wstring_view passwordScript = LR"TFD(<html><head><title>tinyfiledialogsTopWindow</title>
<HTA:APPLICATION ID='tinyfdHTA' APPLICATIONNAME='tinyfd_inputBox' MINIMIZEBUTTON='no'
MAXIMIZEBUTTON='no' BORDER='dialog' SCROLL='no' SINGLEINSTANCE='yes' WINDOWSTATE='hidden'>
<script language='VBScript'>
Dim env, result
Set env=CreateObject("WScript.Shell").Environment("PROCESS")
result=0
intWidth=Screen.Width/4
intHeight=Screen.Height/6
ResizeTo intWidth,intHeight
MoveTo ((Screen.Width/2)-(intWidth/2)),((Screen.Height/2)-(intHeight/2))
Sub Window_onLoad
 document.getElementById("message").innerText=env("TINYFD_MESSAGE")
 txt_input.Focus
End Sub
Sub Window_onUnload
 Dim output
 Set output=CreateObject("Scripting.FileSystemObject").CreateTextFile(env("TINYFD_RESULT"),True,True)
 If result=1 Then
  output.Write "1" & txt_input.Value
 Else
  output.Write "0"
 End If
 output.Close
End Sub
Sub Run_ProgramOK
 result=1
 window.Close
End Sub
Sub Run_ProgramCancel
 window.Close
End Sub
Sub Default_Buttons
 If Window.Event.KeyCode=13 Then
  btn_OK.Click
 ElseIf Window.Event.KeyCode=27 Then
  btn_Cancel.Click
 End If
End Sub
</script></head>
<body style='background-color:#EEEEEE' onkeypress='vbs:Default_Buttons' align='top'>
<table width='100%' height='80%' align='center' border='0'>
<tr border='0'><td align='left' valign='middle' style='Font-Family:Arial'><span id='message'></span></td>
<td align='right' valign='middle' style='margin-top: 0em'>
<table align='right' style='margin-right: 0em;'><tr align='right' style='margin-top: 5em;'>
<input type='button' value='OK' name='btn_OK' onClick='vbs:Run_ProgramOK' style='width: 5em; margin-top: 2em;'><br>
<input type='button' value='Cancel' name='btn_Cancel' onClick='vbs:Run_ProgramCancel' style='width: 5em;'><br><br>
</tr></table></td></tr></table>
<table width='100%' height='100%' align='center' border='0'><tr><td align='left' valign='top'>
<input type='password' id='txt_input' name='txt_input' value='' style='float:left;width:100%'><br>
</td></tr></table>
</body></html>)TFD";
    if (!scriptFile.writeText(initial ? normalScript : passwordScript)) return nullptr;
    scriptFile.close();
    resultFile.close();
    const std::vector<std::wstring> arguments = initial ?
        std::vector<std::wstring>{L"cscript.exe", L"//E:vbscript", L"//Nologo", scriptFile.path()} :
        std::vector<std::wstring>{L"mshta.exe", scriptFile.path()};
    const auto result = process_detail::run(arguments, {{L"TINYFD_MESSAGE", message ? message : L""},
        {L"TINYFD_DEFAULT", initial ? initial : L""}, {L"TINYFD_RESULT", resultFile.path()}},
        true, title ? title : L"");
    if (!result.started) return nullptr;
    const auto bytes = resultFile.readBytes();
    if (bytes.size() < 4 || static_cast<unsigned char>(bytes[0]) != 0xff ||
            static_cast<unsigned char>(bytes[1]) != 0xfe) return nullptr;
    std::wstring value;
    for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
        value.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) |
            (unsigned(static_cast<unsigned char>(bytes[i + 1])) << 8)));
    if (value.empty() || value[0] != L'1') return nullptr;
    const auto count = std::min(value.size() - 1, std::size_t{MAX_PATH_OR_CMD - 1});
    std::copy_n(value.data() + 1, count, buffer);
    buffer[count] = L'\0';
    return buffer;
}

const char* inputBoxWinGui(
    char* aoBuff,
    const char* aTitle,
    const char* aMessage,
    const char* aDefaultInput)
{
    wchar_t* lTitle = utf8to16(aTitle);
    WideAllocation titleOwner(lTitle, &std::free);
    wchar_t* lMessage = utf8to16(aMessage);
    WideAllocation messageOwner(lMessage, &std::free);
    wchar_t* lDefaultInput = utf8to16(aDefaultInput);
    WideAllocation inputOwner(lDefaultInput, &std::free);

    const wchar_t* lTmpWChar = _inputBoxW(lTitle, lMessage, lDefaultInput);

    titleOwner.reset();
    messageOwner.reset();
    inputOwner.reset();

    if (!lTmpWChar)
        return nullptr;

    return copyUTF8(aoBuff, MAX_PATH_OR_CMD, lTmpWChar);
}

wchar_t* buildFilterString(std::span<const TFD_Filter> filters) {
    bool isMime = false;
    std::array<const char*, MAX_PATTERNS> patterns{};
    std::string text;
    for (const auto& filter : filters) {
        const auto count = getValidPatterns(filter, isMime, patterns);
        if (!count || isMime) continue;
        std::string joined;
        for (unsigned i = 0; i < count; ++i) {
            if (i) joined += ';';
            joined += patterns[i];
        }
        text += some(filter.description) ? filter.description : joined;
        text += '\n';
        text += joined;
        text += '\n';
    }
    text += "All Files\n*.*\n";
    auto* result = utf8to16(text.c_str());
    if (!result) return nullptr;
    for (auto* p = result; (p = wcschr(p, L'\n')) != nullptr; ++p) *p = L'\0';
    return result;
}

const char* saveFileDialogWinGui(
    char* aoBuff,
    const char* aTitle,
    const char* aDefaultPathAndFile,
    std::span<const TFD_Filter> aFilters)
{
    static thread_local wchar_t lBuff[MAX_PATH_OR_CMD]{};
    wchar_t lDirname[MAX_PATH_OR_CMD]{};

    wchar_t* lTitle = utf8to16(aTitle);
    WideAllocation titleOwner(lTitle, &std::free);
    wchar_t* lDefaultPathAndFile = utf8to16(aDefaultPathAndFile);
    WideAllocation pathOwner(lDefaultPathAndFile, &std::free);
    wchar_t* lFilterStr = buildFilterString(aFilters);
    WideAllocation filterOwner(lFilterStr, &std::free);

    getPathWithoutFinalSlashW(lDirname, lDefaultPathAndFile);
    getLastNameW(lBuff, lDefaultPathAndFile);

    ComInitialization com(0);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.hInstance = nullptr;
    ofn.lpstrFilter = lFilterStr;
    ofn.lpstrCustomFilter = nullptr;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = lBuff;

    ofn.nMaxFile = MAX_PATH_OR_CMD;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = MAX_PATH_OR_CMD / 2;
    ofn.lpstrInitialDir = wsome(lDirname) ? lDirname : nullptr;
    ofn.lpstrTitle = wsome(lTitle) ? lTitle : nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = nullptr;
    ofn.lCustData = 0;
    ofn.lpfnHook = nullptr;
    ofn.lpTemplateName = nullptr;

    wchar_t* lTmpWChar{};
    if (GetSaveFileNameW(&ofn) == 0)
    {
        lTmpWChar = nullptr;
    }
    else
    {
        lTmpWChar = lBuff;
    }

    com.close();

    titleOwner.reset();
    pathOwner.reset();
    filterOwner.reset();

    if (!lTmpWChar)
        return nullptr;

    return copyUTF8(aoBuff, MAX_PATH_OR_CMD, lTmpWChar);
}

const char* openFileDialogWinGui(
    char* aoBuff,
    const char* aTitle,
    const char* aDefaultPathAndFile,
    std::span<const TFD_Filter> aFilters,
    const bool aAllowMultipleSelects)
{
    static thread_local wchar_t lBuff[MAX_MULTIPLE_FILES * MAX_PATH_OR_CMD]{};
    wchar_t lDirname[MAX_PATH_OR_CMD]{};

    wchar_t* lTitle = utf8to16(aTitle);
    WideAllocation titleOwner(lTitle, &std::free);
    wchar_t* lDefaultPathAndFile = utf8to16(aDefaultPathAndFile);
    WideAllocation pathOwner(lDefaultPathAndFile, &std::free);
    wchar_t* lFilterStr = buildFilterString(aFilters);
    WideAllocation filterOwner(lFilterStr, &std::free);

    getPathWithoutFinalSlashW(lDirname, lDefaultPathAndFile);
    getLastNameW(lBuff, lDefaultPathAndFile);

    ComInitialization com(0);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.hInstance = nullptr;
    ofn.lpstrFilter = lFilterStr;
    ofn.lpstrCustomFilter = nullptr;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = lBuff;
    ofn.nMaxFile = MAX_PATH_OR_CMD;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = MAX_PATH_OR_CMD / 2;
    ofn.lpstrInitialDir = wsome(lDirname) ? lDirname : nullptr;
    ofn.lpstrTitle = wsome(lTitle) ? lTitle : nullptr;
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = nullptr;
    ofn.lCustData = 0;
    ofn.lpfnHook = nullptr;
    ofn.lpTemplateName = nullptr;

    if (aAllowMultipleSelects)
    {
        ofn.Flags |= OFN_ALLOWMULTISELECT;
    }

    std::wstring expanded;
    wchar_t* lTmpWChar = nullptr;
    if (GetOpenFileNameW(&ofn) != 0) {
        auto* limit = lBuff + std::min<std::size_t>(ofn.nMaxFile, std::size(lBuff));
        auto* firstEnd = std::find(lBuff, limit, L'\0');
        if (firstEnd != limit) {
            auto* cursor = firstEnd + 1;
            if (!aAllowMultipleSelects || cursor == limit || !*cursor) {
                lTmpWChar = lBuff;
            } else {
                const std::wstring directory(lBuff, firstEnd);
                std::size_t count = 0;
                bool valid = true;
                while (cursor != limit && *cursor) {
                    auto* end = std::find(cursor, limit, L'\0');
                    if (end == limit || ++count > MAX_MULTIPLE_FILES) { valid = false; break; }
                    if (!expanded.empty()) expanded += L'|';
                    expanded += directory;
                    expanded += L'\\';
                    expanded.append(cursor, static_cast<std::size_t>(end - cursor));
                    if (expanded.size() >= MAX_MULTIPLE_FILES * MAX_PATH_OR_CMD) { valid = false; break; }
                    cursor = end + 1;
                }
                if (valid && cursor != limit && !*cursor) lTmpWChar = expanded.data();
            }
        }
    }

    com.close();

    titleOwner.reset();
    pathOwner.reset();
    filterOwner.reset();

    if (!lTmpWChar)
        return nullptr;

    return copyUTF8(aoBuff, MAX_MULTIPLE_FILES * MAX_PATH_OR_CMD, lTmpWChar);
}



int CALLBACK BrowseCallbackProcW(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    if (uMsg == BFFM_INITIALIZED)
    {
        SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, (LPARAM)pData);
    }
    return 0;
}

const char* selectFolderDialogWinGui(
    char* aoBuff,
    const char* aTitle,
    const char* aDefaultPath)
{
    static thread_local wchar_t lBuff[MAX_PATH_OR_CMD]{};

    wchar_t* lTitle = utf8to16(aTitle);
    WideAllocation titleOwner(lTitle, &std::free);
    wchar_t* lDefaultPath = utf8to16(aDefaultPath);
    WideAllocation pathOwner(lDefaultPath, &std::free);

    ComInitialization com(COINIT_APARTMENTTHREADED);

    BROWSEINFOW bInfo{};
    bInfo.hwndOwner = GetForegroundWindow();
    bInfo.pidlRoot = nullptr;
    bInfo.pszDisplayName = lBuff;
    bInfo.lpszTitle = wsome(lTitle) ? lTitle : nullptr;
    if (com.status == S_OK || com.status == S_FALSE)
    {
        bInfo.ulFlags = BIF_USENEWUI;
    }
    bInfo.lpfn = &BrowseCallbackProcW;
    bInfo.lParam = (LPARAM)lDefaultPath;
    bInfo.iImage = -1;

    LPITEMIDLIST lpItem = SHBrowseForFolderW(&bInfo);
    if (lpItem)
    {
        SHGetPathFromIDListW(lpItem, lBuff);
        CoTaskMemFree(lpItem);
    }

    com.close();

    const auto result = copyUTF8(aoBuff, MAX_PATH_OR_CMD, lBuff);
    titleOwner.reset();
    pathOwner.reset();
    return result;
}

 // TINYFD_SELECTFOLDERWIN

const char* colorChooserWinGui(
    const char* aTitle,
    const char* aDefaultHexRGB,
    const std::array<unsigned char, 3>& aDefaultRGB,
    std::array<unsigned char, 3>& aoResultRGB)
{
    static thread_local char lResultHexRGB[8]{};
    std::array<unsigned char, 3> lDefaultRGB = aDefaultRGB;

    ComInitialization com(0);

    if (aDefaultHexRGB)
    {
        Hex2RGB(aDefaultHexRGB, lDefaultRGB);
    }

    CHOOSECOLORW cc{};
    COLORREF crCustColors[16]{};
    /* we can't use aTitle */
    cc.lStructSize = sizeof(CHOOSECOLORW);
    cc.hwndOwner = GetForegroundWindow();
    cc.hInstance = nullptr;
    cc.rgbResult = RGB(lDefaultRGB[0], lDefaultRGB[1], lDefaultRGB[2]);
    cc.lpCustColors = crCustColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    cc.lCustData = 0;
    cc.lpfnHook = nullptr;
    cc.lpTemplateName = nullptr;

    const char* ret{};
    if (ChooseColorW(&cc))
    {
        aoResultRGB[0] = GetRValue(cc.rgbResult);
        aoResultRGB[1] = GetGValue(cc.rgbResult);
        aoResultRGB[2] = GetBValue(cc.rgbResult);
        RGB2Hex(aoResultRGB, lResultHexRGB);
        ret = lResultHexRGB;
    }

    com.close();
    return ret;
}

 // TINYFD_NOLIB

int dialogPresent()
{
    static thread_local int ret = -1;
    if (ret < 0)
    {
        FILE* lIn = _popen("where dialog.exe", "r");
        if (!lIn)
        {
            ret = 0;
            return 0;
        }

        char lBuff[MAX_PATH_OR_CMD]{};
        const char* lString = "dialog.exe";
        while (fgets(lBuff, sizeof(lBuff), lIn) != nullptr)
        {
        }
        _pclose(lIn);
        removeLastNL(lBuff);
        if (strlen(lBuff) < strlen(lString) ||
                strcmp(lBuff + strlen(lBuff) - strlen(lString), lString))
            ret = 0;
        else
            ret = 1;
    }
    return ret;
}

std::vector<std::wstring> consoleArguments(const char* title) {
    std::vector<std::wstring> result{L"dialog.exe"};
    if (some(title)) result.insert(result.end(), {L"--title", wideArgument(title)});
    return result;
}
std::string lastConsoleLine(const std::string& bytes) {
    std::string text;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\x1a') break;
        if (bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') continue;
        text.push_back(bytes[i]);
    }
    std::string result;
    for (std::size_t begin = 0; begin < text.size();) {
        const auto newline = text.find('\n', begin);
        const auto available = std::min(text.size() - begin, std::size_t{MAX_PATH_OR_CMD - 1});
        const auto count = newline != std::string::npos && newline - begin < available ?
            newline - begin + 1 : available;
        result.assign(text, begin, count);
        begin += count;
    }
    const auto nul = result.find('\0');
    if (nul != std::string::npos) result.resize(nul);
    return result;
}
const char* storeConsoleOutput(char* output, const std::string& text, bool trimNewline = false) {
    auto value = lastConsoleLine(text);
    if (trimNewline && !value.empty() && value.back() == '\n') value.pop_back();
    memcpy(output, value.data(), value.size());
    output[value.size()] = '\0';
    return output;
}
int messageBoxWinConsole(const char* title, const char* message, const char* type,
        const char*, int defaultButton) {
    auto arguments = consoleArguments(title);
    const bool menu = eq(type, "yesnocancel");
    if (eq(type, "okcancel") || eq(type, "yesno") || menu)
        arguments.insert(arguments.end(), {L"--backtitle", L"tab: move focus"});
    if (eq(type, "okcancel") || eq(type, "yesno") || menu) {
        if (!defaultButton) arguments.emplace_back(L"--defaultno");
        if (eq(type, "okcancel"))
            arguments.insert(arguments.end(), {L"--yes-label", L"Ok", L"--no-label", L"Cancel"});
        arguments.emplace_back(menu ? L"--menu" : L"--yesno");
    } else arguments.emplace_back(L"--msgbox");
    auto text = wideArgument(message);
    for (std::size_t i = 0; (i = text.find(L'\n', i)) != std::wstring::npos; i += 2)
        text.replace(i, 1, L"\\n");
    arguments.push_back(std::move(text));
    if (menu) arguments.insert(arguments.end(), {L"0", L"60", L"0", L"Yes", L"", L"No", L""});
    else arguments.insert(arguments.end(), {L"10", L"60"});
    const auto result = process_detail::run(arguments, {}, false, nullptr, menu);
    if (!menu) return result.started && result.status == 0 ? 1 : 0;
    auto value = lastConsoleLine(result.output);
    if (!value.empty() && value.back() == '\n') value.pop_back();
    return value.empty() ? 0 : value.front() == 'Y' ? 1 : 2;
}
const char* inputBoxWinConsole(char* output, const char* title, const char* message, const char* initial) {
    auto arguments = consoleArguments(title);
    std::wstring backtitle = L"tab: move focus";
    if (!initial) backtitle += L" (sometimes nothing, no blink nor star, is shown in text field)";
    arguments.insert(arguments.end(), {L"--backtitle", backtitle});
    if (!initial) arguments.insert(arguments.end(), {L"--insecure", L"--passwordbox"});
    else arguments.emplace_back(L"--inputbox");
    arguments.insert(arguments.end(), {wideArgument(message), L"10", L"60"});
    if (some(initial)) arguments.push_back(wideArgument(initial));
    const auto result = process_detail::run(arguments);
    storeConsoleOutput(output, result.started && result.status == 0 ? "1" + result.output : "0", true);
    if (output[0] != '1' || strlen(output) < 3) return nullptr;
    return output + 3;
}
const char* fileDialogWinConsole(char* output, const char* title, const char* initial, bool folder) {
    auto arguments = consoleArguments(title);
    arguments.insert(arguments.end(), {L"--backtitle",
        L"tab: focus | /: populate | spacebar: fill text field | ok: TEXT FIELD ONLY",
        folder ? L"--dselect" : L"--fselect"});
    auto path = wideArgument(initial);
    if (folder && !path.empty() && path.back() != L'\\') path.push_back(L'\\');
    std::replace(path.begin(), path.end(), L'\\', L'/');
    if (folder) {
        if (path.empty()) path = L"./";
    } else if (path.find(L'/') == std::wstring::npos) path.insert(0, L"./");
    arguments.insert(arguments.end(), {path, L"0", L"60"});
    const auto result = process_detail::run(arguments);
    storeConsoleOutput(output, result.output);
    replaceChr(output, '/', '\\');
    return output;
}
const char* saveFileDialogWinConsole(char* output, const char* title, const char* initial) {
    const auto result = fileDialogWinConsole(output, title, initial, false);
    char name[MAX_PATH_OR_CMD]{};
    getLastName(name, result);
    return some(name) ? result : nullptr;
}
const char* openFileDialogWinConsole(char* output, const char* title, const char* initial, bool) {
    return fileDialogWinConsole(output, title, initial, false);
}
const char* selectFolderDialogWinConsole(char* output, const char* title, const char* initial) {
    return fileDialogWinConsole(output, title, initial, true);
}

int _messageBox(
    const char* aTitle,
    const char* aMessage,
    const char* aDialogType,
    const char* aIconType,
    int aDefaultButton)
{
    char lChar{};
    const bool lQuery = eq(aTitle, "tinyfd_query");

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return 1;
        }
        return messageBoxWinGui(
            aTitle, aMessage, aDialogType, aIconType, aDefaultButton);
    }

    if (dialogPresent())
    {
        if (lQuery)
        {
            response("dialog");
            return 0;
        }
        return messageBoxWinConsole(
            aTitle, aMessage, aDialogType, aIconType, aDefaultButton);
    }
    else
    {
        if (lQuery)
        {
            response("basicinput");
            return 0;
        }
        if (!gWarningDisplayed && !tinyfd_forceConsole)
        {
            gWarningDisplayed = true;
            printf("\n\n%s\n", gTitle);
            printf("%s\n\n", tinyfd_needs);
        }
        if (some(aTitle))
        {
            printf("\n%s\n\n", aTitle);
        }
        if (eq("yesno", aDialogType))
        {
            do
            {
                if (some(aMessage))
                {
                    printf("%s\n", aMessage);
                }
                printf("y/n: ");
                lChar = (char)tolower(_getch());
                printf("\n\n");
            } while (lChar != 'y' && lChar != 'n');
            return lChar == 'y' ? 1 : 0;
        }
        else if (eq("okcancel", aDialogType))
        {
            do
            {
                if (some(aMessage))
                {
                    printf("%s\n", aMessage);
                }
                printf("[O]kay/[C]ancel: ");
                lChar = (char)tolower(_getch());
                printf("\n\n");
            } while (lChar != 'o' && lChar != 'c');
            return lChar == 'o' ? 1 : 0;
        }
        else if (eq("yesnocancel", aDialogType))
        {
            do
            {
                if (some(aMessage))
                {
                    printf("%s\n", aMessage);
                }
                printf("[Y]es/[N]o/[C]ancel: ");
                lChar = (char)tolower(_getch());
                printf("\n\n");
            } while (lChar != 'y' && lChar != 'n' && lChar != 'c');
            return (lChar == 'y') ? 1 : (lChar == 'n') ? 2 : 0;
        }
        else
        {
            if (some(aMessage))
            {
                printf("%s\n\n", aMessage);
            }
            printf("press enter to continue ");
            lChar = (char)_getch();
            printf("\n\n");
            return 1;
        }
    }
}

int _notifyPopup(
    const char* aTitle,
    const char* aMessage,
    const char* aIconType)
{
    const bool lQuery = eq(aTitle, "tinyfd_query");

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return 1;
        }
        return notifyWinGui(aTitle, aMessage, aIconType);
    }

    return _messageBox(aTitle, aMessage, "ok", aIconType, 0);
}

const char* _inputBox(
    const char* aTitle,
    const char* aMessage,
    const char* aDefaultInput)
{
    static thread_local char lBuff[MAX_PATH_OR_CMD]{};
    char* lEOF{};
    const bool lQuery = eq(aTitle, "tinyfd_query");

    

    DWORD mode = 0;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return (const char*)1;
        }
        lBuff[0] = '\0';
        return inputBoxWinGui(lBuff, aTitle, aMessage, aDefaultInput);
    }

    if (dialogPresent())
    {
        if (lQuery)
        {
            response("dialog");
            return (const char*)0;
        }
        lBuff[0] = '\0';
        return inputBoxWinConsole(lBuff, aTitle, aMessage, aDefaultInput);
    }
    else
    {
        if (lQuery)
        {
            response("basicinput");
            return (const char*)0;
        }
        lBuff[0] = '\0';
        if (!gWarningDisplayed && !tinyfd_forceConsole)
        {
            gWarningDisplayed = true;
            printf("\n\n%s\n", gTitle);
            printf("%s\n\n", tinyfd_needs);
        }
        if (some(aTitle))
        {
            printf("\n%s\n\n", aTitle);
        }
        if (some(aMessage))
        {
            printf("%s\n", aMessage);
        }
        printf("(ctrl-Z + enter to cancel): ");
        
            if (!aDefaultInput)
            {
                GetConsoleMode(hStdin, &mode);
                SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
            }
        

        lEOF = fgets(lBuff, sizeof(lBuff), stdin);
        if (!lEOF)
            return nullptr;

        
            if (!aDefaultInput)
            {
                SetConsoleMode(hStdin, mode);
                printf("\n");
            }
        
        printf("\n");
        if (strchr(lBuff, 27))
            return nullptr;
        removeLastNL(lBuff);
        return lBuff;
    }
}

const char* _saveFileDialog(
    const char* aTitle,
    const char* aDefaultPathAndFile,
    std::span<const TFD_Filter> aFilters)
{
    static thread_local char lBuff[MAX_PATH_OR_CMD]{};
    const bool lQuery = eq(aTitle, "tinyfd_query");
    char lString[MAX_PATH_OR_CMD]{};
    const char* p{};
    lBuff[0] = '\0';

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return (const char*)1;
        }
        p = saveFileDialogWinGui(lBuff, aTitle, aDefaultPathAndFile, aFilters);
        goto end;
    }

    if (dialogPresent())
    {
        if (lQuery)
        {
            response("dialog");
            return (const char*)0;
        }
        p = saveFileDialogWinConsole(lBuff, aTitle, aDefaultPathAndFile);
    }
    else
    {
        if (lQuery)
        {
            response("basicinput");
            return (const char*)0;
        }
        p = _inputBox(aTitle, "Save file", "");
    }

    end:

    if (!some(p))
        return nullptr;
    getPathWithoutFinalSlash(lString, p);
    if (!dirExists(lString))
        return nullptr;
    getLastName(lString, p);
    if (!filenameValid(lString))
        return nullptr;
    return p;
}

const char* _openFileDialog(
    const char* aTitle,
    const char* aDefaultPathAndFile,
    std::span<const TFD_Filter> aFilters,
    const bool aAllowMultipleSelects)
{
    static thread_local char lBuff[MAX_MULTIPLE_FILES * MAX_PATH_OR_CMD]{};
    const bool lQuery = eq(aTitle, "tinyfd_query");
    const char* p{};

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return (const char*)1;
        }
        p = openFileDialogWinGui(lBuff, aTitle, aDefaultPathAndFile, aFilters, aAllowMultipleSelects);
        goto end;
    }

    if (dialogPresent())
    {
        if (lQuery)
        {
            response("dialog");
            return (const char*)0;
        }
        p = openFileDialogWinConsole(lBuff,
                                     aTitle, aDefaultPathAndFile, aAllowMultipleSelects);
    }
    else
    {
        if (lQuery)
        {
            response("basicinput");
            return (const char*)0;
        }
        p = _inputBox(aTitle, "Open file", "");
    }

    end:

    if (!some(p))
        return nullptr;
    if (aAllowMultipleSelects && strchr(p, '|'))
        p = ensureFilesExist(lBuff, p);
    else if (!fileExists(p))
        return nullptr;
    return p;
}

const char* _selectFolderDialog(const char* aTitle, const char* aDefaultPath)
{
    static thread_local char lBuff[MAX_PATH_OR_CMD]{};
    const bool lQuery = eq(aTitle, "tinyfd_query");
    const char* p{};

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return (const char*)1;
        }
        
            p = selectFolderDialogWinGui(lBuff, aTitle, aDefaultPath);
            return dirExists(p) ? p : nullptr;
        
    }

    if (dialogPresent())
    {
        if (lQuery)
        {
            response("dialog");
            return (const char*)0;
        }
        p = selectFolderDialogWinConsole(lBuff, aTitle, aDefaultPath);
    }
    else
    {
        if (lQuery)
        {
            response("basicinput");
            return (const char*)0;
        }
        p = _inputBox(aTitle, "Select folder", "");
    }
    return dirExists(p) ? p : nullptr;
}

const char* _colorChooser(
    const char* aTitle,
    const char* aDefaultHexRGB,
    const std::array<unsigned char, 3>& aDefaultRGB,
    std::array<unsigned char, 3>& aoResultRGB)
{
    const bool lQuery = eq(aTitle, "tinyfd_query");
    char lDefaultHexRGB[8]{};
    char* lpDefaultHexRGB{};
    const char* p{};

    

    if (willBeGui())
    {
        if (lQuery)
        {
            response("windows");
            return (const char*)1;
        }
        return colorChooserWinGui(aTitle, aDefaultHexRGB, aDefaultRGB, aoResultRGB);
    }

    if (aDefaultHexRGB)
    {
        lpDefaultHexRGB = (char*)aDefaultHexRGB;
    }
    else
    {
        RGB2Hex(aDefaultRGB, lDefaultHexRGB);
        lpDefaultHexRGB = (char*)lDefaultHexRGB;
    }
    p = _inputBox(aTitle, "Enter hex rgb color (i.e. #f5ca20)", lpDefaultHexRGB);
    if (lQuery)
        return p;

    if (!p || strlen(p) != 7 || p[0] != '#')
        return nullptr;

    for (std::size_t i = 1; i < 7; ++i)
    {
        if (!isxdigit(static_cast<unsigned char>(p[i])))
            return nullptr;
    }
    Hex2RGB(p, aoResultRGB);
    return p;
}


} // namespace
void tinyfd_beep() { _beep(); }
int tinyfd_notifyPopup(const char* t,const char* m,const char* i) { return _notifyPopup(t,m,i); }
int tinyfd_messageBox(const char* t,const char* m,const char* d,const char* i,int b) { return _messageBox(t,m,d,i,b); }
const char* tinyfd_inputBox(const char* t,const char* m,const char* d) { return _inputBox(t,m,d); }
const char* tinyfd_saveFileDialog(const char* t,const char* d,std::span<const TFD_Filter> f) { return _saveFileDialog(t,d,f); }
const char* tinyfd_openFileDialog(const char* t,const char* d,std::span<const TFD_Filter> f,bool m) { return _openFileDialog(t,d,f,m); }
const char* tinyfd_selectFolderDialog(const char* t,const char* d) { return _selectFolderDialog(t,d); }
const char* tinyfd_colorChooser(const char* t,const char* d,const std::array<unsigned char,3>& rgb,std::array<unsigned char,3>& out) { return _colorChooser(t,d,rgb,out); }
} // namespace nijigenerate::platform::tinyfiledialogs


