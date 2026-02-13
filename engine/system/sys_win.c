// engine/system/sys_win.c
#include "quakedef.h"
#include "in.h"
#include "gl_local.h"
#include "keys.h"
#include "client.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// Глобальные переменные
HWND hwnd_main;
HDC hdc_main;
HGLRC hglrc_main;
HINSTANCE hinstance;

qboolean mouse_grabbed = FALSE;

// Мышь - накопление движения
float mouse_dx_accum = 0;
float mouse_dy_accum = 0;

// Флаг: игнорировать следующий WM_MOUSEMOVE (после SetCursorPos)
static qboolean mouse_skip_next = FALSE;

// Аргументы командной строки
#define MAX_NUM_ARGVS 50
int com_argc;
char* com_argv[MAX_NUM_ARGVS + 1];

static WORD original_gamma[3][256];
static qboolean gamma_changed = FALSE;

// High-resolution timer
static LARGE_INTEGER perf_freq;
static LARGE_INTEGER perf_start;
static qboolean perf_init = FALSE;

// Forward declarations
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern void Key_Event(int key, qboolean down);
extern qboolean cl_skip_render;
extern void CL_RenderOnly(void);

// ============================================================================
// COM_Init
// ============================================================================

void COM_Init(char* lpCmdLine)
{
    com_argc = 1;
    com_argv[0] = "engine";

    while (*lpCmdLine && com_argc < MAX_NUM_ARGVS)
    {
        while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
            lpCmdLine++;

        if (*lpCmdLine)
        {
            com_argv[com_argc++] = lpCmdLine;
            while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
                lpCmdLine++;

            if (*lpCmdLine)
            {
                *lpCmdLine = 0;
                lpCmdLine++;
            }
        }
    }
}

int COM_CheckParm(char* parm)
{
    int i;
    for (i = 1; i < com_argc; i++)
    {
        if (!com_argv[i]) continue;
        if (!strcmp(parm, com_argv[i])) return i;
    }
    return 0;
}

// ============================================================================
// Video
// ============================================================================

void VID_SetGamma(float gamma, float brightness)
{
    WORD gamma_ramp[3][256];
    HDC hdc;
    int i;
    float value;

    hdc = GetDC(NULL);

    if (!gamma_changed)
    {
        GetDeviceGammaRamp(hdc, original_gamma);
        gamma_changed = TRUE;
    }

    for (i = 0; i < 256; i++)
    {
        value = 65535.0f * (float)pow(i / 255.0f, 1.0f / gamma) * brightness;
        if (value < 0) value = 0;
        if (value > 65535) value = 65535;

        gamma_ramp[0][i] = (WORD)value;
        gamma_ramp[1][i] = (WORD)value;
        gamma_ramp[2][i] = (WORD)value;
    }

    SetDeviceGammaRamp(hdc, gamma_ramp);
    ReleaseDC(NULL, hdc);
}

void VID_RestoreGamma(void)
{
    HDC hdc;

    if (!gamma_changed)
        return;

    hdc = GetDC(NULL);
    SetDeviceGammaRamp(hdc, original_gamma);
    ReleaseDC(NULL, hdc);
    gamma_changed = FALSE;
}

void VID_Shutdown(void)
{
    if (hglrc_main)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc_main);
        hglrc_main = NULL;
    }
    if (hdc_main)
    {
        ReleaseDC(hwnd_main, hdc_main);
        hdc_main = NULL;
    }
    if (hwnd_main)
    {
        DestroyWindow(hwnd_main);
        hwnd_main = NULL;
    }
    UnregisterClass("OpenGoldSrc", hinstance);
}

qboolean VID_CreateWindow(int width, int height, qboolean fullscreen)
{
    WNDCLASS wc;
    RECT rect;
    DWORD style, exstyle;
    int x, y;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "OpenGoldSrc";

    if (!RegisterClass(&wc))
        return FALSE;

    if (fullscreen)
    {
        style = WS_POPUP | WS_VISIBLE;
        exstyle = WS_EX_TOPMOST;
        x = 0; y = 0;
    }
    else
    {
        style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        exstyle = 0;
        rect.left = 0; rect.top = 0; rect.right = width; rect.bottom = height;
        AdjustWindowRectEx(&rect, style, FALSE, exstyle);
        x = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }

    hwnd_main = CreateWindowEx(exstyle, "OpenGoldSrc", "OpenCoreSrc", style,
        x, y, width, height, NULL, NULL, hinstance, NULL);

    if (!hwnd_main) return FALSE;

    ShowWindow(hwnd_main, SW_SHOW);
    UpdateWindow(hwnd_main);
    SetForegroundWindow(hwnd_main);
    SetFocus(hwnd_main);

    return TRUE;
}

qboolean VID_InitGL(void)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    int pixelformat;

    hdc_main = GetDC(hwnd_main);
    if (!hdc_main) return FALSE;

    pixelformat = ChoosePixelFormat(hdc_main, &pfd);
    if (!pixelformat) return FALSE;

    if (!SetPixelFormat(hdc_main, pixelformat, &pfd)) return FALSE;

    hglrc_main = wglCreateContext(hdc_main);
    if (!hglrc_main) return FALSE;

    if (!wglMakeCurrent(hdc_main, hglrc_main)) return FALSE;

    return TRUE;
}

// ============================================================================
// Raw Input registration
// ============================================================================

static void RegisterRawInput(HWND hwnd)
{
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;   // HID_USAGE_PAGE_GENERIC
    rid.usUsage = 0x02;   // HID_USAGE_GENERIC_MOUSE
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

qboolean VID_Init(int width, int height, qboolean fullscreen)
{
    if (!VID_CreateWindow(width, height, fullscreen))
        return FALSE;

    if (!VID_InitGL())
        return FALSE;

    glstate.width = width;
    glstate.height = height;

    // Register Raw Input for mouse
    RegisterRawInput(hwnd_main);

    return TRUE;
}

void VID_Swap(void)
{
    SwapBuffers(hdc_main);
}

// ============================================================================
// System
// ============================================================================

void Sys_Error(const char* error, ...)
{
    va_list args;
    char text[1024];

    va_start(args, error);
    vsnprintf(text, sizeof(text), error, args);
    va_end(args);

    VID_Shutdown();
    MessageBox(NULL, text, "Error", MB_OK | MB_ICONERROR);
    exit(1);
}

void Sys_Quit(void)
{
    Host_Shutdown();
    VID_Shutdown();
    exit(0);
}

double Sys_FloatTime(void)
{
    static LARGE_INTEGER freq;
    static LARGE_INTEGER start;
    static int initialized = 0;
    LARGE_INTEGER now;

    if (!initialized)
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        initialized = 1;
        return 0.0;
    }

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
}

void Sys_Sleep(int msec)
{
    Sleep(msec);
}

// ============================================================================
// Захват мыши
// ============================================================================

void Sys_GrabMouse(qboolean grab)
{
    if (grab == mouse_grabbed)
        return;

    if (grab)
    {
        RECT r;
        GetClientRect(hwnd_main, &r);

        POINT tl = { r.left, r.top };
        POINT br = { r.right, r.bottom };
        ClientToScreen(hwnd_main, &tl);
        ClientToScreen(hwnd_main, &br);

        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
        SetCapture(hwnd_main);
        while (ShowCursor(FALSE) >= 0);

        // Центрируем
        int cx = (tl.x + br.x) / 2;
        int cy = (tl.y + br.y) / 2;
        SetCursorPos(cx, cy);

        mouse_dx_accum = 0;
        mouse_dy_accum = 0;
        mouse_skip_next = TRUE;
        mouse_grabbed = TRUE;
    }
    else
    {
        ClipCursor(NULL);
        ReleaseCapture();
        while (ShowCursor(TRUE) < 0);
        mouse_grabbed = FALSE;
    }
}

// ============================================================================
// MapKey
// ============================================================================

static int MapKey(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case VK_ESCAPE:  return K_ESCAPE;
    case VK_RETURN:  return K_ENTER;
    case VK_TAB:     return K_TAB;
    case VK_SPACE:   return K_SPACE;
    case VK_BACK:    return K_BACKSPACE;
    case VK_UP:      return K_UPARROW;
    case VK_DOWN:    return K_DOWNARROW;
    case VK_LEFT:    return K_LEFTARROW;
    case VK_RIGHT:   return K_RIGHTARROW;
    case VK_SHIFT:   return K_SHIFT;
    case VK_CONTROL: return K_CTRL;
    case VK_PRIOR:   return K_PGUP;
    case VK_NEXT:    return K_PGDN;
    case 192:        return '`';
    }

    if (wParam >= 'A' && wParam <= 'Z') return wParam + 32;
    if (wParam >= '0' && wParam <= '9') return wParam;

    return 0;
}

// ============================================================================
// WndProc
// ============================================================================

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            if (mouse_grabbed)
                Sys_GrabMouse(FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
        Key_Event(K_MOUSE1, TRUE);
        return 0;

    case WM_LBUTTONUP:
        Key_Event(K_MOUSE1, FALSE);
        return 0;

    case WM_RBUTTONDOWN:
        Key_Event(K_MOUSE2, TRUE);
        return 0;

    case WM_RBUTTONUP:
        Key_Event(K_MOUSE2, FALSE);
        return 0;

    // Raw Input заменяет WM_MOUSEMOVE для игровой мыши
    case WM_INPUT:
    {
        if (!mouse_grabbed)
            break;

        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

        if (dwSize > 0 && dwSize <= 256)
        {
            BYTE lpb[256];
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
            {
                RAWINPUT* raw = (RAWINPUT*)lpb;
                if (raw->header.dwType == RIM_TYPEMOUSE)
                {
                    if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                    {
                        mouse_dx_accum += (float)raw->data.mouse.lLastX;
                        mouse_dy_accum += (float)raw->data.mouse.lLastY;

                    }
                }
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        // Когда мышь захвачена - игнорируем WM_MOUSEMOVE,
        // используем только WM_INPUT (Raw Input)
        if (mouse_grabbed)
            return 0;
        return 0;

    case WM_KEYDOWN:
        Key_Event(MapKey(wParam, lParam), TRUE);
        return 0;

    case WM_KEYUP:
        Key_Event(MapKey(wParam, lParam), FALSE);
        return 0;

    case WM_SIZE:
        glstate.width = LOWORD(lParam);
        glstate.height = HIWORD(lParam);
        if (glstate.width == 0) glstate.width = 1;
        if (glstate.height == 0) glstate.height = 1;
        glViewport(0, 0, glstate.width, glstate.height);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// Message pump & Main
// ============================================================================

void Sys_SendKeyEvents(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            Sys_Quit();
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    double oldtime, newtime, frame_time;
    double accumulator = 0.0;
    const double FIXED_DT = 0.01;  // 100 Hz

    hinstance = hInstance;
    COM_Init(lpCmdLine);
    timeBeginPeriod(1);

    if (!VID_Init(1280, 720, FALSE))
        return 0;

    // VSync off
    typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(0);

    R_Init(hInstance, hwnd_main);
    Host_Init();

    oldtime = Sys_FloatTime();

    while (host_initialized)
    {
        Sys_SendKeyEvents();

        newtime = Sys_FloatTime();
        frame_time = newtime - oldtime;
        oldtime = newtime;

        if (frame_time <= 0.0)
            continue;
        if (frame_time > 0.1)
            frame_time = 0.1;

        accumulator += frame_time;

        cl_skip_render = TRUE;
        while (accumulator >= FIXED_DT)
        {
            Host_Frame((float)FIXED_DT);
            accumulator -= FIXED_DT;
        }
        cl_skip_render = FALSE;

        CL_RenderOnly();
        VID_Swap();
    }

    timeEndPeriod(1);
    Sys_Quit();
    return 0;
}
