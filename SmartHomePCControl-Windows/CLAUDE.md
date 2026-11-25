You are to write C or C++ code for a Windows native desktop program using only the Win32 API (or MFC if absolutely necessary).
Do not use Electron, .NET WPF, Qt, Java, or any other heavy frameworks. The resulting executable must be small and memory-efficient.

Program requirements:

Socket Functionality

Open both UDP and TCP sockets simultaneously.

Listen on port 10675 by default, but allow the user to change the port in the program settings.

When receiving the exact string shutdown-my-pc from any connected network client (UDP or TCP), immediately initiate a Windows system shutdown using standard Win32 API calls.

Background Operation & Tray Icon

The program runs in the background when minimized.

Display a system tray icon when in background.

Right-clicking the tray icon shows a popup menu with:

Quit (exit the program).

GUI Settings Window

Simple GUI for:

Changing the listening port.

Enabling/disabling “Run at system startup” via Windows registry or startup folder.

Performance & Compatibility

Must be lightweight, low RAM usage (<10 MB idle if possible).

Should work on Windows 10 and later.

No unnecessary dependencies; produce a single executable if possible.

Code Expectations

Provide complete source code in a single, self-contained file if possible.

Include necessary #include statements.

Comment the code so each function’s purpose is clear.

Ensure the code compiles without modification in Microsoft Visual Studio.

