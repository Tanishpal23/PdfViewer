#include "Database.h"
#include "sqlite3.h"
#include <fstream>
#include <iostream>

static std::wstring GetDefaultDatabasePath() {
    // Determine AppData folder: %APPDATA%\PdfViewer\history.db
    WCHAR appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\PdfViewer";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\history.db";
    }
    return L"history.db";
}

Database::Database() {
}

Database::~Database() {
    Close();
}

bool Database::Open(const std::wstring& dbPath) {
    Close();
    m_dbPath = dbPath.empty() ? GetDefaultDatabasePath() : dbPath;

    sqlite3* db = nullptr;
    int rc = sqlite3_open16(m_dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        m_db = nullptr;
        return false;
    }

    m_db = db;
    // Enable WAL mode for high performance concurrent offline reads/writes
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    CreateTables();
    return true;
}

void Database::Close() {
    if (m_db) {
        sqlite3_close(reinterpret_cast<sqlite3*>(m_db));
        m_db = nullptr;
    }
}

void Database::CreateTables() {
    if (!m_db) return;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql =
        "CREATE TABLE IF NOT EXISTS documents ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT UNIQUE NOT NULL,"
        "  file_size INTEGER,"
        "  last_modified INTEGER,"
        "  file_hash TEXT,"
        "  last_page INTEGER DEFAULT 0,"
        "  scroll_x INTEGER DEFAULT 0,"
        "  scroll_y INTEGER DEFAULT 0,"
        "  zoom_level REAL DEFAULT 1.0,"
        "  zoom_mode INTEGER DEFAULT 0,"
        "  rotation INTEGER DEFAULT 0,"
        "  total_pages INTEGER DEFAULT 0,"
        "  last_opened INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_docs_hash ON documents(file_hash, file_size);"
        "CREATE INDEX IF NOT EXISTS idx_docs_opened ON documents(last_opened DESC);"
        ""
        "CREATE TABLE IF NOT EXISTS session_tabs ("
        "  tab_index INTEGER PRIMARY KEY,"
        "  file_path TEXT NOT NULL,"
        "  is_active INTEGER DEFAULT 0"
        ");"
        ""
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ");";

    char* errMsg = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }
}

bool Database::SaveProgress(const DocumentProgress& progress) {
    if (!m_db || progress.filePath.empty()) return false;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql =
        "INSERT INTO documents (file_path, file_size, last_modified, file_hash, "
        "last_page, scroll_x, scroll_y, zoom_level, zoom_mode, rotation, total_pages, last_opened) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12) "
        "ON CONFLICT(file_path) DO UPDATE SET "
        "file_size=excluded.file_size, last_modified=excluded.last_modified, file_hash=excluded.file_hash, "
        "last_page=excluded.last_page, scroll_x=excluded.scroll_x, scroll_y=excluded.scroll_y, "
        "zoom_level=excluded.zoom_level, zoom_mode=excluded.zoom_mode, rotation=excluded.rotation, "
        "total_pages=excluded.total_pages, last_opened=excluded.last_opened;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string pathUtf8 = Utf16ToUtf8(progress.filePath);
    int64_t now = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, progress.fileSize);
    sqlite3_bind_int64(stmt, 3, progress.lastModified);
    sqlite3_bind_text(stmt, 4, progress.fileHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, progress.lastPage);
    sqlite3_bind_int(stmt, 6, progress.scrollX);
    sqlite3_bind_int(stmt, 7, progress.scrollY);
    sqlite3_bind_double(stmt, 8, progress.zoomLevel);
    sqlite3_bind_int(stmt, 9, progress.zoomMode);
    sqlite3_bind_int(stmt, 10, progress.rotation);
    sqlite3_bind_int(stmt, 11, progress.totalPages);
    sqlite3_bind_int64(stmt, 12, now);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::optional<DocumentProgress> Database::GetProgressByPath(const std::wstring& filePath) {
    if (!m_db || filePath.empty()) return std::nullopt;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql =
        "SELECT file_path, file_size, last_modified, file_hash, last_page, scroll_x, scroll_y, "
        "zoom_level, zoom_mode, rotation, total_pages, last_opened FROM documents WHERE file_path = ?1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    std::string pathUtf8 = Utf16ToUtf8(filePath);
    sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<DocumentProgress> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        DocumentProgress p;
        p.filePath = Utf8ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        p.fileSize = sqlite3_column_int64(stmt, 1);
        p.lastModified = sqlite3_column_int64(stmt, 2);
        const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.fileHash = hash ? hash : "";
        p.lastPage = sqlite3_column_int(stmt, 4);
        p.scrollX = sqlite3_column_int(stmt, 5);
        p.scrollY = sqlite3_column_int(stmt, 6);
        p.zoomLevel = sqlite3_column_double(stmt, 7);
        p.zoomMode = sqlite3_column_int(stmt, 8);
        p.rotation = sqlite3_column_int(stmt, 9);
        p.totalPages = sqlite3_column_int(stmt, 10);
        p.lastOpenedTime = sqlite3_column_int64(stmt, 11);
        result = p;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<DocumentProgress> Database::FindProgressByMetadata(int64_t fileSize, const std::string& fileHash) {
    if (!m_db || fileHash.empty() || fileSize <= 0) return std::nullopt;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql =
        "SELECT file_path, file_size, last_modified, file_hash, last_page, scroll_x, scroll_y, "
        "zoom_level, zoom_mode, rotation, total_pages, last_opened FROM documents "
        "WHERE file_hash = ?1 AND file_size = ?2 ORDER BY last_opened DESC LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, fileHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, fileSize);

    std::optional<DocumentProgress> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        DocumentProgress p;
        p.filePath = Utf8ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        p.fileSize = sqlite3_column_int64(stmt, 1);
        p.lastModified = sqlite3_column_int64(stmt, 2);
        const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.fileHash = hash ? hash : "";
        p.lastPage = sqlite3_column_int(stmt, 4);
        p.scrollX = sqlite3_column_int(stmt, 5);
        p.scrollY = sqlite3_column_int(stmt, 6);
        p.zoomLevel = sqlite3_column_double(stmt, 7);
        p.zoomMode = sqlite3_column_int(stmt, 8);
        p.rotation = sqlite3_column_int(stmt, 9);
        p.totalPages = sqlite3_column_int(stmt, 10);
        p.lastOpenedTime = sqlite3_column_int64(stmt, 11);
        result = p;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Database::UpdateDocumentPath(const std::wstring& oldPath, const std::wstring& newPath) {
    if (!m_db || oldPath.empty() || newPath.empty()) return false;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql = "UPDATE documents SET file_path = ?1 WHERE file_path = ?2;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string newUtf8 = Utf16ToUtf8(newPath);
    std::string oldUtf8 = Utf16ToUtf8(oldPath);
    sqlite3_bind_text(stmt, 1, newUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, oldUtf8.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<DocumentProgress> Database::GetRecentDocuments(int limit) {
    std::vector<DocumentProgress> results;
    if (!m_db) return results;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql =
        "SELECT file_path, file_size, last_modified, file_hash, last_page, scroll_x, scroll_y, "
        "zoom_level, zoom_mode, rotation, total_pages, last_opened FROM documents "
        "ORDER BY last_opened DESC LIMIT ?1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DocumentProgress p;
        p.filePath = Utf8ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        p.fileSize = sqlite3_column_int64(stmt, 1);
        p.lastModified = sqlite3_column_int64(stmt, 2);
        const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.fileHash = hash ? hash : "";
        p.lastPage = sqlite3_column_int(stmt, 4);
        p.scrollX = sqlite3_column_int(stmt, 5);
        p.scrollY = sqlite3_column_int(stmt, 6);
        p.zoomLevel = sqlite3_column_double(stmt, 7);
        p.zoomMode = sqlite3_column_int(stmt, 8);
        p.rotation = sqlite3_column_int(stmt, 9);
        p.totalPages = sqlite3_column_int(stmt, 10);
        p.lastOpenedTime = sqlite3_column_int64(stmt, 11);
        results.push_back(p);
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::SaveSession(const std::vector<TabSessionItem>& tabs) {
    if (!m_db) return false;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "DELETE FROM session_tabs;", nullptr, nullptr, nullptr);

    const char* sql = "INSERT INTO session_tabs (tab_index, file_path, is_active) VALUES (?1, ?2, ?3);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& tab : tabs) {
            std::string pathUtf8 = Utf16ToUtf8(tab.filePath);
            sqlite3_bind_int(stmt, 1, tab.tabIndex);
            sqlite3_bind_text(stmt, 2, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, tab.isActive ? 1 : 0);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

std::vector<TabSessionItem> Database::LoadSession() {
    std::vector<TabSessionItem> tabs;
    if (!m_db) return tabs;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql = "SELECT tab_index, file_path, is_active FROM session_tabs ORDER BY tab_index ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return tabs;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TabSessionItem item;
        item.tabIndex = sqlite3_column_int(stmt, 0);
        item.filePath = Utf8ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        item.isActive = (sqlite3_column_int(stmt, 2) != 0);
        tabs.push_back(item);
    }

    sqlite3_finalize(stmt);
    return tabs;
}

bool Database::ClearSession() {
    if (!m_db) return false;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);
    return sqlite3_exec(db, "DELETE FROM session_tabs;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::SaveSetting(const std::string& key, const std::string& value) {
    if (!m_db || key.empty()) return false;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string Database::GetSetting(const std::string& key, const std::string& defaultValue) {
    if (!m_db || key.empty()) return defaultValue;
    sqlite3* db = reinterpret_cast<sqlite3*>(m_db);

    const char* sql = "SELECT value FROM settings WHERE key = ?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return defaultValue;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::string val = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) val = text;
    }

    sqlite3_finalize(stmt);
    return val;
}

bool Database::SaveAppSettings(const AppSettings& settings) {
    SaveSetting("theme_dark", settings.isDarkMode ? "1" : "0");
    SaveSetting("window_x", std::to_string(settings.windowX));
    SaveSetting("window_y", std::to_string(settings.windowY));
    SaveSetting("window_w", std::to_string(settings.windowWidth));
    SaveSetting("window_h", std::to_string(settings.windowHeight));
    SaveSetting("window_max", settings.isMaximized ? "1" : "0");
    SaveSetting("sidebar_visible", settings.sidebarVisible ? "1" : "0");
    SaveSetting("sidebar_width", std::to_string(settings.sidebarWidth));
    return true;
}

AppSettings Database::LoadAppSettings() {
    AppSettings s;
    s.isDarkMode = (GetSetting("theme_dark", "1") == "1");
    s.windowX = std::stoi(GetSetting("window_x", "100"));
    s.windowY = std::stoi(GetSetting("window_y", "100"));
    s.windowWidth = std::stoi(GetSetting("window_w", "1200"));
    s.windowHeight = std::stoi(GetSetting("window_h", "800"));
    s.isMaximized = (GetSetting("window_max", "0") == "1");
    s.sidebarVisible = (GetSetting("sidebar_visible", "1") == "1");
    s.sidebarWidth = std::stoi(GetSetting("sidebar_width", "220"));
    return s;
}

std::string Database::ComputeFileHash(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return "";
    }

    // Read first 64KB
    constexpr DWORD BUFFER_SIZE = 65536;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer.data(), BUFFER_SIZE, &bytesRead, NULL);
    CloseHandle(hFile);

    // Compute 64-bit FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (DWORD i = 0; i < bytesRead; ++i) {
        hash ^= buffer[i];
        hash *= 1099511628211ULL;
    }
    // Also fold in file size
    hash ^= static_cast<uint64_t>(size.QuadPart);
    hash *= 1099511628211ULL;

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

