#include <iostream>
#include <fstream>
#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <map>
#include <string>
#include <sstream>
#include <limits>
#include <shellapi.h>
#include <tchar.h>
#include <cstdio>
#include <iomanip>      
#include <conio.h>      
#include <vector>       
#include <algorithm>    
#include <wincrypt.h>
#include <cstring>
#include <cctype>
#include <climits>
#include <cwchar>
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <cwctype>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif
static std::wstring Utf8ToWString(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static std::string WStringToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, NULL, NULL);
    return out;
}

static std::string WStringToAnsi(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(),
        NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string out(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(),
        &out[0], len, NULL, NULL);
    return out;
}

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type;

    bool                             boolVal;
    double                           numVal;
    std::string                      strVal;
    std::vector<JsonValue>           arrVal;
    std::map<std::string, JsonValue> objVal;

    JsonValue() : type(Null), boolVal(false), numVal(0.0) {}

    bool isString() const { return type == String; }
    bool isArray()  const { return type == Array; }
    bool isObject() const { return type == Object; }

    const JsonValue* find(const std::string& key) const {
        if (type != Object) return NULL;
        std::map<std::string, JsonValue>::const_iterator it = objVal.find(key);
        if (it == objVal.end()) return NULL;
        return &it->second;
    }

    static bool parse(const std::string& text, JsonValue& out, std::string& err);
};

namespace json_detail {

    struct Parser {
        const std::string& src;
        size_t i;
        std::string err;

        explicit Parser(const std::string& s) : src(s), i(0) {}

        void skipWs() {
            while (i < src.size()) {
                char c = src[i];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
                else break;
            }
        }

        bool parseValue(JsonValue& v) {
            skipWs();
            if (i >= src.size()) { err = "unexpected end of input"; return false; }
            char c = src[i];
            if (c == '{') return parseObject(v);
            if (c == '[') return parseArray(v);
            if (c == '"') return parseString(v);
            if (c == 't' || c == 'f') return parseBool(v);
            if (c == 'n') return parseNull(v);
            return parseNumber(v);
        }

        bool parseString(JsonValue& v) {
            if (i >= src.size() || src[i] != '"') { err = "expected quote"; return false; }
            ++i;
            std::string out;
            while (i < src.size()) {
                char c = src[i];
                if (c == '"') {
                    ++i;
                    v.type = JsonValue::String;
                    v.strVal = out;
                    return true;
                }
                if (c == '\\') {
                    ++i;
                    if (i >= src.size()) { err = "bad escape"; return false; }
                    char e = src[i];
                    switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (i + 4 >= src.size()) { err = "bad unicode escape"; return false; }
                        unsigned cp = 0;
                        for (int k = 1; k <= 4; ++k) {
                            char h = src[i + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9')      cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else { err = "bad hex in unicode escape"; return false; }
                        }
                        i += 4;
                        if (cp < 0x80) {
                            out += (char)cp;
                        }
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: err = "unknown escape"; return false;
                    }
                    ++i;
                }
                else {
                    out += c;
                    ++i;
                }
            }
            err = "unterminated string";
            return false;
        }

        bool parseNumber(JsonValue& v) {
            size_t start = i;
            while (i < src.size()) {
                char c = src[i];
                if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
                    c == 'e' || c == 'E') {
                    ++i;
                }
                else break;
            }
            if (start == i) { err = "bad number"; return false; }
            v.type = JsonValue::Number;
            v.numVal = std::atof(src.substr(start, i - start).c_str());
            return true;
        }

        bool parseBool(JsonValue& v) {
            if (src.compare(i, 4, "true") == 0) {
                i += 4; v.type = JsonValue::Bool; v.boolVal = true;  return true;
            }
            if (src.compare(i, 5, "false") == 0) {
                i += 5; v.type = JsonValue::Bool; v.boolVal = false; return true;
            }
            err = "bad bool";
            return false;
        }

        bool parseNull(JsonValue& v) {
            if (src.compare(i, 4, "null") == 0) {
                i += 4; v.type = JsonValue::Null; return true;
            }
            err = "bad null";
            return false;
        }

        bool parseArray(JsonValue& v) {
            ++i;
            v.type = JsonValue::Array;
            skipWs();
            if (i < src.size() && src[i] == ']') { ++i; return true; }
            while (true) {
                JsonValue elem;
                if (!parseValue(elem)) return false;
                v.arrVal.push_back(elem);
                skipWs();
                if (i >= src.size()) { err = "unterminated array"; return false; }
                if (src[i] == ',') { ++i; continue; }
                if (src[i] == ']') { ++i; return true; }
                err = "expected comma or bracket";
                return false;
            }
        }

        bool parseObject(JsonValue& v) {
            ++i;
            v.type = JsonValue::Object;
            skipWs();
            if (i < src.size() && src[i] == '}') { ++i; return true; }
            while (true) {
                skipWs();
                JsonValue key;
                if (!parseString(key)) return false;
                skipWs();
                if (i >= src.size() || src[i] != ':') { err = "expected colon"; return false; }
                ++i;
                JsonValue val;
                if (!parseValue(val)) return false;
                v.objVal[key.strVal] = val;
                skipWs();
                if (i >= src.size()) { err = "unterminated object"; return false; }
                if (src[i] == ',') { ++i; continue; }
                if (src[i] == '}') { ++i; return true; }
                err = "expected comma or brace";
                return false;
            }
        }
    };

}

bool JsonValue::parse(const std::string& text, JsonValue& out, std::string& err) {
    json_detail::Parser p(text);
    if (!p.parseValue(out)) { err = p.err; return false; }
    return true;
}

struct HttpResponse {
    DWORD       statusCode;
    std::string body;

    HttpResponse() : statusCode(0) {}
};

static bool HttpRequest(const std::wstring& url,
    const std::wstring& extraHeaders,
    HttpResponse& resp,
    const std::wstring& savePathIfFile)
{
    resp.statusCode = 0;
    resp.body.clear();

    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        std::cerr << "WinHttpCrackUrl failed: " << GetLastError() << "\n";
        return false;
    }

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0)
        path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(
        L"BiterProgramX-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::cerr << "WinHttpOpen failed: " << GetLastError() << "\n";
        return false;
    }

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
        &protocols, sizeof(protocols));

    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) {
        std::cerr << "WinHttpConnect failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        std::cerr << "WinHttpOpenRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
        &redirectPolicy, sizeof(redirectPolicy));

    std::wstring headers =
        L"User-Agent: BiterProgramX-Updater/1.0\r\n"
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n"
        L"Accept-Encoding: identity\r\n";
    if (!extraHeaders.empty()) headers += extraHeaders;

    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        std::cerr << "WinHttpSendRequest failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::cerr << "WinHttpReceiveResponse failed: " << GetLastError() << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD len = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &len, WINHTTP_NO_HEADER_INDEX);
    resp.statusCode = statusCode;

    std::ofstream outFile;
    if (!savePathIfFile.empty()) {
        std::string narrowPath = WStringToAnsi(savePathIfFile);
        outFile.open(narrowPath.c_str(), std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "Cannot create file: " << narrowPath << "\n";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    std::vector<char> buf(16384);
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        if (dwSize > buf.size()) buf.resize(dwSize);

        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead)) break;
        if (dwRead == 0) break;

        if (outFile.is_open()) outFile.write(buf.data(), dwRead);
        else                    resp.body.append(buf.data(), dwRead);
    } while (dwSize > 0);

    if (outFile.is_open()) outFile.close();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

static bool HttpGet(const std::wstring& url, HttpResponse& resp) {
    return HttpRequest(url, L"", resp, L"");
}

static bool HttpDownloadFile(const std::wstring& url, const std::wstring& savePath) {
    HttpResponse dummy;
    return HttpRequest(url, L"", dummy, savePath);
}


static std::wstring NormalizeVersion(const std::wstring& v) {
    std::wstring out = v;
    size_t start = out.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = out.find_last_not_of(L" \t\r\n");
    out = out.substr(start, end - start + 1);
    if (!out.empty() && (out[0] == L'v' || out[0] == L'V'))
        out = out.substr(1);
    for (size_t k = 0; k < out.size(); ++k)
        out[k] = (wchar_t)std::towlower(out[k]);
    return out;
}

static bool SameVersion(const std::wstring& a, const std::wstring& b) {
    return NormalizeVersion(a) == NormalizeVersion(b);
}
static std::wstring ToLowerW(const std::wstring& s) {
    std::wstring out = s;
    for (size_t k = 0; k < out.size(); ++k)
        out[k] = (wchar_t)std::towlower(out[k]);
    return out;
}

static bool EndsWith(const std::wstring& s, const std::wstring& suffix) {
    if (s.size() < suffix.size()) return false;
    return ToLowerW(s.substr(s.size() - suffix.size())) == ToLowerW(suffix);
}

static const JsonValue* SelectAsset(const JsonValue* assets,
    const std::vector<std::wstring>& preferExts,
    const std::wstring& preferNameSubstr)
{
    if (!assets || !assets->isArray() || assets->arrVal.empty())
        return NULL;

    const JsonValue* firstAny = &assets->arrVal[0];
    std::wstring lowerNameSub = ToLowerW(preferNameSubstr);

    for (size_t k = 0; k < assets->arrVal.size(); ++k) {
        const JsonValue& a = assets->arrVal[k];
        const JsonValue* name = a.find("name");
        const JsonValue* url = a.find("browser_download_url");
        if (!name || !name->isString() || !url || !url->isString())
            continue;

        std::wstring wname = Utf8ToWString(name->strVal);

        if (!lowerNameSub.empty() &&
            ToLowerW(wname).find(lowerNameSub) != std::wstring::npos) {
            return &a;
        }

        for (size_t e = 0; e < preferExts.size(); ++e) {
            if (EndsWith(wname, preferExts[e]))
                return &a;
        }
    }
    return firstAny;
}
static bool FetchLatestRelease(const std::wstring& ownerRepo,
    bool includePrerelease,
    JsonValue& outRelease)
{
    std::wstring apiUrl;
    if (includePrerelease) {
        apiUrl = L"https://api.github.com/repos/" + ownerRepo +
            L"/releases?per_page=10";
    }
    else {
        apiUrl = L"https://api.github.com/repos/" + ownerRepo +
            L"/releases/latest";
    }

    HttpResponse resp;
    if (!HttpGet(apiUrl, resp)) {
        std::cerr << "HTTP request failed\n";
        return false;
    }

    if (resp.statusCode != 200) {
        std::cerr << "HTTP status: " << resp.statusCode << "\n";
        if (resp.statusCode == 404) {
            std::cerr << "Repo not found, or no releases\n";
        }
        else if (resp.statusCode == 403) {
            std::cerr << "GitHub API rate limit maybe reached (anonymous 60/hour)\n";
        }
        if (!resp.body.empty()) {
            std::string head = resp.body.substr(0, std::min<size_t>(300, resp.body.size()));
            std::cerr << "Body: " << head << "\n";
        }
        return false;
    }

    JsonValue root;
    std::string jerr;
    if (!JsonValue::parse(resp.body, root, jerr)) {
        std::cerr << "JSON parse error: " << jerr << "\n";
        return false;
    }

    if (includePrerelease) {
        if (!root.isArray() || root.arrVal.empty()) {
            std::cerr << "No releases\n";
            return false;
        }
        for (size_t k = 0; k < root.arrVal.size(); ++k) {
            const JsonValue& r = root.arrVal[k];
            const JsonValue* draft = r.find("draft");
            if (draft && draft->type == JsonValue::Bool && draft->boolVal)
                continue;
            outRelease = r;
            return true;
        }
        std::cerr << "Only drafts available\n";
        return false;
    }
    else {
        outRelease = root;
        return true;
    }
}

bool CheckAndDownloadLatestRelease(
    const std::wstring& ownerRepo,
    const std::wstring& currentVersion,
    const std::wstring& savePath,
    const std::vector<std::wstring>& preferExts,
    const std::wstring& preferNameSubstr,
    bool includePrerelease)
{
    JsonValue release;
    if (!FetchLatestRelease(ownerRepo, includePrerelease, release)) {
        return false;
    }

    const JsonValue* tag = release.find("tag_name");
    if (!tag || !tag->isString() || tag->strVal.empty()) {
        std::cerr << "Release has no tag_name\n";
        return false;
    }

    std::wstring latestVersion = Utf8ToWString(tag->strVal);

    const JsonValue* prereleaseFlag = release.find("prerelease");
    bool isPre = (prereleaseFlag && prereleaseFlag->type == JsonValue::Bool &&
        prereleaseFlag->boolVal);

    std::cout << "Latest  : " << WStringToAnsi(latestVersion);
    if (isPre) std::cout << "  [Pre-release]";
    std::cout << "\n";
    std::cout << "Current : " << WStringToAnsi(currentVersion) << "\n";

    if (SameVersion(latestVersion, currentVersion)) {
        std::cout << "Already up to date.\n";
        return true;
    }

    const JsonValue* assets = release.find("assets");
    const JsonValue* chosen = SelectAsset(assets, preferExts, preferNameSubstr);
    if (!chosen) {
        std::cerr << "No assets in this release\n";
        return false;
    }

    const JsonValue* chosenName = chosen->find("name");
    const JsonValue* chosenUrl = chosen->find("browser_download_url");
    if (!chosenUrl || !chosenUrl->isString()) {
        std::cerr << "Selected asset has no browser_download_url\n";
        return false;
    }

    std::wstring assetName = (chosenName && chosenName->isString())
        ? Utf8ToWString(chosenName->strVal)
        : L"(unknown)";
    std::wstring assetUrl = Utf8ToWString(chosenUrl->strVal);

    std::cout << "Asset   : " << WStringToAnsi(assetName) << "\n";
    std::cout << "URL     : " << WStringToAnsi(assetUrl) << "\n";
    std::cout << "Save to : " << WStringToAnsi(savePath) << "\n";

    if (!HttpDownloadFile(assetUrl, savePath)) {
        std::cerr << "Download failed\n";
        return false;
    }

    std::cout << "Download done.\n";
    return true;
}

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#define TIP_ID          0x1000
#define MAX_INPUT_BUFFER 8192
#define WM_TRAY_MSG     (WM_USER + 100)
#define pass int THIS_IS_A_PASS_HHH_=0xC000000000000005

namespace cajn {
    int run_bytecode_file(const std::string& cjb_path);
}

std::ofstream history_file;

class ErrorDefine {
public:
    static constexpr unsigned long long LOGIN_ERROR = 0x000000000000004F;
    static constexpr unsigned long long LOGIN_ERROR_NAME_CAN_NOT_FALL_TO_THE_PROGRAM = 0x000000000000002F;
    static constexpr unsigned long long REGISTER_ERROR = 0x000000000000006F;
    static constexpr unsigned long long START_ERROR = 0x00000000000003F3;
};

const std::string YOU = "You>";

typedef long double number;

static void console_clear();
static std::string current_time_str();
static void title(std::string title);

class sv_game {
    int health, hunger, air, moisture;
    std::map<std::string, int> Bag = {
        {"wood", 0},
        {"arrow", 100},
        {"bow", 100},
        {"stick", 0},
        {"stone", 0},
        {"iron", 0},
        {"thread", 0},
        {"meat", 0},
        {"fish", 0},
        {"grass", 0},
        {"leather", 0}
    };
public:
    void set(std::string name, int var);
    void find();
    void eat();
    void sleep();
    void drink();
    void build();
    void fill();
    void kill();
    void fishing();
    void start();
    void check();
};

void sv_game::set(std::string name, int var) {
    if (name == "he") {
        this->health = var;
    }
    else if (name == "hu") {
        this->hunger = var;
    }
    else if (name == "ai") {
        this->air = var;
    }
    else if (name == "mo") {
        this->moisture = var;
    }
    else {
        return;
    }
}

void sv_game::find() {
    int tall = 0;
    std::cout << "输入高度:";
    std::cin >> tall;
    if (tall > 50) {
        this->Bag["stone"] += rand() % 20;
        if ((rand() % 200 - tall) > 100) {
            this->Bag["iron"] += rand() % 3;
        }
    }
    else if (tall < 10 and tall > -5) {
        this->Bag["grass"] += rand() % 10;
        if ((rand() % 50 - tall) > 28) {
            this->Bag["wood"] += rand() % 10;
        }
    }
    else if (tall < -50) {
        this->Bag["stone"] += rand() % 45;
        if ((rand() % 100 - tall) > 120) {
            this->Bag["iron"] += rand() % 10;
        }
    }
    else if (tall < -5) {
        this->Bag["stone"] += rand() % 20;
        if ((rand() % 50 - tall) > 20) {
            this->Bag["iron"] += rand() % 10;
        }
    }
}

void sv_game::fill() {
    if (Bag["grass"] < 3) {
        std::cout << "草不够！需要 3 个草才能搓成一根线。" << std::endl;
        return;
    }
    Bag["grass"] -= 3;
    Bag["thread"] += 1;
    std::cout << "你用 3 个草搓成了一根结实的线，线 +1" << std::endl;
}

void sv_game::check() {
    std::cout << "========== 状态检查 ==========" << std::endl;
    std::cout << "生命值 (Health) : " << health << std::endl;
    std::cout << "饱食度 (Hunger) : " << hunger << std::endl;
    std::cout << "氧气值 (Air)    : " << air << std::endl;
    std::cout << "水分值 (Moisture): " << moisture << std::endl;
    std::cout << "----------- 背包物品 -----------" << std::endl;
    bool hasItems = false;
    for (const auto& item : Bag) {
        if (item.second > 0) {
            std::cout << item.first << " : " << item.second << std::endl;
            hasItems = true;
        }
    }
    if (!hasItems) {
        std::cout << "（背包为空）" << std::endl;
    }
    std::cout << "===============================" << std::endl;

    if (health <= 20) std::cout << "[警告] 生命值过低！" << std::endl;
    if (hunger <= 20) std::cout << "[警告] 饥饿值过低！" << std::endl;
    if (air <= 20)    std::cout << "[警告] 氧气不足！" << std::endl;
    if (moisture <= 20) std::cout << "[警告] 水分不足！" << std::endl;
}

void sv_game::eat() {
    std::string choice;
    std::cout << "要吃什么？(meat / fish) : ";
    std::cin >> choice;

    if (choice == "meat" && Bag["meat"] > 0) {
        Bag["meat"]--;
        hunger += 30;
        health += 5;
        std::cout << "你吃了一份肉，饱食度 +30，生命 +5" << std::endl;
    }
    else if (choice == "fish" && Bag["fish"] > 0) {
        Bag["fish"]--;
        hunger += 25;
        health += 3;
        std::cout << "你吃了一条鱼，饱食度 +25，生命 +3" << std::endl;
    }
    else {
        std::cout << "没有这种食物或库存不足！" << std::endl;
    }
    if (hunger > 100) hunger = 100;
    if (health > 100) health = 100;
}

void sv_game::sleep() {
    if (hunger < 10) {
        std::cout << "太饿了，睡不着！" << std::endl;
        return;
    }
    hunger -= 10;
    health += 20;
    moisture += 10;
    if (health > 100) health = 100;
    if (moisture > 100) moisture = 100;
    std::cout << "你睡了一觉，生命 +20，水分 +10，饱食度 -10" << std::endl;
}

void sv_game::drink() {
    moisture += 20;
    if (moisture > 100) moisture = 100;
    std::cout << "你喝了些水，水分 +20" << std::endl;
}

void sv_game::build() {
    std::string what;
    std::cout << "你想建造什么？(house / bridge / weapon) : ";
    std::cin >> what;

    if (what == "house") {
        if (Bag["wood"] >= 10 && Bag["stone"] >= 5) {
            Bag["wood"] -= 10;
            Bag["stone"] -= 5;
            std::cout << "你建造了一间木石房屋！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木材 x10，石头 x5" << std::endl;
        }
    }
    else if (what == "bridge") {
        if (Bag["wood"] >= 5 && Bag["iron"] >= 2) {
            Bag["wood"] -= 5;
            Bag["iron"] -= 2;
            std::cout << "你建造了一座铁木桥！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木材 x5，铁 x2" << std::endl;
        }
    }
    else if (what == "weapon") {
        if (Bag["stick"] >= 1 && Bag["stone"] >= 2) {
            Bag["stick"]--;
            Bag["stone"] -= 2;
            std::cout << "你制作了一把石斧！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木棍 x1，石头 x2" << std::endl;
        }
    }
    else {
        std::cout << "未知建筑类型" << std::endl;
    }
}

void sv_game::kill() {
    if (Bag["arrow"] <= 0) {
        std::cout << "没有箭了！" << std::endl;
        return;
    }
    Bag["arrow"]--;
    int loot = rand() % 100;
    if (loot < 40) {
        Bag["meat"] += 2;
        std::cout << "你杀死了一只鹿，获得 2 块肉" << std::endl;
    }
    else if (loot < 70) {
        Bag["meat"] += 1;
        Bag["leather"]++;
        std::cout << "你杀死了一只狼，获得 1 块肉和 1 张皮" << std::endl;
    }
    else {
        Bag["fish"] += 1;
        std::cout << "你杀死了一只水獭，获得 1 条鱼" << std::endl;
    }
}

void sv_game::fishing() {
    if (Bag["stick"] < 1 || Bag["thread"] < 1) {
        std::cout << "没有鱼竿（需要木棍和线）" << std::endl;
        return;
    }
    int catch_num = rand() % 3 + 1;
    Bag["fish"] += catch_num;
    std::cout << "你钓到了 " << catch_num << " 条鱼！" << std::endl;
}

void sv_game::start() {
    srand(static_cast<unsigned>(time(nullptr)));

    health = 100;
    hunger = 80;
    air = 100;
    moisture = 80;

    Bag["wood"] = 5;
    Bag["stone"] = 3;
    Bag["stick"] = 2;
    Bag["thread"] = 1;

    std::cout << "========== 生存游戏开始 ==========" << std::endl;
    std::cout << "输入 'help' 查看所有命令。\n" << std::endl;

    while (true) {
        this->check();

        if (this->health <= 0) {
            std::cout << "\n 你死了！生命值为 0。" << std::endl;
            break;
        }
        if (this->hunger <= 0) {
            std::cout << "\n 你饿死了！饱食度为 0。" << std::endl;
            break;
        }
        if (this->air <= 0) {
            std::cout << "\n 你窒息了！氧气为 0。" << std::endl;
            break;
        }
        if (this->moisture <= 0) {
            std::cout << "\n 你渴死了！水分为 0。" << std::endl;
            break;
        }

        std::string command;
        std::cout << "\n请输入命令 (输入 help 查看帮助): ";
        std::cin >> command;

        for (auto& c : command) c = tolower(c);

        if (command == "help") {
            std::cout << "可用命令:\n"
                << "  find    - 探索资源（输入高度）\n"
                << "  eat     - 吃东西（meat / fish）\n"
                << "  sleep   - 睡觉（恢复生命和水分）\n"
                << "  drink   - 喝水（恢复水分）\n"
                << "  build   - 建造物品（house / bridge / weapon）\n"
                << "  fill    - 用草搓线（消耗3个草获得1根线）\n"
                << "  kill    - 狩猎（消耗1支箭）\n"
                << "  fishing - 钓鱼（需要木棍和线）\n"
                << "  check   - 重新查看状态\n"
                << "  exit / quit - 退出游戏\n";
        }
        else if (command == "find") {
            this->find();
        }
        else if (command == "eat") {
            this->eat();
        }
        else if (command == "sleep") {
            this->sleep();
        }
        else if (command == "drink") {
            this->drink();
        }
        else if (command == "build") {
            this->build();
        }
        else if (command == "fill") {
            this->fill();
        }
        else if (command == "kill") {
            this->kill();
        }
        else if (command == "fishing") {
            this->fishing();
        }
        else if (command == "check") {
            continue;
        }
        else if (command == "exit" || command == "quit") {
            std::cout << "游戏结束，再见！" << std::endl;
            break;
        }
        else {
            std::cout << "未知命令，请重新输入。" << std::endl;
        }

        std::cout << "\n----------------------------------------\n";
    }
}

struct TodoItem {
    int id;
    std::string desc;
    bool completed;
    std::string created;
    TodoItem() : id(0), completed(false) {}
    TodoItem(int i, const std::string& d, bool c = false, const std::string& t = "")
        : id(i), desc(d), completed(c), created(t) {
    }
};

class gamelist {
private:
    int choose = 0, number_s = 0;
    std::string online_gamename, temp_gamename;
    sv_game sg;
public:
    void chooser() {
        srand(static_cast<unsigned>(time(nullptr)));

        std::cout << "[]=========游戏中心=========[]\n输入1 猜数字游戏\n输入2 启动生存游戏\n输入3 启动对应文件路径的已安装游戏(.exe)" << std::endl << "YOU:";
        std::cin >> choose;
        std::cin.ignore();
        if (choose == 1) {
            int now = std::rand() % 100;
            while (true) {
                std::cout << "猜数字 (0-99): ";
                std::cin >> number_s;
                if (number_s < now) {
                    std::cout << "too small\n";
                }
                else if (number_s > now) {
                    std::cout << "too big\n";
                }
                else {
                    std::cout << "Nice!\n";
                    break;
                }
            }
        }
        else if (choose == 2) {
            this->sg.start();
        }
        else if (choose == 3) {
            std::string path;
            std::cout << "输入路径:" << std::endl;
            std::getline(std::cin, path);
            system(path.c_str());
        }
        else {
            std::cout << "无效选项" << std::endl;
        }
    }
};

class TodoList {
private:
    std::vector<TodoItem> items;
    int next_id;
    std::string data_file;
    bool load_from_file();
    bool save_to_file();
public:
    TodoList(const std::string& filepath = ".\\BiterData\\todo.dat");
    ~TodoList();
    void add(const std::string& description);
    bool remove(int id);
    bool complete(int id);
    void show_all() const;
    void show_details() const;
    void interactive_menu();
};

static void console_clear() {
    system("cls");
}

static std::string current_time_str() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

bool write_file_content(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs) return false;
    ofs << content;
    return true;
}

std::string read_file_content(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool file_exists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::string get_current_time_str() {
    return current_time_str();
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim))
        tokens.push_back(token);
    return tokens;
}

static const char* PLUGIN_LIST_FILE = ".\\BiterData\\expenders_path.data";

std::vector<std::string> load_plugin_paths() {
    std::vector<std::string> paths;
    std::ifstream in(PLUGIN_LIST_FILE);
    if (!in.is_open()) return paths;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (!line.empty()) paths.push_back(line);
    }
    in.close();
    return paths;
}

bool save_plugin_paths(const std::vector<std::string>& paths) {
    std::ofstream out(PLUGIN_LIST_FILE, std::ios::trunc);
    if (!out.is_open()) return false;
    for (size_t i = 0; i < paths.size(); ++i)
        out << paths[i] << '\n';
    out.close();
    return true;
}

bool add_plugin_path(const std::string& path) {
    if (path.empty()) return false;
    std::vector<std::string> paths = load_plugin_paths();
    for (size_t i = 0; i < paths.size(); ++i) {
        if (paths[i] == path) {
            std::cout << "该插件已在列表中\n";
            return false;
        }
    }
    paths.push_back(path);
    if (save_plugin_paths(paths)) {
        std::cout << "插件已添加\n";
        return true;
    }
    std::cout << "保存失败\n";
    return false;
}

bool remove_plugin_path(const std::string& path) {
    std::vector<std::string> paths = load_plugin_paths();
    std::vector<std::string> kept;
    bool removed = false;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (paths[i] == path) { removed = true; continue; }
        kept.push_back(paths[i]);
    }
    if (!removed) {
        std::cout << "未找到该插件\n";
        return false;
    }
    if (save_plugin_paths(kept)) {
        std::cout << "插件已删除\n";
        return true;
    }
    std::cout << "保存失败\n";
    return false;
}

std::string sha256(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32];
    DWORD hash_len = 32;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    if (!CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.size(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < hash_len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

std::string get_password(const std::string& prompt) {
    std::cout << prompt;
    std::string pwd;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode_old, mode_new;
    GetConsoleMode(hStdin, &mode_old);
    mode_new = mode_old & ~ENABLE_ECHO_INPUT;
    SetConsoleMode(hStdin, mode_new);
    std::getline(std::cin, pwd);
    SetConsoleMode(hStdin, mode_old);
    std::cout << "\n";
    return pwd;
}

int safe_get_int(const std::string& prompt, int min_val = INT_MIN, int max_val = INT_MAX) {
    int val;
    while (true) {
        std::cout << prompt;
        if (std::cin >> val) {
            if (val >= min_val && val <= max_val) break;
            else std::cout << "输入超出范围 [" << min_val << ", " << max_val << "]\n";
        }
        else {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "请输入有效数字\n";
        }
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return val;
}

TodoList::TodoList(const std::string& filepath) : data_file(filepath), next_id(1) {
    if (!load_from_file()) {
        next_id = 1;
    }
}

TodoList::~TodoList() {
    save_to_file();
}

bool TodoList::load_from_file() {
    std::ifstream in(data_file, std::ios::binary);
    if (!in.is_open()) return false;

    items.clear();
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }
        if (parts.size() < 4) continue;
        try {
            int id = std::stoi(parts[0]);
            bool completed = (std::stoi(parts[2]) != 0);
            TodoItem item(id, parts[1], completed, parts[3]);
            items.push_back(item);
            if (id >= next_id) next_id = id + 1;
        }
        catch (...) {
        }
    }
    in.close();
    return true;
}

bool TodoList::save_to_file() {
    std::ofstream out(data_file, std::ios::binary);
    if (!out.is_open()) return false;
    for (const auto& item : items) {
        out << item.id << '|'
            << item.desc << '|'
            << (item.completed ? 1 : 0) << '|'
            << item.created << '\n';
    }
    out.close();
    return true;
}

void TodoList::add(const std::string& description) {
    if (description.empty()) {
        std::cout << "任务描述不能为空。\n";
        return;
    }
    TodoItem item(next_id++, description, false, current_time_str());
    items.push_back(item);
    save_to_file();
    std::cout << "任务 #" << item.id << " 添加成功。\n";
}

bool TodoList::remove(int id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const TodoItem& item) {
        return item.id == id;
        });
    if (it == items.end()) {
        std::cout << "未找到任务 #" << id << "\n";
        return false;
    }
    items.erase(it);
    save_to_file();
    std::cout << "任务 #" << id << " 已删除。\n";
    return true;
}

bool TodoList::complete(int id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const TodoItem& item) {
        return item.id == id;
        });
    if (it == items.end()) {
        std::cout << "未找到任务 #" << id << "\n";
        return false;
    }
    if (it->completed) {
        std::cout << "任务 #" << id << " 已完成。\n";
        return true;
    }
    it->completed = true;
    save_to_file();
    std::cout << "任务 #" << id << " 标记为已完成。\n";
    return true;
}

void TodoList::show_all() const {
    if (items.empty()) {
        std::cout << "暂无任务。\n";
        return;
    }
    std::cout << "ID  状态  描述\n";
    std::cout << "-------------------\n";
    for (const auto& item : items) {
        std::cout << std::setw(3) << item.id << "  "
            << (item.completed ? "[√]" : "[ ]") << "  "
            << item.desc << '\n';
    }
}

void TodoList::show_details() const {
    if (items.empty()) {
        std::cout << "暂无任务。\n";
        return;
    }
    std::cout << "ID  状态  描述                    创建时间\n";
    std::cout << "--------------------------------------------------------\n";
    for (const auto& item : items) {
        std::cout << std::setw(3) << item.id << "  "
            << (item.completed ? "[√]" : "[ ]") << "  "
            << std::setw(25) << std::left << item.desc
            << "  " << item.created << '\n';
    }
}

void TodoList::interactive_menu() {
    while (true) {
        console_clear();
        std::cout << "============ 待办事项列表 ============\n";
        std::cout << "1. 查看所有任务\n";
        std::cout << "2. 添加任务\n";
        std::cout << "3. 删除任务\n";
        std::cout << "4. 标记完成\n";
        std::cout << "5. 详细查看\n";
        std::cout << "0. 返回\n";
        std::cout << "=======================================\n";
        std::cout << "请选择: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 0) break;

        switch (choice) {
        case 1:
            show_all();
            break;
        case 2: {
            std::string desc;
            std::cout << "输入任务描述: ";
            std::getline(std::cin, desc);
            add(desc);
            break;
        }
        case 3: {
            int id;
            std::cout << "输入要删除的任务ID: ";
            std::cin >> id;
            std::cin.ignore();
            remove(id);
            break;
        }
        case 4: {
            int id;
            std::cout << "输入要标记完成的任务ID: ";
            std::cin >> id;
            std::cin.ignore();
            complete(id);
            break;
        }
        case 5:
            show_details();
            break;
        default:
            std::cout << "无效选项。\n";
        }
        std::cout << "\n按 Enter 继续...";
        std::cin.get();
    }
}

void Calc(void) {
    console_clear();
    std::cout << "[]================ 计算器 ================[]\n";
    std::cout << "符号: + - * / %\n";
    std::cout << "输入 0 0 退出\n";

    number a, b;
    char op;
    while (true) {
        std::cout << "输入第一个数字: ";
        if (!(std::cin >> a)) {
            std::cout << "Error: Invalid number input!\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::cout << "输入第二个数字:";
        if (!(std::cin >> b)) {
            std::cout << "Error: Invalid number input!\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (a == 0 && b == 0) {
            std::cout << "Exit calculator...\n";
            Sleep(800);
            break;
        }

        std::cout << "输入符号:";
        std::cin >> op;

        number res = 0;
        bool err = false;
        switch (op) {
        case '+': res = a + b; break;
        case '-': res = a - b; break;
        case '*': res = a * b; break;
        case '/':
            if (b == 0) {
                std::cout << "Error: Division by zero!\n";
                err = true;
                break;
            }
            res = a / b;
            break;
        case '%': {
            long long ia = (long long)a;
            long long ib = (long long)b;
            if (ib == 0) {
                std::cout << "Error: Mod zero invalid!\n";
                err = true;
                break;
            }
            res = (number)(ia % ib);
            break;
        }
        default:
            std::cout << "Error: Unknown operator!\n";
            err = true;
            break;
        }
        if (!err) {
            std::cout << "结果 = " << res << "\n";
        }
        std::cout << "------------------------------------------------------\n";
    }
}

void TimeClock(void) {
    console_clear();
    std::cout << "[]======== 时钟 ========[]\n";
    std::cout << "按任意键退出时钟\n";

    while (true) {
        if (_kbhit()) break;

        std::time_t curTime = std::time(NULL);
        std::tm* t = std::localtime(&curTime);
        int h = t->tm_hour;
        int m = t->tm_min;
        int s = t->tm_sec;

        COORD pos = { 0,3 };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
        std::cout << "现在的时间是: "
            << std::setfill('0') << std::setw(2) << h << ":"
            << std::setfill('0') << std::setw(2) << m << ":"
            << std::setfill('0') << std::setw(2) << s;
        Sleep(100);
    }
    std::cin.ignore();
    std::cout << "------------------------------------------------------\n";
}

void SimpleCalendar(void) {
    console_clear();
    std::cout << "[]==================== 日历 ====================[]\n";
    int year, month;
    std::cout << "输入年份>";
    if (!(std::cin >> year)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "无效年份\n";
        std::cout << "按下Enter键返回";
        std::cin.get();
        return;
    }
    std::cout << "输入月份>";
    if (!(std::cin >> month) || month < 1 || month > 12) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "无效月份\n";
        std::cout << "按下Enter键返回";
        std::cin.get();
        return;
    }

    std::tm t = { 0 };
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 1;
    std::mktime(&t);

    int weekStart = t.tm_wday;
    int dayCount[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        dayCount[1] = 29;
    int totalDay = dayCount[month - 1];

    std::cout << "Sun Mon Tue Wed Thu Fri Sat\n";
    for (int i = 0; i < weekStart; i++)
        std::cout << "    ";

    for (int d = 1; d <= totalDay; d++) {
        std::cout << std::setw(3) << d << " ";
        if ((weekStart + d) % 7 == 0)
            std::cout << "\n";
    }
    std::cout << "\n\n";
    std::cout << "按下Enter键返回\n";
    std::cin.ignore();
    std::cin.get();
    std::cout << "------------------------------------------------------\n";
}

void notepad(void) {
    int opt;
    while (true) {
        console_clear();
        std::cout << "================== 记事本 ==================\n";
        std::cout << "1. 开新的一页\n";
        std::cout << "2. 查看已经保存的\n";
        std::cout << "0. 退出\n";
        std::cout << "============================================\n";
        std::cout << YOU;
        std::cin >> opt;
        std::cin.ignore();

        if (opt == 0) break;
        else if (opt == 1) {
            std::ofstream outFile(".\\BiterData\\NoteData.text", std::ios::app);
            if (!outFile.is_open()) {
                std::cout << "找不到保存的记事本\n";
                Sleep(1000);
                continue;
            }
            std::string content;
            bool is_exit_np = false;
            int count = 0;
            while (!is_exit_np) {
                count++;
                std::cout << count << ":";
                std::getline(std::cin, content);
                if (content == ".end")
                    is_exit_np = true;
                else
                    outFile << content << "\n";
            }
            outFile.close();
            std::cout << "文件保存成功\n";
        }
        else if (opt == 2) {
            console_clear();
            std::cout << "============ 查看已经保存的 ============\n";
            std::ifstream inFile(".\\BiterData\\NoteData.text");
            if (!inFile.is_open()) {
                std::cout << "你没有保存文件\n";
            }
            else {
                std::string line;
                while (std::getline(inFile, line)) {
                    std::cout << line << "\n";
                }
                inFile.close();
            }
            std::cout << "------------------------------------------------------\n";
            std::cout << "\n按下Enter键退出";
            std::cin.get();
        }
        Sleep(200);
    }
}

void about(void) {
    console_clear();
    std::cout << "[]==================== 关于 ====================[]\n";
    std::cout << "Biter.ProgramX-V1.0\n";
    std::cout << "开发者: ZengLizard\n";
    std::cout << "版本:" << "V1.0\n";
    std::cout << "发布日期: 2024-09-05\n";
    std::cout << "[]==============================================[]\n";
    std::cout << "\n按下Enter键退出";
    std::cin.get();
}

struct AppContext {
    HWND        tray_hwnd = nullptr;
    HICON       tray_icon = nullptr;
    std::wstring exe_dir;
    std::wstring data_dir;
    std::string  current_user;
    bool         is_exit = false;
};

static AppContext g_app;

std::map<int, std::string> week_table = {
    {1, "Monday"},
    {2, "Tuesday"},
    {3, "Wednesday"},
    {4, "Thursday"},
    {5, "Friday"},
    {6, "Saturday"},
    {7, "Sunday"}
};

static std::wstring get_exe_directory() {
    wchar_t buf[MAX_PATH + 1] = { 0 };
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    wchar_t* last_backslash = wcsrchr(buf, L'\\');
    if (last_backslash != nullptr) {
        *(last_backslash + 1) = L'\0';
    }
    return std::wstring(buf);
}

static bool create_directory_if_not_exist(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryW(path.c_str(), nullptr) != 0;
}

static LRESULT CALLBACK TrayUIProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TRAY_MSG:
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static bool init_tray_window() {
    if (g_app.tray_hwnd != nullptr)
        return true;
    WNDCLASSEXW wndclass;
    ZeroMemory(&wndclass, sizeof(wndclass));
    wndclass.cbSize = sizeof(WNDCLASSEXW);
    wndclass.lpfnWndProc = TrayUIProc;
    wndclass.hInstance = GetModuleHandleW(NULL);
    wndclass.lpszClassName = L"BiterProgramXTrayClass";

    if (!RegisterClassExW(&wndclass)) {
        return false;
    }

    HWND hwnd = CreateWindowExW(
        0,
        L"BiterProgramXTrayClass",
        L"",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandleW(NULL),
        nullptr
    );
    if (!hwnd) {
        UnregisterClassW(L"BiterProgramXTrayClass", GetModuleHandleW(NULL));
        return false;
    }
    g_app.tray_hwnd = hwnd;
    g_app.tray_icon = (HICON)LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    return true;
}

template<size_t N>
static void safe_tstrcpy(TCHAR(&dest)[N], LPCTSTR src) {
    if (src == nullptr) {
        dest[0] = 0;
        return;
    }
    _tcsncpy(dest, src, N - 1);
    dest[N - 1] = 0;
}

static void show_tray_notify(LPCTSTR title, LPCTSTR text) {
    if (g_app.tray_hwnd == nullptr) return;

    NOTIFYICONDATA icondata;
    ZeroMemory(&icondata, sizeof(NOTIFYICONDATA));
    icondata.cbSize = sizeof(NOTIFYICONDATA);
    icondata.hWnd = g_app.tray_hwnd;
    icondata.uID = TIP_ID;
    icondata.uFlags = NIF_ICON | NIF_MESSAGE | NIF_INFO;
    icondata.uCallbackMessage = WM_TRAY_MSG;
    icondata.hIcon = g_app.tray_icon;
    icondata.uTimeout = 30 * 1000;

    safe_tstrcpy(icondata.szInfo, text);
    safe_tstrcpy(icondata.szInfoTitle, title);

    if (!Shell_NotifyIcon(NIM_MODIFY, &icondata)) {
        Shell_NotifyIcon(NIM_ADD, &icondata);
    }
}

static void remove_tray_icon() {
    if (g_app.tray_hwnd == nullptr) return;
    NOTIFYICONDATA icondata;
    ZeroMemory(&icondata, sizeof(NOTIFYICONDATA));
    icondata.cbSize = sizeof(NOTIFYICONDATA);
    icondata.hWnd = g_app.tray_hwnd;
    icondata.uID = TIP_ID;
    Shell_NotifyIcon(NIM_DELETE, &icondata);
}

static void cleanup_tray() {
    remove_tray_icon();
    if (g_app.tray_hwnd) {
        DestroyWindow(g_app.tray_hwnd);
        g_app.tray_hwnd = nullptr;
    }
    if (g_app.tray_icon) {
        DestroyIcon(g_app.tray_icon);
        g_app.tray_icon = nullptr;
    }
    UnregisterClassW(L"BiterProgramXTrayClass", GetModuleHandleW(NULL));
}

class LoginFunction {
public:
    std::string get_current_weekday() {
        time_t now = time(nullptr);
        tm* p_ltm = localtime(&now);
        int wday = p_ltm->tm_wday;
        if (wday == 0) wday = 7;
        return week_table[wday];
    }

    void print_logo() {
        std::cout << "#       ######  #####  #  ##   #\n";
        std::cout << "#       #    #  #         # #  #\n";
        std::cout << "#       #    #  #      #  #  # #\n";
        std::cout << "#       #    #  #   #  #  #   ##\n";
        std::cout << "######  ######  #####  #  #    #\n";
        std::cout << "     LizardBiterXeStudio CO.    \n";
    }

    void prompt_return_console() {
        int result = MessageBox(NULL, TEXT("请返回命令行进行登录"), TEXT("Biter.ProgramX-V1.0"), MB_OKCANCEL | MB_ICONINFORMATION);
        if (result == IDCANCEL) {
            g_app.is_exit = true;
        }
        else {
            console_clear();
        }
    }

    void login(const std::wstring& user_data_filepath, std::string& out_user) {
        ErrorDefine errdef;
        print_logo();
        std::string username;
        std::cout << "请输入用户名:";
        std::getline(std::cin, username);

        FILE* fin = _wfopen(user_data_filepath.c_str(), L"r");
        if (!fin) {
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR));
        }
        char buf[1024];
        if (fgets(buf, sizeof(buf), fin) == nullptr) {
            fclose(fin);
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR));
        }
        fclose(fin);

        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = 0;
            len--;
        }
        std::string stored_name(buf);

        if (username != stored_name) {
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR_NAME_CAN_NOT_FALL_TO_THE_PROGRAM));
        }
        out_user = username;
        std::cout << "登录成功,欢迎使用Biter.ProgramX-V1.0\n";
    }

    void register_user(const std::wstring& user_data_filepath, std::string& out_user) {
        ErrorDefine ed;
        print_logo();
        std::string username;
        std::cout << "请输入用户名:";
        std::getline(std::cin, username);

        FILE* fout = _wfopen(user_data_filepath.c_str(), L"w");
        if (!fout) {
            throw std::runtime_error(std::to_string(ed.REGISTER_ERROR));
        }
        fputs(username.c_str(), fout);
        fputc('\n', fout);
        fclose(fout);

        out_user = username;
        std::cout << "注册成功,欢迎使用Biter.ProgramX-V1.0\n";
    }

    void choose_login_or_register(const std::wstring& user_data_filepath) {
        while (true) {
            console_clear();
            std::cout << "请选择登录或注册:\n";
            std::cout << "1. 登录\n";
            std::cout << "2. 注册\n";
            std::cout << YOU;

            int choice = 0;
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cerr << "无效输入，请输入数字1或2\n";
                Sleep(800);
                continue;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            try {
                if (choice == 1) {
                    this->login(user_data_filepath, g_app.current_user);
                    break;
                }
                else if (choice == 2) {
                    this->register_user(user_data_filepath, g_app.current_user);
                    break;
                }
                else {
                    std::cerr << "无效的选择，请输入1或2\n";
                    Sleep(800);
                }
            }
            catch (const std::runtime_error& e) {
                std::cerr << "操作失败,ERCD:" << e.what() << "\n";
                std::cout << "按回车继续...";
                std::cin.get();
            }
        }
    }
};

class ShellFunction {
    int open_program(std::string path) {
        int runtime_status = system(path.c_str());
        return runtime_status;
    }
public:
    void shell_loop() {
        console_clear();
        std::cout << "#     # #####   ###  #####  ####  \n"
            << "#         #    #   # #   #  #   #\n"
            << "#     #  #     ##### #####  #   #\n"
            << "#     # #      #   # #  #   #   #\n"
            << "##### # #####  #   # #   #  #### \n";
        std::cout << "        LizardBiterXe CO.       \n";
        std::cout << "[]============Shell===========[]\n";
        std::cout << "输入Help或HELP可以获得部分指令使用方法\n";
        std::cout << "检测到你正在使用开发者版本(预览版),有些功能尚未开发完全\n\n";

        while (!g_app.is_exit) {
            std::string prompt = g_app.current_user + ":";
            std::cout << prompt;
            std::string cmdline;
            if (!std::getline(std::cin, cmdline)) {
                break;
            }
            history_file << cmdline << std::endl;

            if (cmdline == "exit") {
                show_tray_notify(TEXT("Biter.ProgramX-V1.0"), TEXT("感谢使用Biter ProgramX"));
                g_app.is_exit = true;
            }
            else if (cmdline == "Help" || cmdline == "HELP" || cmdline == "help") {
                show_tray_notify(TEXT("Biter.ProgramX-V1.0"), TEXT("Help菜单里有好东西"));
                std::cout << "[]=========帮助=================[]\n";
                std::cout << " exit          退出程序\n";
                std::cout << " Help          帮助\n";
                std::cout << " clean/clear   清屏\n";
                std::cout << " notepad       记事本\n";
                std::cout << " calendar      日历\n";
                std::cout << " clock/time    时钟\n";
                std::cout << " about         关于\n";
                std::cout << " calc          计算器\n";
                std::cout << " ping          连接测试\n";
                std::cout << " todo          TODO列表\n";
                std::cout << " return        返回命令行\n";
                std::cout << " gamelist      游戏菜单\n";
                std::cout << " expend        插件中心\n";
				std::cout << " update		 更新程序(包括预览版)\n";
                std::cout << "[]==============================[]\n";
            }
            else if (cmdline == "clean" || cmdline == "clear") {
                console_clear();
            }
            else if (cmdline == "notepad") {
                notepad();
            }
            else if (cmdline == "calendar") {
                SimpleCalendar();
            }
            else if (cmdline == "clock" || cmdline == "time") {
                TimeClock();
            }
            else if (cmdline == "about") {
                about();
            }
            else if (cmdline == "calc") {
                Calc();
            }
            else if (cmdline == "expend") {
                std::cout << "[]=========插件功能=========[]\n";
                std::cout << "1.打开插件(.cjb) 2.查看插件列表\n";
                std::cout << "3.删除插件 4.添加插件(.cjb)\n";
                int tmp;
                std::cin >> tmp;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                if (tmp == 1) {
                    std::string path;
                    std::cout << "输入插件路径: ";
                    std::getline(std::cin, path);
                    cajn::run_bytecode_file(path);
                }
                else if (tmp == 2) {
                    std::vector<std::string> paths = load_plugin_paths();
                    if (paths.empty()) {
                        std::cout << "（没有已注册插件）\n";
                    }
                    else {
                        std::cout << "已注册插件:\n";
                        for (size_t i = 0; i < paths.size(); ++i) {
                            std::cout << "  " << (i + 1) << ". " << paths[i] << "\n";
                        }
                    }
                }
                else if (tmp == 3) {
                    std::string path;
                    std::cout << "输入插件路径: ";
                    std::getline(std::cin, path);
                    remove_plugin_path(path);
                }
                else if (tmp == 4) {
                    std::string path;
                    std::cout << "输入插件路径: ";
                    std::getline(std::cin, path);
                    add_plugin_path(path);
                }
                else {
                    std::cout << "无效选项\n";
                }
            }
            else if (cmdline == "ping") {
                std::string ip;
                std::cout << "PingIP:";
                std::getline(std::cin, ip);
                if (!ip.empty())
                    system(("ping " + ip).c_str());
                else
                    std::cout << "IP地址不能为空。\n";
            }
            else if (cmdline == "todo") {
                TodoList todo;
                todo.interactive_menu();
            }
            else if (cmdline == "return") {
                std::cout << "输入系统命令，输入 END 退出\n";
                std::string cmd;
                while (true) {
                    std::cout << "CMD> ";
                    if (!std::getline(std::cin, cmd)) break;
                    if (cmd == "END") break;
                    if (!cmd.empty())
                        system(cmd.c_str());
                }
            }
            else if (cmdline == "open") {
                std::string path;
                std::cin >> path;
                this->open_program(path);
                std::cout << "输出的数字代表运行状态" << std::endl;
            }
            else if (cmdline == "gamelist") {
                gamelist gl;
                gl.chooser();
            }
            else if (cmdline.empty()) {
                continue;
            }
            else if(cmdline=="update"){
                std::vector<std::wstring> exts;
                exts.push_back(L".zip");
                exts.push_back(L".exe");

                bool ok = CheckAndDownloadLatestRelease(
                    L"LizardBiterXeBoss-LBXB/Biter-ProgramX", 
                    L"V1.0",                       
                    L".\\Biter.ProgramX-New.exe",
                    exts,
                    L"",
                    true
                );

                std::cout << (ok ? "OK,请删除旧版本" : "FAILED") << std::endl;
            }
            else {
                std::cerr << "未知指令,看看help\n";
            }
        }
    }
};

static void title(std::string title) {
    system(("title " + title).c_str());
}

LoginFunction logfunc;
ShellFunction shlfunc;
int main() {
    try {
        g_app.exe_dir = get_exe_directory();
        g_app.data_dir = g_app.exe_dir + L"BiterData\\";
        create_directory_if_not_exist(g_app.data_dir);

        history_file.open(".\\BiterData\\History.data", std::ios::app);
        if (!history_file.is_open()) {
            std::cerr << "无法创建历史记录文件\n";
            return 1;
        }

        std::wstring user_file = g_app.data_dir + L"Biter.Safe.Login.UserData.data";
        title("initing...");
        if (!init_tray_window()) {
            MessageBox(NULL, TEXT("托盘组件初始化失败"), TEXT("警告"), MB_OK | MB_ICONWARNING);
        }
        else {
            show_tray_notify(TEXT("Biter.ProgramX-V1.0"), TEXT("HI~欢迎使用Biter ProgramX"));
        }

        logfunc.prompt_return_console();
        if (g_app.is_exit) {
            cleanup_tray();
            return 0;
        }
        title("loging...");
        logfunc.choose_login_or_register(user_file);
        title("SHELL");
        show_tray_notify(TEXT("Biter.ProgramX-V1.0"), TEXT("成功进入Shell\n开始使用吧!"));

        std::vector<std::string> auto_paths = load_plugin_paths();
        for (size_t i = 0; i < auto_paths.size(); ++i) {
            std::cout << "[auto] 运行插件: " << auto_paths[i] << "\n";
            cajn::run_bytecode_file(auto_paths[i]);
        }

        shlfunc.shell_loop();

        cleanup_tray();
    }
    catch (...) {
        std::cerr << "程序异常退出,ERCD:" << ErrorDefine::START_ERROR << "\n";
    }
    return 0;
}