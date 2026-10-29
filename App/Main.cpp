// CyInsights native Win32/D3D11 host. Dear ImGui docking and detached windows.
// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "DocumentWorkspace.hpp"
#include "Branding.hpp"
#include "QuestTileReplay.hpp"
#include "UiTheme.hpp"
#include "ResourceIds.h"
#include <chrono>
#include <algorithm>
#include <d3d11.h>
#include <exception>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
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
    HWND window = CreateWindowW(wc.lpszClassName, L"CyInsights - Unreal & GPU profiling", WS_OVERLAPPEDWINDOW,
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
    cyinsights::InitializeUi(ImGui_ImplWin32_GetDpiScaleForHwnd(window));
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, context);
    ShowWindow(window, SW_MAXIMIZE);
    UpdateWindow(window);
    {
        cyinsights::SetQuestImageDevice(device);
        cyinsights::DocumentWorkspace workspace(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(brand_logo)));
        auto& startup_app=workspace.Primary();
        bool multi_trace_smoke=false,ai_control_all=false;int multi_trace_step=0,multi_trace_wait=0;
        bool document_lifecycle_smoke=false;int document_lifecycle_step=0,document_lifecycle_wait=0;
        bool standalone_ai_smoke=false,standalone_ai_complete=false;size_t standalone_ai_step=0;
        std::future<cygi::Json> standalone_ai_future;auto standalone_ai_results=cygi::Json::Array();
        const std::array<const char*,7> standalone_ai_operations{{"get_state","get_analysis_guide","get_capabilities","histogram","query_frames","query_scopes","list_counters"}};
        int count = 0;
        auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        std::filesystem::path snapshot;
        std::filesystem::path loading_snapshot;
        std::filesystem::path ui_profile;std::vector<double> draw_profile,drag_profile,command_profile;
        bool vulkan_details_view=false,vulkan_details_focused=false;int vulkan_details_settle=0;
        bool navigation_smoke=false,drag_pan=false,drag_zoom=false,wheel_zoom=false;
        bool frame_stats_range_smoke=false,frame_stats_range_filtered=false;int frame_stats_step=0;uint64_t frame_stats_full=0;
        bool activity_range_smoke=false,activity_range_filtered=false,activity_plot_reused=false;int activity_step=0;uint64_t activity_full=0,activity_plot_before=0;
        bool diagnostic_smoke=false,diagnostic_focused=false;int diagnostic_settle=0;
        bool inspect_task=false,task_inspected=false;
        bool interaction_smoke=false,interaction_filter=false,interaction_range=false,interaction_dock=false,interaction_bin=false,interaction_clear=false,interaction_blank_clear=false;
        int interaction_step=0,interaction_selected_bin=0,interaction_menu_wait=0;uint64_t interaction_count=0,interaction_full_count=0;
        std::string interaction_name;double interaction_begin=0,interaction_before_a=0,interaction_before_b=0;
        int navigation_step=0,navigation_wait=0,drag_samples=0;
        bool continuous_drag=true,frames_follow=false,dynamic_height=false,ruler_drag=false,track_drag=false,right_pan=false,right_scroll=false,right_no_menu=false;
        uint64_t drag_reload_count=0;
        double drag_started=0;
        auto navigation_point=ImVec2();
        cyinsights::Application::NavigationState navigation_before{},navigation_after_drag{};
        for (int i = 1; i < count; ++i) {
            std::wstring argument = args[i];
            if (argument == L"--snapshot" && i + 1 < count)
                snapshot = args[++i];
            else if(argument==L"--ui-theme"&&i+1<count){auto theme=std::filesystem::path(args[++i]).u8string();if(!cyui::setTheme(theme.c_str()))return 23;}
            else if(argument==L"--ui-comfortable")cyui::setDensity(cyui::Density::comfortable);
            else if(argument==L"--ui-size"&&i+1<count){
                unsigned client_width=0,client_height=0;
                if(swscanf_s(args[++i],L"%ux%u",&client_width,&client_height)!=2||client_width<800||client_height<600||client_width>7680||client_height>4320)return 23;
                RECT rect{0,0,static_cast<LONG>(client_width),static_cast<LONG>(client_height)};
                AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
                ShowWindow(window,SW_RESTORE);SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            }
            else if(argument==L"--profile-ui"&&i+1<count)ui_profile=args[++i];
            else if(argument==L"--loading-snapshot"&&i+1<count)
                loading_snapshot=args[++i];
            else if(argument==L"--multi-trace-smoke")multi_trace_smoke=true;
            else if(argument==L"--document-lifecycle-smoke")document_lifecycle_smoke=true;
            else if(argument==L"--ai-control-all")ai_control_all=true;
            else if(argument==L"--vulkan-details-view")vulkan_details_view=true;
            else if(argument==L"--navigation-smoke")navigation_smoke=true;
            else if(argument==L"--frame-stats-range-smoke"){frame_stats_range_smoke=true;startup_app.FocusFrameStats();}
            else if(argument==L"--activity-range-smoke"){activity_range_smoke=true;startup_app.FocusActivities();}
            else if(argument==L"--diagnostics-smoke")diagnostic_smoke=true;
            else if(argument==L"--distribution-view")startup_app.FocusDistribution();
            else if(argument==L"--tasks-view")startup_app.FocusTasks();
            else if(argument==L"--activities-view")startup_app.FocusActivities();
            else if(argument==L"--inspect-task"){startup_app.FocusTasks();inspect_task=true;}
            else if(argument==L"--interaction-smoke")interaction_smoke=true;
            else if(argument==L"--ai-enable")startup_app.EnableAi();
            else if(argument==L"--ai-control")startup_app.EnableAi(true);
            else if(argument==L"--ai-capture")startup_app.EnableAi(true,true);
            else if(argument==L"--ai-view")startup_app.FocusAi();
            else if(argument==L"--ai-method-view")startup_app.FocusAiMethod();
            else if(argument==L"--standalone-ai-smoke"){standalone_ai_smoke=true;startup_app.FocusAiMethod();}
            else if(argument==L"--session-view")startup_app.FocusSession();
            else if(argument==L"--capture-options-view")startup_app.FocusCaptureOptions();
            else if (argument == L"--gpu-view")
                startup_app.FocusGpu();
            else if (argument == L"--graph-view")
                startup_app.FocusGraph();
            else if(argument==L"--frame-stats-view")
                startup_app.FocusFrameStats();
            else if (argument.rfind(L"--connect=", 0) == 0)
                startup_app.Connect(static_cast<uint32_t>(wcstoul(argument.c_str() + 10, nullptr, 10)));
            else
                workspace.Open(args[i]);
        }
        if (args)
            LocalFree(args);
        if(ai_control_all)for(size_t i=0;i<workspace.Count();++i)workspace.At(i).EnableAi(true);
        int frames = 0;
        auto begin = std::chrono::steady_clock::now();
        bool done = false;
        while (!done) {
            auto& app=workspace.Primary();
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
            const float dpi=ImGui_ImplWin32_GetDpiScaleForHwnd(window);
            if(std::abs(cyui::dpi()-dpi)>.01f)cyui::applyDpi(dpi);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            if(frame_stats_range_smoke){
                io.ConfigInputTrickleEventQueue=false;const auto state=app.FrameStatsViewState();const auto nav=app.NavigationView();
                auto pointer=[&](float fraction){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(nav.ruler_x+nav.ruler_width*fraction,nav.ruler_y+3);};
                if(frame_stats_step==0&&!app.AnalysisBusy()&&state.rows>1&&state.curve){frame_stats_full=state.rows;pointer(.05f);io.AddKeyEvent(ImGuiMod_Shift,true);io.AddMouseButtonEvent(0,true);frame_stats_step=1;}
                else if(frame_stats_step==1){pointer(.5f);frame_stats_step=2;}
                else if(frame_stats_step==2){io.AddMouseButtonEvent(0,false);io.AddKeyEvent(ImGuiMod_Shift,false);frame_stats_step=3;}
                else if(frame_stats_step==3&&!app.AnalysisBusy()&&state.selected_interval){
                    frame_stats_range_filtered=state.rows>0&&state.rows<frame_stats_full&&state.curve;frame_stats_step=4;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"frame-stats-range-smoke.json");report<<std::boolalpha<<"{\"timeline_range_filters_frame_stats\":"<<frame_stats_range_filtered<<",\"curve_retained\":"<<state.curve<<",\"visible_frames\":"<<frame_stats_full<<",\"selected_frames\":"<<state.rows<<'}';
                }
            }
            if(activity_range_smoke){
                io.ConfigInputTrickleEventQueue=false;auto state=app.ActivityView();auto nav=app.NavigationView();auto range=app.InteractionView();
                auto pointer=[&](float fraction){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(nav.ruler_x+nav.ruler_width*fraction,nav.ruler_y+3);};
                if(activity_step==0&&!app.AnalysisBusy()&&state.events&&state.matches>1){activity_full=state.matches;activity_plot_before=state.plot_queries;pointer(.05f);io.AddKeyEvent(ImGuiMod_Shift,true);io.AddMouseButtonEvent(0,true);activity_step=1;}
                else if(activity_step==1){pointer(.5f);activity_step=2;}
                else if(activity_step==2){io.AddMouseButtonEvent(0,false);io.AddKeyEvent(ImGuiMod_Shift,false);activity_step=3;}
                else if(activity_step==3&&!app.AnalysisBusy()&&range.selection_a!=range.selection_b&&state.a==std::min(range.selection_a,range.selection_b)&&state.b==std::max(range.selection_a,range.selection_b)){
                    activity_range_filtered=state.matches>0&&state.matches<activity_full;activity_plot_reused=state.plot_queries==activity_plot_before;activity_step=4;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"activity-range-smoke.json");report<<std::boolalpha<<"{\"timeline_range_filters_activity_details\":"<<activity_range_filtered<<",\"selection_reuses_plot_query\":"<<activity_plot_reused<<",\"full_matches\":"<<activity_full<<",\"selected_matches\":"<<state.matches<<'}';
                }
            }
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
                // Popups obtain their final size/position after their first frame.
                else if(interaction_step==7&&state.menu_x>0&&++interaction_menu_wait>2){pointer(state.menu_x,state.menu_y);io.AddMouseButtonEvent(0,true);interaction_step=8;}
                else if(interaction_step==8){io.AddMouseButtonEvent(0,false);interaction_step=9;}
                else if(interaction_step==9&&!app.AnalysisBusy()&&state.count&&state.function_name==interaction_name){
                    interaction_full_count=state.count;interaction_dock=state.separate_dock;
                    pointer(nav.measure_x+nav.measure_width*.65f,nav.measure_y+nav.measure_height*.5f);io.AddMouseButtonEvent(0,true);interaction_step=10;
                }else if(interaction_step==10){pointer(nav.measure_x+nav.measure_width*.2f,nav.measure_y+nav.measure_height+30);interaction_step=11;}
                else if(interaction_step==11){io.AddMouseButtonEvent(0,false);interaction_step=12;}
                else if(interaction_step==12&&!app.AnalysisBusy()&&state.selection_a!=state.selection_b&&state.hist_a==std::min(state.selection_a,state.selection_b)&&state.hist_b==std::max(state.selection_a,state.selection_b)){
                    interaction_range=state.count>0&&state.count<interaction_full_count&&nav.start==interaction_before_a&&nav.end==interaction_before_b;interaction_count=state.count;
                    interaction_selected_bin=static_cast<int>(std::max_element(state.bins.begin(),state.bins.end())-state.bins.begin());
                    pointer(state.hist_x+state.hist_width*(interaction_selected_bin+.5f)/64,state.hist_y+30);interaction_step=13;
                }else if(interaction_step==13){io.AddMouseButtonEvent(0,true);interaction_step=14;}
                else if(interaction_step==14){io.AddMouseButtonEvent(0,false);interaction_step=15;}
                else if(interaction_step==15&&!app.AnalysisBusy()&&state.bin==interaction_selected_bin){
                    interaction_bin=state.count==interaction_count&&state.matched==state.bins[interaction_selected_bin];
                    pointer(nav.ruler_x+nav.ruler_width*.5f,nav.ruler_y+3);interaction_step=17;
                }else if(interaction_step==17){io.AddMouseButtonEvent(0,true);interaction_step=18;}
                else if(interaction_step==18){io.AddMouseButtonEvent(0,false);interaction_step=19;}
                else if(interaction_step==19){io.AddMouseButtonEvent(0,true);interaction_step=20;}
                else if(interaction_step==20){io.AddMouseButtonEvent(0,false);interaction_step=21;}
                else if(interaction_step==21&&!app.AnalysisBusy()){
                    interaction_clear=state.filter_name.empty()&&state.selection_a!=state.selection_b;
                    app.FilterFunction(interaction_name);interaction_step=22;
                }else if(interaction_step==22&&!app.AnalysisBusy()&&state.blank_y>0){pointer(state.blank_x,state.blank_y);interaction_step=23;}
                else if(interaction_step==23){io.AddMouseButtonEvent(0,true);interaction_step=24;}
                else if(interaction_step==24){io.AddMouseButtonEvent(0,false);interaction_step=25;}
                else if(interaction_step==25){io.AddMouseButtonEvent(0,true);interaction_step=26;}
                else if(interaction_step==26){io.AddMouseButtonEvent(0,false);interaction_step=27;}
                else if(interaction_step==27&&!app.AnalysisBusy()){
                    interaction_blank_clear=state.filter_name.empty()&&state.selection_a!=state.selection_b;interaction_step=16;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"interaction-smoke.json");
                    report<<std::boolalpha<<"{\"double_click_exact_without_zoom\":"<<interaction_filter<<",\"function_histogram_from_context\":"<<(state.function_name==interaction_name)
                        <<",\"timeline_range_filters_histogram\":"<<interaction_range<<",\"histogram_docked_beside_timeline\":"<<interaction_dock<<",\"cached_bin_retains_statistics\":"<<interaction_bin
                        <<",\"empty_double_click_clears_filter_keeps_range\":"<<interaction_clear
                        <<",\"blank_track_double_click_clears_filter\":"<<interaction_blank_clear
                        <<",\"full_samples\":"<<interaction_full_count<<",\"range_samples\":"<<interaction_count<<",\"selected_range_start\":"<<state.hist_a<<",\"selected_range_end\":"<<state.hist_b<<"}";
                }
                if(interaction_step>0&&interaction_step!=16&&ImGui::GetTime()-interaction_begin>90){auto failure=std::filesystem::path(executable).parent_path()/"interaction-failure";std::ofstream(failure.wstring()+L".txt")<<interaction_step;app.WriteAnalysisReport(failure.wstring()+L".json");if(!snapshot.empty())SaveSnapshot(snapshot);return 7;}
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
                    navigation_before=navigation;drag_started=ImGui::GetTime();drag_samples=0;
                    navigation_point=ImVec2(navigation.tracks_x+navigation.ruler_width*.5f,navigation.tracks_y);
                    pointer(navigation_point.x,navigation_point.y);io.AddMouseButtonEvent(1,true);navigation_step=12;
                }else if(navigation_step==12){
                    double elapsed=ImGui::GetTime()-drag_started;float progress=static_cast<float>(std::min(1.,elapsed/2.));
                    pointer(navigation_point.x-85*progress,navigation_point.y-100*progress);
                    if(++drag_samples>2)continuous_drag&=navigation.dragging;
                    if(elapsed>2.5){
                        right_pan=navigation.start!=navigation_before.start&&std::abs((navigation.end-navigation.start)-(navigation_before.end-navigation_before.start))<1e-9;
                        right_scroll=navigation.tracks_scroll>navigation_before.tracks_scroll+10;
                        io.AddMouseButtonEvent(1,false);navigation_step=13;navigation_wait=0;
                    }
                }else if(navigation_step==13&&++navigation_wait>2&&!app.AnalysisBusy()) {
                    right_no_menu=app.InteractionView().menu_x==0;
                    frames_follow=navigation.frames_start==navigation.start&&navigation.frames_end==navigation.end&&
                        std::abs(navigation.x-navigation.ruler_x)<1&&std::abs(navigation.width-navigation.ruler_width)<1;
                    dynamic_height=navigation.visible_rows<navigation.recorded_rows;
                    std::ofstream report(std::filesystem::path(executable).parent_path()/"navigation-smoke.json");
                    report<<std::boolalpha<<"{\"horizontal_pan\":"<<drag_pan<<",\"vertical_zoom\":"<<drag_zoom<<",\"wheel_zoom\":"<<wheel_zoom
                        <<",\"continuous_drag\":"<<continuous_drag<<",\"reloads_while_held\":"<<drag_reload_count
                        <<",\"frames_follow_timeline\":"<<frames_follow<<",\"dynamic_track_height\":"<<dynamic_height
                        <<",\"ruler_drag_outside_widget\":"<<ruler_drag<<",\"track_drag\":"<<track_drag
                        <<",\"right_drag_pan_without_zoom\":"<<right_pan<<",\"right_drag_vertical_scroll\":"<<right_scroll<<",\"right_drag_does_not_open_menu\":"<<right_no_menu
                        <<",\"visible_rows\":"<<navigation.visible_rows<<",\"recorded_rows\":"<<navigation.recorded_rows<<"}";
                    navigation_step=11;
                }
            }
            if(diagnostic_smoke&&!app.AnalysisBusy()&&(!navigation_smoke||navigation_step==11)) {
                if(!diagnostic_focused){if(!app.FocusSlowFrame())return 5;diagnostic_focused=true;}
                auto nav=app.NavigationView();io.AddMousePosEvent(nav.x+nav.width*.5f,nav.y+nav.height*.6f);
                ++diagnostic_settle;
            }
            auto draw_begin=std::chrono::steady_clock::now();
            ImGui::NewFrame();
            workspace.Draw();
            if(standalone_ai_smoke&&!standalone_ai_complete&&!workspace.Busy()&&frames>20){
                auto host=std::filesystem::path(executable).parent_path();
                if(std::filesystem::exists(host/"AI/app/CyTool.json")||std::filesystem::exists(host.parent_path().parent_path()/"AI/app/CyTool.json"))return 21;
                if(standalone_ai_future.valid()&&standalone_ai_future.wait_for(std::chrono::seconds(0))==std::future_status::ready){
                    auto local_result=standalone_ai_future.get();if(local_result["error"].IsObject()){auto diagnostic_path=snapshot;diagnostic_path+=L".standalone-ai-error.json";local_result["operation"]=standalone_ai_operations[standalone_ai_step];std::ofstream(diagnostic_path)<<local_result.Write(2);return 22;}
                    if(standalone_ai_step==0&&(!local_result["ready"].AsBool()||local_result["permissions"]["appControl"].AsBool()))return 23;
                    if(standalone_ai_step==1&&local_result["method"]["workflow"].Size()!=9)return 24;
                    if(standalone_ai_step==2&&local_result["operations"].Size()!=39)return 25;
                    if(standalone_ai_step==3&&local_result["summary"]["count"].AsString()=="0")return 26;
                    if((standalone_ai_step==4||standalone_ai_step==5)&&local_result["rows"].Size()==0)return 27;
                    auto entry=cygi::Json::Object();entry["operation"]=standalone_ai_operations[standalone_ai_step];entry["result"]=local_result;standalone_ai_results.Push(entry);++standalone_ai_step;
                }
                if(standalone_ai_step==standalone_ai_operations.size()){
                    auto proof=app.StandaloneAiState();if(proof["bridgeRunning"].AsBool()||proof["embeddedOperations"].AsNumber()!=39)return 28;
                    proof["passed"]=true;proof["externalCompanionPresent"]=false;proof["localOperations"]=standalone_ai_results;
                    auto proof_path=snapshot;proof_path+=L".standalone-ai.json";std::ofstream(proof_path)<<proof.Write(2);standalone_ai_complete=true;
                }else if(!standalone_ai_future.valid()){
                    auto parameters=cygi::Json::Object();if(standalone_ai_step>=3)parameters["limit"]=3.;standalone_ai_future=app.RunLocalAiTool(standalone_ai_operations[standalone_ai_step],parameters);
                }
            }
            if(multi_trace_smoke&&!workspace.Busy()&&frames>20){
                if(workspace.Count()<2)return 11;
                if(multi_trace_step==0){workspace.At(0).FilterFunction("MobileSceneRender");workspace.At(1).FilterFunction("FEngineLoop::Tick");workspace.Select(0);workspace.Detach(1);multi_trace_step=1;multi_trace_wait=0;}
                else if(++multi_trace_wait>8){
                    if(workspace.At(0).InteractionView().filter_name!="MobileSceneRender"||workspace.At(1).InteractionView().filter_name!="FEngineLoop::Tick")return 12;
                    if(multi_trace_step==1){if(ImGui::GetPlatformIO().Viewports.Size<2)return 13;workspace.Attach(1);workspace.Select(0);multi_trace_step=2;multi_trace_wait=0;}
                    else if(multi_trace_step==2){workspace.Detach(1);if(workspace.Count()>2)workspace.CloseCapture(2);multi_trace_step=3;multi_trace_wait=0;}
                    else multi_trace_step=4;
                }
            }
            if(document_lifecycle_smoke&&!workspace.Busy()&&frames>20&&document_lifecycle_step<4){
                if(document_lifecycle_step==0){if(workspace.Count()!=2)return 24;workspace.Detach(1);document_lifecycle_step=1;document_lifecycle_wait=0;}
                else if(++document_lifecycle_wait>8){
                    if(document_lifecycle_step==1){
                        if(ImGui::GetPlatformIO().Viewports.Size<2)return 25;
                        auto proof=snapshot;proof+=L".detached.json";workspace.WriteReport(proof);
                        workspace.Attach(1);document_lifecycle_step=2;
                    }else if(document_lifecycle_step==2){workspace.Detach(1);document_lifecycle_step=3;}
                    else {workspace.CloseCapture(1);document_lifecycle_step=4;Log("Detached document closed after reattachment cycle");}
                    document_lifecycle_wait=0;
                }
            }
            if(vulkan_details_view&&!vulkan_details_focused&&!app.AnalysisBusy())vulkan_details_focused=app.FocusVulkanPassContents();
            if(vulkan_details_focused)++vulkan_details_settle;
            if(inspect_task&&!task_inspected&&frames>20&&!app.AnalysisBusy()){
                task_inspected=app.InspectFirstTask();if(!task_inspected)return 6;
            }
            cyinsights::RenderUiExtras();
            ImGui::Render();
            auto background=cyui::vec(cyui::palette().bg0);
            float clear[] = {background.x, background.y, background.z, 1};
            context->OMSetRenderTargets(1, &target, nullptr);
            context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if(!ui_profile.empty()&&app.NavigationView().model_version){
                double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-draw_begin).count();
                draw_profile.push_back(ms);if(app.NavigationView().dragging)drag_profile.push_back(ms);
                // Count visible indexed submissions in the main viewport only.
                // This is not a GPU duration or a count of game draw calls.
                const auto* data=ImGui::GetDrawData();size_t commands=0;
                for(const auto* list:data->CmdLists)for(const auto& command:list->CmdBuffer){
                    if(command.UserCallback||!command.ElemCount)continue;
                    if(std::min(command.ClipRect.z,data->DisplayPos.x+data->DisplaySize.x)<=std::max(command.ClipRect.x,data->DisplayPos.x)||
                       std::min(command.ClipRect.w,data->DisplayPos.y+data->DisplaySize.y)<=std::max(command.ClipRect.y,data->DisplayPos.y))continue;
                    ++commands;
                }
                command_profile.push_back(static_cast<double>(commands));
            }
            if(!loading_snapshot.empty()&&frames>5&&app.AnalysisBusy()) {
                SaveSnapshot(loading_snapshot);loading_snapshot.clear();
            }
            if (!snapshot.empty() && ++frames > 20 && !workspace.Busy()&&(!standalone_ai_smoke||standalone_ai_complete)&&(!multi_trace_smoke||multi_trace_step==4)&&(!document_lifecycle_smoke||document_lifecycle_step==4&&++document_lifecycle_wait>8)&&(!vulkan_details_view||vulkan_details_settle>3)&&(!navigation_smoke||navigation_step==11)&&(!diagnostic_smoke||diagnostic_settle>5)&&(!interaction_smoke||interaction_step==16)&&(!activity_range_smoke||activity_step==4)&&(!frame_stats_range_smoke||frame_stats_step==4)) {
                if (!SaveSnapshot(snapshot))
                    return 2;
                auto report=snapshot;report+=L".json";app.WriteAnalysisReport(report);auto documents=snapshot;documents+=L".documents.json";workspace.WriteReport(documents);
                if(multi_trace_smoke){
                    if(workspace.Count()!=2)return 14;
                    ImGuiID viewport_ids[2]{};
                    for(size_t i=0;i<2;++i){
                        auto* timing=ImGui::FindWindowByName(workspace.At(i).WindowName("Timing").c_str());
                        if(!timing||!timing->Active||timing->Hidden||timing->SkipItems||timing->DrawList->VtxBuffer.empty())return 15;
                        auto* viewport=ImGui::FindViewportByID(timing->ViewportId);
                        if(!viewport||!viewport->PlatformHandleRaw)return 16;
                        viewport_ids[i]=viewport->ID;
                    }
                    if(viewport_ids[0]==viewport_ids[1])return 17;
                    auto* header=ImGui::FindWindowByName("##CaptureDocuments");
                    if(!header||header->Size.y>ImGui::GetMainViewport()->WorkSize.y*.25f)return 18;
                }
                if(navigation_smoke&&(!drag_pan||!drag_zoom||!wheel_zoom||!continuous_drag||drag_reload_count<3||!frames_follow||!dynamic_height||!ruler_drag||!track_drag||!right_pan||!right_scroll||!right_no_menu))return 4;
                if(interaction_smoke&&(!interaction_filter||!interaction_range||!interaction_dock||!interaction_bin||!interaction_clear||!interaction_blank_clear))return 8;
                if(activity_range_smoke&&(!activity_range_filtered||!activity_plot_reused))return 9;
                if(frame_stats_range_smoke&&!frame_stats_range_filtered)return 10;
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
        if(!ui_profile.empty()){
            std::ofstream report(ui_profile);report<<"{\"measurement\":\"CPU main viewport frame preparation; Present and detached windows excluded\",";
            auto write=[&](const char* name,std::vector<double>& times){std::sort(times.begin(),times.end());report<<'"'<<name<<"\":{\"samples\":"<<times.size();if(!times.empty())report<<",\"median_ms\":"<<times[times.size()/2]<<",\"p95_ms\":"<<times[static_cast<size_t>((times.size()-1)*.95)]<<",\"maximum_ms\":"<<times.back();report<<'}';};
            write("loaded",draw_profile);report<<',';write("dragging",drag_profile);
            std::sort(command_profile.begin(),command_profile.end());
            report<<",\"main_viewport_draw_commands\":{\"samples\":"<<command_profile.size();
            if(!command_profile.empty())report<<",\"median\":"<<command_profile[command_profile.size()/2]<<",\"p95\":"<<command_profile[static_cast<size_t>((command_profile.size()-1)*.95)]<<",\"maximum\":"<<command_profile.back();
            report<<"}}";
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
