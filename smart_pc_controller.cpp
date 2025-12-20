#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <commctrl.h>
#include <thread>
#include <string>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Constants
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SETTINGS 1002
#define IDC_PORT_EDIT 1003
#define IDC_STARTUP_CHECK 1004
#define IDC_OK_BUTTON 1005
#define IDC_CANCEL_BUTTON 1006
#define DEFAULT_PORT 10675
#define BUFFER_SIZE 1024
#define REGISTRY_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_NAME "SmartPCController"

// Global variables
HINSTANCE g_hInst;
HWND g_hMainWnd;
NOTIFYICONDATA g_nid;
std::atomic<bool> g_running(true);
std::atomic<int> g_port(DEFAULT_PORT);
SOCKET g_udpSocket = INVALID_SOCKET;
SOCKET g_tcpSocket = INVALID_SOCKET;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void InitializeTrayIcon();
void CleanupTrayIcon();
void ShowContextMenu(HWND hwnd);
void ShowSettingsDialog();
bool SetStartupRegistry(bool enable);
bool IsStartupEnabled();
void InitializeSockets();
void CleanupSockets();
void UdpListenerThread();
void TcpListenerThread();
void HandleShutdownCommand();
bool ProcessReceivedData(const char* data, int length);

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    g_hInst = hInstance;
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(NULL, L"Failed to initialize Winsock", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Register window class
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SmartPCControllerClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    // Create hidden main window
    g_hMainWnd = CreateWindowEx(0, L"SmartPCControllerClass", L"Smart PC Controller",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    // Initialize tray icon
    InitializeTrayIcon();

    // Initialize sockets and start listener threads
    InitializeSockets();
    std::thread udpThread(UdpListenerThread);
    std::thread tcpThread(TcpListenerThread);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    g_running = false;
    CleanupSockets();
    if (udpThread.joinable()) udpThread.join();
    if (tcpThread.joinable()) tcpThread.join();
    CleanupTrayIcon();
    WSACleanup();

    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        ShowWindow(hwnd, SW_HIDE); // Start hidden
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            ShowSettingsDialog();
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE); // Hide instead of closing
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Initialize system tray icon
void InitializeTrayIcon()
{
    memset(&g_nid, 0, sizeof(NOTIFYICONDATA));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hMainWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(g_nid.szTip[0]), L"Smart PC Controller");

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Cleanup system tray icon
void CleanupTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// Show context menu for tray icon
void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Settings window procedure
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OK_BUTTON:
            {
                // Get port value
                HWND hPortEdit = GetDlgItem(hwnd, IDC_PORT_EDIT);
                WCHAR portText[10];
                GetWindowText(hPortEdit, portText, 10);
                int newPort = _wtoi(portText);
                
                if (newPort > 0 && newPort <= 65535) {
                    if (newPort != g_port.load()) {
                        g_port = newPort;
                        CleanupSockets();
                        InitializeSockets();
                    }
                    
                    // Handle startup setting
                    HWND hStartupCheck = GetDlgItem(hwnd, IDC_STARTUP_CHECK);
                    bool startupEnabled = SendMessage(hStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    SetStartupRegistry(startupEnabled);
                    
                    DestroyWindow(hwnd);
                } else {
                    MessageBox(hwnd, L"Please enter a valid port number (1-65535)", L"Invalid Port", MB_OK | MB_ICONWARNING);
                }
            }
            return 0;

        case IDC_CANCEL_BUTTON:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        EnableWindow(g_hMainWnd, TRUE);
        SetForegroundWindow(g_hMainWnd);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Settings dialog procedure (kept for compatibility, but not used)
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        {
            // Set current port value
            SetDlgItemInt(hDlg, IDC_PORT_EDIT, g_port.load(), FALSE);
            
            // Set startup checkbox
            CheckDlgButton(hDlg, IDC_STARTUP_CHECK, IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OK_BUTTON:
            {
                // Get new port value
                UINT newPort = GetDlgItemInt(hDlg, IDC_PORT_EDIT, NULL, FALSE);
                if (newPort > 0 && newPort <= 65535) {
                    if (newPort != g_port.load()) {
                        g_port = newPort;
                        // Restart sockets with new port
                        CleanupSockets();
                        InitializeSockets();
                    }
                } else {
                    MessageBox(hDlg, L"Please enter a valid port number (1-65535)", L"Invalid Port", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }

                // Handle startup setting
                bool startupEnabled = IsDlgButtonChecked(hDlg, IDC_STARTUP_CHECK) == BST_CHECKED;
                SetStartupRegistry(startupEnabled);

                EndDialog(hDlg, IDOK);
            }
            return TRUE;

        case IDC_CANCEL_BUTTON:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Show settings dialog
void ShowSettingsDialog()
{
    // Create a simple settings window instead of complex dialog template
    WNDCLASSEX wcSettings = { 0 };
    wcSettings.cbSize = sizeof(WNDCLASSEX);
    wcSettings.lpfnWndProc = SettingsWindowProc;  // Use our custom window procedure
    wcSettings.hInstance = g_hInst;
    wcSettings.lpszClassName = L"SettingsWindowClass";
    wcSettings.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcSettings.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcSettings.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    
    static bool classRegistered = false;
    if (!classRegistered) {
        RegisterClassEx(&wcSettings);
        classRegistered = true;
    }
    
    // Create settings window
    HWND hSettingsWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"SettingsWindowClass",
        L"Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 250, 150,
        g_hMainWnd, NULL, g_hInst, NULL);
    
    if (hSettingsWnd) {
        // Disable main window to make this modal
        EnableWindow(g_hMainWnd, FALSE);
        
        // Create controls
        CreateWindow(L"STATIC", L"Port:", WS_VISIBLE | WS_CHILD,
            10, 15, 40, 20, hSettingsWnd, NULL, g_hInst, NULL);
        
        HWND hPortEdit = CreateWindow(L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            60, 15, 80, 20, hSettingsWnd, (HMENU)IDC_PORT_EDIT, g_hInst, NULL);
        
        HWND hStartupCheck = CreateWindow(L"BUTTON", L"Run at startup", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            10, 45, 120, 20, hSettingsWnd, (HMENU)IDC_STARTUP_CHECK, g_hInst, NULL);
        
        CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            50, 85, 50, 25, hSettingsWnd, (HMENU)IDC_OK_BUTTON, g_hInst, NULL);
        
        CreateWindow(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            110, 85, 50, 25, hSettingsWnd, (HMENU)IDC_CANCEL_BUTTON, g_hInst, NULL);

        // Initialize values
        SetWindowText(hPortEdit, std::to_wstring(g_port.load()).c_str());
        SendMessage(hStartupCheck, BM_SETCHECK, IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        
        // Center the window
        RECT rc;
        GetWindowRect(hSettingsWnd, &rc);
        int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
        SetWindowPos(hSettingsWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        // Set focus to the port edit box
        SetFocus(hPortEdit);
    }
}

// Set startup registry entry
bool SetStartupRegistry(bool enable)
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }

    if (enable) {
        WCHAR exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        
        result = RegSetValueEx(hKey, L"SmartPCController", 0, REG_SZ,
            (LPBYTE)exePath, (wcslen(exePath) + 1) * sizeof(WCHAR));
    } else {
        result = RegDeleteValue(hKey, L"SmartPCController");
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS; // Not an error if it doesn't exist
        }
    }

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

// Check if startup is enabled
bool IsStartupEnabled()
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }

    DWORD dataSize = 0;
    result = RegQueryValueEx(hKey, L"SmartPCController", NULL, NULL, NULL, &dataSize);
    
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

// Initialize UDP and TCP sockets
void InitializeSockets()
{
    int port = g_port.load();
    
    // Create UDP socket
    g_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udpSocket != INVALID_SOCKET) {
        sockaddr_in udpAddr = { 0 };
        udpAddr.sin_family = AF_INET;
        udpAddr.sin_addr.s_addr = INADDR_ANY;
        udpAddr.sin_port = htons(port);
        
        if (bind(g_udpSocket, (sockaddr*)&udpAddr, sizeof(udpAddr)) == SOCKET_ERROR) {
            closesocket(g_udpSocket);
            g_udpSocket = INVALID_SOCKET;
        }
    }

    // Create TCP socket
    g_tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_tcpSocket != INVALID_SOCKET) {
        sockaddr_in tcpAddr = { 0 };
        tcpAddr.sin_family = AF_INET;
        tcpAddr.sin_addr.s_addr = INADDR_ANY;
        tcpAddr.sin_port = htons(port);
        
        if (bind(g_tcpSocket, (sockaddr*)&tcpAddr, sizeof(tcpAddr)) == SOCKET_ERROR ||
            listen(g_tcpSocket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(g_tcpSocket);
            g_tcpSocket = INVALID_SOCKET;
        }
    }
}

// Cleanup sockets
void CleanupSockets()
{
    if (g_udpSocket != INVALID_SOCKET) {
        closesocket(g_udpSocket);
        g_udpSocket = INVALID_SOCKET;
    }
    if (g_tcpSocket != INVALID_SOCKET) {
        closesocket(g_tcpSocket);
        g_tcpSocket = INVALID_SOCKET;
    }
}

// UDP listener thread
void UdpListenerThread()
{
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    while (g_running && g_udpSocket != INVALID_SOCKET) {
        // Set socket timeout
        DWORD timeout = 1000; // 1 second
        setsockopt(g_udpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        
        int bytesReceived = recvfrom(g_udpSocket, buffer, BUFFER_SIZE - 1, 0,
            (sockaddr*)&clientAddr, &clientAddrSize);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            if (ProcessReceivedData(buffer, bytesReceived)) {
                HandleShutdownCommand();
                break;
            }
        }
    }
}

// TCP listener thread
void TcpListenerThread()
{
    while (g_running && g_tcpSocket != INVALID_SOCKET) {
        // Set socket timeout
        DWORD timeout = 1000; // 1 second
        setsockopt(g_tcpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        
        SOCKET clientSocket = accept(g_tcpSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            char buffer[BUFFER_SIZE];
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                if (ProcessReceivedData(buffer, bytesReceived)) {
                    closesocket(clientSocket);
                    HandleShutdownCommand();
                    break;
                }
            }
            
            closesocket(clientSocket);
        }
    }
}

// Process received data to check for shutdown command
bool ProcessReceivedData(const char* data, int length)
{
    const char* shutdownCommand = "shutdown-my-pc";
    int commandLength = strlen(shutdownCommand);
    
    if (length >= commandLength) {
        // Check if the received data contains the exact shutdown command
        for (int i = 0; i <= length - commandLength; i++) {
            if (strncmp(data + i, shutdownCommand, commandLength) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

// Handle shutdown command
void HandleShutdownCommand()
{
    // Get shutdown privilege
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
        CloseHandle(hToken);
    }
    
    // Initiate system shutdown
    ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
}

