// TorGames.Binder - Main window
#pragma once

#include "../core/common.h"
#include "../builder/Builder.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    // Initialize and create the window
    bool Create(HINSTANCE hInstance);

    // Run the message loop
    int Run();

    // Get the window handle
    HWND GetHandle() const { return m_hwnd; }

private:
    HWND m_hwnd;
    HWND m_listView;
    HWND m_statusBar;
    HINSTANCE m_hInstance;
    BinderConfig m_config;
    Builder m_builder;
    HIMAGELIST m_imageList;

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // UI creation
    void CreateControls();
    void CreateListView();
    void CreateToolbar();
    void CreateStatusBar();

    // List view operations
    void RefreshListView();
    void AddFileToList(const BoundFileConfig& file);
    int GetSelectedIndex();
    void UpdateStatusBar();

    // Command handlers
    void OnAddFile();
    void OnRemoveFile();
    void OnMoveUp();
    void OnMoveDown();
    void OnFileSettings();
    void OnGlobalSettings();
    void OnBuild();

    // Dialog procedures
    static INT_PTR CALLBACK FileSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK GlobalSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
