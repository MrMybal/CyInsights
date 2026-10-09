// CyInsights native Win32/D3D11 host. Dear ImGui docking and detached windows.
// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "Branding.hpp"
#include "ResourceIds.h"
#include <chrono>
#include <d3d11.h>
#include <exception>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shellapi.h>

namespace {
ID3D11Device *device = nullptr;
ID3D11DeviceContext *context = nullptr;
IDXGISwapChain *swap = nullptr;
ID3D11RenderTargetView *target = nullptr;
ID3D11ShaderResourceView *brand_logo = nullptr;
UINT width = 0, height = 0;
std::filesystem::path diagnostic;
void Log(const std::string &line) {
    if (!diagnostic.empty())
        std::ofstream(diagnostic, std::ios::app) << line << "\n";
}
LONG WINAPI Fault(EXCEPTION_POINTERS *exception) {
    HMODULE module = nullptr;
    wchar_t name[32768]{};
    auto address = exception->ExceptionRecord->ExceptionAddress;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(address), &module);
    GetModuleFileNameW(module, name, 32768);
    Log("Unhandled exception code=" + std::to_string(exception->ExceptionRecord->ExceptionCode) +
        " module=" + std::filesystem::path(name).u8string() + " offset=" +
        std::to_string(reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module)));
    return EXCEPTION_EXECUTE_HANDLER;
}
void Target() {
    ID3D11Texture2D *t = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&t)))) {
        device->CreateRenderTargetView(t, nullptr, &target);
        t->Release();
    }
}
void Clean() {
    if(brand_logo){brand_logo->Release();brand_logo=nullptr;}
    if (target) {
        target->Release();
        target = nullptr;
    }
    if (swap)
        swap->Release();
    if (context)
        context->Release();
    if (device)
        device->Release();
}
bool SaveSnapshot(const std::filesystem::path &path) {
    ID3D11Texture2D *back = nullptr;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&back))))
        return false;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D *staging = nullptr;
    if (FAILED(device->CreateTexture2D(&d, nullptr, &staging))) {
        back->Release();
        return false;
    }
    context->CopyResource(staging, back);
    back->Release();
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
        staging->Release();
        return false;
    }
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    fh.bfType = 0x4d42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + d.Width * d.Height * 4;
    ih.biSize = sizeof(ih);
    ih.biWidth = d.Width;
    ih.biHeight = -static_cast<LONG>(d.Height);
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char *>(&fh), sizeof(fh));
    out.write(reinterpret_cast<char *>(&ih), sizeof(ih));
    std::vector<unsigned char> row(d.Width * 4);
    for (UINT y = 0; y < d.Height; ++y) {
        auto *src = static_cast<unsigned char *>(map.pData) + y * map.RowPitch;
        for (UINT x = 0; x < d.Width; ++x) {
            row[4 * x] = src[4 * x + 2];
            row[4 * x + 1] = src[4 * x + 1];
            row[4 * x + 2] = src[4 * x];
            row[4 * x + 3] = 255;
        }
        out.write(reinterpret_cast<char *>(row.data()), row.size());
    }
    context->Unmap(staging, 0);
    staging->Release();
    return bool(out);
}
} // namespace
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI WindowProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, w, l))
        return true;
    if (msg == WM_SIZE && w != SIZE_MINIMIZED) {
        width = LOWORD(l);
        height = HIWORD(l);
        return 0;
    }
    if (msg == WM_SYSCOMMAND && (w & 0xfff0) == SC_KEYMENU)
        return 0;
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if(msg==WM_PAINT&&ImGui::GetCurrentContext()==nullptr) {
        PAINTSTRUCT paint{};auto dc=BeginPaint(h,&paint);
        RECT area{};GetClientRect(h,&area);auto brush=CreateSolidBrush(RGB(31,31,31));
        FillRect(dc,&area,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,230,235));
        const wchar_t label[]=L"Starting CyInsights - preparing the renderer...";
        TextOutW(dc,24,28,label,static_cast<int>(std::size(label)-1));EndPaint(h,&paint);return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, 32768);
    diagnostic = std::filesystem::path(executable).parent_path() /
                 ("CyInsightsApp-" + std::to_string(GetCurrentProcessId()) + ".log");
    Log("Starting");
    SetUnhandledExceptionFilter(Fault);
    std::set_terminate([] {
        try {
            if (auto exception = std::current_exception())
                std::rethrow_exception(exception);
        } catch (const std::exception &e) {
            Log(std::string("Terminated: ") + e.what());
        } catch (...) {
            Log("Terminated: unknown exception");
        }
        ExitProcess(3);
    });
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WindowProc,    0,      0, instance, nullptr, nullptr,
                   nullptr,    nullptr,    L"CyInsights", nullptr};
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_CYINSIGHTS));
    wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_CYINSIGHTS),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    RegisterClassExW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"CyInsights — Unreal & GPU profiling", WS_OVERLAPPEDWINDOW,
                                60, 60, 1680, 980, nullptr, nullptr, instance, nullptr);
    ShowWindow(window,SW_MAXIMIZE);UpdateWindow(window);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level;
    auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1,
                                                D3D11_SDK_VERSION, &desc, &swap, &device, &level, &context);
    if (FAILED(result)) {
        Log("Hardware device failed: " + std::to_string(static_cast<unsigned>(result)));
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 1,
                                               D3D11_SDK_VERSION, &desc, &swap, &device, &level, &context);
    }
    if (FAILED(result)) {
        Log("Renderer initialization failed: " + std::to_string(static_cast<unsigned>(result)));
        Clean();
        return 1;
    }
    Target();
    std::string brand_error;
    brand_logo=cyinsights::LoadBrandLogo(instance,device,brand_error);
    Log(brand_logo?"Embedded brand logo ready":brand_error);
    Log(wc.hIcon&&wc.hIconSm?"Windows brand icons ready":"Windows brand icon unavailable");
    Log("Renderer ready");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable |
                      ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    static auto ini = (std::filesystem::path(path).parent_path() / "CyInsights-layout-v2.ini").u8string();
    io.IniFilename = ini.c_str();
    // Read the Windows UI font at runtime; no system font is redistributed.
    wchar_t windows[32768]{};
    GetWindowsDirectoryW(windows, 32768);
    auto font = (std::filesystem::path(windows) / "Fonts/segoeui.ttf").u8string();
    if (std::filesystem::is_regular_file(font))
        io.Fonts->AddFontFromFileTTF(font.c_str(), 14.f);
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.FrameRounding = 3;
    style.TabRounding = 0;
    style.ScrollbarSize = 11;
    style.WindowPadding = ImVec2(5, 4);
    style.FramePadding = ImVec2(5, 3);
    style.ItemSpacing = ImVec2(5, 3);
    float dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
    style.ScaleAllSizes(dpi_scale);
    style.FontScaleDpi = dpi_scale;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.12f, .12f, .12f, 1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(.10f, .10f, .10f, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(.16f, .29f, .42f, 1);
    style.Colors[ImGuiCol_Tab] = ImVec4(.09f, .09f, .09f, 1);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(.22f, .22f, .22f, 1);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(.28f, .28f, .28f, 1);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(.18f, .18f, .18f, 1);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(.12f, .12f, .12f, 1);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(.16f, .16f, .16f, 1);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(.25f, .25f, .25f, 1);
    style.Colors[ImGuiCol_TabDimmed] = ImVec4(.10f, .10f, .10f, 1);
    style.Colors[ImGuiCol_TabDimmedSelected] = ImVec4(.19f, .19f, .19f, 1);
    style.Colors[ImGuiCol_TabSelectedOverline] = ImVec4(.15f, .48f, .78f, 1);
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(.28f, .28f, .28f, 1);
    style.Colors[ImGuiCol_Border] = ImVec4(.22f, .22f, .22f, 1);
    style.Colors[ImGuiCol_Separator] = ImVec4(.22f, .22f, .22f, 1);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(.17f, .17f, .17f, 1);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(.40f, .40f, .40f, 1);
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, context);
    ShowWindow(window, SW_MAXIMIZE);
    UpdateWindow(window);
    {
        cyinsights::Application app;
        app.SetBrandLogo(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(brand_logo)));
        int count = 0;
        auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        std::filesystem::path snapshot;
        std::filesystem::path loading_snapshot;
        bool navigation_smoke=false,drag_pan=false,drag_zoom=false,wheel_zoom=false;
        bool diagnostic_smoke=false,diagnostic_focused=false;int diagnostic_settle=0;
        bool inspect_task=false,task_inspected=false;
        bool interaction_smoke=false,interaction_filter=false,interaction_range=false,interaction_dock=false,interaction_bin=false;
        int interaction_step=0,interaction_selected_bin=0;uint64_t interaction_count=0,interaction_full_count=0;
        std::string interaction_name;double interaction_begin=0,interaction_before_a=0,interaction_before_b=0;
        int navigation_step=0,navigation_wait=0,drag_samples=0;
        bool continuous_drag=true,frames_follow=false,dynamic_height=false,ruler_drag=false,track_drag=false;
        uint64_t drag_reload_count=0;
        double drag_started=0;
        auto navigation_point=ImVec2();
        cyinsights::Application::NavigationState navigation_before{},navigation_after_drag{};
        for (int i = 1; i < count; ++i) {
            std::wstring argument = args[i];
            if (argument == L"--snapshot" && i + 1 < count)
                snapshot = args[++i];
            else if(argument==L"--loading-snapshot"&&i+1<count)
                loading_snapshot=args[++i];
            else if(argument==L"--navigation-smoke")navigation_smoke=true;
            else if(argument==L"--diagnostics-smoke")diagnostic_smoke=true;
            else if(argument==L"--distribution-view")app.FocusDistribution();
            else if(argument==L"--tasks-view")app.FocusTasks();
            else if(argument==L"--inspect-task"){app.FocusTasks();inspect_task=true;}
            else if(argument==L"--interaction-smoke")interaction_smoke=true;
            else if(argument==L"--ai-enable")app.EnableAi();
            else if(argument==L"--ai-control")app.EnableAi(true);
            else if(argument==L"--ai-capture")app.EnableAi(true,true);
            else if(argument==L"--ai-view")app.FocusAi();
            else if(argument==L"--session-view")app.FocusSession();
            else if (argument == L"--gpu-view")
                app.FocusGpu();
            else if (argument == L"--graph-view")
                app.FocusGraph();
            else if (argument.rfind(L"--connect=", 0) == 0)
                app.Connect(static_cast<uint32_t>(wcstoul(argument.c_str() + 10, nullptr, 10)));
            else
                app.Open(args[i]);
        }
        if (args)
            LocalFree(args);
        int frames = 0;
        auto begin = std::chrono::steady_clock::now();
        bool done = false;
        while (!done) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (msg.message == WM_QUIT)
                    done = true;
            }
            if (done)
                break;
            if (width && height) {
                if (target) {
                    target->Release();
                    target = nullptr;
                }
                swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                width = height = 0;
                Target();
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            if(interaction_smoke){
                io.ConfigInputTrickleEventQueue=false;const auto state=app.InteractionView();const auto nav=app.NavigationView();
                auto pointer=[&](float x,float y){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(x,y);};
                if(interaction_step==0&&!app.AnalysisBusy()&&!state.scope_name.empty()){
                    interaction_name=state.scope_name;interaction_before_a=nav.start;interaction_before_b=nav.end;interaction_begin=ImGui::GetTime();pointer(state.scope_x,state.scope_y);interaction_step=1;
                }else if(interaction_step==1){io.AddMouseButtonEvent(0,true);interaction_step=2;}
                else if(interaction_step==2){io.AddMouseButtonEvent(0,false);interaction_step=3;}
                else if(interaction_step==3){io.AddMouseButtonEvent(0,true);interaction_step=4;}
                else if(interaction_step==4){io.AddMouseButtonEvent(0,false);interaction_step=5;}
                else if(interaction_step==5&&!app.AnalysisBusy()){
                    interaction_filter=state.filter_name==interaction_name&&nav.start==interaction_before_a&&nav.end==interaction_before_b;
                    pointer(state.scope_x,state.scope_y);io.AddMouseButtonEvent(1,true);interaction_step=6;
                }else if(interaction_step==6){io.AddMouseButtonEvent(1,false);interaction_step=7;}
                else if(interaction_step==7&&state.menu_x>0){pointer(state.menu_x,state.menu_y);io.AddMouseButtonEvent(0,true);interaction_step=8;}
                else if(interaction_step==8){io.AddMouseButtonEvent(0,false);interaction_step=9;}
                else if(interaction_step==9&&!app.AnalysisBusy()&&state.count&&state.function_name==interaction_name){
                    interaction_full_count=state.count;interaction_dock=state.separate_dock;
                    pointer(nav.ruler_x+nav.ruler_width*.2f,nav.ruler_y+3);io.AddKeyEvent(ImGuiMod_Shift,true);io.AddMouseButtonEvent(0,true);interaction_step=10;
                }else if(interaction_step==10){pointer(nav.ruler_x+nav.ruler_width*.65f,nav.ruler_y+3);interaction_step=11;}
                else if(interaction_step==11){io.AddMouseButtonEvent(0,false);io.AddKeyEvent(ImGuiMod_Shift,false);interaction_step=12;}
                else if(interaction_step==12&&!app.AnalysisBusy()&&state.selection_a!=state.selection_b&&state.hist_a==std::min(state.selection_a,state.selection_b)&&state.hist_b==std::max(state.selection_a,state.selection_b)){
                    interaction_range=state.count>0&&state.count<interaction_full_count;interaction_count=state.count;
                    interaction_selected_bin=static_cast<int>(std::max_element(state.bins.begin(),state.bins.end())-state.bins.begin());
                    pointer(state.hist_x+state.hist_width*(interaction_selected_bin+.5f)/64,state.hist_y+30);interaction_step=13;
                }else if(interaction_step==13){io.AddMouseButtonEvent(0,true);interaction_step=14;}
                else if(interaction_step==14){io.AddMouseButtonEvent(0,false);interaction_step=15;}
                else if(interaction_step==15&&!app.AnalysisBusy()&&state.bin==interaction_selected_bin){
                    interaction_bin=state.count==interaction_count&&state.matched==state.bins[interaction_selected_bin];interaction_step=16;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"interaction-smoke.json");
                    report<<std::boolalpha<<"{\"double_click_exact_without_zoom\":"<<interaction_filter<<",\"function_histogram_from_context\":"<<(state.function_name==interaction_name)
                        <<",\"timeline_range_filters_histogram\":"<<interaction_range<<",\"histogram_docked_beside_timeline\":"<<interaction_dock<<",\"cached_bin_retains_statistics\":"<<interaction_bin
                        <<",\"full_samples\":"<<interaction_full_count<<",\"range_samples\":"<<interaction_count<<",\"selected_range_start\":"<<state.hist_a<<",\"selected_range_end\":"<<state.hist_b<<"}";
                }
                if(interaction_step>0&&interaction_step<16&&ImGui::GetTime()-interaction_begin>90){std::ofstream(std::filesystem::path(executable).parent_path()/"interaction-failure.txt")<<interaction_step;return 7;}
            }
            if(navigation_smoke) {
                // Exercise held input over multiple asynchronous publications, not a single move.
                io.ConfigInputTrickleEventQueue=false;
                const auto navigation=app.NavigationView();
                auto pointer=[&](float x,float y) {io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(x,y);};
                if(navigation_step==0&&!app.AnalysisBusy()&&navigation.width>0) {
                    navigation_before=navigation;drag_started=ImGui::GetTime();drag_samples=0;
                    navigation_point=ImVec2(navigation.x+navigation.width*.5f,navigation.y+navigation.height*.5f);
                    pointer(navigation_point.x,navigation_point.y);io.AddMouseButtonEvent(0,true);navigation_step=1;
                }else if(navigation_step==1) {
                    double elapsed=ImGui::GetTime()-drag_started;float progress=static_cast<float>(std::min(1.,elapsed/3.));
                    pointer(navigation_point.x+120*progress,navigation_point.y-15*progress);
                    if(++drag_samples>2)continuous_drag&=navigation.dragging;
                    drag_reload_count=navigation.model_version-navigation_before.model_version;
                    if((elapsed>=3.&&drag_reload_count>=3)||elapsed>8.)navigation_step=2;
                }else if(navigation_step==2) {
                    io.AddMouseButtonEvent(0,false);navigation_step=3;navigation_wait=0;
                }else if(navigation_step==3&&++navigation_wait>2&&!app.AnalysisBusy()) {
                    navigation_after_drag=navigation;
                    drag_pan=std::abs((navigation.start+navigation.end)-(navigation_before.start+navigation_before.end))>1e-9;
                    drag_zoom=navigation.end-navigation.start<navigation_before.end-navigation_before.start;
                    pointer(navigation.x+navigation.width*.5f,navigation.y+navigation.height*.5f);navigation_step=4;
                }else if(navigation_step==4) {
                    io.AddMouseWheelEvent(0,1);navigation_step=5;navigation_wait=0;
                }else if(navigation_step==5&&++navigation_wait>2&&!app.AnalysisBusy()) {
                    wheel_zoom=navigation.end-navigation.start<navigation_after_drag.end-navigation_after_drag.start;
                    navigation_before=navigation;drag_started=ImGui::GetTime();drag_samples=0;
                    navigation_point=ImVec2(navigation.ruler_x+navigation.ruler_width*.5f,navigation.ruler_y+4);
                    pointer(navigation_point.x,navigation_point.y);io.AddMouseButtonEvent(0,true);navigation_step=7;
                }else if(navigation_step==7) {
                    double elapsed=ImGui::GetTime()-drag_started;float progress=static_cast<float>(std::min(1.,elapsed/2.));
                    // Continue holding after leaving the ruler and entering the track area.
                    pointer(navigation_point.x-80*progress,navigation_point.y+60*progress);
                    if(++drag_samples>2)continuous_drag&=navigation.dragging;
                    if((elapsed>=2.&&navigation.model_version>=navigation_before.model_version+2)||elapsed>8.) {
                        ruler_drag=navigation.dragging&&navigation.start!=navigation_before.start;
                        io.AddMouseButtonEvent(0,false);navigation_step=8;navigation_wait=0;
                    }
                }else if(navigation_step==8&&++navigation_wait>2&&!app.AnalysisBusy()) {
                    navigation_before=navigation;drag_started=ImGui::GetTime();drag_samples=0;
                    navigation_point=ImVec2(navigation.tracks_x+navigation.ruler_width*.5f,navigation.tracks_y);
                    pointer(navigation_point.x,navigation_point.y);io.AddMouseButtonEvent(0,true);navigation_step=9;
                }else if(navigation_step==9) {
                    double elapsed=ImGui::GetTime()-drag_started;float progress=static_cast<float>(std::min(1.,elapsed/2.));
                    pointer(navigation_point.x+100*progress,navigation_point.y);
                    if(++drag_samples>2)continuous_drag&=navigation.dragging;
                    if((elapsed>=2.&&navigation.model_version>=navigation_before.model_version+2)||elapsed>8.) {
                        track_drag=navigation.dragging&&navigation.start!=navigation_before.start;
                        io.AddMouseButtonEvent(0,false);navigation_step=10;navigation_wait=0;
                    }
                }else if(navigation_step==10&&++navigation_wait>2&&!app.AnalysisBusy()) {
                    frames_follow=navigation.frames_start==navigation.start&&navigation.frames_end==navigation.end&&
                        std::abs(navigation.x-navigation.ruler_x)<1&&std::abs(navigation.width-navigation.ruler_width)<1;
                    dynamic_height=navigation.visible_rows<navigation.recorded_rows;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"navigation-smoke.json");
                    report<<std::boolalpha<<"{\"horizontal_pan\":"<<drag_pan<<",\"vertical_zoom\":"<<drag_zoom<<",\"wheel_zoom\":"<<wheel_zoom
                        <<",\"continuous_drag\":"<<continuous_drag<<",\"reloads_while_held\":"<<drag_reload_count
                        <<",\"frames_follow_timeline\":"<<frames_follow<<",\"dynamic_track_height\":"<<dynamic_height
                        <<",\"ruler_drag_outside_widget\":"<<ruler_drag<<",\"track_drag\":"<<track_drag
                        <<",\"visible_rows\":"<<navigation.visible_rows<<",\"recorded_rows\":"<<navigation.recorded_rows<<"}";
                    navigation_step=11;
                }
            }
            if(diagnostic_smoke&&!app.AnalysisBusy()&&(!navigation_smoke||navigation_step==11)) {
                if(!diagnostic_focused){if(!app.FocusSlowFrame())return 5;diagnostic_focused=true;}
                auto nav=app.NavigationView();io.AddMousePosEvent(nav.x+nav.width*.5f,nav.y+nav.height*.6f);
                ++diagnostic_settle;
            }
            ImGui::NewFrame();
            app.Draw();
            if(inspect_task&&!task_inspected&&frames>20&&!app.AnalysisBusy()){
                task_inspected=app.InspectFirstTask();if(!task_inspected)return 6;
            }
            ImGui::Render();
            float clear[] = {.06f, .06f, .065f, 1};
            context->OMSetRenderTargets(1, &target, nullptr);
            context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if(!loading_snapshot.empty()&&frames>5&&app.AnalysisBusy()) {
                SaveSnapshot(loading_snapshot);loading_snapshot.clear();
            }
            if (!snapshot.empty() && ++frames > 20 && !app.AnalysisBusy()&&(!navigation_smoke||navigation_step==11)&&(!diagnostic_smoke||diagnostic_settle>5)&&(!interaction_smoke||interaction_step==16)) {
                if (!SaveSnapshot(snapshot))
                    return 2;
                auto report=snapshot;report+=L".json";app.WriteAnalysisReport(report);
                if(navigation_smoke&&(!drag_pan||!drag_zoom||!wheel_zoom||!continuous_drag||drag_reload_count<3||!frames_follow||!dynamic_height||!ruler_drag||!track_drag))return 4;
                if(interaction_smoke&&(!interaction_filter||!interaction_range||!interaction_dock||!interaction_bin))return 8;
                done = true;
            }
            if (!snapshot.empty() && std::chrono::steady_clock::now() - begin > std::chrono::minutes(20))
                done = true;
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
            swap->Present(1, 0);
        }
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Clean();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, instance);
    Log("Closed");
    return 0;
}
