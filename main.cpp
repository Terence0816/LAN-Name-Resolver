#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr wchar_t kWindowClassName[] = L"LANNameResolverWindowClass";
constexpr wchar_t kAppTitle[] = L"LAN Name Resolver";
constexpr wchar_t kConfigFileName[] = L"lan_name_resolver_entries.txt";
constexpr wchar_t kSettingsFileName[] = L"lan_name_resolver_settings.ini";
constexpr wchar_t kStartupValueName[] = L"LANNameResolver";
constexpr wchar_t kHostsManagedBlockBegin[] = L"# BEGIN LAN Name Resolver";
constexpr wchar_t kHostsManagedBlockEnd[] = L"# END LAN Name Resolver";
constexpr wchar_t kAboutGithubUrl[] = L"https://github.com/Terence0816/LAN-Name-Resolver";

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_APP_LOG = WM_APP + 2;
constexpr UINT WM_APP_STATUS = WM_APP + 3;
constexpr UINT kTrayIconId = 1;

constexpr int kWindowWidth = 1120;
constexpr int kWindowHeight = 820;

constexpr int IDC_GROUP_LEFT = 100;
constexpr int IDC_GROUP_SERVICE = 101;
constexpr int IDC_GROUP_STARTUP = 102;
constexpr int IDC_GROUP_LOG = 103;
constexpr int IDC_LEFT_INTRO = 104;
constexpr int IDC_NAME_HEADER = 105;
constexpr int IDC_IP_HEADER = 106;
constexpr int IDC_NAME_INPUT = 107;
constexpr int IDC_IP_INPUT = 108;
constexpr int IDC_ADD_ROW = 109;
constexpr int IDC_REMOVE_ROW = 110;
constexpr int IDC_SAVE_SETTINGS = 111;
constexpr int IDC_ENTRY_LIST = 112;
constexpr int IDC_INFO_TEXT = 113;
constexpr int IDC_START_SERVER = 114;
constexpr int IDC_STOP_SERVER = 115;
constexpr int IDC_STATUS_TEXT = 116;
constexpr int IDC_SERVICE_DETAIL = 117;
constexpr int IDC_ENABLE_STARTUP = 118;
constexpr int IDC_DISABLE_STARTUP = 119;
constexpr int IDC_STARTUP_STATUS = 120;
constexpr int IDC_STARTUP_DETAIL = 121;
constexpr int IDC_HINT_TEXT = 122;
constexpr int IDC_LOG_EDIT = 123;
constexpr int IDC_TAB_CONTROL = 124;

constexpr int IDC_HOSTS_GROUP = 130;
constexpr int IDC_HOSTS_INTRO = 131;
constexpr int IDC_HOSTS_EDIT = 132;
constexpr int IDC_WRITE_HOSTS = 134;
constexpr int IDC_OPEN_HOSTS = 135;
constexpr int IDC_HOSTS_FORMAT = 136;
constexpr int IDC_HOSTS_NOTE = 137;

constexpr int IDC_SETTINGS_APP_GROUP = 140;
constexpr int IDC_MINIMIZE_TO_TRAY = 141;
constexpr int IDC_SETTINGS_LANGUAGE_GROUP = 142;
constexpr int IDC_LANGUAGE_LABEL = 143;
constexpr int IDC_LANGUAGE_COMBO = 144;
constexpr int IDC_CUSTOM_LANGUAGE_HINT = 147;
constexpr int IDC_SETTINGS_ABOUT_GROUP = 148;
constexpr int IDC_ABOUT_TITLE = 149;
constexpr int IDC_ABOUT_BODY = 150;
constexpr int IDC_ABOUT_GITHUB_LINK = 151;

constexpr UINT ID_TRAY_SHOW = 40001;
constexpr UINT ID_TRAY_EXIT = 40002;

constexpr unsigned short kNbnsPort = 137;
constexpr unsigned short kLlmnrPort = 5355;
constexpr unsigned short kNbType = 0x0020;
constexpr unsigned short kNbStatType = 0x0021;
constexpr unsigned short kInClass = 0x0001;
constexpr unsigned short kDnsAType = 0x0001;
constexpr unsigned short kDnsAnyType = 0x00FF;
constexpr uint32_t kDefaultTtlSeconds = 300;
constexpr uint32_t kLlmnrTtlSeconds = 30;
constexpr int kAnnounceIntervalSeconds = 30;

constexpr COLORREF kStartFill = RGB(233, 247, 239);
constexpr COLORREF kStartBorder = RGB(35, 119, 67);
constexpr COLORREF kStartText = RGB(18, 94, 47);
constexpr COLORREF kStopFill = RGB(255, 242, 242);
constexpr COLORREF kStopBorder = RGB(188, 70, 63);
constexpr COLORREF kStopText = RGB(155, 44, 44);
constexpr COLORREF kSecondaryFill = RGB(239, 245, 251);
constexpr COLORREF kSecondaryBorder = RGB(98, 127, 160);
constexpr COLORREF kSecondaryText = RGB(53, 84, 120);
constexpr COLORREF kNeutralFill = RGB(248, 248, 248);
constexpr COLORREF kNeutralBorder = RGB(160, 160, 160);
constexpr COLORREF kNeutralText = RGB(90, 90, 90);

struct RowControl {
    HWND nameEdit = nullptr;
    HWND ipEdit = nullptr;
};

struct ResolverEntry {
    std::wstring displayName;
    std::string upperName;
    std::wstring ipText;
    uint32_t ipAddress = 0;
};

struct NameWireResult {
    std::vector<std::vector<uint8_t>> labels;
    std::vector<uint8_t> wireName;
    size_t endOffset = 0;
    bool ok = false;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HFONT font = nullptr;
    HFONT tabFont = nullptr;
    HFONT sectionFont = nullptr;
    HFONT bannerFont = nullptr;
    HFONT detailFont = nullptr;
    HFONT monoFont = nullptr;
    HFONT linkFont = nullptr;
    HWND tabControl = nullptr;
    HWND leftGroup = nullptr;
    HWND serviceGroup = nullptr;
    HWND startupGroup = nullptr;
    HWND logGroup = nullptr;
    HWND leftIntro = nullptr;
    HWND nameHeader = nullptr;
    HWND ipHeader = nullptr;
    HWND nameInput = nullptr;
    HWND ipInput = nullptr;
    HWND addButton = nullptr;
    HWND removeButton = nullptr;
    HWND saveButton = nullptr;
    HWND entryList = nullptr;
    HWND infoText = nullptr;
    HWND startButton = nullptr;
    HWND stopButton = nullptr;
    HWND statusText = nullptr;
    HWND serviceDetailText = nullptr;
    HWND hintText = nullptr;
    HWND enableStartupButton = nullptr;
    HWND disableStartupButton = nullptr;
    HWND startupStatusText = nullptr;
    HWND startupDetailText = nullptr;
    HWND logEdit = nullptr;
    HWND hostsGroup = nullptr;
    HWND hostsIntro = nullptr;
    HWND hostsEdit = nullptr;
    HWND writeHostsButton = nullptr;
    HWND openHostsButton = nullptr;
    HWND hostsFormatText = nullptr;
    HWND hostsNoteText = nullptr;
    HWND settingsAppGroup = nullptr;
    HWND minimizeToTrayCheck = nullptr;
    HWND settingsLanguageGroup = nullptr;
    HWND languageLabel = nullptr;
    HWND languageCombo = nullptr;
    HWND customLanguageHintText = nullptr;
    HWND settingsAboutGroup = nullptr;
    HWND aboutTitleText = nullptr;
    HWND aboutBodyText = nullptr;
    HWND aboutGithubLink = nullptr;
    std::vector<RowControl> rows;
    std::unique_ptr<class ResolverService> service;
    std::filesystem::path configPath;
    std::filesystem::path settingsPath;
    std::map<std::wstring, std::wstring> strings;
    std::vector<std::wstring> languageCodes;
    std::wstring languageCode = L"tw";
    int activeTab = 0;
    bool trayAdded = false;
    bool exiting = false;
    bool startMinimized = false;
    bool autoStartServer = false;
    bool serviceRunning = false;
    bool startupEnabled = false;
    bool minimizeToTray = true;
    bool isElevated = false;
};

AppState* g_app = nullptr;

std::wstring Trim(const std::wstring& input) {
    const auto begin = input.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return L"";
    }
    const auto end = input.find_last_not_of(L" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::wstring ToUpperAscii(const std::wstring& input) {
    std::wstring output = input;
    for (auto& ch : output) {
        if (ch >= L'a' && ch <= L'z') {
            ch = static_cast<wchar_t>(ch - L'a' + L'A');
        }
    }
    return output;
}

std::wstring ToLowerAscii(const std::wstring& input) {
    std::wstring output = input;
    for (auto& ch : output) {
        if (ch >= L'A' && ch <= L'Z') {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
    }
    return output;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }

    const int needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return {};
    }

    std::string output(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, output.data(), needed, nullptr, nullptr);
    output.resize(static_cast<size_t>(needed - 1));
    return output;
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (needed <= 1) {
        return {};
    }

    std::wstring output(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), needed);
    output.resize(static_cast<size_t>(needed - 1));
    return output;
}

std::wstring FormatTimeNow() {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    wchar_t buffer[32] = {};
    wsprintfW(buffer, L"%02u:%02u:%02u", localTime.wHour, localTime.wMinute, localTime.wSecond);
    return buffer;
}

std::wstring FormatErrorMessage(DWORD code) {
    LPWSTR raw = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);

    std::wstring message = raw ? Trim(raw) : L"未知錯誤";
    if (raw) {
        LocalFree(raw);
    }
    return message;
}

std::wstring QuoteForCommandLine(const std::wstring& path) {
    if (path.find(L' ') == std::wstring::npos) {
        return path;
    }
    return L"\"" + path + L"\"";
}

std::filesystem::path GetExePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path GetConfigPath() {
    return GetExePath().parent_path() / kConfigFileName;
}

std::filesystem::path GetSettingsPath() {
    return GetExePath().parent_path() / kSettingsFileName;
}

std::filesystem::path GetLanguageFilePath(const std::wstring& selection) {
    if (selection.empty()) {
        return GetExePath().parent_path() / L"en.txt";
    }

    std::wstring fileName = selection;
    if (fileName == L"tw" || fileName == L"en") {
        fileName += L".txt";
    }
    return GetExePath().parent_path() / fileName;
}

std::filesystem::path GetSystemHostsPath() {
    std::wstring buffer(MAX_PATH, L'\0');
    const UINT length = GetSystemWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return L"C:\\Windows\\System32\\drivers\\etc\\hosts";
    }

    buffer.resize(length);
    return std::filesystem::path(buffer) / L"System32" / L"drivers" / L"etc" / L"hosts";
}

bool IsProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok == TRUE && elevation.TokenIsElevated != 0;
}

bool ReadWholeFileUtf8(const std::filesystem::path& path, std::string& content) {
    content.clear();
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }

    content.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytesRead = 0;
    const BOOL ok = content.empty() ? TRUE : ReadFile(file, content.data(), static_cast<DWORD>(content.size()), &bytesRead, nullptr);
    CloseHandle(file);
    if (!ok) {
        content.clear();
        return false;
    }

    content.resize(bytesRead);
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return true;
}

bool WriteWholeFileUtf8(const std::filesystem::path& path, const std::string& content) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten = 0;
    const BOOL ok = content.empty() ? TRUE : WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &bytesWritten, nullptr);
    CloseHandle(file);
    return ok && bytesWritten == content.size();
}

using StringMap = std::map<std::wstring, std::wstring>;

std::wstring EscapeConfigValue(const std::wstring& value) {
    std::wstring output;
    output.reserve(value.size());
    for (const wchar_t ch : value) {
        switch (ch) {
        case L'\\':
            output += L"\\\\";
            break;
        case L'\r':
            break;
        case L'\n':
            output += L"\\n";
            break;
        default:
            output.push_back(ch);
            break;
        }
    }
    return output;
}

std::wstring UnescapeConfigValue(const std::wstring& value) {
    std::wstring output;
    output.reserve(value.size());
    bool escaping = false;
    for (const wchar_t ch : value) {
        if (escaping) {
            if (ch == L'n') {
                output += L'\n';
            } else {
                output.push_back(ch);
            }
            escaping = false;
            continue;
        }

        if (ch == L'\\') {
            escaping = true;
            continue;
        }

        output.push_back(ch);
    }
    if (escaping) {
        output.push_back(L'\\');
    }
    return output;
}

bool LoadKeyValueFile(const std::filesystem::path& path, StringMap& values) {
    values.clear();

    std::string content;
    if (!ReadWholeFileUtf8(path, content)) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        const std::wstring key = Trim(Utf8ToWide(line.substr(0, eqPos)));
        const std::wstring value = UnescapeConfigValue(Utf8ToWide(line.substr(eqPos + 1)));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return true;
}

bool SaveKeyValueFile(const std::filesystem::path& path, const StringMap& values) {
    std::ostringstream stream;
    for (const auto& [key, value] : values) {
        stream << WideToUtf8(key) << '=' << WideToUtf8(EscapeConfigValue(value)) << "\n";
    }
    return WriteWholeFileUtf8(path, stream.str());
}

StringMap BuildTraditionalChineseStrings() {
    return {
        {L"window_title", L"LAN Name Resolver"},
        {L"tab_server", L"伺服器管理"},
        {L"tab_hosts", L"Hosts 修改"},
        {L"tab_settings", L"設定"},
        {L"tab_about", L"關於"},
        {L"group_name_ip", L"名稱 / IP 設定"},
        {L"group_service", L"服務控制"},
        {L"group_startup", L"開機自動啟動"},
        {L"group_log", L"執行紀錄"},
        {L"server_intro", L"先輸入名稱與 IP，再按「新增一組」加入下方列表，啟動時會以列表內容為主。"},
        {L"label_name", L"名稱"},
        {L"label_ip", L"IPv4"},
        {L"button_add", L"新增一組"},
        {L"button_remove", L"刪除一組"},
        {L"button_save", L"儲存設定"},
        {L"server_info", L"支援 NBNS / LLMNR，同網段可用名稱直接解析到指定 IP。"},
        {L"status_running", L"[OK] 解析服務已啟動"},
        {L"status_stopped", L"[STOP] 解析服務未啟動"},
        {L"button_start", L"啟動服務器"},
        {L"button_stop", L"停止服務器"},
        {L"service_count_prefix", L"已設定名稱: "},
        {L"service_mode", L"解析方式: 依下方狀態顯示"},
        {L"service_nbns_on", L"NBNS / NetBIOS: 已啟用"},
        {L"service_nbns_waiting", L"NBNS / NetBIOS: 等待啟動"},
        {L"service_nbns_failed", L"NBNS / NetBIOS: 未啟用，137 無法綁定"},
        {L"service_llmnr_on", L"LLMNR: 已啟用"},
        {L"service_llmnr_waiting", L"LLMNR: 等待啟動"},
        {L"service_llmnr_blocked", L"LLMNR: 未啟用，5355 可能被占用"},
        {L"service_llmnr_failed", L"LLMNR: 未啟用，5355 無法綁定"},
        {L"service_purpose", L"用途: 同網段名稱解析，減少改 hosts"},
        {L"service_hint", L"說明:\r\n1. 監聽 UDP 137 / 5355\r\n2. 回應 NBNS 與 LLMNR 查詢"},
        {L"startup_enabled", L"[OK] 已加入開機自動啟動"},
        {L"startup_disabled", L"[STOP] 未啟用開機自動啟動"},
        {L"startup_detail_enabled", L"開機後會自動縮到托盤並啟動解析服務"},
        {L"startup_detail_disabled", L"目前未加入 Windows 開機啟動"},
        {L"button_enable_startup", L"加入開機自動啟動"},
        {L"button_disable_startup", L"移除開機自動啟動"},
        {L"hosts_group", L"Hosts 修改"},
        {L"hosts_intro", L"可直接手動編輯下方內容，每行格式為「IP 名稱」。"},
        {L"hosts_format", L"範例:\r\n192.168.11.22 pc1\r\n192.168.11.33 pc2\r\n192.168.11.44 pc3"},
        {L"button_import_server", L"匯入伺服器設定"},
        {L"button_write_hosts", L"寫入 hosts"},
        {L"button_open_hosts", L"開啟 hosts"},
        {L"hosts_note_admin", L"本功能需以管理員身分執行本程式，才能直接寫入 hosts。"},
        {L"hosts_note_admin_ok", L"目前已使用管理員身分執行，可直接寫入 hosts。"},
        {L"group_app_settings", L"應用程式設定"},
        {L"check_minimize_to_tray", L"關閉視窗時縮小到系統區"},
        {L"group_language", L"語系"},
        {L"label_language", L"介面語言"},
        {L"button_apply_language", L"套用語言"},
        {L"button_open_language_file", L"開啟語言檔"},
        {L"custom_language_hint", L"會自動讀取程式同目錄的 *.txt 語言檔。"},
        {L"group_about", L"關於"},
        {L"about_title", L"LAN Name Resolver"},
        {L"about_description",
         L"這是一個適用於 Windows 7 /Windows 10 / 11 的區域網路名稱解析工具。\r\n\r\n"
         L"主要功能：\r\n"
         L"- 回應 NBNS / NetBIOS 名稱查詢\r\n"
         L"- 回應 LLMNR 名稱查詢\r\n"
         L"- 將自訂主機名稱對應到指定 IPv4 位址\r\n"
         L"- 提供選用的 hosts 修改工具\r\n"
         L"- 支援系統托盤與開機自動啟動\r\n\r\n"
         L"適用情境：\r\n"
         L"- VPN 後需要使用 \\\\pcname 連線內部電腦\r\n"
         L"- 小型區域網路沒有 DNS / WINS Server\r\n"
         L"- 多台電腦不想逐台手動修改 hosts\r\n"
         L"- 臨時維修、部署、測試或故障排除環境\r\n\r\n"
         L"安全性說明：\r\n"
         L"本工具僅適合在自己的 LAN、VPN、維護環境或已授權的網路中使用。\r\n"
         L"hosts 檔案只會在使用者手動操作時修改。"},
        {L"about_version", L"版本 1.0"},
        {L"about_copyright", L"Copyright \u00A9 2026 Terence0816"},
        {L"about_github_label", L"Project:"},
        {L"language_option_tw", L"繁體中文"},
        {L"language_option_en", L"English"},
        {L"language_option_custom", L"自定義語言"},
        {L"title_error", L"錯誤"},
        {L"title_info", L"提示"},
        {L"title_warning", L"警告"},
        {L"title_config_error", L"設定錯誤"},
        {L"title_start_failed", L"啟動失敗"},
        {L"title_duplicate_name", L"名稱重複"},
        {L"message_save_failed", L"儲存設定失敗，請確認程式目錄可寫入。"},
        {L"message_select_remove_entry", L"請先選取要刪除的那一組名稱 / IP。"},
        {L"message_name_empty", L"名稱不能是空的。"},
        {L"message_name_too_long_prefix", L"名稱「"},
        {L"message_name_too_long_suffix", L"」超過 15 個字元，NetBIOS 名稱最多 15 個字元。"},
        {L"message_name_ascii_prefix", L"名稱「"},
        {L"message_name_ascii_suffix", L"」只能使用 ASCII 字元。"},
        {L"message_invalid_ip_prefix", L"IP「"},
        {L"message_invalid_ip_suffix", L"」不是有效的 IPv4 位址。"},
        {L"message_duplicate_unique_prefix", L"名稱「"},
        {L"message_duplicate_unique_suffix", L"」重複，請改成唯一名稱。"},
        {L"message_duplicate_exists_prefix", L"名稱「"},
        {L"message_duplicate_exists_suffix", L"」已存在，請先刪除舊項目或改用其他名稱。"},
        {L"message_need_entry_list", L"請先新增至少一組名稱與 IP 到下方列表。"},
        {L"message_hosts_line_prefix", L"第 "},
        {L"message_hosts_line_invalid_format_suffix", L" 行格式錯誤。"},
        {L"message_hosts_line_missing_ip_suffix", L" 行缺少有效的 IPv4 位址。"},
        {L"message_hosts_line_missing_name_suffix", L" 行缺少主機名稱。"},
        {L"message_winsock_init_failed", L"無法初始化 Winsock。"},
        {L"message_custom_language_missing", L"找不到自定義語言檔，已替你建立範本後再開啟。"},
        {L"message_custom_language_open_failed", L"無法開啟自定義語言檔。"},
        {L"message_hosts_open_failed", L"無法開啟 hosts 檔案。"},
        {L"message_admin_required", L"此功能需要以管理員身分執行本程式。"},
        {L"message_no_server_entries", L"目前伺服器管理列表沒有可匯入的名稱 / IP。"},
        {L"message_hosts_written", L"已將內容寫入 hosts。"},
        {L"message_language_applied", L"介面語言已重新載入。"},
        {L"log_program_ready", L"程式已開啟，可在上方輸入名稱 / IP 後加入列表。"},
        {L"log_entries_saved", L"已儲存名稱 / IP 設定。"},
        {L"log_startup_enabled", L"已啟用開機自動啟動。"},
        {L"log_startup_disabled", L"已停用開機自動啟動。"},
        {L"log_hide_to_tray", L"視窗已縮到右下角系統托盤。"},
        {L"log_hosts_imported", L"已將伺服器管理頁的名稱 / IP 匯入 hosts 編輯區。"},
        {L"log_hosts_written", L"已將編輯區內容寫入 hosts。"},
        {L"log_language_applied", L"已重新載入介面語言。"},
        {L"log_service_started", L"已啟動 LAN Name Resolver 服務。"},
        {L"log_nbns_enabled", L"NBNS UDP 137 已啟用。"},
        {L"log_nbns_failed_prefix", L"NBNS UDP 137 啟動失敗，將改以其他可用解析方式繼續服務。原因: "},
        {L"log_llmnr_enabled", L"LLMNR UDP 5355 已啟用。"},
        {L"log_llmnr_failed_prefix", L"LLMNR UDP 5355 啟動失敗，將改以其他可用解析方式繼續服務。原因: "},
        {L"log_service_stopped", L"服務已停止。"},
        {L"log_periodic_prefix", L"週期宣告完成，共宣告 "},
        {L"log_periodic_suffix", L" 組名稱/IP。"},
        {L"log_nbns_query_prefix", L"已回應 NBNS 查詢: "},
        {L"log_llmnr_query_prefix", L"已回應 LLMNR 查詢: "},
        {L"log_node_status_prefix", L"已回應 Node Status 查詢，來源 "},
        {L"log_source_label", L"，來源 "},
        {L"tray_show", L"顯示畫面"},
        {L"tray_exit", L"退出"}
    };
}

StringMap BuildEnglishStrings() {
    return {
        {L"window_title", L"LAN Name Resolver"},
        {L"tab_server", L"Server"},
        {L"tab_hosts", L"Hosts"},
        {L"tab_settings", L"Settings"},
        {L"tab_about", L"About"},
        {L"group_name_ip", L"Name / IP Entries"},
        {L"group_service", L"Service Control"},
        {L"group_startup", L"Startup"},
        {L"group_log", L"Log"},
        {L"server_intro", L"Enter name and IP, click Add, then start with the list below."},
        {L"label_name", L"Name"},
        {L"label_ip", L"IPv4"},
        {L"button_add", L"Add"},
        {L"button_remove", L"Delete"},
        {L"button_save", L"Save"},
        {L"server_info", L"Supports NBNS / LLMNR so names can resolve to the target IP on the same LAN."},
        {L"status_running", L"[OK] Resolver Active"},
        {L"status_stopped", L"[STOP] Resolver Stopped"},
        {L"button_start", L"Start Server"},
        {L"button_stop", L"Stop Server"},
        {L"service_count_prefix", L"Configured names: "},
        {L"service_mode", L"Resolver mode: shown below"},
        {L"service_nbns_on", L"NBNS / NetBIOS: enabled"},
        {L"service_nbns_waiting", L"NBNS / NetBIOS: waiting to start"},
        {L"service_nbns_failed", L"NBNS / NetBIOS: unavailable, 137 could not be bound"},
        {L"service_llmnr_on", L"LLMNR: enabled"},
        {L"service_llmnr_waiting", L"LLMNR: waiting to start"},
        {L"service_llmnr_blocked", L"LLMNR: unavailable, 5355 is likely already in use"},
        {L"service_llmnr_failed", L"LLMNR: unavailable, 5355 could not be bound"},
        {L"service_purpose", L"Purpose: LAN name resolution without editing hosts everywhere"},
        {L"service_hint", L"Notes:\r\n1. Listen on UDP 137 / 5355\r\n2. Reply to NBNS and LLMNR queries"},
        {L"startup_enabled", L"[OK] Startup Enabled"},
        {L"startup_disabled", L"[STOP] Startup Disabled"},
        {L"startup_detail_enabled", L"The app will start minimized to tray and launch the resolver automatically."},
        {L"startup_detail_disabled", L"Not added to Windows startup."},
        {L"button_enable_startup", L"Enable Startup"},
        {L"button_disable_startup", L"Disable Startup"},
        {L"hosts_group", L"Hosts Editor"},
        {L"hosts_intro", L"Edit the lines below manually. Each line should use the format \"IP name\"."},
        {L"hosts_format", L"Example:\r\n192.168.11.22 pc1\r\n192.168.11.33 pc2\r\n192.168.11.44 pc3"},
        {L"button_import_server", L"Import Server Entries"},
        {L"button_write_hosts", L"Write to hosts"},
        {L"button_open_hosts", L"Open hosts"},
        {L"hosts_note_admin", L"This feature requires the application to run as administrator before it can write to hosts directly."},
        {L"hosts_note_admin_ok", L"The application is already running as administrator and can write to hosts directly."},
        {L"group_app_settings", L"Application Settings"},
        {L"check_minimize_to_tray", L"Minimize to tray when closing the window"},
        {L"group_language", L"Language"},
        {L"label_language", L"Interface language"},
        {L"button_apply_language", L"Apply"},
        {L"button_open_language_file", L"Open Language File"},
        {L"custom_language_hint", L"The program automatically reads *.txt language files from its folder."},
        {L"group_about", L"About"},
        {L"about_title", L"LAN Name Resolver"},
        {L"about_description",
         L"This is a LAN name resolution tool for Windows 7 / Windows 10 / 11.\r\n\r\n"
         L"Main features:\r\n"
         L"- Reply to NBNS / NetBIOS name queries\r\n"
         L"- Reply to LLMNR name queries\r\n"
         L"- Map custom host names to target IPv4 addresses\r\n"
         L"- Provide an optional hosts editing helper\r\n"
         L"- Support system tray and Windows startup\r\n\r\n"
         L"Recommended scenarios:\r\n"
         L"- Access internal computers by \\\\pcname after connecting through VPN\r\n"
         L"- Small LAN environments without a DNS / WINS server\r\n"
         L"- Multiple PCs where editing hosts one by one is inconvenient\r\n"
         L"- Temporary maintenance, deployment, testing, or troubleshooting environments\r\n\r\n"
         L"Security note:\r\n"
         L"This tool should only be used in your own LAN, VPN, maintenance environment, or other authorized networks.\r\n"
         L"The hosts file is modified only when the user performs the action manually."},
        {L"about_version", L"Version 1.0"},
        {L"about_copyright", L"Copyright \u00A9 2026 Terence0816"},
        {L"about_github_label", L"Project:"},
        {L"language_option_tw", L"Traditional Chinese"},
        {L"language_option_en", L"English"},
        {L"language_option_custom", L"Custom Language"},
        {L"title_error", L"Error"},
        {L"title_info", L"Information"},
        {L"title_warning", L"Warning"},
        {L"title_config_error", L"Invalid Settings"},
        {L"title_start_failed", L"Start Failed"},
        {L"title_duplicate_name", L"Duplicate Name"},
        {L"message_save_failed", L"Failed to save settings. Please make sure the program folder is writable."},
        {L"message_select_remove_entry", L"Select the entry you want to remove first."},
        {L"message_name_empty", L"Name cannot be empty."},
        {L"message_name_too_long_prefix", L"Name \""},
        {L"message_name_too_long_suffix", L"\" is longer than 15 characters. NetBIOS names allow up to 15 characters."},
        {L"message_name_ascii_prefix", L"Name \""},
        {L"message_name_ascii_suffix", L"\" must use ASCII characters only."},
        {L"message_invalid_ip_prefix", L"IP \""},
        {L"message_invalid_ip_suffix", L"\" is not a valid IPv4 address."},
        {L"message_duplicate_unique_prefix", L"Name \""},
        {L"message_duplicate_unique_suffix", L"\" is duplicated. Please make it unique."},
        {L"message_duplicate_exists_prefix", L"Name \""},
        {L"message_duplicate_exists_suffix", L"\" already exists. Remove the old entry or choose another name."},
        {L"message_need_entry_list", L"Add at least one name / IP entry to the list first."},
        {L"message_hosts_line_prefix", L"Line "},
        {L"message_hosts_line_invalid_format_suffix", L" format is invalid."},
        {L"message_hosts_line_missing_ip_suffix", L" is missing a valid IPv4 address."},
        {L"message_hosts_line_missing_name_suffix", L" is missing a host name."},
        {L"message_winsock_init_failed", L"Unable to initialize Winsock."},
        {L"message_custom_language_missing", L"The custom language file was not found. A template will be created before opening it."},
        {L"message_custom_language_open_failed", L"Unable to open the custom language file."},
        {L"message_hosts_open_failed", L"Unable to open the hosts file."},
        {L"message_admin_required", L"This feature requires the program to run as administrator."},
        {L"message_no_server_entries", L"There are no server entries available to import."},
        {L"message_hosts_written", L"The hosts file has been updated."},
        {L"message_language_applied", L"The interface language has been reloaded."},
        {L"log_program_ready", L"The program is ready. Enter a name and IP above, then add it to the list."},
        {L"log_entries_saved", L"Name / IP settings were saved."},
        {L"log_startup_enabled", L"Startup was enabled."},
        {L"log_startup_disabled", L"Startup was disabled."},
        {L"log_hide_to_tray", L"The window was minimized to the system tray."},
        {L"log_hosts_imported", L"Server entries were imported into the hosts editor."},
        {L"log_hosts_written", L"The hosts editor content was written to the hosts file."},
        {L"log_language_applied", L"The interface language was reloaded."},
        {L"log_service_started", L"LAN Name Resolver service started."},
        {L"log_nbns_enabled", L"NBNS UDP 137 is enabled."},
        {L"log_nbns_failed_prefix", L"NBNS UDP 137 failed to start. The service will continue with other available resolver modes. Reason: "},
        {L"log_llmnr_enabled", L"LLMNR UDP 5355 is enabled."},
        {L"log_llmnr_failed_prefix", L"LLMNR UDP 5355 failed to start. The service will continue with other available resolver modes. Reason: "},
        {L"log_service_stopped", L"The service was stopped."},
        {L"log_periodic_prefix", L"Periodic announcement finished. Total name/IP entries announced: "},
        {L"log_periodic_suffix", L"."},
        {L"log_nbns_query_prefix", L"Replied to NBNS query: "},
        {L"log_llmnr_query_prefix", L"Replied to LLMNR query: "},
        {L"log_node_status_prefix", L"Replied to Node Status query, source "},
        {L"log_source_label", L", source "},
        {L"tray_show", L"Show Window"},
        {L"tray_exit", L"Exit"}
    };
}

const StringMap& GetBuiltInStrings(const std::wstring& languageCode) {
    static const StringMap kTraditionalChinese = BuildTraditionalChineseStrings();
    static const StringMap kEnglish = BuildEnglishStrings();
    return languageCode == L"en" ? kEnglish : kTraditionalChinese;
}

std::wstring NormalizeLanguageCode(const std::wstring& value) {
    std::wstring code = ToLowerAscii(Trim(value));
    std::replace(code.begin(), code.end(), L'_', L'-');

    if (code.empty()) {
        return L"";
    }
    if (code == L"tw" || code == L"tw.txt" || code == L"zh-tw" || code == L"zh-hk" || code == L"zh-mo" || code == L"zh-hant") {
        return L"tw";
    }
    if (code.rfind(L"zh-", 0) == 0 && code.find(L"hant") != std::wstring::npos) {
        return L"tw";
    }
    if (code == L"en" || code == L"en.txt" || code == L"en-us" || code == L"en-gb") {
        return L"en";
    }
    if (code.size() < 4 || code.substr(code.size() - 4) != L".txt") {
        code += L".txt";
    }
    return code;
}

std::wstring DetectDefaultLanguage() {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
        if (NormalizeLanguageCode(localeName) == L"tw") {
            return L"tw";
        }
    }

    const LANGID languageId = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(languageId) == LANG_CHINESE) {
        switch (SUBLANGID(languageId)) {
        case SUBLANG_CHINESE_TRADITIONAL:
        case SUBLANG_CHINESE_HONGKONG:
        case SUBLANG_CHINESE_MACAU:
            return L"tw";
        default:
            break;
        }
    }

    return L"en";
}

std::wstring GetText(const AppState& app, const wchar_t* key) {
    const auto found = app.strings.find(key);
    if (found != app.strings.end()) {
        return found->second;
    }
    return key;
}

bool LoadCustomLanguageOverrides(const std::filesystem::path& path, StringMap& values) {
    StringMap overrides;
    if (!LoadKeyValueFile(path, overrides)) {
        return false;
    }

    for (const auto& [key, value] : overrides) {
        values[key] = value;
    }
    return true;
}

std::string SerializeLanguageTemplate(const StringMap& values);

bool EnsureBuiltInLanguageFiles() {
    for (const std::wstring& code : {std::wstring(L"tw"), std::wstring(L"en")}) {
        StringMap merged = GetBuiltInStrings(code);
        LoadCustomLanguageOverrides(GetLanguageFilePath(code), merged);
        if (!WriteWholeFileUtf8(GetLanguageFilePath(code), SerializeLanguageTemplate(merged))) {
            return false;
        }
    }
    return true;
}

bool IsLanguageFileCandidate(const std::filesystem::path& path) {
    StringMap values;
    if (!LoadKeyValueFile(path, values)) {
        return false;
    }
    return values.find(L"window_title") != values.end() ||
           values.find(L"tab_server") != values.end() ||
           values.find(L"label_language") != values.end();
}

std::vector<std::wstring> GetAvailableLanguageSelections() {
    EnsureBuiltInLanguageFiles();

    std::vector<std::wstring> result = {L"tw", L"en"};
    WIN32_FIND_DATAW data{};
    const std::wstring pattern = (GetExePath().parent_path() / L"*.txt").wstring();
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return result;
    }

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        const std::wstring fileName = data.cFileName;
        const std::wstring normalized = NormalizeLanguageCode(fileName);
        if (normalized == L"tw" || normalized == L"en") {
            continue;
        }
        if (!IsLanguageFileCandidate(GetExePath().parent_path() / fileName)) {
            continue;
        }
        result.push_back(fileName);
    } while (FindNextFileW(handle, &data));

    FindClose(handle);
    if (result.size() > 2) {
        std::sort(result.begin() + 2, result.end(), [](const std::wstring& left, const std::wstring& right) {
            return ToLowerAscii(left) < ToLowerAscii(right);
        });
    }
    return result;
}

bool LoadLanguageBundle(AppState& app) {
    EnsureBuiltInLanguageFiles();

    app.languageCode = NormalizeLanguageCode(app.languageCode);
    if (app.languageCode.empty()) {
        app.languageCode = DetectDefaultLanguage();
    }

    const std::wstring baseLanguage = (app.languageCode == L"tw") ? L"tw" : L"en";
    app.strings = GetBuiltInStrings(baseLanguage);
    const bool loadedFile = LoadCustomLanguageOverrides(GetLanguageFilePath(app.languageCode), app.strings);
    if (app.languageCode != L"tw" && app.languageCode != L"en" && !loadedFile) {
        app.languageCode = L"en";
    }
    return true;
}

bool SaveAppSettings(const AppState& app) {
    StringMap values;
    values[L"language"] = app.languageCode;
    values[L"minimize_to_tray"] = app.minimizeToTray ? L"1" : L"0";
    return SaveKeyValueFile(app.settingsPath, values);
}

void LoadAppSettings(AppState& app) {
    StringMap values;
    if (!LoadKeyValueFile(app.settingsPath, values)) {
        app.languageCode = DetectDefaultLanguage();
        app.minimizeToTray = true;
        return;
    }

    const auto language = values.find(L"language");
    app.languageCode = language == values.end() ? DetectDefaultLanguage() : NormalizeLanguageCode(language->second);
    if (app.languageCode.empty()) {
        app.languageCode = DetectDefaultLanguage();
    }

    const auto minimize = values.find(L"minimize_to_tray");
    app.minimizeToTray = minimize == values.end() ? true : (minimize->second != L"0");
}

std::string SerializeLanguageTemplate(const StringMap& values) {
    std::ostringstream stream;
    stream << "# Language file for LAN Name Resolver\n";
    stream << "# Edit the values on the right side of '=' and save the file.\n";
    for (const auto& [key, value] : values) {
        stream << WideToUtf8(key) << '=' << WideToUtf8(EscapeConfigValue(value)) << "\n";
    }
    return stream.str();
}

std::vector<uint8_t> EncodeNetbiosName(const std::string& upperName, uint8_t suffix) {
    std::string padded = upperName;
    padded.resize(15, ' ');
    padded.push_back(static_cast<char>(suffix));

    std::vector<uint8_t> encoded;
    encoded.reserve(34);
    encoded.push_back(32);
    for (const unsigned char byte : padded) {
        encoded.push_back(static_cast<uint8_t>('A' + ((byte >> 4) & 0x0F)));
        encoded.push_back(static_cast<uint8_t>('A' + (byte & 0x0F)));
    }
    encoded.push_back(0x00);
    return encoded;
}

bool DecodeFirstLevelLabel(const std::vector<uint8_t>& label, std::string& nameOut, uint8_t& suffixOut) {
    if (label.size() != 32) {
        return false;
    }

    std::string raw;
    raw.reserve(16);
    for (size_t index = 0; index < 32; index += 2) {
        const int high = static_cast<int>(label[index]) - 'A';
        const int low = static_cast<int>(label[index + 1]) - 'A';
        if (high < 0 || high > 0x0F || low < 0 || low > 0x0F) {
            return false;
        }
        raw.push_back(static_cast<char>((high << 4) | low));
    }

    suffixOut = static_cast<uint8_t>(raw[15]);
    raw.resize(15);
    while (!raw.empty() && raw.back() == ' ') {
        raw.pop_back();
    }
    nameOut = raw;
    return true;
}

std::vector<uint8_t> LabelsToWire(const std::vector<std::vector<uint8_t>>& labels) {
    std::vector<uint8_t> wire;
    for (const auto& label : labels) {
        wire.push_back(static_cast<uint8_t>(label.size()));
        wire.insert(wire.end(), label.begin(), label.end());
    }
    wire.push_back(0x00);
    return wire;
}

NameWireResult ReadCompressedName(const std::vector<uint8_t>& packet, size_t startOffset) {
    NameWireResult result;
    std::set<size_t> seenOffsets;
    size_t cursor = startOffset;
    bool jumped = false;
    size_t wireEnd = startOffset;

    while (cursor < packet.size()) {
        const uint8_t length = packet[cursor];
        if (length == 0) {
            if (!jumped) {
                wireEnd = cursor + 1;
            }
            result.endOffset = wireEnd;
            result.wireName = jumped ? LabelsToWire(result.labels) : std::vector<uint8_t>(packet.begin() + startOffset, packet.begin() + wireEnd);
            result.ok = true;
            return result;
        }

        if ((length & 0xC0) == 0xC0) {
            if (cursor + 1 >= packet.size()) {
                return result;
            }
            if (!jumped) {
                wireEnd = cursor + 2;
            }
            const size_t pointer = ((length & 0x3F) << 8) | packet[cursor + 1];
            if (!seenOffsets.insert(pointer).second || pointer >= packet.size()) {
                return result;
            }
            cursor = pointer;
            jumped = true;
            continue;
        }

        if (cursor + 1 + length > packet.size()) {
            return result;
        }

        result.labels.emplace_back(packet.begin() + cursor + 1, packet.begin() + cursor + 1 + length);
        cursor += 1 + length;
    }

    return result;
}

std::wstring DecodeDnsName(const std::vector<std::vector<uint8_t>>& labels) {
    std::wstring output;
    bool first = true;
    for (const auto& label : labels) {
        if (!first) {
            output += L".";
        }
        first = false;
        for (const auto ch : label) {
            output.push_back(static_cast<wchar_t>(ch));
        }
    }
    return output;
}

void Write16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Write32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> BuildNbnsPositiveQueryResponse(uint16_t transactionId, const std::vector<uint8_t>& rrName, uint32_t ipAddress) {
    std::vector<uint8_t> packet;
    packet.reserve(64);
    Write16(packet, transactionId);
    Write16(packet, 0x8580);
    Write16(packet, 0x0000);
    Write16(packet, 0x0001);
    Write16(packet, 0x0000);
    Write16(packet, 0x0000);
    packet.insert(packet.end(), rrName.begin(), rrName.end());
    Write16(packet, kNbType);
    Write16(packet, kInClass);
    Write32(packet, kDefaultTtlSeconds);
    Write16(packet, 0x0006);
    Write16(packet, 0x0000);
    Write32(packet, ntohl(ipAddress));
    return packet;
}

std::vector<uint8_t> BuildLlmnrAResponse(uint16_t transactionId, const std::vector<uint8_t>& question, uint32_t ipAddress) {
    std::vector<uint8_t> packet;
    packet.reserve(64);
    Write16(packet, transactionId);
    Write16(packet, 0x8000);
    Write16(packet, 0x0001);
    Write16(packet, 0x0001);
    Write16(packet, 0x0000);
    Write16(packet, 0x0000);
    packet.insert(packet.end(), question.begin(), question.end());
    Write16(packet, 0xC00C);
    Write16(packet, kDnsAType);
    Write16(packet, kInClass);
    Write32(packet, kLlmnrTtlSeconds);
    Write16(packet, 0x0004);
    Write32(packet, ntohl(ipAddress));
    return packet;
}

std::vector<uint8_t> BuildNodeStatusResponse(uint16_t transactionId, const std::vector<uint8_t>& rrName, const std::vector<ResolverEntry>& entries) {
    std::vector<uint8_t> nodeNameArray;
    nodeNameArray.reserve(entries.size() * 36);
    for (const auto& entry : entries) {
        std::string padded = entry.upperName;
        padded.resize(15, ' ');
        for (char ch : padded) {
            nodeNameArray.push_back(static_cast<uint8_t>(ch));
        }
        nodeNameArray.push_back(0x00);
        Write16(nodeNameArray, 0x0600);

        for (char ch : padded) {
            nodeNameArray.push_back(static_cast<uint8_t>(ch));
        }
        nodeNameArray.push_back(0x20);
        Write16(nodeNameArray, 0x0400);
    }

    std::vector<uint8_t> rdata;
    rdata.push_back(static_cast<uint8_t>(nodeNameArray.size() / 18));
    rdata.insert(rdata.end(), nodeNameArray.begin(), nodeNameArray.end());
    rdata.resize(rdata.size() + 46, 0x00);

    std::vector<uint8_t> packet;
    packet.reserve(128);
    Write16(packet, transactionId);
    Write16(packet, 0x8400);
    Write16(packet, 0x0000);
    Write16(packet, 0x0001);
    Write16(packet, 0x0000);
    Write16(packet, 0x0000);
    packet.insert(packet.end(), rrName.begin(), rrName.end());
    Write16(packet, kNbStatType);
    Write16(packet, kInClass);
    Write32(packet, 0x00000000);
    Write16(packet, static_cast<uint16_t>(rdata.size()));
    packet.insert(packet.end(), rdata.begin(), rdata.end());
    return packet;
}

std::vector<uint8_t> BuildNbnsRegistrationLikePacket(uint16_t flags, const ResolverEntry& entry, uint8_t suffix) {
    const auto questionName = EncodeNetbiosName(entry.upperName, suffix);

    std::vector<uint8_t> packet;
    packet.reserve(80);
    Write16(packet, static_cast<uint16_t>(GetTickCount64() & 0xFFFF));
    Write16(packet, flags);
    Write16(packet, 0x0001);
    Write16(packet, 0x0000);
    Write16(packet, 0x0000);
    Write16(packet, 0x0001);
    packet.insert(packet.end(), questionName.begin(), questionName.end());
    Write16(packet, kNbType);
    Write16(packet, kInClass);
    Write16(packet, 0xC00C);
    Write16(packet, kNbType);
    Write16(packet, kInClass);
    Write32(packet, kDefaultTtlSeconds);
    Write16(packet, 0x0006);
    Write16(packet, 0x0000);
    Write32(packet, ntohl(entry.ipAddress));
    return packet;
}

class ResolverService {
public:
    explicit ResolverService(HWND ownerWindow)
        : ownerWindow_(ownerWindow) {}

    ~ResolverService() {
        Stop();
    }

    bool Start(const std::vector<ResolverEntry>& entries, std::wstring& errorMessage) {
        if (running_.load()) {
            return true;
        }

        errorMessage.clear();
        entries_ = entries;
        stopRequested_.store(false);

        std::wstring nbnsError;
        std::wstring llmnrError;
        nbnsSocket_ = CreateNbnsSocket(nbnsError);
        llmnrSocket_ = CreateLlmnrSocket(llmnrError);
        lastNbnsError_ = nbnsError;
        lastLlmnrError_ = llmnrError;
        nbnsEnabled_ = (nbnsSocket_ != INVALID_SOCKET);
        llmnrEnabled_ = (llmnrSocket_ != INVALID_SOCKET);

        if (nbnsSocket_ == INVALID_SOCKET && llmnrSocket_ == INVALID_SOCKET) {
            errorMessage = L"NBNS 與 LLMNR 都無法啟動。\n\nNBNS: " + nbnsError + L"\nLLMNR: " + llmnrError;
            return false;
        }

        running_.store(true);
        worker_ = std::thread([this]() { WorkerLoop(); });

        PostLog(g_app ? GetText(*g_app, L"log_service_started") : L"LAN Name Resolver service started.");
        if (nbnsSocket_ != INVALID_SOCKET) {
            PostLog(g_app ? GetText(*g_app, L"log_nbns_enabled") : L"NBNS UDP 137 is enabled.");
        } else {
            PostLog((g_app ? GetText(*g_app, L"log_nbns_failed_prefix") : L"NBNS UDP 137 failed to start. Reason: ") + nbnsError);
        }
        if (llmnrSocket_ != INVALID_SOCKET) {
            PostLog(g_app ? GetText(*g_app, L"log_llmnr_enabled") : L"LLMNR UDP 5355 is enabled.");
        } else {
            PostLog((g_app ? GetText(*g_app, L"log_llmnr_failed_prefix") : L"LLMNR UDP 5355 failed to start. Reason: ") + llmnrError);
        }
        PostStatus(L"執行中");
        return true;
    }

    void Stop() {
        if (!running_.load() && !worker_.joinable()) {
            return;
        }

        stopRequested_.store(true);
        CloseSockets();

        if (worker_.joinable()) {
            worker_.join();
        }

        running_.store(false);
        nbnsEnabled_ = false;
        llmnrEnabled_ = false;
        PostLog(g_app ? GetText(*g_app, L"log_service_stopped") : L"The service was stopped.");
        PostStatus(L"已停止");
    }

    bool IsRunning() const {
        return running_.load();
    }

    bool HasNbnsBinding() const {
        return nbnsEnabled_;
    }

    bool HasLlmnrBinding() const {
        return llmnrEnabled_;
    }

    const std::wstring& LastNbnsError() const {
        return lastNbnsError_;
    }

    const std::wstring& LastLlmnrError() const {
        return lastLlmnrError_;
    }

private:
    SOCKET CreateNbnsSocket(std::wstring& error) {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            error = FormatErrorMessage(WSAGetLastError());
            return INVALID_SOCKET;
        }

        const BOOL reuse = TRUE;
        const BOOL broadcast = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(kNbnsPort);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            error = FormatErrorMessage(WSAGetLastError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        return sock;
    }

    SOCKET CreateLlmnrSocket(std::wstring& error) {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            error = FormatErrorMessage(WSAGetLastError());
            return INVALID_SOCKET;
        }

        const BOOL reuse = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(kLlmnrPort);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            error = FormatErrorMessage(WSAGetLastError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        ip_mreq membership{};
        membership.imr_multiaddr.s_addr = inet_addr("224.0.0.252");
        membership.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&membership), sizeof(membership)) == SOCKET_ERROR) {
            error = FormatErrorMessage(WSAGetLastError());
            closesocket(sock);
            return INVALID_SOCKET;
        }

        return sock;
    }

    void CloseSockets() {
        if (nbnsSocket_ != INVALID_SOCKET) {
            closesocket(nbnsSocket_);
            nbnsSocket_ = INVALID_SOCKET;
        }
        if (llmnrSocket_ != INVALID_SOCKET) {
            closesocket(llmnrSocket_);
            llmnrSocket_ = INVALID_SOCKET;
        }
    }

    void WorkerLoop() {
        auto nextAnnouncement = std::chrono::steady_clock::now();

        while (!stopRequested_.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (nbnsSocket_ != INVALID_SOCKET && now >= nextAnnouncement) {
                SendAnnouncements();
                nextAnnouncement = now + std::chrono::seconds(kAnnounceIntervalSeconds);
            }

            fd_set readSet{};
            FD_ZERO(&readSet);
            SOCKET maxSocket = 0;
            int socketCount = 0;

            if (nbnsSocket_ != INVALID_SOCKET) {
                FD_SET(nbnsSocket_, &readSet);
                maxSocket = std::max(maxSocket, nbnsSocket_);
                ++socketCount;
            }
            if (llmnrSocket_ != INVALID_SOCKET) {
                FD_SET(llmnrSocket_, &readSet);
                maxSocket = std::max(maxSocket, llmnrSocket_);
                ++socketCount;
            }

            if (socketCount == 0) {
                break;
            }

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;
            const int ready = select(static_cast<int>(maxSocket) + 1, &readSet, nullptr, nullptr, &timeout);
            if (ready == SOCKET_ERROR) {
                break;
            }
            if (ready == 0) {
                continue;
            }

            if (nbnsSocket_ != INVALID_SOCKET && FD_ISSET(nbnsSocket_, &readSet)) {
                HandleNbnsSocket();
            }
            if (llmnrSocket_ != INVALID_SOCKET && FD_ISSET(llmnrSocket_, &readSet)) {
                HandleLlmnrSocket();
            }
        }

        CloseSockets();
        running_.store(false);
    }

    void SendAnnouncements() {
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(kNbnsPort);
        target.sin_addr.s_addr = INADDR_BROADCAST;

        for (const auto& entry : entries_) {
            for (const auto suffix : {static_cast<uint8_t>(0x00), static_cast<uint8_t>(0x20)}) {
                const auto registration = BuildNbnsRegistrationLikePacket(0x2910, entry, suffix);
                sendto(nbnsSocket_, reinterpret_cast<const char*>(registration.data()), static_cast<int>(registration.size()), 0,
                       reinterpret_cast<sockaddr*>(&target), sizeof(target));

                const auto refresh = BuildNbnsRegistrationLikePacket(0x4810, entry, suffix);
                sendto(nbnsSocket_, reinterpret_cast<const char*>(refresh.data()), static_cast<int>(refresh.size()), 0,
                       reinterpret_cast<sockaddr*>(&target), sizeof(target));
            }
        }

        PostLog((g_app ? GetText(*g_app, L"log_periodic_prefix") : L"Periodic announcement finished. Total entries: ") +
                std::to_wstring(entries_.size()) +
                (g_app ? GetText(*g_app, L"log_periodic_suffix") : L"."));
    }

    const ResolverEntry* FindEntry(const std::string& upperName) const {
        for (const auto& entry : entries_) {
            if (entry.upperName == upperName) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::wstring SuffixHex(uint8_t suffix) const {
        wchar_t buffer[8] = {};
        wsprintfW(buffer, L"%02X", suffix);
        return buffer;
    }

    void HandleNbnsSocket() {
        std::vector<uint8_t> packet(4096);
        sockaddr_in remote{};
        int remoteLength = sizeof(remote);
        const int received = recvfrom(nbnsSocket_, reinterpret_cast<char*>(packet.data()), static_cast<int>(packet.size()), 0,
                                      reinterpret_cast<sockaddr*>(&remote), &remoteLength);
        if (received <= 0) {
            return;
        }
        packet.resize(received);
        if (packet.size() < 12) {
            return;
        }

        const uint16_t flags = (packet[2] << 8) | packet[3];
        const uint16_t qdcount = (packet[4] << 8) | packet[5];
        const bool isResponse = (flags & 0x8000) != 0;
        const uint16_t opcode = (flags >> 11) & 0x0F;
        if (isResponse || qdcount < 1 || opcode != 0) {
            return;
        }

        const auto nameResult = ReadCompressedName(packet, 12);
        if (!nameResult.ok || nameResult.endOffset + 4 > packet.size() || nameResult.labels.empty()) {
            return;
        }

        const uint16_t qtype = (packet[nameResult.endOffset] << 8) | packet[nameResult.endOffset + 1];
        const uint16_t qclass = (packet[nameResult.endOffset + 2] << 8) | packet[nameResult.endOffset + 3];
        if (qclass != kInClass) {
            return;
        }

        std::string queriedName;
        uint8_t queriedSuffix = 0;
        if (!DecodeFirstLevelLabel(nameResult.labels.front(), queriedName, queriedSuffix)) {
            return;
        }

        const std::wstring sourceIp = Utf8ToWide(inet_ntoa(remote.sin_addr));
        const auto* entry = FindEntry(queriedName);

        if (qtype == kNbType && entry != nullptr) {
            const uint16_t transactionId = (packet[0] << 8) | packet[1];
            const auto response = BuildNbnsPositiveQueryResponse(transactionId, nameResult.wireName, entry->ipAddress);
            sendto(nbnsSocket_, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0,
                   reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
            PostLog((g_app ? GetText(*g_app, L"log_nbns_query_prefix") : L"Replied to NBNS query: ") +
                    entry->displayName + L" <" + SuffixHex(queriedSuffix) + L"> -> " +
                    entry->ipText + (g_app ? GetText(*g_app, L"log_source_label") : L", source ") +
                    sourceIp + L":" + std::to_wstring(ntohs(remote.sin_port)));
            return;
        }

        if (qtype == kNbStatType) {
            std::vector<ResolverEntry> responseEntries;
            if (queriedName.empty() || queriedName == "*") {
                responseEntries = entries_;
            } else if (entry != nullptr) {
                responseEntries.push_back(*entry);
            }

            if (!responseEntries.empty()) {
                const uint16_t transactionId = (packet[0] << 8) | packet[1];
                const auto response = BuildNodeStatusResponse(transactionId, nameResult.wireName, responseEntries);
                sendto(nbnsSocket_, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0,
                       reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
                PostLog((g_app ? GetText(*g_app, L"log_node_status_prefix") : L"Replied to Node Status query, source ") +
                        sourceIp + L":" + std::to_wstring(ntohs(remote.sin_port)));
            }
        }
    }

    void HandleLlmnrSocket() {
        std::vector<uint8_t> packet(4096);
        sockaddr_in remote{};
        int remoteLength = sizeof(remote);
        const int received = recvfrom(llmnrSocket_, reinterpret_cast<char*>(packet.data()), static_cast<int>(packet.size()), 0,
                                      reinterpret_cast<sockaddr*>(&remote), &remoteLength);
        if (received <= 0) {
            return;
        }
        packet.resize(received);
        if (packet.size() < 12) {
            return;
        }

        const uint16_t flags = (packet[2] << 8) | packet[3];
        const uint16_t qdcount = (packet[4] << 8) | packet[5];
        const uint16_t ancount = (packet[6] << 8) | packet[7];
        const uint16_t nscount = (packet[8] << 8) | packet[9];
        const bool isResponse = (flags & 0x8000) != 0;
        const bool conflict = (flags & 0x0400) != 0;
        const uint16_t opcode = (flags >> 11) & 0x0F;
        if (isResponse || conflict || opcode != 0 || qdcount != 1 || ancount != 0 || nscount != 0) {
            return;
        }

        const auto nameResult = ReadCompressedName(packet, 12);
        if (!nameResult.ok || nameResult.endOffset + 4 > packet.size() || nameResult.labels.empty()) {
            return;
        }

        const uint16_t qtype = (packet[nameResult.endOffset] << 8) | packet[nameResult.endOffset + 1];
        const uint16_t qclass = (packet[nameResult.endOffset + 2] << 8) | packet[nameResult.endOffset + 3];
        if (qclass != kInClass || (qtype != kDnsAType && qtype != kDnsAnyType)) {
            return;
        }

        const std::wstring queryName = DecodeDnsName(nameResult.labels);
        if (queryName.empty() || queryName.find(L'.') != std::wstring::npos) {
            return;
        }

        const auto normalized = WideToUtf8(ToUpperAscii(queryName));
        const auto* entry = FindEntry(normalized);
        if (entry == nullptr) {
            return;
        }

        std::vector<uint8_t> question = nameResult.wireName;
        Write16(question, qtype);
        Write16(question, qclass);

        const uint16_t transactionId = (packet[0] << 8) | packet[1];
        const auto response = BuildLlmnrAResponse(transactionId, question, entry->ipAddress);
        sendto(llmnrSocket_, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0,
               reinterpret_cast<sockaddr*>(&remote), sizeof(remote));

        const std::wstring sourceIp = Utf8ToWide(inet_ntoa(remote.sin_addr));
        PostLog((g_app ? GetText(*g_app, L"log_llmnr_query_prefix") : L"Replied to LLMNR query: ") +
                queryName + L" -> " + entry->ipText +
                (g_app ? GetText(*g_app, L"log_source_label") : L", source ") +
                sourceIp + L":" + std::to_wstring(ntohs(remote.sin_port)));
    }

    void PostLog(const std::wstring& message) const {
        auto* payload = new std::wstring(message);
        PostMessageW(ownerWindow_, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(payload));
    }

    void PostStatus(const std::wstring& status) const {
        auto* payload = new std::wstring(status);
        PostMessageW(ownerWindow_, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(payload));
    }

    HWND ownerWindow_ = nullptr;
    std::vector<ResolverEntry> entries_;
    std::atomic<bool> running_ = false;
    std::atomic<bool> stopRequested_ = false;
    std::thread worker_;
    SOCKET nbnsSocket_ = INVALID_SOCKET;
    SOCKET llmnrSocket_ = INVALID_SOCKET;
    bool nbnsEnabled_ = false;
    bool llmnrEnabled_ = false;
    std::wstring lastNbnsError_;
    std::wstring lastLlmnrError_;
};

HFONT CreateAppFont(int height, int weight = FW_NORMAL, bool mono = false, bool underline = false) {
    return CreateFontW(
        -height,
        0,
        0,
        0,
        weight,
        FALSE,
        underline ? TRUE : FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        mono ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_DONTCARE,
        mono ? L"Consolas" : L"Microsoft JhengHei UI");
}

void SendFont(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

std::wstring GetWindowTextString(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length + 1), L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

std::wstring GetListViewText(HWND listView, int itemIndex, int subItem) {
    wchar_t buffer[256] = {};
    ListView_GetItemText(listView, itemIndex, subItem, buffer, ARRAYSIZE(buffer));
    return buffer;
}

void ClearEntryInputs(AppState& app) {
    SetWindowTextW(app.nameInput, L"");
    SetWindowTextW(app.ipInput, L"");
    SetFocus(app.nameInput);
}

int FindEntryByName(HWND listView, const std::wstring& normalizedName) {
    const int itemCount = ListView_GetItemCount(listView);
    for (int index = 0; index < itemCount; ++index) {
        if (ToUpperAscii(Trim(GetListViewText(listView, index, 0))) == normalizedName) {
            return index;
        }
    }
    return -1;
}

void AddEntryToList(AppState& app, const std::wstring& name, const std::wstring& ip) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(app.entryList);
    item.iSubItem = 0;
    item.pszText = const_cast<LPWSTR>(name.c_str());
    const int insertedIndex = ListView_InsertItem(app.entryList, &item);
    ListView_SetItemText(app.entryList, insertedIndex, 1, const_cast<LPWSTR>(ip.c_str()));
    ListView_SetItemState(app.entryList, insertedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(app.entryList, insertedIndex, FALSE);
}

void AppendLog(AppState& app, const std::wstring& message) {
    const std::wstring line = L"[" + FormatTimeNow() + L"] " + message + L"\r\n";
    const int textLength = GetWindowTextLengthW(app.logEdit);
    SendMessageW(app.logEdit, EM_SETSEL, textLength, textLength);
    SendMessageW(app.logEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(app.logEdit, EM_SCROLLCARET, 0, 0);
}

void DrawActionButton(const DRAWITEMSTRUCT* drawInfo, COLORREF fillColor, COLORREF borderColor, COLORREF textColor) {
    HDC dc = drawInfo->hDC;
    RECT rect = drawInfo->rcItem;
    const bool disabled = (drawInfo->itemState & ODS_DISABLED) != 0;
    const bool pressed = (drawInfo->itemState & ODS_SELECTED) != 0;

    COLORREF actualFill = disabled ? RGB(239, 239, 239) : fillColor;
    COLORREF actualBorder = disabled ? RGB(180, 180, 180) : borderColor;
    COLORREF actualText = disabled ? RGB(155, 155, 155) : textColor;

    if (pressed && !disabled) {
        actualFill = RGB(
            static_cast<BYTE>(GetRValue(fillColor) * 0.90),
            static_cast<BYTE>(GetGValue(fillColor) * 0.90),
            static_cast<BYTE>(GetBValue(fillColor) * 0.90));
    }

    HBRUSH background = CreateSolidBrush(actualFill);
    FillRect(dc, &rect, background);
    DeleteObject(background);

    HPEN borderPen = CreatePen(PS_SOLID, 1, actualBorder);
    HGDIOBJ oldPen = SelectObject(dc, borderPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(borderPen);

    wchar_t text[256] = {};
    GetWindowTextW(drawInfo->hwndItem, text, ARRAYSIZE(text));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, actualText);
    DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (drawInfo->itemState & ODS_FOCUS) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -4, -4);
        DrawFocusRect(dc, &focusRect);
    }
}

int CountConfiguredEntries(AppState& app) {
    return ListView_GetItemCount(app.entryList);
}

bool ContainsInsensitive(const std::wstring& text, const std::wstring& pattern) {
    return ToLowerAscii(text).find(ToLowerAscii(pattern)) != std::wstring::npos;
}

bool LooksLikePortOccupied(const std::wstring& errorText) {
    if (errorText.empty()) {
        return false;
    }

    return errorText.find(L"\u5b58\u53d6\u6b0a\u9650\u4e0d\u8db3") != std::wstring::npos ||
           errorText.find(L"\u88ab\u62d2\u7d55") != std::wstring::npos ||
           errorText.find(L"\u4f7f\u7528\u4e2d") != std::wstring::npos ||
           ContainsInsensitive(errorText, L"access is denied") ||
           ContainsInsensitive(errorText, L"forbidden by its access permissions") ||
           ContainsInsensitive(errorText, L"address already in use");
}

std::wstring BuildProtocolStatusLine(AppState& app, bool forNbns) {
    if (!app.serviceRunning || !app.service) {
        return GetText(app, forNbns ? L"service_nbns_waiting" : L"service_llmnr_waiting");
    }

    if (forNbns) {
        return app.service->HasNbnsBinding()
            ? GetText(app, L"service_nbns_on")
            : GetText(app, L"service_nbns_failed");
    }

    if (app.service->HasLlmnrBinding()) {
        return GetText(app, L"service_llmnr_on");
    }

    if (LooksLikePortOccupied(app.service->LastLlmnrError())) {
        return GetText(app, L"service_llmnr_blocked");
    }

    return GetText(app, L"service_llmnr_failed");
}

std::wstring BuildServiceSummaryText(AppState& app) {
    return GetText(app, L"service_count_prefix") + std::to_wstring(CountConfiguredEntries(app));
}

void UpdateStatusVisuals(AppState& app) {
    SetWindowTextW(app.statusText, GetText(app, app.serviceRunning ? L"status_running" : L"status_stopped").c_str());
    SetWindowTextW(app.startupStatusText, GetText(app, app.startupEnabled ? L"startup_enabled" : L"startup_disabled").c_str());
}

void RefreshServiceSummary(AppState& app) {
    const std::wstring detail = BuildServiceSummaryText(app);
    SetWindowTextW(app.serviceDetailText, detail.c_str());
}

void UpdateStartupStatus(AppState& app) {
    app.startupEnabled = false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        const LONG status = RegQueryValueExW(key, kStartupValueName, nullptr, &type, nullptr, &size);
        RegCloseKey(key);
        app.startupEnabled = status == ERROR_SUCCESS && type == REG_SZ && size > sizeof(wchar_t);
    }

    UpdateStatusVisuals(app);
    SetWindowTextW(app.startupDetailText, GetText(app, app.startupEnabled ? L"startup_detail_enabled" : L"startup_detail_disabled").c_str());
    EnableWindow(app.enableStartupButton, app.startupEnabled ? FALSE : TRUE);
    EnableWindow(app.disableStartupButton, app.startupEnabled ? TRUE : FALSE);
    InvalidateRect(app.startupStatusText, nullptr, TRUE);
    InvalidateRect(app.enableStartupButton, nullptr, TRUE);
    InvalidateRect(app.disableStartupButton, nullptr, TRUE);
}

void ApplyStatusVisuals(AppState& app) {
    SetWindowTextW(app.statusText, GetText(app, app.serviceRunning ? L"status_running" : L"status_stopped").c_str());
    SetWindowTextW(app.startupStatusText, GetText(app, app.startupEnabled ? L"startup_enabled" : L"startup_disabled").c_str());
}

void ApplyServiceSummary(AppState& app) {
    const std::wstring detail = BuildServiceSummaryText(app);
    SetWindowTextW(app.serviceDetailText, detail.c_str());
}

void RefreshStartupState(AppState& app) {
    app.startupEnabled = false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        const LONG status = RegQueryValueExW(key, kStartupValueName, nullptr, &type, nullptr, &size);
        RegCloseKey(key);
        app.startupEnabled = status == ERROR_SUCCESS && type == REG_SZ && size > sizeof(wchar_t);
    }

    ApplyStatusVisuals(app);
    SetWindowTextW(app.startupDetailText, GetText(app, app.startupEnabled ? L"startup_detail_enabled" : L"startup_detail_disabled").c_str());
    EnableWindow(app.enableStartupButton, app.startupEnabled ? FALSE : TRUE);
    EnableWindow(app.disableStartupButton, app.startupEnabled ? TRUE : FALSE);
    InvalidateRect(app.startupStatusText, nullptr, TRUE);
    InvalidateRect(app.enableStartupButton, nullptr, TRUE);
    InvalidateRect(app.disableStartupButton, nullptr, TRUE);
}

bool SaveListEntriesToConfig(AppState& app) {
    std::ostringstream stream;
    stream << "# LAN Name Resolver entries\n";
    const int itemCount = ListView_GetItemCount(app.entryList);
    for (int index = 0; index < itemCount; ++index) {
        stream << WideToUtf8(Trim(GetListViewText(app.entryList, index, 0))) << '\t'
               << WideToUtf8(Trim(GetListViewText(app.entryList, index, 1))) << "\n";
    }
    return WriteWholeFileUtf8(app.configPath, stream.str());
}

void LoadListEntriesFromConfig(AppState& app) {
    std::string content;
    if (!ReadWholeFileUtf8(app.configPath, content)) {
        return;
    }

    ListView_DeleteAllItems(app.entryList);

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto tabPos = line.find('\t');
        const std::wstring name = Utf8ToWide(tabPos == std::string::npos ? line : line.substr(0, tabPos));
        const std::wstring ip = Utf8ToWide(tabPos == std::string::npos ? std::string() : line.substr(tabPos + 1));
        if (Trim(name).empty() && Trim(ip).empty()) {
            continue;
        }
        AddEntryToList(app, Trim(name), Trim(ip));
    }
}

void ShowControls(const std::initializer_list<HWND>& controls, bool show) {
    for (HWND control : controls) {
        if (control) {
            ShowWindow(control, show ? SW_SHOW : SW_HIDE);
        }
    }
}

int MeasureTabTextWidth(HWND window, HFONT font, const std::wstring& text) {
    HDC dc = GetDC(window);
    if (!dc) {
        return 0;
    }
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, oldFont);
    ReleaseDC(window, dc);
    return size.cx;
}

void ResizeTabControlToFit(AppState& app) {
    const std::wstring serverText = GetText(app, L"tab_server");
    const std::wstring hostsText = GetText(app, L"tab_hosts");
    const std::wstring settingsText = GetText(app, L"tab_settings");
    const std::wstring aboutText = GetText(app, L"tab_about");

    const int tabPadding = 36;
    const int estimatedWidth =
        MeasureTabTextWidth(app.window, app.tabFont ? app.tabFont : app.font, serverText) + tabPadding +
        MeasureTabTextWidth(app.window, app.tabFont ? app.tabFont : app.font, hostsText) + tabPadding +
        MeasureTabTextWidth(app.window, app.tabFont ? app.tabFont : app.font, settingsText) + tabPadding +
        MeasureTabTextWidth(app.window, app.tabFont ? app.tabFont : app.font, aboutText) + tabPadding + 8;

    MoveWindow(app.tabControl, 16, 8, estimatedWidth, 34, TRUE);

    RECT lastTabRect{};
    const int itemCount = TabCtrl_GetItemCount(app.tabControl);
    if (itemCount > 0 && TabCtrl_GetItemRect(app.tabControl, itemCount - 1, &lastTabRect)) {
        const int snugWidth = lastTabRect.right + 6;
        if (snugWidth > 0) {
            MoveWindow(app.tabControl, 16, 8, snugWidth, 34, TRUE);
        }
    }
}

std::wstring BuildAboutBodyText(const AppState& app) {
    std::wstring body;
    body += GetText(app, L"about_version");
    body += L"\r\n\r\n";
    body += GetText(app, L"about_copyright");
    body += L"\r\n\r\n";
    body += GetText(app, L"about_description");
    body += L"\r\n\r\n";
    body += GetText(app, L"about_github_label");
    return body;
}

void PopulateLanguageCombo(AppState& app) {
    app.languageCodes = GetAvailableLanguageSelections();
    SendMessageW(app.languageCombo, CB_RESETCONTENT, 0, 0);

    for (const std::wstring& code : app.languageCodes) {
        std::wstring label;
        if (code == L"tw") {
            label = GetText(app, L"language_option_tw");
        } else if (code == L"en") {
            label = GetText(app, L"language_option_en");
        } else {
            label = code;
        }
        SendMessageW(app.languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }

    int selectedIndex = 0;
    for (std::size_t index = 0; index < app.languageCodes.size(); ++index) {
        if (NormalizeLanguageCode(app.languageCodes[index]) == NormalizeLanguageCode(app.languageCode)) {
            selectedIndex = static_cast<int>(index);
            break;
        }
    }
    SendMessageW(app.languageCombo, CB_SETCURSEL, selectedIndex, 0);
}

void UpdateTabTitles(AppState& app) {
    const std::wstring serverText = GetText(app, L"tab_server");
    const std::wstring hostsText = GetText(app, L"tab_hosts");
    const std::wstring settingsText = GetText(app, L"tab_settings");
    const std::wstring aboutText = GetText(app, L"tab_about");

    TCITEMW item{};
    item.mask = TCIF_TEXT;

    item.pszText = const_cast<LPWSTR>(serverText.c_str());
    TabCtrl_SetItem(app.tabControl, 0, &item);

    item.pszText = const_cast<LPWSTR>(hostsText.c_str());
    TabCtrl_SetItem(app.tabControl, 1, &item);

    item.pszText = const_cast<LPWSTR>(settingsText.c_str());
    TabCtrl_SetItem(app.tabControl, 2, &item);

    item.pszText = const_cast<LPWSTR>(aboutText.c_str());
    TabCtrl_SetItem(app.tabControl, 3, &item);
}

void UpdatePageVisibility(AppState& app) {
    const bool showServer = (app.activeTab == 0);
    const bool showHosts = (app.activeTab == 1);
    const bool showSettings = (app.activeTab == 2);
    const bool showAbout = (app.activeTab == 3);

    ShowControls(
        {app.leftGroup, app.serviceGroup, app.startupGroup, app.logGroup, app.leftIntro, app.nameHeader, app.ipHeader,
         app.nameInput, app.ipInput, app.addButton, app.removeButton, app.saveButton, app.entryList, app.infoText,
         app.statusText, app.serviceDetailText, app.hintText, app.startButton, app.stopButton, app.startupStatusText,
         app.startupDetailText, app.enableStartupButton, app.disableStartupButton, app.logEdit},
        showServer);

    ShowControls(
        {app.hostsGroup, app.hostsIntro, app.hostsEdit, app.writeHostsButton, app.openHostsButton, app.hostsFormatText, app.hostsNoteText},
        showHosts);

    ShowControls(
        {app.settingsAppGroup, app.minimizeToTrayCheck, app.settingsLanguageGroup, app.languageLabel, app.languageCombo,
         app.customLanguageHintText},
        showSettings);

    ShowControls(
        {app.settingsAboutGroup, app.aboutTitleText, app.aboutBodyText, app.aboutGithubLink},
        showAbout);
}

void ApplyLocalizedText(AppState& app) {
    SetWindowTextW(app.window, GetText(app, L"window_title").c_str());
    UpdateTabTitles(app);

    SetWindowTextW(app.leftGroup, GetText(app, L"group_name_ip").c_str());
    SetWindowTextW(app.serviceGroup, GetText(app, L"group_service").c_str());
    SetWindowTextW(app.startupGroup, GetText(app, L"group_startup").c_str());
    SetWindowTextW(app.logGroup, GetText(app, L"group_log").c_str());
    SetWindowTextW(app.leftIntro, GetText(app, L"server_intro").c_str());
    SetWindowTextW(app.nameHeader, GetText(app, L"label_name").c_str());
    SetWindowTextW(app.ipHeader, GetText(app, L"label_ip").c_str());
    SetWindowTextW(app.addButton, GetText(app, L"button_add").c_str());
    SetWindowTextW(app.removeButton, GetText(app, L"button_remove").c_str());
    SetWindowTextW(app.saveButton, GetText(app, L"button_save").c_str());
    SetWindowTextW(app.infoText, GetText(app, L"server_info").c_str());
    SetWindowTextW(app.startButton, GetText(app, L"button_start").c_str());
    SetWindowTextW(app.stopButton, GetText(app, L"button_stop").c_str());
    SetWindowTextW(app.hintText, GetText(app, L"service_hint").c_str());
    SetWindowTextW(app.enableStartupButton, GetText(app, L"button_enable_startup").c_str());
    SetWindowTextW(app.disableStartupButton, GetText(app, L"button_disable_startup").c_str());

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT;
    std::wstring nameColumn = GetText(app, L"label_name");
    std::wstring ipColumn = GetText(app, L"label_ip");
    column.pszText = const_cast<LPWSTR>(nameColumn.c_str());
    ListView_SetColumn(app.entryList, 0, &column);
    column.pszText = const_cast<LPWSTR>(ipColumn.c_str());
    ListView_SetColumn(app.entryList, 1, &column);

    SetWindowTextW(app.hostsGroup, GetText(app, L"hosts_group").c_str());
    SetWindowTextW(app.hostsIntro, GetText(app, L"hosts_intro").c_str());
    SetWindowTextW(app.writeHostsButton, GetText(app, L"button_write_hosts").c_str());
    SetWindowTextW(app.openHostsButton, GetText(app, L"button_open_hosts").c_str());
    SetWindowTextW(app.hostsFormatText, GetText(app, L"hosts_format").c_str());
    SetWindowTextW(app.hostsNoteText, GetText(app, app.isElevated ? L"hosts_note_admin_ok" : L"hosts_note_admin").c_str());

    SetWindowTextW(app.settingsAppGroup, GetText(app, L"group_app_settings").c_str());
    SetWindowTextW(app.minimizeToTrayCheck, GetText(app, L"check_minimize_to_tray").c_str());
    SetWindowTextW(app.settingsLanguageGroup, GetText(app, L"group_language").c_str());
    SetWindowTextW(app.languageLabel, GetText(app, L"label_language").c_str());
    SetWindowTextW(app.customLanguageHintText, GetText(app, L"custom_language_hint").c_str());
    SetWindowTextW(app.settingsAboutGroup, GetText(app, L"group_about").c_str());
    SetWindowTextW(app.aboutTitleText, GetText(app, L"about_title").c_str());
    SetWindowTextW(app.aboutBodyText, BuildAboutBodyText(app).c_str());
    SetWindowTextW(app.aboutGithubLink, kAboutGithubUrl);

    ResizeTabControlToFit(app);
    PopulateLanguageCombo(app);
    SendMessageW(app.minimizeToTrayCheck, BM_SETCHECK, app.minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
    ApplyStatusVisuals(app);
    ApplyServiceSummary(app);
    RefreshStartupState(app);
    UpdatePageVisibility(app);
}

std::wstring BuildHostsDraftFromServerEntries(const AppState& app) {
    std::wstring text;
    const int itemCount = ListView_GetItemCount(app.entryList);
    for (int index = 0; index < itemCount; ++index) {
        const std::wstring name = Trim(GetListViewText(app.entryList, index, 0));
        const std::wstring ip = Trim(GetListViewText(app.entryList, index, 1));
        if (name.empty() || ip.empty()) {
            continue;
        }
        text += ip + L" " + name + L"\r\n";
    }
    return text;
}

void ImportServerEntriesToHostsEditor(AppState& app) {
    const std::wstring text = BuildHostsDraftFromServerEntries(app);
    if (Trim(text).empty()) {
        MessageBoxW(app.window, GetText(app, L"message_no_server_entries").c_str(), GetText(app, L"title_info").c_str(), MB_ICONINFORMATION);
        return;
    }

    SetWindowTextW(app.hostsEdit, text.c_str());
    AppendLog(app, GetText(app, L"log_hosts_imported"));
}

bool NormalizeHostsEditorText(const std::wstring& input, std::wstring& normalized, std::wstring& error) {
    normalized.clear();
    error.clear();

    std::wistringstream stream(input);
    std::wstring line;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }

        const std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == L'#') {
            continue;
        }

        std::wistringstream tokenStream(trimmed);
        std::vector<std::wstring> tokens;
        std::wstring token;
        while (tokenStream >> token) {
            tokens.push_back(token);
        }

        if (tokens.size() < 2) {
            error = GetText(*g_app, L"message_hosts_line_prefix") + std::to_wstring(lineNumber) +
                    GetText(*g_app, L"message_hosts_line_invalid_format_suffix");
            return false;
        }

        std::wstring ip;
        std::vector<std::wstring> names;
        IN_ADDR address{};
        if (InetPtonW(AF_INET, tokens.front().c_str(), &address) == 1) {
            ip = tokens.front();
            names.assign(tokens.begin() + 1, tokens.end());
        } else if (InetPtonW(AF_INET, tokens.back().c_str(), &address) == 1) {
            ip = tokens.back();
            names.assign(tokens.begin(), tokens.end() - 1);
        } else {
            error = GetText(*g_app, L"message_hosts_line_prefix") + std::to_wstring(lineNumber) +
                    GetText(*g_app, L"message_hosts_line_missing_ip_suffix");
            return false;
        }

        if (names.empty()) {
            error = GetText(*g_app, L"message_hosts_line_prefix") + std::to_wstring(lineNumber) +
                    GetText(*g_app, L"message_hosts_line_missing_name_suffix");
            return false;
        }

        normalized += ip;
        for (const std::wstring& name : names) {
            normalized += L" " + name;
        }
        normalized += L"\r\n";
    }

    return true;
}

bool WriteHostsEditorToSystemHosts(AppState& app) {
    if (!app.isElevated) {
        MessageBoxW(app.window, GetText(app, L"message_admin_required").c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
        return false;
    }

    const std::wstring draft = GetWindowTextString(app.hostsEdit);
    std::wstring normalized;
    std::wstring error;
    if (!NormalizeHostsEditorText(draft, normalized, error)) {
        MessageBoxW(app.window, error.c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
        return false;
    }

    std::string currentContent;
    if (!ReadWholeFileUtf8(GetSystemHostsPath(), currentContent)) {
        MessageBoxW(app.window, GetText(app, L"message_hosts_open_failed").c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
        return false;
    }

    std::wstring hosts = Utf8ToWide(currentContent);
    hosts.erase(std::remove(hosts.begin(), hosts.end(), L'\r'), hosts.end());

    const std::wstring beginMarker = kHostsManagedBlockBegin;
    const std::wstring endMarker = kHostsManagedBlockEnd;
    const std::size_t beginPos = hosts.find(beginMarker);
    if (beginPos != std::wstring::npos) {
        const std::size_t endPos = hosts.find(endMarker, beginPos);
        if (endPos != std::wstring::npos) {
            std::size_t eraseEnd = endPos + endMarker.size();
            while (eraseEnd < hosts.size() && (hosts[eraseEnd] == L'\n' || hosts[eraseEnd] == L' ' || hosts[eraseEnd] == L'\t')) {
                ++eraseEnd;
            }
            hosts.erase(beginPos, eraseEnd - beginPos);
        }
    }

    while (!hosts.empty() && (hosts.back() == L'\n' || hosts.back() == L' ' || hosts.back() == L'\t')) {
        hosts.pop_back();
    }

    if (!normalized.empty()) {
        if (!hosts.empty()) {
            hosts += L"\n\n";
        }
        hosts += beginMarker + L"\n" + normalized + endMarker + L"\n";
    } else if (!hosts.empty()) {
        hosts += L"\n";
    }

    std::wstring finalText;
    finalText.reserve(hosts.size() + 16);
    for (const wchar_t ch : hosts) {
        if (ch == L'\n') {
            finalText += L"\r\n";
        } else {
            finalText.push_back(ch);
        }
    }

    if (!WriteWholeFileUtf8(GetSystemHostsPath(), WideToUtf8(finalText))) {
        MessageBoxW(app.window, GetText(app, L"message_hosts_open_failed").c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
        return false;
    }

    AppendLog(app, GetText(app, L"log_hosts_written"));
    MessageBoxW(app.window, GetText(app, L"message_hosts_written").c_str(), GetText(app, L"title_info").c_str(), MB_ICONINFORMATION);
    return true;
}

bool OpenTextFileInNotepad(const std::filesystem::path& path, bool elevate) {
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        elevate ? L"runas" : L"open",
        L"notepad.exe",
        QuoteForCommandLine(path.wstring()).c_str(),
        nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

void OpenSystemHostsFile(AppState& app) {
    if (!OpenTextFileInNotepad(GetSystemHostsPath(), !app.isElevated)) {
        MessageBoxW(app.window, GetText(app, L"message_hosts_open_failed").c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
    }
}

void ApplyLanguageSelection(AppState& app, const std::wstring& languageCode, bool writeLog) {
    app.languageCode = NormalizeLanguageCode(languageCode);
    if (app.languageCode.empty()) {
        app.languageCode = DetectDefaultLanguage();
    }
    LoadLanguageBundle(app);
    ApplyLocalizedText(app);
    SaveAppSettings(app);
    if (writeLog) {
        AppendLog(app, GetText(app, L"log_language_applied"));
    }
    InvalidateRect(app.window, nullptr, TRUE);
}

void OpenAboutGithubLink() {
    ShellExecuteW(nullptr, L"open", kAboutGithubUrl, nullptr, nullptr, SW_SHOWNORMAL);
}

bool SetStartupEnabled(bool enabled, std::wstring& error) {
    error.clear();
    HKEY key = nullptr;
    const LONG openStatus = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        0,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &key,
        nullptr);

    if (openStatus != ERROR_SUCCESS) {
        error = FormatErrorMessage(openStatus);
        return false;
    }

    bool success = true;
    if (enabled) {
        const std::wstring command = QuoteForCommandLine(GetExePath().wstring()) + L" --minimized --auto-start";
        const LONG setStatus = RegSetValueExW(
            key,
            kStartupValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        if (setStatus != ERROR_SUCCESS) {
            error = FormatErrorMessage(setStatus);
            success = false;
        }
    } else {
        const LONG deleteStatus = RegDeleteValueW(key, kStartupValueName);
        if (deleteStatus != ERROR_SUCCESS && deleteStatus != ERROR_FILE_NOT_FOUND) {
            error = FormatErrorMessage(deleteStatus);
            success = false;
        }
    }

    RegCloseKey(key);
    return success;
}

bool ValidateName(const std::wstring& rawName, std::wstring& normalizedName, std::wstring& error) {
    normalizedName = ToUpperAscii(Trim(rawName));
    if (normalizedName.empty()) {
        error = g_app ? GetText(*g_app, L"message_name_empty") : L"Name cannot be empty.";
        return false;
    }
    if (normalizedName.size() > 15) {
        error = (g_app ? GetText(*g_app, L"message_name_too_long_prefix") : L"Name \"") +
                rawName +
                (g_app ? GetText(*g_app, L"message_name_too_long_suffix") : L"\" is too long.");
        return false;
    }
    for (const auto ch : normalizedName) {
        if (ch < 0x20 || ch > 0x7E) {
            error = (g_app ? GetText(*g_app, L"message_name_ascii_prefix") : L"Name \"") +
                    rawName +
                    (g_app ? GetText(*g_app, L"message_name_ascii_suffix") : L"\" must use ASCII characters only.");
            return false;
        }
    }
    return true;
}

bool ValidateIp(const std::wstring& rawIp, std::wstring& normalizedIp, uint32_t& ipAddress, std::wstring& error) {
    normalizedIp = Trim(rawIp);
    IN_ADDR address{};
    if (InetPtonW(AF_INET, normalizedIp.c_str(), &address) != 1) {
        error = (g_app ? GetText(*g_app, L"message_invalid_ip_prefix") : L"IP \"") +
                rawIp +
                (g_app ? GetText(*g_app, L"message_invalid_ip_suffix") : L"\" is not a valid IPv4 address.");
        return false;
    }
    ipAddress = address.S_un.S_addr;
    return true;
}

bool CollectInputEntry(AppState& app, ResolverEntry& entry, std::wstring& error) {
    const auto rawName = GetWindowTextString(app.nameInput);
    const auto rawIp = GetWindowTextString(app.ipInput);

    if (!ValidateName(rawName, entry.displayName, error)) {
        return false;
    }
    if (!ValidateIp(rawIp, entry.ipText, entry.ipAddress, error)) {
        return false;
    }

    entry.upperName = WideToUtf8(entry.displayName);
    return true;
}

bool CollectEntriesFromList(AppState& app, std::vector<ResolverEntry>& entries, std::wstring& error) {
    entries.clear();
    std::set<std::string> seenNames;
    const int itemCount = ListView_GetItemCount(app.entryList);

    for (int index = 0; index < itemCount; ++index) {
        ResolverEntry entry;
        const auto name = GetListViewText(app.entryList, index, 0);
        const auto ip = GetListViewText(app.entryList, index, 1);

        if (!ValidateName(name, entry.displayName, error)) {
            return false;
        }
        if (!ValidateIp(ip, entry.ipText, entry.ipAddress, error)) {
            return false;
        }

        entry.upperName = WideToUtf8(entry.displayName);
        if (!seenNames.insert(entry.upperName).second) {
            error = (g_app ? GetText(*g_app, L"message_duplicate_unique_prefix") : L"Name \"") +
                    entry.displayName +
                    (g_app ? GetText(*g_app, L"message_duplicate_unique_suffix") : L"\" is duplicated.");
            return false;
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        error = g_app ? GetText(*g_app, L"message_need_entry_list") : L"Add at least one name / IP entry to the list first.";
        return false;
    }

    return true;
}

void AddCurrentEntry(AppState& app) {
    ResolverEntry entry;
    std::wstring error;
    if (!CollectInputEntry(app, entry, error)) {
        MessageBoxW(app.window, error.c_str(), GetText(app, L"title_config_error").c_str(), MB_ICONERROR);
        return;
    }

    if (FindEntryByName(app.entryList, entry.displayName) >= 0) {
        const std::wstring duplicateMessage =
            GetText(app, L"message_duplicate_exists_prefix") +
            entry.displayName +
            GetText(app, L"message_duplicate_exists_suffix");
        MessageBoxW(app.window, duplicateMessage.c_str(), GetText(app, L"title_duplicate_name").c_str(), MB_ICONWARNING);
        return;
    }

    AddEntryToList(app, entry.displayName, entry.ipText);
    RefreshServiceSummary(app);
    ClearEntryInputs(app);
}

void RemoveSelectedEntry(AppState& app) {
    const int selectedIndex = ListView_GetNextItem(app.entryList, -1, LVNI_SELECTED);
    if (selectedIndex < 0) {
        MessageBoxW(app.window, GetText(app, L"message_select_remove_entry").c_str(), GetText(app, L"title_info").c_str(), MB_ICONINFORMATION);
        return;
    }

    ListView_DeleteItem(app.entryList, selectedIndex);
    RefreshServiceSummary(app);

    const int itemCount = ListView_GetItemCount(app.entryList);
    if (itemCount > 0) {
        const int nextIndex = std::min(selectedIndex, itemCount - 1);
        ListView_SetItemState(app.entryList, nextIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(app.entryList, nextIndex, FALSE);
    }
}

bool CollectEntries(AppState& app, std::vector<ResolverEntry>& entries, std::wstring& error) {
    return CollectEntriesFromList(app, entries, error);
}

void LayoutRows(AppState& app) {
    UNREFERENCED_PARAMETER(app);
}

bool SaveEntriesToConfig(AppState& app) {
    return SaveListEntriesToConfig(app);
}

void AddRow(AppState& app, const std::wstring& name = L"", const std::wstring& ip = L"") {
    if (app.entryList && (!Trim(name).empty() || !Trim(ip).empty())) {
        AddEntryToList(app, Trim(name), Trim(ip));
        RefreshServiceSummary(app);
    }
}

void RemoveLastRow(AppState& app) {
    const int itemCount = app.entryList ? ListView_GetItemCount(app.entryList) : 0;
    if (itemCount > 0) {
        ListView_DeleteItem(app.entryList, itemCount - 1);
        RefreshServiceSummary(app);
    }
}

void LoadEntriesFromConfig(AppState& app) {
    LoadListEntriesFromConfig(app);
}

void SyncUiRunningState(AppState& app, bool running) {
    app.serviceRunning = running;
    ApplyStatusVisuals(app);

    EnableWindow(app.startButton, running ? FALSE : TRUE);
    EnableWindow(app.stopButton, running ? TRUE : FALSE);
    EnableWindow(app.addButton, running ? FALSE : TRUE);
    EnableWindow(app.removeButton, running ? FALSE : TRUE);
    EnableWindow(app.saveButton, running ? FALSE : TRUE);
    EnableWindow(app.nameInput, running ? FALSE : TRUE);
    EnableWindow(app.ipInput, running ? FALSE : TRUE);
    EnableWindow(app.entryList, running ? FALSE : TRUE);

    InvalidateRect(app.statusText, nullptr, TRUE);
    InvalidateRect(app.startButton, nullptr, TRUE);
    InvalidateRect(app.stopButton, nullptr, TRUE);
    InvalidateRect(app.addButton, nullptr, TRUE);
    InvalidateRect(app.removeButton, nullptr, TRUE);
    InvalidateRect(app.saveButton, nullptr, TRUE);
}

bool AddTrayIcon(AppState& app) {
    if (app.trayAdded) {
        return true;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = app.window;
    data.uID = kTrayIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WM_TRAYICON;
    data.hIcon = static_cast<HICON>(LoadImageW(app.instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    lstrcpynW(data.szTip, kAppTitle, ARRAYSIZE(data.szTip));

    const bool ok = Shell_NotifyIconW(NIM_ADD, &data) == TRUE;
    if (data.hIcon) {
        DestroyIcon(data.hIcon);
    }
    app.trayAdded = ok;
    return ok;
}

void RemoveTrayIcon(AppState& app) {
    if (!app.trayAdded) {
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = app.window;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    app.trayAdded = false;
}

void ShowMainWindow(AppState& app, bool activate) {
    ShowWindow(app.window, SW_RESTORE);
    if (activate) {
        SetForegroundWindow(app.window);
        SetFocus(app.window);
    }
}

void HideToTray(AppState& app) {
    AddTrayIcon(app);
    ShowWindow(app.window, SW_HIDE);
}

void ShowTrayMenu(AppState& app) {
    HMENU menu = CreatePopupMenu();
    const std::wstring showText = GetText(app, L"tray_show");
    const std::wstring exitText = GetText(app, L"tray_exit");
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, showText.c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, exitText.c_str());

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(app.window);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, app.window, nullptr);
    PostMessageW(app.window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

bool StartResolver(AppState& app) {
    std::vector<ResolverEntry> entries;
    std::wstring error;
    if (!CollectEntriesFromList(app, entries, error)) {
        MessageBoxW(app.window, error.c_str(), GetText(app, L"title_config_error").c_str(), MB_ICONERROR);
        return false;
    }

    SaveListEntriesToConfig(app);
    if (!app.service) {
        app.service = std::make_unique<ResolverService>(app.window);
    }

    if (!app.service->Start(entries, error)) {
        MessageBoxW(app.window, error.c_str(), GetText(app, L"title_start_failed").c_str(), MB_ICONERROR);
        return false;
    }

    SyncUiRunningState(app, true);
    ApplyServiceSummary(app);
    return true;
}

void StopResolver(AppState& app) {
    if (app.service && app.service->IsRunning()) {
        app.service->Stop();
    }
    SyncUiRunningState(app, false);
}

LRESULT CALLBACK EntryEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR subclassId, DWORD_PTR refData) {
    UNREFERENCED_PARAMETER(subclassId);

    auto* app = reinterpret_cast<AppState*>(refData);
    if (!app) {
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_GETDLGCODE:
        return DefSubclassProc(hwnd, message, wParam, lParam) | DLGC_WANTTAB;

    case WM_KEYDOWN:
        if (wParam == VK_TAB) {
            const bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (hwnd == app->nameInput) {
                SetFocus(shiftPressed ? app->tabControl : app->ipInput);
            } else if (hwnd == app->ipInput) {
                SetFocus(shiftPressed ? app->nameInput : app->addButton);
            }
            return 0;
        }

        if (wParam == VK_RETURN && hwnd == app->ipInput) {
            AddCurrentEntry(*app);
            return 0;
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, EntryEditSubclassProc, subclassId);
        break;

    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void CreateUi(AppState& app) {
    app.font = CreateAppFont(19);
    app.tabFont = CreateAppFont(17);
    app.sectionFont = CreateAppFont(19, FW_BOLD);
    app.bannerFont = CreateAppFont(26, FW_BOLD);
    app.detailFont = CreateAppFont(17);
    app.monoFont = CreateAppFont(17, FW_NORMAL, true);
    app.linkFont = CreateAppFont(17, FW_NORMAL, false, true);

    app.tabControl = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
                                     16, 8, 420, 34, app.window, reinterpret_cast<HMENU>(IDC_TAB_CONTROL), app.instance, nullptr);

    TCITEMW tabItem{};
    tabItem.mask = TCIF_TEXT;
    tabItem.pszText = const_cast<LPWSTR>(L"");
    TabCtrl_InsertItem(app.tabControl, 0, &tabItem);
    tabItem.pszText = const_cast<LPWSTR>(L"");
    TabCtrl_InsertItem(app.tabControl, 1, &tabItem);
    tabItem.pszText = const_cast<LPWSTR>(L"");
    TabCtrl_InsertItem(app.tabControl, 2, &tabItem);
    tabItem.pszText = const_cast<LPWSTR>(L"");
    TabCtrl_InsertItem(app.tabControl, 3, &tabItem);

    app.leftGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                    16, 42, 690, 470, app.window, reinterpret_cast<HMENU>(IDC_GROUP_LEFT), app.instance, nullptr);
    app.serviceGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                       720, 42, 380, 310, app.window, reinterpret_cast<HMENU>(IDC_GROUP_SERVICE), app.instance, nullptr);
    app.startupGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                       720, 362, 380, 150, app.window, reinterpret_cast<HMENU>(IDC_GROUP_STARTUP), app.instance, nullptr);
    app.logGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                   16, 524, 1084, 240, app.window, reinterpret_cast<HMENU>(IDC_GROUP_LOG), app.instance, nullptr);

    app.leftIntro = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    34, 74, 640, 28, app.window, reinterpret_cast<HMENU>(IDC_LEFT_INTRO), app.instance, nullptr);
    app.nameHeader = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                     34, 122, 90, 24, app.window, reinterpret_cast<HMENU>(IDC_NAME_HEADER), app.instance, nullptr);
    app.ipHeader = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   414, 122, 80, 24, app.window, reinterpret_cast<HMENU>(IDC_IP_HEADER), app.instance, nullptr);

    app.nameInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                                    34, 152, 360, 32, app.window, reinterpret_cast<HMENU>(IDC_NAME_INPUT), app.instance, nullptr);
    app.ipInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                                  414, 152, 160, 32, app.window, reinterpret_cast<HMENU>(IDC_IP_INPUT), app.instance, nullptr);
    SetWindowSubclass(app.nameInput, EntryEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(&app));
    SetWindowSubclass(app.ipInput, EntryEditSubclassProc, 2, reinterpret_cast<DWORD_PTR>(&app));

    app.addButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                    578, 150, 96, 36, app.window, reinterpret_cast<HMENU>(IDC_ADD_ROW), app.instance, nullptr);
    app.removeButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                       34, 202, 132, 40, app.window, reinterpret_cast<HMENU>(IDC_REMOVE_ROW), app.instance, nullptr);
    app.saveButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     180, 202, 132, 40, app.window, reinterpret_cast<HMENU>(IDC_SAVE_SETTINGS), app.instance, nullptr);
    app.entryList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                    34, 254, 640, 180, app.window, reinterpret_cast<HMENU>(IDC_ENTRY_LIST), app.instance, nullptr);
    app.infoText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   34, 448, 640, 28, app.window, reinterpret_cast<HMENU>(IDC_INFO_TEXT), app.instance, nullptr);

    ListView_SetExtendedListViewStyle(app.entryList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(app.entryList, GetSysColor(COLOR_WINDOW));
    ListView_SetTextBkColor(app.entryList, GetSysColor(COLOR_WINDOW));

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    column.fmt = LVCFMT_LEFT;
    column.cx = 390;
    column.pszText = const_cast<LPWSTR>(L"名稱");
    ListView_InsertColumn(app.entryList, 0, &column);
    column.cx = 220;
    column.pszText = const_cast<LPWSTR>(L"IPv4");
    ListView_InsertColumn(app.entryList, 1, &column);

    app.statusText = CreateWindowExW(0, L"STATIC", L"",
                                     WS_CHILD | WS_VISIBLE | SS_CENTER,
                                     742, 74, 334, 44, app.window, reinterpret_cast<HMENU>(IDC_STATUS_TEXT), app.instance, nullptr);
    app.startButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      742, 132, 158, 42, app.window, reinterpret_cast<HMENU>(IDC_START_SERVER), app.instance, nullptr);
    app.stopButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     918, 132, 158, 42, app.window, reinterpret_cast<HMENU>(IDC_STOP_SERVER), app.instance, nullptr);
    app.serviceDetailText = CreateWindowExW(0, L"STATIC", L"",
                                            WS_CHILD | WS_VISIBLE,
                                            742, 186, 334, 72, app.window, reinterpret_cast<HMENU>(IDC_SERVICE_DETAIL), app.instance, nullptr);
    app.hintText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   742, 262, 334, 64, app.window, reinterpret_cast<HMENU>(IDC_HINT_TEXT), app.instance, nullptr);

    app.startupStatusText = CreateWindowExW(0, L"STATIC", L"",
                                            WS_CHILD | WS_VISIBLE | SS_CENTER,
                                            742, 392, 334, 40, app.window, reinterpret_cast<HMENU>(IDC_STARTUP_STATUS), app.instance, nullptr);
    app.startupDetailText = CreateWindowExW(0, L"STATIC", L"",
                                            WS_CHILD | WS_VISIBLE,
                                            742, 438, 334, 24, app.window, reinterpret_cast<HMENU>(IDC_STARTUP_DETAIL), app.instance, nullptr);
    app.enableStartupButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                              742, 466, 158, 42, app.window, reinterpret_cast<HMENU>(IDC_ENABLE_STARTUP), app.instance, nullptr);
    app.disableStartupButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                               918, 466, 158, 42, app.window, reinterpret_cast<HMENU>(IDC_DISABLE_STARTUP), app.instance, nullptr);

    app.logEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        34, 550, 1048, 198, app.window, reinterpret_cast<HMENU>(IDC_LOG_EDIT), app.instance, nullptr);

    app.hostsGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_GROUPBOX,
                                     16, 42, 1084, 706, app.window, reinterpret_cast<HMENU>(IDC_HOSTS_GROUP), app.instance, nullptr);
    app.hostsIntro = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                     34, 74, 840, 40, app.window, reinterpret_cast<HMENU>(IDC_HOSTS_INTRO), app.instance, nullptr);
    app.hostsEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
        34, 134, 860, 500, app.window, reinterpret_cast<HMENU>(IDC_HOSTS_EDIT), app.instance, nullptr);
    app.writeHostsButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                           914, 140, 146, 42, app.window, reinterpret_cast<HMENU>(IDC_WRITE_HOSTS), app.instance, nullptr);
    app.openHostsButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                          914, 194, 146, 42, app.window, reinterpret_cast<HMENU>(IDC_OPEN_HOSTS), app.instance, nullptr);
    app.hostsFormatText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                          914, 252, 146, 100, app.window, reinterpret_cast<HMENU>(IDC_HOSTS_FORMAT), app.instance, nullptr);
    app.hostsNoteText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                        34, 652, 1026, 54, app.window, reinterpret_cast<HMENU>(IDC_HOSTS_NOTE), app.instance, nullptr);

    app.settingsAppGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_GROUPBOX,
                                           16, 42, 500, 110, app.window, reinterpret_cast<HMENU>(IDC_SETTINGS_APP_GROUP), app.instance, nullptr);
    app.minimizeToTrayCheck = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                                              34, 92, 450, 28, app.window, reinterpret_cast<HMENU>(IDC_MINIMIZE_TO_TRAY), app.instance, nullptr);
    app.settingsLanguageGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_GROUPBOX,
                                                16, 170, 500, 140, app.window, reinterpret_cast<HMENU>(IDC_SETTINGS_LANGUAGE_GROUP), app.instance, nullptr);
    app.languageLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                        34, 224, 120, 24, app.window, reinterpret_cast<HMENU>(IDC_LANGUAGE_LABEL), app.instance, nullptr);
    app.languageCombo = CreateWindowExW(WS_EX_CLIENTEDGE, WC_COMBOBOXW, L"",
                                        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        156, 220, 310, 240, app.window, reinterpret_cast<HMENU>(IDC_LANGUAGE_COMBO), app.instance, nullptr);
    app.customLanguageHintText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                                 34, 264, 450, 24, app.window, reinterpret_cast<HMENU>(IDC_CUSTOM_LANGUAGE_HINT), app.instance, nullptr);
    app.settingsAboutGroup = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_GROUPBOX,
                                             16, 42, 1084, 678, app.window, reinterpret_cast<HMENU>(IDC_SETTINGS_ABOUT_GROUP), app.instance, nullptr);
    app.aboutTitleText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                         36, 82, 520, 28, app.window, reinterpret_cast<HMENU>(IDC_ABOUT_TITLE), app.instance, nullptr);
    app.aboutBodyText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                        36, 110, 1000, 536, app.window, reinterpret_cast<HMENU>(IDC_ABOUT_BODY), app.instance, nullptr);
    app.aboutGithubLink = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_NOTIFY,
                                          36, 658, 700, 24, app.window, reinterpret_cast<HMENU>(IDC_ABOUT_GITHUB_LINK), app.instance, nullptr);

    for (HWND control : {
             app.tabControl, app.leftGroup, app.serviceGroup, app.startupGroup, app.logGroup, app.leftIntro, app.nameHeader, app.ipHeader,
             app.nameInput, app.ipInput, app.addButton, app.removeButton, app.saveButton, app.entryList, app.infoText,
             app.statusText, app.serviceDetailText, app.hintText, app.startButton, app.stopButton, app.startupStatusText,
             app.startupDetailText, app.enableStartupButton, app.disableStartupButton, app.logEdit, app.hostsGroup,
             app.hostsIntro, app.hostsEdit, app.writeHostsButton, app.openHostsButton, app.hostsFormatText, app.hostsNoteText,
             app.settingsAppGroup, app.minimizeToTrayCheck, app.settingsLanguageGroup, app.languageLabel, app.languageCombo,
             app.customLanguageHintText, app.settingsAboutGroup, app.aboutTitleText, app.aboutBodyText, app.aboutGithubLink }) {
        if (!control) {
            continue;
        }

        HFONT targetFont = app.font;
        if (control == app.tabControl) {
            targetFont = app.tabFont ? app.tabFont : app.font;
        } else
        if (control == app.aboutGithubLink) {
            targetFont = app.linkFont;
        } else
        if (control == app.statusText || control == app.startupStatusText) {
            targetFont = app.bannerFont;
        } else if (control == app.leftGroup || control == app.serviceGroup || control == app.startupGroup || control == app.logGroup ||
                   control == app.hostsGroup || control == app.settingsAppGroup || control == app.settingsLanguageGroup ||
                   control == app.settingsAboutGroup || control == app.nameHeader || control == app.ipHeader ||
                   control == app.aboutTitleText) {
            targetFont = app.sectionFont;
        } else if (control == app.serviceDetailText || control == app.startupDetailText || control == app.hintText ||
                   control == app.leftIntro || control == app.hostsIntro || control == app.hostsFormatText ||
                   control == app.hostsNoteText || control == app.customLanguageHintText || control == app.aboutBodyText) {
            targetFont = app.detailFont;
        } else if (control == app.logEdit || control == app.hostsEdit) {
            targetFont = app.monoFont;
        }
        SendFont(control, targetFont);
    }

    SendMessageW(app.languageCombo, CB_SETMINVISIBLE, 6, 0);
    SendMessageW(app.languageCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 24);
    SendMessageW(app.languageCombo, CB_SETDROPPEDWIDTH, 280, 0);

    LoadListEntriesFromConfig(app);
    SetWindowTextW(app.hostsEdit, L"");
    SyncUiRunningState(app, false);
    ClearEntryInputs(app);
    ApplyLocalizedText(app);
    AppendLog(app, GetText(app, L"log_program_ready"));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppState& app = *g_app;

    switch (message) {
    case WM_COMMAND: {
        const int commandId = LOWORD(wParam);
        const int notifyCode = HIWORD(wParam);

        switch (commandId) {
        case IDC_ADD_ROW:
            AddCurrentEntry(app);
            return 0;
        case IDC_REMOVE_ROW:
            RemoveSelectedEntry(app);
            return 0;
        case IDC_SAVE_SETTINGS:
            if (!SaveListEntriesToConfig(app)) {
                MessageBoxW(hwnd, GetText(app, L"message_save_failed").c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
            } else {
                AppendLog(app, GetText(app, L"log_entries_saved"));
            }
            return 0;
        case IDC_WRITE_HOSTS:
            WriteHostsEditorToSystemHosts(app);
            return 0;
        case IDC_OPEN_HOSTS:
            OpenSystemHostsFile(app);
            return 0;
        case IDC_START_SERVER:
            StartResolver(app);
            return 0;
        case IDC_STOP_SERVER:
            StopResolver(app);
            return 0;
        case IDC_MINIMIZE_TO_TRAY:
            app.minimizeToTray = (SendMessageW(app.minimizeToTrayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveAppSettings(app);
            return 0;
        case IDC_LANGUAGE_COMBO: {
            if (notifyCode != CBN_SELCHANGE) {
                return 0;
            }
            const LRESULT selection = SendMessageW(app.languageCombo, CB_GETCURSEL, 0, 0);
            if (selection != CB_ERR && selection >= 0 && static_cast<std::size_t>(selection) < app.languageCodes.size()) {
                const std::wstring selected = app.languageCodes[static_cast<std::size_t>(selection)];
                if (NormalizeLanguageCode(selected) != NormalizeLanguageCode(app.languageCode)) {
                    ApplyLanguageSelection(app, selected, true);
                }
            }
            return 0;
        }
        case IDC_ABOUT_GITHUB_LINK:
            if (notifyCode == STN_CLICKED) {
                OpenAboutGithubLink();
            }
            return 0;
        case IDC_ENABLE_STARTUP: {
            std::wstring error;
            if (!SetStartupEnabled(true, error)) {
                MessageBoxW(hwnd, error.c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
            } else {
                RefreshStartupState(app);
                InvalidateRect(app.enableStartupButton, nullptr, TRUE);
                InvalidateRect(app.disableStartupButton, nullptr, TRUE);
                AppendLog(app, GetText(app, L"log_startup_enabled"));
            }
            return 0;
        }
        case IDC_DISABLE_STARTUP: {
            std::wstring error;
            if (!SetStartupEnabled(false, error)) {
                MessageBoxW(hwnd, error.c_str(), GetText(app, L"title_error").c_str(), MB_ICONERROR);
            } else {
                RefreshStartupState(app);
                InvalidateRect(app.enableStartupButton, nullptr, TRUE);
                InvalidateRect(app.disableStartupButton, nullptr, TRUE);
                AppendLog(app, GetText(app, L"log_startup_disabled"));
            }
            return 0;
        }
        case ID_TRAY_SHOW:
            ShowMainWindow(app, true);
            return 0;
        case ID_TRAY_EXIT:
            app.exiting = true;
            DestroyWindow(hwnd);
            return 0;
        default:
            UNREFERENCED_PARAMETER(notifyCode);
            return 0;
        }
    }

    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header && header->idFrom == IDC_TAB_CONTROL && header->code == TCN_SELCHANGE) {
            app.activeTab = TabCtrl_GetCurSel(app.tabControl);
            UpdatePageVisibility(app);
            return 0;
        }
        break;
    }

    case WM_DRAWITEM: {
        const auto* drawInfo = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!drawInfo) {
            break;
        }

        switch (drawInfo->CtlID) {
        case IDC_START_SERVER:
            DrawActionButton(drawInfo, kStartFill, kStartBorder, kStartText);
            return TRUE;
        case IDC_STOP_SERVER:
            DrawActionButton(drawInfo, kStopFill, kStopBorder, kStopText);
            return TRUE;
        case IDC_ENABLE_STARTUP:
            DrawActionButton(drawInfo, kSecondaryFill, kSecondaryBorder, kSecondaryText);
            return TRUE;
        case IDC_DISABLE_STARTUP:
            if (app.startupEnabled) {
                DrawActionButton(drawInfo, kStopFill, kStopBorder, kStopText);
            } else {
                DrawActionButton(drawInfo, kNeutralFill, kNeutralBorder, kNeutralText);
            }
            return TRUE;
        case IDC_ADD_ROW:
            DrawActionButton(drawInfo, kSecondaryFill, kSecondaryBorder, kSecondaryText);
            return TRUE;
        case IDC_REMOVE_ROW:
            DrawActionButton(drawInfo, kNeutralFill, kNeutralBorder, kNeutralText);
            return TRUE;
        case IDC_SAVE_SETTINGS:
            DrawActionButton(drawInfo, kSecondaryFill, kSecondaryBorder, kSecondaryText);
            return TRUE;
        case IDC_WRITE_HOSTS:
            DrawActionButton(drawInfo, kStartFill, kStartBorder, kStartText);
            return TRUE;
        case IDC_OPEN_HOSTS:
            DrawActionButton(drawInfo, kNeutralFill, kNeutralBorder, kNeutralText);
            return TRUE;
        default:
            break;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));

        if (control == app.statusText) {
            SetTextColor(dc, app.serviceRunning ? RGB(18, 110, 55) : RGB(200, 32, 32));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.startupStatusText) {
            SetTextColor(dc, app.startupEnabled ? RGB(18, 110, 55) : RGB(200, 32, 32));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.leftIntro || control == app.hostsIntro || control == app.hostsFormatText || control == app.customLanguageHintText || control == app.aboutBodyText) {
            SetTextColor(dc, RGB(30, 30, 30));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.infoText || control == app.hintText || control == app.serviceDetailText || control == app.startupDetailText) {
            SetTextColor(dc, RGB(70, 70, 70));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.hostsNoteText) {
            SetTextColor(dc, app.isElevated ? RGB(18, 110, 55) : RGB(188, 70, 63));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.nameHeader || control == app.ipHeader) {
            SetTextColor(dc, RGB(35, 35, 35));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.aboutTitleText) {
            SetTextColor(dc, RGB(25, 25, 25));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (control == app.aboutGithubLink) {
            SetTextColor(dc, RGB(0, 102, 204));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetTextColor(dc, RGB(0, 0, 0));
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        SetTextColor(dc, RGB(0, 0, 0));
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
    }

    case WM_SETCURSOR:
        if (reinterpret_cast<HWND>(wParam) == app.aboutGithubLink && LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_CLOSE:
        if (!app.exiting && app.minimizeToTray) {
            HideToTray(app);
            AppendLog(app, GetText(app, L"log_hide_to_tray"));
            return 0;
        }
        break;

    case WM_DESTROY:
        SaveListEntriesToConfig(app);
        SaveAppSettings(app);
        StopResolver(app);
        RemoveTrayIcon(app);
        if (app.font) {
            DeleteObject(app.font);
        }
        if (app.tabFont) {
            DeleteObject(app.tabFont);
        }
        if (app.sectionFont) {
            DeleteObject(app.sectionFont);
        }
        if (app.bannerFont) {
            DeleteObject(app.bannerFont);
        }
        if (app.detailFont) {
            DeleteObject(app.detailFont);
        }
        if (app.monoFont) {
            DeleteObject(app.monoFont);
        }
        if (app.linkFont) {
            DeleteObject(app.linkFont);
        }
        PostQuitMessage(0);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK) {
            ShowMainWindow(app, true);
            return 0;
        }
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu(app);
            return 0;
        }
        break;

    case WM_APP_LOG: {
        std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(lParam));
        if (text) {
            AppendLog(app, *text);
        }
        return 0;
    }

    case WM_APP_STATUS: {
        std::unique_ptr<std::wstring> status(reinterpret_cast<std::wstring*>(lParam));
        if (status) {
            app.serviceRunning = (*status == L"執行中");
            ApplyStatusVisuals(app);
            InvalidateRect(app.statusText, nullptr, TRUE);
        }
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    INITCOMMONCONTROLSEX initControls{};
    initControls.dwSize = sizeof(initControls);
    initControls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&initControls);

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxW(nullptr, GetBuiltInStrings(L"tw").at(L"message_winsock_init_failed").c_str(), kAppTitle, MB_ICONERROR);
        return 1;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startMinimized = false;
    bool autoStartServer = false;
    for (int index = 1; index < argc; ++index) {
        const std::wstring arg = argv[index];
        if (arg == L"--minimized") {
            startMinimized = true;
        } else if (arg == L"--auto-start") {
            autoStartServer = true;
        }
    }
    if (argv) {
        LocalFree(argv);
    }

    AppState app;
    app.instance = instance;
    app.configPath = GetConfigPath();
    app.settingsPath = GetSettingsPath();
    app.startMinimized = startMinimized;
    app.autoStartServer = autoStartServer;
    app.isElevated = IsProcessElevated();
    LoadAppSettings(app);
    LoadLanguageBundle(app);
    g_app = &app;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    RegisterClassExW(&windowClass);

    app.window = CreateWindowExW(
        0,
        kWindowClassName,
        kAppTitle,
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!app.window) {
        WSACleanup();
        return 1;
    }

    CreateUi(app);
    AddTrayIcon(app);

    if (app.autoStartServer) {
        const bool started = StartResolver(app);
        if (!started) {
            ShowWindow(app.window, SW_SHOWNORMAL);
            UpdateWindow(app.window);
        } else if (app.startMinimized) {
            ShowWindow(app.window, SW_HIDE);
        }
    } else if (app.startMinimized) {
        ShowWindow(app.window, SW_HIDE);
    } else {
        ShowWindow(app.window, SW_SHOWNORMAL);
        UpdateWindow(app.window);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    WSACleanup();
    return static_cast<int>(message.wParam);
}
