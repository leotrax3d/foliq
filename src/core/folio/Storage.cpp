// SPDX-License-Identifier: GPL-2.0-or-later
#include "Storage.h"

#include <map>
#include <mutex>
#include <stdexcept>

#include <glib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace foliq {
FileLock::FileLock(const fs::path& path) {
#ifdef _WIN32
    handle = CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Datei gesperrt / zweite App-Instanz: " + path.u8string());
#else
    handle = ::open(path.c_str(), O_CREAT | O_RDWR, 0600);
    if (handle < 0)
        throw std::runtime_error("Sperrdatei kann nicht geöffnet werden: " + path.u8string());
    if (flock(handle, LOCK_EX | LOCK_NB) != 0) {
        ::close(handle);
        throw std::runtime_error("Datei gesperrt / zweite App-Instanz: " + path.u8string());
    }
#endif
}
FileLock::~FileLock() {
#ifdef _WIN32
    CloseHandle(handle);
#else
    ::close(handle);
#endif
}
std::string read(const fs::path& path) {
    gchar* data = nullptr;
    gsize size = 0;
    GError* error = nullptr;
    if (!g_file_get_contents(path.u8string().c_str(), &data, &size, &error)) {
        std::string message = error->message;
        g_error_free(error);
        throw std::runtime_error(message);
    }
    std::string result(data, size);
    g_free(data);
    return result;
}
std::string fingerprint(const fs::path& path) {
    if (!fs::exists(path))
        return "missing";
    auto bytes = read(path);
    auto* sum =
            g_compute_checksum_for_data(G_CHECKSUM_SHA256, reinterpret_cast<const guchar*>(bytes.data()), bytes.size());
    std::string result(sum);
    g_free(sum);
    return result;
}
static void replace(const fs::path& path, const std::string& bytes) {
    GError* error = nullptr;
    if (!g_file_set_contents_full(
                path.u8string().c_str(), bytes.data(), bytes.size(),
                static_cast<GFileSetContentsFlags>(G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE), 0600,
                &error)) {
        std::string message = error->message;
        g_error_free(error);
        throw std::runtime_error(message);
    }
}
void atomicWrite(const fs::path& path, const std::string& bytes) {
    if (fs::exists(path))
        replace(fs::path(path).concat(".bak"), read(path));
    replace(path, bytes);
}
static std::mutex documentMutex;
static std::map<fs::path, std::string> documents;
void watchDocument(const fs::path& path) {
    std::lock_guard<std::mutex> guard(documentMutex);
    documents[fs::absolute(path).lexically_normal()] = fingerprint(path);
}
void checkDocument(const fs::path& path) {
    std::lock_guard<std::mutex> guard(documentMutex);
    auto it = documents.find(fs::absolute(path).lexically_normal());
    if (it != documents.end() && it->second != fingerprint(path))
        throw std::runtime_error("Speicherkonflikt: Datei wurde außerhalb dieser Instanz geändert. Änderungen mit "
                                 "Speichern unter sichern.");
}
void acceptDocument(const fs::path& path) { watchDocument(path); }
}  // namespace foliq
