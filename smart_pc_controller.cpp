#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <commctrl.h>
#include <thread>
#include <string>
#include <atomic>
#include <mutex>
#include "resource.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Constants
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SETTINGS 1002
#define DEFAULT_PORT 10675
#define DEFAULT_SHUTDOWN_COMMAND "shutdown-my-pc"
#define BUFFER_SIZE 1024
#define REGISTRY_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_NAME "SmartPCController"
#define SINGLE_INSTANCE_MUTEX_NAME L"Local\\SmartPCController.SingleInstance"

// Global variables
HINSTANCE g_hInst;
HWND g_hMainWnd;
NOTIFYICONDATA g_nid;
std::atomic<bool> g_running(true);
std::atomic<int> g_port(DEFAULT_PORT);
std::string g_shutdownCommand(DEFAULT_SHUTDOWN_COMMAND);
std::mutex g_shutdownCommandMutex;
SOCKET g_udpSocket = INVALID_SOCKET;
SOCKET g_tcpSocket = INVALID_SOCKET;
HICON g_hTrayIcon = NULL;
HANDLE g_hInstanceMutex = NULL;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void InitializeTrayIcon();
void CleanupTrayIcon();
void ShowContextMenu(HWND hwnd);
void ShowSettingsDialog();
HICON CreatePowerTrayIcon();
bool InitializeSingleInstance();
void CleanupSingleInstance();
std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::string GetShutdownCommand();
void SetShutdownCommand(const std::string& command);
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

    if (!InitializeSingleInstance()) {
        return 0;
    }
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(NULL, L"Failed to initialize Winsock", L"Error", MB_OK | MB_ICONERROR);
        CleanupSingleInstance();
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
        CleanupSingleInstance();
        return 1;
    }

    // Create hidden main window
    g_hMainWnd = CreateWindowEx(0, L"SmartPCControllerClass", L"Smart PC Controller",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        CleanupSingleInstance();
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
    CleanupSingleInstance();

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
    g_hTrayIcon = CreatePowerTrayIcon();
    g_nid.hIcon = g_hTrayIcon ? g_hTrayIcon : LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(g_nid.szTip[0]), L"Smart PC Controller");

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Cleanup system tray icon
void CleanupTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    if (g_hTrayIcon) {
        DestroyIcon(g_hTrayIcon);
        g_hTrayIcon = NULL;
    }
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

// Settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        {
            SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hTrayIcon);
            SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hTrayIcon);
            SetDlgItemInt(hDlg, IDC_PORT_EDIT, g_port.load(), FALSE);
            SetDlgItemText(hDlg, IDC_COMMAND_EDIT, Utf8ToWide(GetShutdownCommand()).c_str());
            CheckDlgButton(hDlg, IDC_STARTUP_CHECK, IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
            SendDlgItemMessage(hDlg, IDC_PORT_EDIT, EM_SETLIMITTEXT, 5, 0);
            SendDlgItemMessage(hDlg, IDC_COMMAND_EDIT, EM_SETLIMITTEXT, 255, 0);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            {
                UINT newPort = GetDlgItemInt(hDlg, IDC_PORT_EDIT, NULL, FALSE);
                if (newPort == 0 || newPort > 65535) {
                    MessageBox(hDlg, L"Please enter a valid port number (1-65535)", L"Invalid Port", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }

                WCHAR commandText[256];
                GetDlgItemText(hDlg, IDC_COMMAND_EDIT, commandText, 256);
                std::string newCommand = WideToUtf8(commandText);
                if (newCommand.empty()) {
                    MessageBox(hDlg, L"Please enter a shutdown message.", L"Invalid Message", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                SetShutdownCommand(newCommand);

                if ((int)newPort != g_port.load()) {
                    g_port = (int)newPort;
                    CleanupSockets();
                    InitializeSockets();
                }

                bool startupEnabled = IsDlgButtonChecked(hDlg, IDC_STARTUP_CHECK) == BST_CHECKED;
                SetStartupRegistry(startupEnabled);

                EndDialog(hDlg, IDOK);
            }
            return TRUE;

        case IDCANCEL:
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
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), g_hMainWnd, SettingsDialogProc);
}

HICON CreatePowerTrayIcon()
{
    const int size = 32;
    HDC screenDc = GetDC(NULL);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP colorBitmap = CreateCompatibleBitmap(screenDc, size, size);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDc, colorBitmap);

    HBRUSH bgBrush = CreateSolidBrush(RGB(0, 120, 212));
    HPEN whitePen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255));
    HPEN oldPen = (HPEN)SelectObject(memDc, whitePen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDc, bgBrush);

    RECT rc = { 0, 0, size, size };
    FillRect(memDc, &rc, bgBrush);
    SelectObject(memDc, bgBrush);
    Ellipse(memDc, 2, 2, 30, 30);

    SelectObject(memDc, GetStockObject(NULL_BRUSH));
    Arc(memDc, 8, 8, 24, 25, 22, 10, 10, 10);
    MoveToEx(memDc, 16, 7, NULL);
    LineTo(memDc, 16, 17);

    SelectObject(memDc, oldBrush);
    SelectObject(memDc, oldPen);
    SelectObject(memDc, oldBitmap);

    HDC maskDc = CreateCompatibleDC(NULL);
    HBITMAP maskBitmap = CreateBitmap(size, size, 1, 1, NULL);
    HBITMAP oldMaskBitmap = (HBITMAP)SelectObject(maskDc, maskBitmap);
    HBRUSH transparentBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    HBRUSH opaqueBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(maskDc, &rc, transparentBrush);
    SelectObject(maskDc, opaqueBrush);
    Ellipse(maskDc, 2, 2, 30, 30);
    SelectObject(maskDc, oldMaskBitmap);

    ICONINFO iconInfo = { 0 };
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(maskBitmap);
    DeleteDC(maskDc);
    DeleteObject(colorBitmap);
    DeleteObject(bgBrush);
    DeleteObject(whitePen);
    DeleteDC(memDc);
    ReleaseDC(NULL, screenDc);

    return icon;
}

bool InitializeSingleInstance()
{
    g_hInstanceMutex = CreateMutex(NULL, TRUE, SINGLE_INSTANCE_MUTEX_NAME);
    if (!g_hInstanceMutex) {
        MessageBox(NULL, L"Failed to initialize single-instance guard.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_hInstanceMutex);
        g_hInstanceMutex = NULL;
        return false;
    }

    return true;
}

void CleanupSingleInstance()
{
    if (g_hInstanceMutex) {
        ReleaseMutex(g_hInstanceMutex);
        CloseHandle(g_hInstanceMutex);
        g_hInstanceMutex = NULL;
    }
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return L"";
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0);
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], sizeNeeded);
    return result;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return "";
    }

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], sizeNeeded, NULL, NULL);
    return result;
}

std::string GetShutdownCommand()
{
    std::lock_guard<std::mutex> lock(g_shutdownCommandMutex);
    return g_shutdownCommand;
}

void SetShutdownCommand(const std::string& command)
{
    std::lock_guard<std::mutex> lock(g_shutdownCommandMutex);
    g_shutdownCommand = command;
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
        
        DWORD dataSize = (DWORD)((wcslen(exePath) + 1) * sizeof(WCHAR));
        result = RegSetValueEx(hKey, L"SmartPCController", 0, REG_SZ,
            (LPBYTE)exePath, dataSize);
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
    std::string shutdownCommand = GetShutdownCommand();
    int commandLength = (int)shutdownCommand.length();
    
    if (commandLength > 0 && length >= commandLength) {
        std::string receivedData(data, length);
        return receivedData.find(shutdownCommand) != std::string::npos;
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

