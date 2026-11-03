// Clipline installer for Windows (x64).
//
// A single self-contained .exe: the plugin binary is embedded as a resource (installer.rc). It installs
//
//     C:\Program Files\Common Files\VST3\Clipline.vst3\Contents\x86_64-win\Clipline.vst3   (the plugin)
//     C:\Program Files\Common Files\Clipline\                                               (read-me, licences,
//                                                                                            uninstaller)
//
// and registers an entry in "Apps & features" / "Programs and Features". User presets in
// %APPDATA%\Clipline are never touched (neither by installing nor by uninstalling).
//
// Command line:
//     Clipline-Setup.exe               interactive install
//     Clipline-Setup.exe /S            silent install
//     Uninstall.exe /uninstall [/S]    (what "Apps & features" runs)
//
// Plain Win32 + GDI, no dependencies beyond system DLLs. Builds with MSVC and with llvm-mingw.

#ifndef UNICODE
 #define UNICODE
#endif
#ifndef _UNICODE
 #define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <string>
#include <vector>

#include "resource.h"

namespace
{
    // ---- product data (kept in sync with CMakeLists.txt by configure_file) ---------------------------
    const wchar_t* const kProduct   = L"Clipline";
    const wchar_t* const kVersion   = L"" CLIPLINE_VERSION_W;
    const wchar_t* const kPublisher = L"Clipline Audio";
    const wchar_t* const kUninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Clipline";

    // ---- colours (the plugin's palette) ---------------------------------------------------------------
    const COLORREF cBg      = RGB (0x10, 0x10, 0x10);
    const COLORREF cSurface = RGB (0x1c, 0x1c, 0x1c);
    const COLORREF cHair    = RGB (0x2c, 0x2c, 0x2c);
    const COLORREF cText    = RGB (0xF2, 0xF2, 0xF2);
    const COLORREF cDim     = RGB (0x88, 0x88, 0x88);
    const COLORREF cAccent  = RGB (0xC5, 0xF4, 0x67);
    const COLORREF cWarn    = RGB (0xFF, 0x6B, 0x4A);

    enum class Mode { install, uninstall };
    enum class Stage { ready, working, done, failed };

    struct Payload { int resourceId; const wchar_t* relativePath; };

    // Files of the VST3 bundle, relative to the bundle folder Clipline.vst3.
    const Payload kBundleFiles[] = {
        { IDR_PLUGIN_BINARY, L"Contents\\x86_64-win\\Clipline.vst3" },
    };
    // Files for C:\Program Files\Common Files\Clipline.
    const Payload kDocFiles[] = {
        { IDR_README,      L"README.txt" },
        { IDR_LICENSE_OFL, L"OFL-Geist.txt" },
    };

    // ---- language: Russian when Windows' display language is Russian, English otherwise -------------------
    bool russian()
    {
        static const bool ru = PRIMARYLANGID (GetUserDefaultUILanguage()) == LANG_RUSSIAN;
        return ru;
    }

    const wchar_t* tr (const wchar_t* en, const wchar_t* ru) { return russian() ? ru : en; }

    // ---- small helpers ----------------------------------------------------------------------------------
    std::wstring knownFolder (REFKNOWNFOLDERID id)
    {
        PWSTR path = nullptr;
        std::wstring result;
        if (SUCCEEDED (SHGetKnownFolderPath (id, 0, nullptr, &path)))
            result = path;
        CoTaskMemFree (path);
        return result;
    }

    std::wstring commonFiles()      { return knownFolder (FOLDERID_ProgramFilesCommonX64); }
    std::wstring vst3Folder()       { return commonFiles() + L"\\VST3"; }
    std::wstring bundleFolder()     { return vst3Folder() + L"\\Clipline.vst3"; }
    std::wstring appFolder()        { return commonFiles() + L"\\Clipline"; }
    std::wstring uninstallerPath()  { return appFolder() + L"\\Uninstall.exe"; }

    std::wstring parentOf (const std::wstring& path)
    {
        const auto slash = path.find_last_of (L"\\/");
        return slash == std::wstring::npos ? std::wstring() : path.substr (0, slash);
    }

    bool fileExists (const std::wstring& p)
    {
        const DWORD a = GetFileAttributesW (p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring errorText (DWORD code)
    {
        wchar_t* buffer = nullptr;
        FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr, code, 0, (LPWSTR) &buffer, 0, nullptr);
        std::wstring s = buffer != nullptr ? buffer : L"";
        LocalFree (buffer);
        while (! s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ' || s.back() == L'.'))
            s.pop_back();
        return s + L" (" + std::to_wstring (code) + L")";
    }

    bool writeResource (int id, const std::wstring& target, std::wstring& error)
    {
        HRSRC info = FindResourceW (nullptr, MAKEINTRESOURCEW (id), RT_RCDATA);
        HGLOBAL handle = info != nullptr ? LoadResource (nullptr, info) : nullptr;
        const void* data = handle != nullptr ? LockResource (handle) : nullptr;
        const DWORD size = info != nullptr ? SizeofResource (nullptr, info) : 0;
        if (data == nullptr || size == 0)
        {
            error = L"The installer is damaged (missing resource " + std::to_wstring (id) + L").";
            return false;
        }

        SHCreateDirectoryExW (nullptr, parentOf (target).c_str(), nullptr);

        // Write next to the target first, then swap it in, so a failed copy never leaves a broken plugin.
        const std::wstring temp = target + L".new";
        HANDLE file = CreateFileW (temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = L"Cannot write " + temp + L":\n" + errorText (GetLastError());
            return false;
        }
        DWORD written = 0;
        const BOOL ok = WriteFile (file, data, size, &written, nullptr);
        const DWORD writeError = GetLastError();
        CloseHandle (file);
        if (! ok || written != size)
        {
            DeleteFileW (temp.c_str());
            error = L"Cannot write " + temp + L":\n" + errorText (writeError);
            return false;
        }

        if (! MoveFileExW (temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const DWORD code = GetLastError();
            DeleteFileW (temp.c_str());
            error = L"Cannot replace " + target + L":\n" + errorText (code);
            if (code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION)
                error += tr (L"\n\nThe plugin is probably loaded right now. Close your DAW (FL Studio, Ableton Live, "
                         L"REAPER, ...) and run the installer again.",
                         L"\n\nСкорее всего, плагин сейчас загружен. Закройте DAW (FL Studio, Ableton Live, "
                         L"REAPER, ...) и запустите установку ещё раз.");
            return false;
        }
        return true;
    }

    void setRegString (HKEY key, const wchar_t* name, const std::wstring& value)
    {
        RegSetValueExW (key, name, 0, REG_SZ, reinterpret_cast<const BYTE*> (value.c_str()),
                        (DWORD) ((value.size() + 1) * sizeof (wchar_t)));
    }

    void setRegDword (HKEY key, const wchar_t* name, DWORD value)
    {
        RegSetValueExW (key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&value), sizeof (value));
    }

    // ---- install / uninstall ----------------------------------------------------------------------------
    bool install (std::wstring& error)
    {
        if (commonFiles().empty())
        {
            error = tr (L"Cannot locate the \"Common Files\" folder.", L"Не найдена папка \"Common Files\".");
            return false;
        }

        for (const auto& f : kBundleFiles)
            if (! writeResource (f.resourceId, bundleFolder() + L"\\" + f.relativePath, error))
                return false;

        for (const auto& f : kDocFiles)
            if (! writeResource (f.resourceId, appFolder() + L"\\" + f.relativePath, error))
                return false;

        // The uninstaller is a copy of this executable.
        wchar_t self[MAX_PATH * 4] = {};
        GetModuleFileNameW (nullptr, self, (DWORD) (sizeof (self) / sizeof (self[0])));
        if (_wcsicmp (self, uninstallerPath().c_str()) != 0
            && ! CopyFileW (self, uninstallerPath().c_str(), FALSE))
        {
            error = L"Cannot write " + uninstallerPath() + L":\n" + errorText (GetLastError());
            return false;
        }

        HKEY key = nullptr;
        if (RegCreateKeyExW (HKEY_LOCAL_MACHINE, kUninstKey, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY,
                             nullptr, &key, nullptr) == ERROR_SUCCESS)
        {
            setRegString (key, L"DisplayName", std::wstring (kProduct) + L" (VST3)");
            setRegString (key, L"DisplayVersion", kVersion);
            setRegString (key, L"Publisher", kPublisher);
            setRegString (key, L"InstallLocation", bundleFolder());
            setRegString (key, L"DisplayIcon", uninstallerPath() + L",0");
            setRegString (key, L"UninstallString", L"\"" + uninstallerPath() + L"\" /uninstall");
            setRegString (key, L"QuietUninstallString", L"\"" + uninstallerPath() + L"\" /uninstall /S");
            setRegDword (key, L"NoModify", 1);
            setRegDword (key, L"NoRepair", 1);
            setRegDword (key, L"EstimatedSize", 16 * 1024);
            RegCloseKey (key);
        }
        return true;
    }

    bool uninstall (std::wstring& error)
    {
        for (const auto& f : kBundleFiles)
        {
            const auto path = bundleFolder() + L"\\" + f.relativePath;
            if (fileExists (path) && ! DeleteFileW (path.c_str()))
            {
                const DWORD code = GetLastError();
                error = L"Cannot remove " + path + L":\n" + errorText (code);
                if (code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION)
                    error += tr (L"\n\nClose your DAW and try again.", L"\n\nЗакройте DAW и повторите.");
                return false;
            }
        }
        RemoveDirectoryW ((bundleFolder() + L"\\Contents\\x86_64-win").c_str());
        RemoveDirectoryW ((bundleFolder() + L"\\Contents\\Resources").c_str());
        RemoveDirectoryW ((bundleFolder() + L"\\Contents").c_str());
        RemoveDirectoryW (bundleFolder().c_str());

        for (const auto& f : kDocFiles)
            DeleteFileW ((appFolder() + L"\\" + f.relativePath).c_str());

        RegDeleteKeyExW (HKEY_LOCAL_MACHINE, kUninstKey, KEY_WOW64_64KEY, 0);
        return true;
    }

    // A running .exe cannot delete itself. When the uninstaller exits, a hidden cmd.exe waits two seconds and
    // removes it and its folder. (Not MoveFileEx(..., DELAY_UNTIL_REBOOT): a reinstall before the next reboot
    // would then lose its freshly installed uninstaller.)
    void removeUninstallerAfterExit()
    {
        wchar_t self[MAX_PATH * 4] = {};
        GetModuleFileNameW (nullptr, self, (DWORD) (sizeof (self) / sizeof (self[0])));
        if (_wcsicmp (self, uninstallerPath().c_str()) != 0)
            return;                                         // not running from the install folder: nothing to do

        wchar_t system[MAX_PATH] = {};
        GetSystemDirectoryW (system, MAX_PATH);
        std::wstring command = L"\"" + std::wstring (system) + L"\\cmd.exe\" /d /c ping 127.0.0.1 -n 3 >nul"
                               L" & del /f /q \"" + uninstallerPath() + L"\""
                               L" & rmdir \"" + appFolder() + L"\"";

        STARTUPINFOW si { sizeof (si) };
        PROCESS_INFORMATION pi {};
        if (CreateProcessW (nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS,
                            nullptr, system, &si, &pi))
        {
            CloseHandle (pi.hThread);
            CloseHandle (pi.hProcess);
        }
    }

    // ---- UI ---------------------------------------------------------------------------------------------
    struct Ui
    {
        Mode mode = Mode::install;
        Stage stage = Stage::ready;
        std::wstring message;
        HFONT title = nullptr, body = nullptr, note = nullptr, button = nullptr;
        RECT primary {}, secondary {};
        int hot = 0;        // 1 primary, 2 secondary
        float dpi = 1.0f;
        HICON icon = nullptr;
    };

    Ui ui;

    int px (float v) { return (int) (v * ui.dpi + 0.5f); }

    HFONT makeFont (int size, int weight)
    {
        return CreateFontW (-px ((float) size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    }

    void fillRound (HDC dc, RECT r, COLORREF fill, COLORREF border, int radius)
    {
        HBRUSH brush = CreateSolidBrush (fill);
        HPEN pen = CreatePen (PS_SOLID, 1, border);
        auto oldBrush = SelectObject (dc, brush);
        auto oldPen = SelectObject (dc, pen);
        RoundRect (dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject (dc, oldBrush);
        SelectObject (dc, oldPen);
        DeleteObject (brush);
        DeleteObject (pen);
    }

    void text (HDC dc, const std::wstring& s, RECT r, HFONT font, COLORREF colour, UINT format)
    {
        SelectObject (dc, font);
        SetTextColor (dc, colour);
        DrawTextW (dc, s.c_str(), (int) s.size(), &r, format);
    }

    const wchar_t* primaryLabel()
    {
        switch (ui.stage)
        {
            case Stage::ready:   return ui.mode == Mode::install ? tr (L"Install", L"Установить") : tr (L"Uninstall", L"Удалить");
            case Stage::working: return tr (L"Working\u2026", L"Подождите\u2026");
            case Stage::done:
            case Stage::failed:  return tr (L"Close", L"Закрыть");
        }
        return L"";
    }

    void paint (HWND hwnd, HDC dc)
    {
        RECT client;
        GetClientRect (hwnd, &client);
        HBRUSH bg = CreateSolidBrush (cBg);
        FillRect (dc, &client, bg);
        DeleteObject (bg);
        SetBkMode (dc, TRANSPARENT);

        const int pad = px (28);
        if (ui.icon != nullptr)
            DrawIconEx (dc, pad, pad, ui.icon, px (44), px (44), 0, nullptr, DI_NORMAL);

        RECT r { pad + px (60), pad - px (2), client.right - pad, pad + px (28) };
        text (dc, std::wstring (kProduct) + L"  " + kVersion, r, ui.title, cText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        r = { pad + px (60), pad + px (26), client.right - pad, pad + px (48) };
        text (dc, ui.mode == Mode::install ? tr (L"VST3 clipper  \u00B7  Windows x64", L"VST3-клиппер  \u00B7  Windows x64")
                                           : tr (L"Remove the plugin from this computer", L"Удаление плагина с компьютера"),
              r, ui.note, cDim, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT panel { pad, pad + px (72), client.right - pad, client.bottom - pad - px (56) };
        fillRound (dc, panel, cSurface, cHair, px (10));
        RECT inner { panel.left + px (18), panel.top + px (14), panel.right - px (18), panel.bottom - px (14) };

        std::wstring body = ui.message;
        COLORREF colour = cText;
        if (ui.stage == Stage::ready)
        {
            body = ui.mode == Mode::install
                ? tr (L"The plugin will be installed to\n", L"Плагин будет установлен в\n") + bundleFolder()
                  + tr (L"\n\nThis is the standard VST3 folder: FL Studio, Ableton Live, REAPER and other hosts find it there "
                        L"after a plugin rescan. Close your DAW before installing.",
                        L"\n\nЭто стандартная папка VST3: FL Studio, Ableton Live, REAPER и другие DAW находят плагин там "
                        L"после пересканирования. Перед установкой закройте DAW.")
                : tr (L"Clipline will be removed from\n", L"Clipline будет удалён из\n") + bundleFolder()
                  + tr (L"\n\nYour own presets in %APPDATA%\\Clipline are kept.",
                        L"\n\nВаши пресеты в %APPDATA%\\Clipline сохранятся.");
        }
        else if (ui.stage == Stage::failed)
        {
            colour = cWarn;
        }
        text (dc, body, inner, ui.body, colour, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        auto drawButton = [&] (RECT b, const wchar_t* label, bool accent, bool hot)
        {
            const COLORREF fill = accent ? (hot ? RGB (0xD4, 0xFA, 0x86) : cAccent) : (hot ? RGB (0x2a, 0x2a, 0x2a) : cSurface);
            fillRound (dc, b, fill, accent ? fill : cHair, px (8));
            text (dc, label, b, ui.button, accent ? cBg : cText, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        };

        const int bw = px (132), bh = px (36), by = client.bottom - pad - bh;
        ui.primary = { client.right - pad - bw, by, client.right - pad, by + bh };
        const bool showSecondary = ui.stage == Stage::ready;
        ui.secondary = showSecondary ? RECT { ui.primary.left - px (10) - bw, by, ui.primary.left - px (10), by + bh } : RECT {};

        drawButton (ui.primary, primaryLabel(), ui.stage != Stage::working, ui.hot == 1);
        if (showSecondary)
            drawButton (ui.secondary, tr (L"Cancel", L"Отмена"), false, ui.hot == 2);
    }

    bool inside (const RECT& r, LPARAM lp)
    {
        const POINT p { (short) LOWORD (lp), (short) HIWORD (lp) };
        return PtInRect (&r, p) != FALSE;
    }

    void run (HWND hwnd)
    {
        ui.stage = Stage::working;
        InvalidateRect (hwnd, nullptr, FALSE);
        UpdateWindow (hwnd);
        SetCursor (LoadCursor (nullptr, IDC_WAIT));

        std::wstring error;
        const bool ok = ui.mode == Mode::install ? install (error) : uninstall (error);
        ui.stage = ok ? Stage::done : Stage::failed;
        if (ok)
            ui.message = ui.mode == Mode::install
                ? tr (L"Clipline is installed.\n\nFL Studio: Options \u203A Manage plugins \u203A Find more plugins (or Start scan), "
                      L"then add Clipline from the plugin database or the mixer slot menu.\n"
                      L"Ableton Live: Preferences \u203A Plug-Ins \u203A Rescan.   REAPER: Options \u203A Preferences \u203A VST \u203A Re-scan.",
                      L"Clipline установлен.\n\nFL Studio: Options \u203A Manage plugins \u203A Find more plugins (или Start scan), "
                      L"затем добавьте Clipline в слот эффекта микшера.\n"
                      L"Ableton Live: Preferences \u203A Plug-Ins \u203A Rescan.   REAPER: Options \u203A Preferences \u203A VST \u203A Re-scan.")
                : tr (L"Clipline has been removed.", L"Clipline удалён.");
        else
            ui.message = error;
        SetCursor (LoadCursor (nullptr, IDC_ARROW));
        InvalidateRect (hwnd, nullptr, FALSE);
    }

    LRESULT CALLBACK windowProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint (hwnd, &ps);
                RECT client;
                GetClientRect (hwnd, &client);
                // Double buffering: no flicker on hover.
                HDC mem = CreateCompatibleDC (dc);
                HBITMAP bmp = CreateCompatibleBitmap (dc, client.right, client.bottom);
                auto old = SelectObject (mem, bmp);
                paint (hwnd, mem);
                BitBlt (dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
                SelectObject (mem, old);
                DeleteObject (bmp);
                DeleteDC (mem);
                EndPaint (hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_MOUSEMOVE:
            {
                const int hot = inside (ui.primary, lp) ? 1 : (inside (ui.secondary, lp) ? 2 : 0);
                if (hot != ui.hot)
                {
                    ui.hot = hot;
                    InvalidateRect (hwnd, nullptr, FALSE);
                }
                TRACKMOUSEEVENT tme { sizeof (tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent (&tme);
                return 0;
            }
            case WM_MOUSELEAVE:
                ui.hot = 0;
                InvalidateRect (hwnd, nullptr, FALSE);
                return 0;
            case WM_LBUTTONUP:
                if (ui.stage == Stage::working)
                    return 0;
                if (inside (ui.primary, lp))
                {
                    if (ui.stage == Stage::ready)
                        run (hwnd);
                    else
                        DestroyWindow (hwnd);
                }
                else if (inside (ui.secondary, lp))
                {
                    DestroyWindow (hwnd);
                }
                return 0;
            case WM_KEYDOWN:
                if (wp == VK_ESCAPE && ui.stage != Stage::working)
                    DestroyWindow (hwnd);
                else if (wp == VK_RETURN && ui.stage == Stage::ready)
                    run (hwnd);
                else if (wp == VK_RETURN && ui.stage != Stage::working)
                    DestroyWindow (hwnd);
                return 0;
            case WM_SETCURSOR:
                if (LOWORD (lp) == HTCLIENT)
                {
                    SetCursor (LoadCursor (nullptr, ui.stage == Stage::working ? IDC_WAIT : (ui.hot != 0 ? IDC_HAND : IDC_ARROW)));
                    return TRUE;
                }
                break;
            case WM_DESTROY:
                PostQuitMessage (ui.stage == Stage::failed ? 1 : 0);
                return 0;
            default:
                break;
        }
        return DefWindowProcW (hwnd, msg, wp, lp);
    }

    void useDarkTitleBar (HWND hwnd)
    {
        // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1+); ignored on older systems.
        if (HMODULE dwm = LoadLibraryW (L"dwmapi.dll"))
        {
            using SetAttr = HRESULT (WINAPI*) (HWND, DWORD, LPCVOID, DWORD);
            if (auto set = reinterpret_cast<SetAttr> (reinterpret_cast<void*> (GetProcAddress (dwm, "DwmSetWindowAttribute"))))
            {
                const BOOL on = TRUE;
                set (hwnd, 20, &on, sizeof (on));
            }
            FreeLibrary (dwm);
        }
    }
}

int WINAPI wWinMain (HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW (GetCommandLineW(), &argc);
    bool silent = false;
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsicmp (argv[i], L"/S") == 0 || _wcsicmp (argv[i], L"/silent") == 0)
            silent = true;
        else if (_wcsicmp (argv[i], L"/uninstall") == 0)
            ui.mode = Mode::uninstall;
    }
    LocalFree (argv);

    if (silent)
    {
        std::wstring error;
        const bool ok = ui.mode == Mode::install ? install (error) : uninstall (error);
        if (ok && ui.mode == Mode::uninstall)
            removeUninstallerAfterExit();
        return ok ? 0 : 1;
    }

    SetProcessDPIAware();
    HDC screen = GetDC (nullptr);
    ui.dpi = (float) GetDeviceCaps (screen, LOGPIXELSX) / 96.0f;
    ReleaseDC (nullptr, screen);

    ui.title  = makeFont (19, FW_SEMIBOLD);
    ui.body   = makeFont (13, FW_NORMAL);
    ui.note  = makeFont (12, FW_NORMAL);
    ui.button = makeFont (13, FW_SEMIBOLD);
    ui.icon = (HICON) LoadImageW (instance, MAKEINTRESOURCEW (IDI_APP), IMAGE_ICON, px (44), px (44), 0);

    WNDCLASSEXW wc { sizeof (wc) };
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW (instance, MAKEINTRESOURCEW (IDI_APP));
    wc.lpszClassName = L"CliplineSetup";
    RegisterClassExW (&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT frame { 0, 0, px (560), px (360) };
    AdjustWindowRect (&frame, style, FALSE);
    const int w = frame.right - frame.left, h = frame.bottom - frame.top;
    const std::wstring caption = std::wstring (kProduct) + (ui.mode == Mode::install ? tr (L" Setup", L" \u2014 установка")
                                                                                     : tr (L" Uninstall", L" \u2014 удаление"));
    HWND hwnd = CreateWindowExW (0, wc.lpszClassName, caption.c_str(), style,
                                 (GetSystemMetrics (SM_CXSCREEN) - w) / 2, (GetSystemMetrics (SM_CYSCREEN) - h) / 2, w, h,
                                 nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr)
        return 1;

    useDarkTitleBar (hwnd);
    ShowWindow (hwnd, show);
    UpdateWindow (hwnd);

    MSG msg;
    int result = 0;
    while (GetMessageW (&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }
    result = (int) msg.wParam;

    for (HFONT f : { ui.title, ui.body, ui.note, ui.button })
        DeleteObject (f);

    if (ui.mode == Mode::uninstall && ui.stage == Stage::done)
        removeUninstallerAfterExit();
    return result;
}
