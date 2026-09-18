#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include "por2/render.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <random>

namespace {
RECT viewport(HWND window) {
    RECT r{}; GetClientRect(window, &r);
    const double scale = std::min(r.right / 1000.0, r.bottom / 600.0);
    const LONG width = static_cast<LONG>(1000 * scale), height = static_cast<LONG>(600 * scale);
    return {(r.right-width)/2, (r.bottom-height)/2, (r.right+width)/2, (r.bottom+height)/2};
}
std::optional<por2::Vec2> logicalPoint(HWND window, int x, int y) {
    const RECT r = viewport(window);
    if (x < r.left || x >= r.right || y < r.top || y >= r.bottom) return {};
    return por2::Vec2{(x-r.left)*1000.0/(r.right-r.left), (y-r.top)*600.0/(r.bottom-r.top)};
}
RECT card(int slot) { return {60+(slot%3)*300, 140+(slot/3)*64, 340+(slot%3)*300, 192+(slot/3)*64}; }
bool contains(RECT r, por2::Vec2 p) { return p.x>=r.left && p.x<r.right && p.y>=r.top && p.y<r.bottom; }
void label(HDC dc, RECT r, const std::wstring& text, int size, COLORREF color) {
    HFONT font = CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");
    auto old = SelectObject(dc,font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    DrawTextW(dc,text.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc,old); DeleteObject(font);
}
void fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush);
}
struct Application {
    explicit Application(int level) : game(level), levels(por2::Campaign.begin(),por2::Campaign.end()) {
        levels.push_back(1); levels.push_back(2); // Sandbox levels outside the campaign.
        auto found=std::find(levels.begin(),levels.end(),level);
        if(found!=levels.end()) selected=static_cast<int>(found-levels.begin());
    }
    por2::Game game;
    por2::Renderer renderer;
    std::array<bool, 256> keys{};
    std::array<bool, 2> mousePressed{};
    std::vector<por2::Shot> shots;
    bool debug = true;
    bool grid = false;
    int previewPortal = 0;
    bool previewEnabled = true;
    bool exitPressed = false;
    std::string title;
    bool smoke = false;
    int smokeTicks = 0;
    int paintCount = 0;
    std::string screenshot;
    std::string failure;
    std::vector<int> levels;
    int selected=0;
    bool menu=true, started=false, fullscreen=false;
    bool intro=true;
    por2::Game demo{0};
    std::mt19937 random{std::random_device{}()};
    int demoTicks=0, demoMove=0;
    void leaveIntro() { intro=false; menu=true; clearInput(); }
    void updateDemo() {
        por2::InputFrame input;
        if (demoTicks % 30 == 0) demoMove=static_cast<int>(random()%3)-1;
        input.movement={demoMove<0,demoMove>0,random()%45==0};
        if (demoTicks % 25 == 0)
            input.shots.push_back({static_cast<int>(random()%2),
                {static_cast<double>(random()%1000),static_cast<double>(random()%540)}});
        if (++demoTicks % 1800 == 0) demo.restart();
        demo.tick(input);
        renderer.draw(demo,false,false);
    }
    void drawIntro(HDC dc) const {
        fill(dc,{310,35,690,135},RGB(15,21,34));
        label(dc,{310,40,690,100},L"Por2D",48,RGB(235,242,255));
        label(dc,{310,100,690,130},L"PORTAL / LEVEL 0",16,RGB(150,173,201));
        fill(dc,{250,465,750,545},RGB(15,21,34));
        label(dc,{250,475,750,535},L"按任意键开始",28,RGB(255,215,100));
    }
    WINDOWPLACEMENT placement{};
    LONG_PTR savedStyle=0;
    void clearInput() { keys={}; mousePressed={}; shots.clear(); exitPressed=false; ReleaseCapture(); }
    void toggleFullscreen(HWND window) {
        clearInput();
        if (!fullscreen) {
            placement.length=sizeof(placement); GetWindowPlacement(window,&placement);
            savedStyle=GetWindowLongPtrW(window,GWL_STYLE);
            MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
            if (!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return;
            SetWindowLongPtrW(window,GWL_STYLE,savedStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,
                monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
                SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        } else {
            SetWindowLongPtrW(window,GWL_STYLE,savedStyle);
            if(smoke) placement.showCmd=SW_HIDE;
            SetWindowPlacement(window,&placement);
            SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
        fullscreen=!fullscreen; InvalidateRect(window,nullptr,FALSE);
    }
    void startSelected() {
        game=por2::Game(levels[selected]); menu=false; started=true; clearInput();
        renderer.draw(game,debug,grid);
    }
    void menuClick(HWND window, por2::Vec2 point) {
        const int first=(selected/15)*15;
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i)
            if(contains(card(i-first),point)) { selected=i; startSelected(); return; }
        if(contains({60,480,240,526},point)) selected=selected>=15?selected-15:static_cast<int>(levels.size())-1;
        else if(contains({260,480,440,526},point)) selected=(first+15)%levels.size();
        else if(contains({460,480,640,526},point) && started) { menu=false; clearInput(); }
        else if(contains({660,480,840,526},point)) toggleFullscreen(window);
        else if(contains({850,480,950,526},point)) DestroyWindow(window);
    }
    void drawMenu(HDC dc) const {
        fill(dc,{0,0,1000,600},RGB(15,21,34));
        label(dc,{60,25,940,82},L"Por2D  /  选择关卡",36,RGB(235,242,255));
        label(dc,{60,85,940,120},L"点击关卡开始  ·  方向键选择 / Enter 开始  ·  F11 全屏",18,RGB(150,173,201));
        const int first=(selected/15)*15;
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i) {
            RECT r=card(i-first); fill(dc,r,i==selected?RGB(35,110,174):RGB(31,43,61));
            const auto name=por2::makeLevel(levels[i]).name;
            const std::wstring caption=L"Level"+std::to_wstring(i)+L"  "+std::wstring(name.begin(),name.end());
            label(dc,r,caption,20,RGB(240,245,255));
        }
        label(dc,{60,480,240,526},L"上一页",20,RGB(130,194,255));
        label(dc,{260,480,440,526},L"下一页",20,RGB(130,194,255));
        label(dc,{460,480,640,526},started?L"继续游戏 (Esc)":L"请选择关卡",20,RGB(210,221,238));
        label(dc,{660,480,840,526},fullscreen?L"切换窗口":L"切换全屏",20,RGB(255,183,100));
        label(dc,{850,480,950,526},L"退出",20,RGB(210,221,238));
        label(dc,{60,540,940,580},L"第 "+std::to_wstring(selected/15+1)+L" / "+std::to_wstring((levels.size()+14)/15)+L" 页  ·  按战役顺序排列，末尾为实验地图",17,RGB(139,157,183));
    }

    void update(HWND window) {
        if (intro && !smoke) {
            updateDemo();
            InvalidateRect(window,nullptr,FALSE);
            return;
        }
        if (smoke) {
            ++smokeTicks;
            if (smokeTicks == 1) {
                updateDemo();
                if (!intro || demoTicks!=1 || demo.level().id!=0)
                    throw std::runtime_error("intro demo failed");
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
                if(intro || !menu || started) throw std::runtime_error("intro did not open level selection");
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,static_cast<LPARAM>(1LL<<30));
                if(started) throw std::runtime_error("intro key repeat started a level");
                SendMessageW(window,WM_KEYUP,VK_RETURN,0);
                SendMessageW(window, WM_KEYDOWN, VK_RETURN, 0);
                if(exitPressed) throw std::runtime_error("menu E leaked into exit action");
                SendMessageW(window, WM_KEYUP, VK_RETURN, 0);
                SendMessageW(window, WM_KEYDOWN, 'D', 0);
            }
            if (smokeTicks == 8) SendMessageW(window, WM_KEYUP, 'D', 0);
            if (smokeTicks == 2) {
                SendMessageW(window,WM_KEYDOWN,'Q',0);
                SendMessageW(window,WM_KEYDOWN,'Q',static_cast<LPARAM>(1LL<<30));
                if(previewEnabled) throw std::runtime_error("Q must disable preview once per press");
                SendMessageW(window,WM_KEYUP,'Q',0);
            }
            if (smokeTicks == 3) {
                SendMessageW(window,WM_KEYDOWN,'Q',0);
                if(!previewEnabled) throw std::runtime_error("Q must restore preview");
                SendMessageW(window,WM_KEYUP,'Q',0);
            }
            if (smokeTicks == 9) {
                SendMessageW(window, WM_LBUTTONDOWN, 0, MAKELPARAM(260, 389));
                SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(260, 389));
                SendMessageW(window, WM_RBUTTONDOWN, 0, MAKELPARAM(730, 389));
                SendMessageW(window, WM_RBUTTONUP, 0, MAKELPARAM(730, 389));
            }
            if (smokeTicks == 10 && (!game.portals()[0].active() || !game.portals()[1].active()))
                throw std::runtime_error("window smoke: mouse events did not place both portals");
            if (smokeTicks == 11) {
                if (!screenshot.empty()) renderer.saveBitmap(screenshot);
                SendMessageW(window, WM_KEYDOWN, 'R', 0);
            }
            if (smokeTicks == 12) {
                SendMessageW(window, WM_KEYUP, 'R', 0);
                if (game.portals()[0].active() || game.portals()[1].active() ||
                    game.player().body.position.x != game.level().spawn.position.x)
                    throw std::runtime_error("window smoke: restart failed");
                SendMessageW(window, WM_KEYDOWN, 'A', 0);
                SendMessageW(window, WM_KILLFOCUS, 0, 0);
                if (keys['A']) throw std::runtime_error("window smoke: focus loss left a key pressed");
            }
            if (smokeTicks == 13) {
                SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
                if(!menu) throw std::runtime_error("menu did not pause");
                RECT before{}; GetWindowRect(window,&before);
                toggleFullscreen(window);
                const RECT r=viewport(window);
                auto point=logicalPoint(window,(r.left+r.right)/2,(r.top+r.bottom)/2);
                if(!fullscreen || !point || std::abs(point->x-500)>1 || std::abs(point->y-300)>1)
                    throw std::runtime_error("fullscreen coordinate mapping failed");
                toggleFullscreen(window);
                RECT after{}; GetWindowRect(window,&after);
                if(!EqualRect(&before,&after)) throw std::runtime_error("window bounds not restored");
            }
            if(smokeTicks==14) {
                auto found=std::find(levels.begin(),levels.end(),16);
                if(found==levels.end()) throw std::runtime_error("map16 missing from menu");
                selected=static_cast<int>(found-levels.begin());
                const RECT button=card(selected%15), view=viewport(window);
                const int x=view.left+(button.left+10)*(view.right-view.left)/1000;
                const int y=view.top+(button.top+10)*(view.bottom-view.top)/600;
                SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(x,y));
                SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(x,y));
                if(game.level().id!=16) throw std::runtime_error("level selection failed");
                if(!shots.empty()) throw std::runtime_error("menu click leaked into shooting");
                SendMessageW(window,WM_KEYDOWN,'S',0);
                if(exitPressed) throw std::runtime_error("S still triggers exit");
                SendMessageW(window,WM_KEYUP,'S',0);
                SendMessageW(window,WM_KEYDOWN,'E',0);
                if(!exitPressed) throw std::runtime_error("E did not trigger exit");
                exitPressed=false;
                SendMessageW(window,WM_KEYDOWN,'E',static_cast<LPARAM>(1LL<<30));
                if(exitPressed) throw std::runtime_error("repeated Enter triggered exit");
                SendMessageW(window,WM_KEYUP,'E',0);
            }
            if (smokeTicks == 15) {
                if (!paintCount) throw std::runtime_error("window smoke: paint callback was never called");
                DestroyWindow(window);
                return;
            }
        }
        por2::InputFrame input;
        input.movement = {keys['A'], keys['D'], keys['W']};
        input.restart = keys['R'];
        input.useExit = exitPressed;
        exitPressed = false;
        //input.skip = keys[VK_LCONTROL];
        input.shots.swap(shots);
        const auto before=game.player().body.position;
        if(!menu) game.tick(input);
        if(smoke && menu && (before.x!=game.player().body.position.x || before.y!=game.player().body.position.y))
            throw std::runtime_error("menu did not freeze physics");
        std::optional<por2::Shot> preview;
        if (previewEnabled && !menu && !smoke && GetForegroundWindow()==window) {
            POINT cursor{};
            if (GetCursorPos(&cursor) && ScreenToClient(window,&cursor)) {
                const auto point=logicalPoint(window,cursor.x,cursor.y);
                if (point) preview=por2::Shot{previewPortal,*point};
            }
        }
        if(previewEnabled && smoke && smokeTicks==10) preview=por2::Shot{0,{260,389}};
        renderer.draw(game, debug, grid, preview);
        const std::string nextTitle = game.finished()
            ? "Por2D - Completed! R: play again | Esc: menu | F11: fullscreen"
            : "Por2D - " + game.level().name + " | Esc: levels  F11: fullscreen | A/D: move  W: jump  E: exit  R: restart";
        if (nextTitle != title) { title = nextTitle; SetWindowTextA(window, title.c_str()); }
        InvalidateRect(window, nullptr, FALSE);
        if (smoke) SendMessageW(window, WM_PAINT, 0, 0);
    }
};

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_TIMER:
        try { app->update(window); }
        catch (const std::exception& error) {
            app->failure = error.what();
            DestroyWindow(window);
        }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP: {
        const bool pressed = message != WM_KEYUP;
        unsigned key = static_cast<unsigned>(wparam);
        if (app->intro && pressed && !(lparam & (1LL<<30))) {
            app->leaveIntro();
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        if (key == VK_CONTROL) key = (lparam & (1LL << 24)) ? VK_RCONTROL : VK_LCONTROL;
        if (key < app->keys.size()) app->keys[key] = pressed;
        if (pressed && !(lparam & (1LL << 30))) {
            if (key == VK_F11 || (key==VK_RETURN && (lparam & (1LL<<29)))) { app->toggleFullscreen(window); return 0; }
            if (key == VK_ESCAPE) { if(!app->menu || app->started) app->menu=!app->menu; app->clearInput(); }
            if(key=='E' && !app->menu) app->exitPressed=true;
            if(app->menu) {
                int delta=0;
                if(key==VK_LEFT) delta=-1;
                if(key==VK_RIGHT) delta=1;
                if(key==VK_UP) delta=-3;
                if(key==VK_DOWN) delta=3;
                if(key==VK_PRIOR) delta=-15;
                if(key==VK_NEXT) delta=15;
                app->selected=std::clamp(app->selected+delta,0,static_cast<int>(app->levels.size())-1);
                if(key==VK_RETURN) app->startSelected();
            }
            if (key == VK_F1) app->debug = !app->debug;
            if (key == VK_F2) app->grid = !app->grid;
            if (key == 'Q') app->previewEnabled = !app->previewEnabled;
        }
        if(message==WM_SYSKEYDOWN) return DefWindowProcW(window,message,wparam,lparam);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if(app->intro) {
            app->leaveIntro();
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        app->previewPortal = message == WM_LBUTTONDOWN ? 0 : 1;
        app->mousePressed[message == WM_LBUTTONDOWN ? 0 : 1] = true;
        SetCapture(window);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        const int id = message == WM_LBUTTONUP ? 0 : 1;
        const auto point=logicalPoint(window,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        if(app->mousePressed[id] && point) {
            if(app->menu) { if(id==0) app->menuClick(window,*point); }
            else app->shots.push_back({id,*point});
        }
        app->mousePressed[id] = false;
        if (!app->mousePressed[0] && !app->mousePressed[1]) ReleaseCapture();
        return 0;
    }
    case WM_CAPTURECHANGED:
        app->mousePressed = {};
        return 0;
    case WM_KILLFOCUS:
        app->clearInput();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        ++app->paintCount;
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(window, &paint);
        HDC dc=CreateCompatibleDC(target);
        HBITMAP bitmap=CreateCompatibleBitmap(target,1000,600);
        auto oldBitmap=SelectObject(dc,bitmap);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = por2::WindowWidth;
        info.bmiHeader.biHeight = -por2::WindowHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, 0, 0, por2::WindowWidth, por2::WindowHeight, 0, 0, 0,
                         por2::WindowHeight, app->renderer.pixels(), &info, DIB_RGB_COLORS);
        if(app->intro) app->drawIntro(dc);
        else if(app->menu) app->drawMenu(dc);
        else if (app->game.finished()) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            RECT area{0, 30, por2::WindowWidth, 70};
            DrawTextW(dc, L"Completed!  R: play again    Esc: levels", -1, &area, DT_CENTER | DT_SINGLELINE);
        }
        if(!app->menu && !app->intro) {
            const auto entry=std::find(app->levels.begin(),app->levels.end(),app->game.level().id);
            const std::string caption = "Level" + std::to_string(entry-app->levels.begin()) + "  " + app->game.level().name;
            const std::wstring wide(caption.begin(), caption.end());
            RECT levelArea{690, 548, 975, 588};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(220, 230, 245));
            DrawTextW(dc, wide.c_str(), -1, &levelArea, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
        }
        RECT client{}; GetClientRect(window,&client);
        const RECT view=viewport(window);
        // Compose the scaled scene AND letterboxing offscreen. Clearing the visible
        // window before StretchBlt exposed a black frame on every timer tick.
        if(client.right>0 && client.bottom>0) {
            HDC frame=CreateCompatibleDC(target);
            HBITMAP frameBitmap=CreateCompatibleBitmap(target,client.right,client.bottom);
            auto oldFrame=SelectObject(frame,frameBitmap);
            fill(frame,client,RGB(0,0,0));
            SetStretchBltMode(frame,COLORONCOLOR);
            StretchBlt(frame,view.left,view.top,view.right-view.left,view.bottom-view.top,dc,0,0,1000,600,SRCCOPY);
            BitBlt(target,0,0,client.right,client.bottom,frame,0,0,SRCCOPY);
            SelectObject(frame,oldFrame); DeleteObject(frameBitmap); DeleteDC(frame);
        }
        SelectObject(dc,oldBitmap);
        if(app->smoke && (app->smokeTicks==13 || app->intro) && !app->screenshot.empty()) {
            std::vector<std::uint32_t> pixels(1000*600);
            if(!GetDIBits(dc,bitmap,0,600,pixels.data(),&info,DIB_RGB_COLORS))
                app->failure="menu screenshot failed";
            else {
                BITMAPFILEHEADER header{}; header.bfType=0x4d42;
                header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
                header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size()*4);
                std::ofstream file(app->screenshot+(app->intro?".intro.bmp":".menu.bmp"),std::ios::binary);
                file.write(reinterpret_cast<const char*>(&header),sizeof(header));
                file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
                file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
                if(!file) app->failure="menu screenshot write failed";
            }
        }
        DeleteObject(bitmap); DeleteDC(dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int runWindow(int level, bool smoke, const std::string& screenshot, bool direct, bool fullscreen) {
    SetProcessDPIAware();
    Application app(level);
    app.smoke = smoke;
    app.menu=!direct || smoke; app.started=direct && !smoke;
    app.intro=!direct || smoke;
    app.screenshot = screenshot;
    app.renderer.draw(app.intro ? app.demo : app.game);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.lpfnWndProc = windowProcedure;
    type.hInstance = instance;
    type.lpszClassName = L"Por2ReconstructedWindow";
    type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    if (!RegisterClassW(&type)) throw std::runtime_error("window registration failed");
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT bounds{0, 0, por2::WindowWidth, por2::WindowHeight};
    AdjustWindowRect(&bounds, style, FALSE);
    HWND window = CreateWindowExW(0, type.lpszClassName, L"Por2D", style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, &app);
    if (!window) throw std::runtime_error("window creation failed");
    if(fullscreen) app.toggleFullscreen(window);
    ShowWindow(window, smoke ? SW_HIDE : SW_SHOW);
    if (!SetTimer(window, 1, por2::TickMilliseconds, nullptr)) {
        DestroyWindow(window);
        throw std::runtime_error("game timer creation failed");
    }
    MSG message{};
    BOOL status = 0;
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (status < 0) throw std::runtime_error("window message loop failed");
    if (!app.failure.empty()) throw std::runtime_error(app.failure);
    if (smoke) std::cout << "PASS native window: timer, keyboard, mouse, restart, focus, paint, menu, map16 and fullscreen\n";
    return static_cast<int>(message.wParam);
}
} // namespace

int main(int argc, char** argv) {
    try {
        int level = 0;
        int frames = 60;
        bool headless = false;
        bool windowSmoke = false;
        bool direct=false, fullscreen=false;
        std::string screenshot;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--headless") headless = true;
            else if (argument == "--window-smoke-test") windowSmoke = true;
            else if (argument == "--fullscreen") fullscreen=true;
            else if (argument == "--level" && i + 1 < argc) { level = std::stoi(argv[++i]); direct=true; }
            else if (argument == "--frames" && i + 1 < argc) frames = std::stoi(argv[++i]);
            else if (argument == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
            else if (argument == "--help") {
                std::cout << "Por2D.exe [--fullscreen] [--level ID] [--headless --frames N --screenshot path.bmp]\n"
                             "Por2D.exe --window-smoke-test [--screenshot path.bmp]\n";
                return 0;
            } else throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
        if (frames < 0 || frames > 1000000) throw std::invalid_argument("frames must be 0..1000000");
        if (headless && windowSmoke) throw std::invalid_argument("choose headless or window smoke, not both");
        if (windowSmoke && level != 0) throw std::invalid_argument("window smoke requires level 0");
        if (!headless) return runWindow(level, windowSmoke, screenshot,direct,fullscreen);
        por2::Game game(level);
        for (int i = 0; i < frames; ++i) game.tick({});
        por2::Renderer renderer;
        renderer.draw(game);
        if (!screenshot.empty()) renderer.saveBitmap(screenshot);
        const auto& player = game.player();
        std::cout << "level=" << level << " frames=" << frames << " position="
                  << player.body.position.x << ',' << player.body.position.y
                  << " velocity=" << player.velocity.x << ',' << player.velocity.y << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
