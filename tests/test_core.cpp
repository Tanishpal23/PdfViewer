#include "Database.h"
#include "Document.h"
#include <fpdfview.h>
#include <iostream>
#include <cassert>

void RunTests() {
    std::cout << "============================================" << std::endl;
    std::cout << "RUNNING PDFVIEWER CORE TEST SUITE" << std::endl;
    std::cout << "============================================" << std::endl;

    // Initialize PDFium
    FPDF_LIBRARY_CONFIG config = {};
    config.version = 2;
    FPDF_InitLibraryWithConfig(&config);

    // 1. Test opening 5 PDFs simultaneously
    std::cout << "\n[TEST 1] Opening 5 PDFs simultaneously..." << std::endl;
    std::vector<std::wstring> pdfFiles = {
        L"sample_docs/1_Quick_Start_Guide.pdf",
        L"sample_docs/2_Windows_Architecture.pdf",
        L"sample_docs/3_PdfViewer_Manual.pdf",
        L"sample_docs/4_Research_Paper.pdf",
        L"sample_docs/5_Large_Encyclopedia.pdf"
    };

    std::vector<std::shared_ptr<Document>> docs;
    for (const auto& path : pdfFiles) {
        auto doc = std::make_shared<Document>();
        bool ok = doc->Open(path);
        assert(ok && "Failed to open PDF");
        std::wcout << L"  Loaded: " << doc->GetFileName() 
                   << L" (" << doc->GetPageCount() << L" pages)" << std::endl;
        docs.push_back(doc);
    }
    assert(docs.size() == 5);
    std::cout << "-> Test 1 PASSED: All 5 PDFs loaded simultaneously!" << std::endl;

    // 2. Test page count & large document handling
    std::cout << "\n[TEST 2] Verifying page counts & large document..." << std::endl;
    assert(docs[0]->GetPageCount() == 3);
    assert(docs[1]->GetPageCount() == 10);
    assert(docs[2]->GetPageCount() == 25);
    assert(docs[3]->GetPageCount() == 50);
    assert(docs[4]->GetPageCount() == 200); // 200 page large PDF
    std::cout << "  Large PDF page count: " << docs[4]->GetPageCount() << std::endl;
    std::cout << "-> Test 2 PASSED: Large PDF with 200 pages verified!" << std::endl;

    // 3. Test page rendering (lazy bitmap generation)
    std::cout << "\n[TEST 3] Testing lazy page rendering..." << std::endl;
    int w = 0, h = 0;
    HBITMAP bmp = docs[4]->GetPageBitmap(0, 1.0, 0, &w, &h);
    assert(bmp != nullptr && "Page 0 failed to render");
    assert(w > 0 && h > 0);
    std::cout << "  Page 0 bitmap rendered: " << w << "x" << h << " pixels" << std::endl;

    HBITMAP thumb = docs[4]->GetThumbnailBitmap(0, 150, 150);
    assert(thumb != nullptr && "Thumbnail failed to render");
    std::cout << "  Thumbnail rendered successfully." << std::endl;
    std::cout << "-> Test 3 PASSED: Lazy rendering & thumbnails work!" << std::endl;

    // 4. Test text extraction and search
    std::cout << "\n[TEST 4] Testing text extraction & search..." << std::endl;
    std::wstring pageText = docs[4]->GetPageText(0);
    std::wcout << L"  Extracted sample text: " << pageText.substr(0, 60) << L"..." << std::endl;
    assert(!pageText.empty());

    auto matches = docs[4]->Search(L"PersistenceSQLite");
    std::cout << "  Found " << matches.size() << " occurrences of 'PersistenceSQLite'" << std::endl;
    assert(!matches.empty());
    std::cout << "-> Test 4 PASSED: Text extraction and search work!" << std::endl;

    // 5. Test SQLite reading progress persistence
    std::cout << "\n[TEST 5] Testing SQLite Reading Progress persistence..." << std::endl;
    Database db;
    bool dbOk = db.Open(L"test_history.db");
    assert(dbOk && "Failed to open test database");

    // Save progress for each document
    for (size_t i = 0; i < docs.size(); ++i) {
        DocumentProgress prog;
        prog.filePath = pdfFiles[i];
        prog.fileSize = 1000 + i;
        prog.lastModified = 123456789;
        prog.fileHash = Database::ComputeFileHash(pdfFiles[i]);
        prog.lastPage = static_cast<int>(i * 5); // navigate to different pages
        prog.scrollX = static_cast<int>(i * 10);
        prog.scrollY = static_cast<int>(i * 250);
        prog.zoomLevel = 1.0 + (i * 0.25);
        prog.zoomMode = 0;
        prog.rotation = (i % 2 == 1) ? 90 : 0;
        prog.totalPages = docs[i]->GetPageCount();

        bool saved = db.SaveProgress(prog);
        assert(saved && "Failed to save document progress");
        std::wcout << L"  Saved progress for " << pdfFiles[i]
                   << L" -> Page: " << prog.lastPage
                   << L", ScrollY: " << prog.scrollY
                   << L", Zoom: " << prog.zoomLevel
                   << L", Rotation: " << prog.rotation << std::endl;
    }

    // Verify reading positions retrieved from database
    std::cout << "\n[TEST 6] Verifying reading progress restoration..." << std::endl;
    for (size_t i = 0; i < docs.size(); ++i) {
        auto optProg = db.GetProgressByPath(pdfFiles[i]);
        assert(optProg.has_value());
        assert(optProg->lastPage == static_cast<int>(i * 5));
        assert(optProg->scrollY == static_cast<int>(i * 250));
        assert(optProg->rotation == ((i % 2 == 1) ? 90 : 0));
        std::wcout << L"  Verified restored progress for " << pdfFiles[i]
                   << L": Resumed exactly at Page " << optProg->lastPage
                   << L", ScrollY " << optProg->scrollY << std::endl;
    }
    std::cout << "-> Test 6 PASSED: Reading progress accurately restored for all documents!" << std::endl;

    // 7. Test session tab saving & restoration
    std::cout << "\n[TEST 7] Testing session tabs save and restore..." << std::endl;
    std::vector<TabSessionItem> sessionTabs;
    for (size_t i = 0; i < pdfFiles.size(); ++i) {
        TabSessionItem item;
        item.tabIndex = static_cast<int>(i);
        item.filePath = pdfFiles[i];
        item.isActive = (i == 2); // Tab 2 active
        sessionTabs.push_back(item);
    }
    db.SaveSession(sessionTabs);

    auto restoredTabs = db.LoadSession();
    assert(restoredTabs.size() == 5);
    assert(restoredTabs[2].isActive == true);
    std::cout << "  Restored 5 session tabs, active tab index: 2" << std::endl;
    std::cout << "-> Test 7 PASSED: Session tabs restore verified!" << std::endl;

    // 8. Test file relocation / metadata matching
    std::cout << "\n[TEST 8] Testing moved/renamed file metadata matching..." << std::endl;
    std::string hash = Database::ComputeFileHash(pdfFiles[4]);
    auto optReloc = db.FindProgressByMetadata(1000 + 4, hash);
    assert(optReloc.has_value());
    assert(optReloc->lastPage == 20); // 4 * 5
    std::cout << "  Found relocated document by content hash & size! Last page: " << optReloc->lastPage << std::endl;
    std::cout << "-> Test 8 PASSED: Relocated file progress matching works!" << std::endl;

    // 9. Clean up
    docs.clear(); // Close all documents BEFORE destroying PDFium library!
    db.Close();
    DeleteFileW(L"test_history.db");
    DeleteFileW(L"test_history.db-wal");
    DeleteFileW(L"test_history.db-shm");

    FPDF_DestroyLibrary();

    std::cout << "\n============================================" << std::endl;
    std::cout << "ALL TESTS PASSED SUCCESSFULLY! (100% SUCCESS)" << std::endl;
    std::cout << "============================================" << std::endl;
}

int main() {
    RunTests();
    return 0;
}
