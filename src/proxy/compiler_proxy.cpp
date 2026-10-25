#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#ifndef SO_EXPECTED_PLUGIN_SHA256
#error Build the separate business plugin first and define its exact SHA-256.
#endif
#ifndef SO_PROXY_PLUGIN_PATH
#define SO_PROXY_PLUGIN_PATH L"\\plugin\\StrongholdProtocol\\StrongholdBusinessListener.dll"
#endif
#ifndef SO_PROXY_CONFIG_PATH
#define SO_PROXY_CONFIG_PATH L"\\plugin\\StrongholdProtocol\\compiler-trial.ini"
#endif
#ifndef SO_PROXY_CAPTURE_SUFFIX
#define SO_PROXY_CAPTURE_SUFFIX L"\\StrongholdProtocol\\captures\\TC-native"
#endif

extern "C" { FARPROC SO_CompilerTargets[30]{}; }
namespace {
constexpr char kOriginalHash[] = "21df5696011156fe64f2dff47c8ed5e90817021f91f70b6d9707fd58cd1b0b81";
constexpr char kPluginHash[] = SO_EXPECTED_PLUGIN_SHA256;
struct ExportSpec { uint32_t ordinal; const char* name; };
constexpr ExportSpec exports[] = {
#define SO_COMPILER_EXPORT(ordinal, name) {ordinal, #name},
#include "compiler_exports.inc"
#undef SO_COMPILER_EXPORT
};
static_assert(sizeof(exports) / sizeof(exports[0]) == 29, "Verified TC D3DCompiler export set");
HMODULE self_module = nullptr;
std::atomic<bool> original_loaded{false};
std::atomic<bool> original_hash_verified{false};
std::atomic<DWORD> original_error{ERROR_SUCCESS};

std::wstring module_path(HMODULE module) {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) return {};
    return std::wstring(buffer.data(), length);
}
std::wstring directory(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
}
bool equal_path(const std::wstring& a, const std::wstring& b) {
    return !a.empty() && !b.empty() && _wcsicmp(a.c_str(), b.c_str()) == 0;
}
bool hash_matches(const std::wstring& path, const char* expected) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, returned = 0;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size),
                          sizeof(object_size), &returned, 0) >= 0;
    std::vector<unsigned char> object(ok ? object_size : 0);
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536];
    while (ok) {
        DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, buffer, read, 0) >= 0;
    }
    unsigned char digest[32]{};
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (!ok) return false;
    constexpr char hex[] = "0123456789abcdef";
    char actual[65]{};
    for (size_t i = 0; i < 32; ++i) { actual[i * 2] = hex[digest[i] >> 4]; actual[i * 2 + 1] = hex[digest[i] & 15]; }
    return std::strcmp(actual, expected) == 0;
}
bool load_original() noexcept {
    if (original_loaded.load(std::memory_order_acquire)) return true;
    try {
        const auto path = directory(module_path(self_module)) + L"\\d3dcompiler_47.original.dll";
        HMODULE original = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!original) { original_error.store(GetLastError()); return false; }
        if (!equal_path(module_path(original), path)) { original_error.store(ERROR_BAD_PATHNAME); return false; }
        for (const auto& spec : exports) {
            FARPROC named = GetProcAddress(original, spec.name);
            FARPROC ordinal = GetProcAddress(original, MAKEINTRESOURCEA(spec.ordinal));
            if (!named || named != ordinal) { original_error.store(ERROR_PROC_NOT_FOUND); return false; }
            InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&SO_CompilerTargets[spec.ordinal]),
                                       reinterpret_cast<PVOID>(named));
        }
        original_loaded.store(true, std::memory_order_release);
    } catch (...) { original_error.store(ERROR_NOT_ENOUGH_MEMORY); }
    // No additional initialization lock: a caller inside another DLL's DllMain
    // must never wait for our worker while that worker waits for the loader lock.
    // Concurrent first calls may retain a few extra process-lifetime references.
    return original_loaded.load(std::memory_order_acquire);
}
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!size) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}
std::string json_string(const std::wstring& value) {
    const auto bytes = utf8(value);
    std::string output = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char byte : bytes) {
        if (byte == '\\' || byte == '"') { output += '\\'; output += static_cast<char>(byte); }
        else if (byte < 32) { output += "\\u00"; output += hex[byte >> 4]; output += hex[byte & 15]; }
        else output += static_cast<char>(byte);
    }
    return output + '"';
}
void create_directories(const std::wstring& path) {
    for (size_t position = 3; position < path.size(); ++position)
        if (path[position] == L'\\' || path[position] == L'/') CreateDirectoryW(path.substr(0, position).c_str(), nullptr);
    CreateDirectoryW(path.c_str(), nullptr);
}
void write_status(const std::wstring& executable, const std::wstring& proxy, const std::wstring& plugin,
                  const char* role, uint32_t mode, bool plugin_hash, bool plugin_loaded,
                  const char* stage, DWORD error, int32_t bootstrap_result) {
    wchar_t local[32768]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    if (!length || length >= 32768) return;
    std::wstring folder = std::wstring(local) + SO_PROXY_CAPTURE_SUFFIX;
    create_directories(folder);
    const std::wstring path = folder + L"\\compiler-proxy-" + std::to_wstring(GetCurrentProcessId()) + L".json";
    const std::wstring temporary = path + L".tmp";
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    const uint64_t started = (static_cast<uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    SYSTEMTIME now{};
    GetSystemTime(&now);
    char utc[40]{};
    std::snprintf(utc, sizeof(utc), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    const std::string json = "{\"version\":\"0.5.0\",\"pid\":" + std::to_string(GetCurrentProcessId()) +
        ",\"processStartedFileTime\":\"" + std::to_string(started) + "\",\"updatedUtc\":\"" + utc + "\"" +
        ",\"processRole\":\"" + role + "\",\"executable\":" + json_string(executable) +
        ",\"proxyPath\":" + json_string(proxy) + ",\"originalPath\":" +
        json_string(directory(proxy) + L"\\d3dcompiler_47.original.dll") + ",\"pluginPath\":" + json_string(plugin) +
        ",\"originalHashVerified\":" + (original_hash_verified.load() ? "true" : "false") +
        ",\"originalLoaded\":" + (original_loaded.load() ? "true" : "false") +
        ",\"pluginHashVerified\":" + (plugin_hash ? "true" : "false") +
        ",\"pluginLoaded\":" + (plugin_loaded ? "true" : "false") +
        ",\"mode\":" + std::to_string(mode) + ",\"errorStage\":\"" + stage +
        "\",\"errorCode\":" + std::to_string(error) + ",\"bootstrapResult\":" + std::to_string(bootstrap_result) +
        ",\"actualGameLoadValidated\":false,\"actualGameCaptureValidated\":false}\n";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    bool ok = WriteFile(file, json.data(), static_cast<DWORD>(json.size()), &written, nullptr) && written == json.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (ok) MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
DWORD WINAPI dependency_worker(LPVOID) {
    std::wstring executable, proxy, plugin;
    const char* role = "other";
    const char* stage = "worker";
    DWORD error = ERROR_SUCCESS;
    uint32_t mode = 0;
    bool plugin_hash = false, plugin_loaded = false;
    int32_t accepted = -1;
    try {
        executable = module_path(nullptr);
        proxy = module_path(self_module);
        const auto game_root = directory(proxy);
        plugin = game_root + SO_PROXY_PLUGIN_PATH;
        role = equal_path(proxy, game_root + L"\\d3dcompiler_47.dll")
            ? (equal_path(executable, game_root + L"\\Arknights.exe") ? "main" : "helper") : "wrong-location";
        original_hash_verified.store(hash_matches(directory(proxy) + L"\\d3dcompiler_47.original.dll", kOriginalHash));
        load_original();
        if (!original_loaded.load()) { stage = "original-library"; error = original_error.load(); }
        else if (!equal_path(proxy, game_root + L"\\d3dcompiler_47.dll")) {
            role = "wrong-location"; stage = "process-gate"; error = ERROR_BAD_PATHNAME;
        } else if (!equal_path(executable, game_root + L"\\Arknights.exe")) {
            role = "helper"; stage = "helper-forwarding-only";
        } else {
            role = "main";
            const auto config = game_root + SO_PROXY_CONFIG_PATH;
            wchar_t selected[32]{};
            GetPrivateProfileStringW(L"listener", L"mode", L"probe", selected, 32, config.c_str());
            if (_wcsicmp(selected, L"probe") == 0) mode = 0;
            else if (_wcsicmp(selected, L"discover") == 0) mode = 1;
            else if (_wcsicmp(selected, L"listen") == 0) mode = 2;
            else { stage = "configuration"; error = ERROR_INVALID_DATA; }
            if (!original_hash_verified.load()) { stage = "original-hash-plugin-disabled"; error = ERROR_CRC; }
            const bool enabled = GetPrivateProfileIntW(L"listener", L"enabled", 1, config.c_str()) != 0;
            if (!error && !enabled) stage = "plugin-disabled";
            if (!error && enabled) {
                if (!hash_matches(plugin, kPluginHash)) { stage = "plugin-hash"; error = ERROR_CRC; }
                else {
                    plugin_hash = true;
                    HMODULE body = LoadLibraryExW(plugin.c_str(), nullptr,
                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
                    if (!body) { stage = "plugin-load"; error = GetLastError(); }
                    else if (!equal_path(module_path(body), plugin)) { stage = "plugin-module-path"; error = ERROR_BAD_PATHNAME; }
                    else {
                        plugin_loaded = true;
                        using Bootstrap = int32_t (__cdecl*)(const wchar_t*, uint32_t);
                        FARPROC raw = GetProcAddress(body, "SO_BootstrapFromDependency");
                        Bootstrap bootstrap = nullptr;
                        std::memcpy(&bootstrap, &raw, sizeof(bootstrap));
                        if (!bootstrap) { stage = "plugin-export"; error = ERROR_PROC_NOT_FOUND; }
                        else {
                            accepted = bootstrap(proxy.c_str(), mode);
                            stage = accepted == 0 ? "plugin-bootstrap-accepted" : "plugin-bootstrap-rejected";
                            if (accepted != 0) error = ERROR_DLL_INIT_FAILED;
                        }
                    }
                }
            }
        }
    } catch (...) { stage = "worker-exception"; error = ERROR_NOT_ENOUGH_MEMORY; }
    try { write_status(executable, proxy, plugin, role, mode, plugin_hash, plugin_loaded, stage, error, accepted); } catch (...) {}
    return 0;
}
}

extern "C" FARPROC SO_ResolveCompilerExport(uint32_t ordinal) noexcept {
    if (ordinal > 0 && ordinal < 30) {
        FARPROC target = reinterpret_cast<FARPROC>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(&SO_CompilerTargets[ordinal]), nullptr, nullptr));
        if (target) return target;
        if (load_original()) return reinterpret_cast<FARPROC>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(&SO_CompilerTargets[ordinal]), nullptr, nullptr));
    }
    // Unknown D3DCompiler signatures cannot be replaced with a guessed return value.
    RaiseFailFastException(nullptr, nullptr, 0);
    TerminateProcess(GetCurrentProcess(), ERROR_PROC_NOT_FOUND);
    return nullptr;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = module;
        DisableThreadLibraryCalls(module);
        // Acquire residency before starting a worker. The thread cannot execute
        // its entry point until loader notifications complete; never wait here.
        HMODULE pinned = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&dependency_worker), &pinned)) {
            HANDLE worker = CreateThread(nullptr, 0, dependency_worker, nullptr, 0, nullptr);
            if (worker) CloseHandle(worker);
        }
    }
    return TRUE;
}
