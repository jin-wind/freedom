#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "CorGuids.lib")

#include "stdafx.h"
#include <atlbase.h>
#include <atlconv.h>
#include <atlexcept.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "stb_sprintf.h"

#include "hook.h"
#include "offsets.h"
#include "utility.h"
#include "detours.h"
#include "config.h"

HWND g_hwnd = NULL;
HANDLE g_process = 0;

WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK find_osu_window(HWND hwnd, LPARAM lParam)
{
    DWORD lpdwProcessId;
    GetWindowThreadProcessId(hwnd, &lpdwProcessId);
    if (lpdwProcessId == lParam)
    {
        g_hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

BOOL __stdcall freedom_update(HDC hDc)
{
    static ImFont *font = 0;
    static bool init = false;
    if (!init)
    {
        g_process = GetCurrentProcess();
        EnumWindows(find_osu_window, GetCurrentProcessId());
        oWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        init_hooks();
        if (ar_lock) enable_ar_hooks();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = 0;

        ImFontConfig config;
        config.OversampleH = config.OversampleV = 1;
        config.PixelSnapH = true;

        for (int i = 32; i > 16; i -= 2)
        {
            config.SizePixels = i;
            io.Fonts->AddFontDefault(&config);
        }

        font = io.Fonts->Fonts[3];

        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplOpenGL3_Init();

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

#define BLACK ImVec4(0.05f, 0.05f, 0.05f, 1.0f)
#define WHITE ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
#define BLACK_TRANSPARENT ImVec4(0.05f, 0.05f, 0.05f, 0.7f)
#define PURPLE ImVec4(0.28f, 0.05f, 0.66f, 1.0f)
#define MAGENTA ImVec4(0.34f, 0.04f, 0.68f, 1.0f)
        style.Colors[ImGuiCol_TitleBgActive] = PURPLE;
        style.Colors[ImGuiCol_Button] = PURPLE;
        style.Colors[ImGuiCol_ButtonHovered] = MAGENTA;
        style.Colors[ImGuiCol_ButtonActive] = MAGENTA;
        style.Colors[ImGuiCol_PopupBg] = BLACK;
        style.Colors[ImGuiCol_MenuBarBg] = BLACK;
        style.Colors[ImGuiCol_Header] = PURPLE;
        style.Colors[ImGuiCol_HeaderHovered] = MAGENTA;
        style.Colors[ImGuiCol_HeaderActive] = MAGENTA;
        style.Colors[ImGuiCol_FrameBg] = PURPLE;
        style.Colors[ImGuiCol_FrameBgHovered] = PURPLE;
        style.Colors[ImGuiCol_FrameBgActive] = MAGENTA;
        style.Colors[ImGuiCol_SliderGrab] = BLACK_TRANSPARENT;
        style.Colors[ImGuiCol_SliderGrabActive] = BLACK_TRANSPARENT;
        style.Colors[ImGuiCol_CheckMark] = WHITE;

        init = true;
    }

    static bool main_window_visible = true;
    if (GetAsyncKeyState(VK_F11) & 1)
        main_window_visible = !main_window_visible;

    if (!main_window_visible)
        return wglSwapBuffersGateway(hDc);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO &io = ImGui::GetIO();
    ImGui::PushFont(font);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
    ImGui::Begin("Freedom", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    static uintptr_t osu_auth_base = GetModuleBaseAddress(L"osu!auth.dll");
    static uintptr_t current_song_ptr = internal_multi_level_pointer_dereference(g_process, osu_auth_base + selected_song_ptr_base_offset, selected_song_ptr_offsets);
    static char song_name_u8[128] = {'F', 'r', 'e', 'e', 'd', 'o', 'm', '\0'};
    if (current_song_ptr)
    {
        uintptr_t song_name_ptr = 0;
        if (internal_memory_read(g_process, current_song_ptr, &song_name_ptr))
        {
            song_name_ptr += 0x80;
            static uintptr_t prev_song_name_ptr = 0;
            if (song_name_ptr != prev_song_name_ptr)
            {
                char *song_name_u16 = 0;
                if (internal_memory_read(g_process, song_name_ptr, &song_name_u16))
                {
                    // uint32_t song_name_length = *(uint32_t *)(*(char **)song_name_ptr + 0x4);
                    song_name_u16 += 0x8;
                    ATL::CW2A utf8((wchar_t *)song_name_u16, CP_UTF8);
                    memcpy(song_name_u8, utf8.m_psz, 127);
                }
            }
            prev_song_name_ptr = song_name_ptr;
        }
    }
    else
    {
        current_song_ptr = internal_multi_level_pointer_dereference(g_process, osu_auth_base + selected_song_ptr_base_offset, selected_song_ptr_offsets);
    }

    ImGui::Text("%s", song_name_u8);

    if (ImGui::BeginPopupContextItem("##settings"))
    {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        static char preview_font_size[16] = {0};
        stbsp_snprintf(preview_font_size, 16, "Font Size: %dpx", (int)g.Font->ConfigData->SizePixels);

        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Settings").x) * 0.5f);
        ImGui::Text("Settings");
        ImGui::Dummy(ImVec2(0.0f, 5.0f));

#define ITEM_DISABLED ImVec4(0.50f, 0.50f, 0.50f, 1.00f)
        if (!ar_lock)
        {
            if (current_song_ptr)
            {
                uintptr_t current_song_ar_ptr = 0;
                if (internal_memory_read(g_process, current_song_ptr, &current_song_ar_ptr))
                {
                    current_song_ar_ptr += 0x2C;
                    internal_memory_read(g_process, current_song_ar_ptr, &ar_value);
                }
            }
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleColor(ImGuiCol_Text, ITEM_DISABLED);
            ImGui::SliderFloat("##AR", &ar_value, 0.0f, 10.0f, "AR: %.1f");
            ImGui::PopStyleColor();
            ImGui::PopItemFlag();
        }
        else
        {
            ImGui::SliderFloat("##AR", &ar_value, 0.0f, 10.0f, "AR: %.1f");
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("##ar_lock", &ar_lock))
            ar_lock ? enable_ar_hooks() : disable_ar_hooks();

        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        if (ImGui::BeginCombo("##font_size", preview_font_size))
        {
            for (const auto &f : io.Fonts->Fonts)
            {
                char font_size[8] = {0};
                stbsp_snprintf(font_size, 4, "%d", (int)f->ConfigData->SizePixels);
                const bool is_selected = f == font;
                if (ImGui::Selectable(font_size, is_selected))
                    font = f;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    ImGui::PopFont();

    io.MouseDrawCursor = io.WantCaptureMouse;

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return wglSwapBuffersGateway(hDc);
}

DWORD WINAPI freedom_main(HMODULE hModule)
{
    // AllocConsole();
    // FILE* f;
    // freopen_s(&f, "CONOUT$", "w", stdout);

    Hook SwapBuffersHook("wglSwapBuffers", "opengl32.dll", (BYTE *)freedom_update, (BYTE *)&wglSwapBuffersGateway, 5);
    SwapBuffersHook.Enable();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)freedom_main,
                                 hModule, 0, nullptr));
    default:
        break;
    }
    return TRUE;
}
