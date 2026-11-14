#pragma once

#include "Common.h"
#include <string>
#include <vector>
#include <optional>

struct DocumentProgress {
    std::wstring filePath;
    int64_t fileSize = 0;
    int64_t lastModified = 0;
    std::string fileHash;
    int lastPage = 0;
    int scrollX = 0;
    int scrollY = 0;
    double zoomLevel = 1.0;
    int zoomMode = 0; // 0: Custom, 1: FitWidth, 2: FitPage, 3: ActualSize
    int rotation = 0; // 0, 90, 180, 270
    int totalPages = 0;
    int64_t lastOpenedTime = 0;
};

struct TabSessionItem {
    int tabIndex = 0;
    std::wstring filePath;
    bool isActive = false;
};

struct AppSettings {
    bool isDarkMode = true;
    int windowX = 100;
    int windowY = 100;
    int windowWidth = 1200;
    int windowHeight = 800;
    bool isMaximized = false;
    bool sidebarVisible = true;
    int sidebarWidth = 220;
};

class Database {
public:
    Database();
    ~Database();

    bool Open(const std::wstring& dbPath = L"");
    void Close();

    // Document Reading Progress
    bool SaveProgress(const DocumentProgress& progress);
    std::optional<DocumentProgress> GetProgressByPath(const std::wstring& filePath);
    std::optional<DocumentProgress> FindProgressByMetadata(int64_t fileSize, const std::string& fileHash);
    bool UpdateDocumentPath(const std::wstring& oldPath, const std::wstring& newPath);
    std::vector<DocumentProgress> GetRecentDocuments(int limit = 20);

    // Session Management (Tabs)
    bool SaveSession(const std::vector<TabSessionItem>& tabs);
    std::vector<TabSessionItem> LoadSession();
    bool ClearSession();

    // App Settings
    bool SaveSetting(const std::string& key, const std::string& value);
    std::string GetSetting(const std::string& key, const std::string& defaultValue = "");
    bool SaveAppSettings(const AppSettings& settings);
    AppSettings LoadAppSettings();

    // Helper: compute quick hash of first 64KB + size
    static std::string ComputeFileHash(const std::wstring& path);

private:
    void* m_db = nullptr; // sqlite3* pointer
    std::wstring m_dbPath;

    void CreateTables();
};

