#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include "por2/replay.hpp"
#include "por2/render.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <random>
#include <cwctype>

namespace {
std::wstring utf8(const std::string& text) {
    if (text.empty()) return {};
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if (!count) throw std::runtime_error("invalid UTF-8 text");
    std::wstring result(count,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),count);
    return result;
}
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
RECT menuRow(int row) { return {310,170+row*72,690,228+row*72}; }
RECT menuButton(int item){if(item<3)return menuRow(item);if(item==3)return menuRow(4);return item==4?RECT{310,386,495,444}:RECT{505,386,690,444};}
RECT bindingRow(int row) { return {190,100+row*40,810,136+row*40}; }
constexpr RECT SpeedButton{190,430,810,474};
constexpr RECT ReplayUiButton{800,12,985,48};
const std::array<unsigned,8> DefaultBindings{'A','D','W','E','R','Q',VK_LBUTTON,VK_RBUTTON};
const std::array<std::wstring,8> ActionNames{L"向左移动",L"向右移动",L"跳跃",L"进入出口",L"重新开始",L"切换落点预览",L"蓝色传送门",L"橙色传送门"};
bool bindable(unsigned key, int action) {
    if(key==VK_LBUTTON || key==VK_RBUTTON) return action>=6;
    return (key>='A'&&key<='Z') || (key>='0'&&key<='9') || key==VK_SPACE ||
        (key>=VK_LEFT&&key<=VK_DOWN) || (key>=VK_NUMPAD0&&key<=VK_DIVIDE) ||
        (key>=VK_OEM_1&&key<=VK_OEM_3) || (key>=VK_OEM_4&&key<=VK_OEM_8);
}
std::wstring keyName(unsigned key) {
    if(key==VK_LBUTTON)return L"鼠标左键";
    if(key==VK_RBUTTON)return L"鼠标右键";
    wchar_t text[64]{};
    LONG scan=static_cast<LONG>(MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16);
    if(key>=VK_LEFT&&key<=VK_DOWN)scan|=1<<24;
    if(GetKeyNameTextW(scan,text,64))return text;
    return L"Key "+std::to_wstring(key);
}
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
    por2::Replay replay;
    std::vector<por2::Level> customLevels;
    std::wstring mapWarnings;
    por2::Level levelById(int id) const {
        for(const auto& level:customLevels)if(level.id==id)return level;
        return por2::makeLevel(id);
    }
    por2::Game newGame(int id) const {const auto level=levelById(id);return level.editorJson.empty()?por2::Game(id):por2::Game(level);}
    void loadMaps(){
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        const auto folder=std::filesystem::path(executable).parent_path();
        // Development builds share the project's levels folder; packaged builds use one beside the executable.
        const auto directory=folder.filename()==L"build"?folder.parent_path()/L"levels":folder/L"levels";
        try{
            std::filesystem::create_directories(directory);
            std::vector<std::filesystem::path> paths;
            for(const auto& entry:std::filesystem::directory_iterator(directory)){
                auto extension=entry.path().extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
                if(entry.is_regular_file()&&extension==L".json")paths.push_back(entry.path());
            }
            std::sort(paths.begin(),paths.end());
            for(const auto& path:paths){try{const int id=1000+static_cast<int>(customLevels.size());customLevels.push_back(por2::loadEditorLevel(path,id));levels.push_back(id);}
                catch(const std::exception& error){mapWarnings+=path.filename().wstring()+L": "+utf8(error.what())+L"\n";}}
        }catch(const std::exception& error){mapWarnings+=utf8(error.what());}
    }
    bool replaySingle=false, importing=false;
    bool replayUiHidden=false;
    int speedTenths=10, gameClock=0;
    bool logicDue(int& clock) const {clock+=speedTenths;if(clock<10)return false;clock-=10;return true;}
    std::wstring speedText() const {
        return speedTenths==10?L"1.0×":L"0."+std::to_wstring(speedTenths)+L"×";
    }
    por2::Recorder recorder;
    std::wstring recordingMessage;
    std::filesystem::path lastRecordingPath;
    bool stopRecording() {
        if(recorder.active&&!recorder.pending)recordingMessage=L"录制已停止：尚未推进逻辑帧，未生成文件。";
        recorder.active=false;
        if(!recorder.pending)return true;
        if(smoke){recorder.pending=false;return true;}
        try {
            const auto directory=bindingsPath.parent_path()/L"recordings";
            std::filesystem::create_directories(directory);
            const auto path=directory/(L"level-"+std::to_wstring(recorder.script.level)+L"-"+std::to_wstring(GetTickCount64())+L".txt");
            std::ofstream output(path);output<<recorder.text();output.close();
            if(!output)throw std::runtime_error("Cannot write recording");
            lastRecordingPath=path;
            recorder.pending=false;recordingMessage=L"录制已保存：recordings/"+path.filename().wstring();
            return true;
        }catch(const std::exception&){
            recordingMessage=L"录制保存失败，数据仍保留；按 F10 重试。";
            MessageBoxW(nullptr,recordingMessage.c_str(),L"录制保存失败",MB_OK|MB_ICONERROR);return false;
        }
    }
    void toggleRecording() {
        if(recorder.active||recorder.pending){stopRecording();return;}
        if(replay.active){recordingMessage=L"请先按 F7 退出回放，再开始录制。";return;}
        if(!started){recordingMessage=L"请先选择关卡，再开始录制。";return;}
        game=game.level().editorJson.empty()?por2::Game(game.level().id):por2::Game(game.level());recorder.start(game.level());
        intro=false;menu=false;endLevelIntro();gameClock=0;
        recordingMessage=L"录制中 · F10 停止并保存";
        renderer.draw(game,debug,grid);
    }
    bool hideReplayUi() const { return replay.active&&replayUiHidden; }
    void toggleReplayUi(HWND window) {
        replayUiHidden=!replayUiHidden;
        renderer.draw(game,debug,grid&&!hideReplayUi());
        InvalidateRect(window,nullptr,FALSE);
    }
    int replayClock=0, aimTicks=0;
    std::optional<por2::Shot> replayAim;
    void startReplay(por2::ReplayScript script) {
        if(!stopRecording())throw std::runtime_error("Save the pending recording before importing a replay");
        if(!script.customLevel&&script.level<0&&menu)game=newGame(levels[selected]);
        replay.start(std::move(script),menu?levels[selected]:game.level().id,game);
        intro=false; menu=false; started=true; endLevelIntro();
        replayClock=aimTicks=0; replaySingle=false; replayAim.reset();
        renderer.draw(game,debug,grid&&!hideReplayUi());
    }
    void importReplay(HWND window) {
        const bool wasPaused=replay.paused;
        replay.paused=true; importing=true; clearInput();
        wchar_t path[32768]{};
        OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog);
        dialog.hwndOwner=window; dialog.lpstrFile=path; dialog.nMaxFile=32768;
        dialog.lpstrFilter=L"操作脚本 (*.txt)\0*.txt\0所有文件\0*.*\0";
        dialog.lpstrTitle=L"导入回放（无 level 指令时使用当前选中关卡）";
        dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog)) {
            try { startReplay(por2::ReplayScript::load(std::filesystem::path(path))); }
            catch(const std::exception& error) {
                MessageBoxW(window,utf8(error.what()).c_str(),L"脚本导入失败",MB_OK|MB_ICONERROR);
                replay.paused=wasPaused;
            }
        } else replay.paused=wasPaused;
        importing=false; clearInput(); InvalidateRect(window,nullptr,FALSE);
    }
    void drawReplay(HDC dc) const {
        if (!replay.active) return;
        fill(dc,ReplayUiButton,buttonColor(ReplayUiButton));
        label(dc,ReplayUiButton,replayUiHidden?L"显示 UI (F9)":L"隐藏 UI (F9)",18,RGB(240,245,255));
        if(replayUiHidden)return;
        if (replayAim && aimTicks>0) {
            const int x=static_cast<int>(std::clamp(replayAim->target.x,0.0,999.0));
            const int y=static_cast<int>(std::clamp(replayAim->target.y,0.0,599.0));
            HPEN pen=CreatePen(PS_SOLID,2,replayAim->portal==0?RGB(70,170,255):RGB(255,160,60));
            auto old=SelectObject(dc,pen);
            MoveToEx(dc,x-10,y,nullptr); LineTo(dc,x+11,y);
            MoveToEx(dc,x,y-10,nullptr); LineTo(dc,x,y+11);
            SelectObject(dc,old); DeleteObject(pen);
        }
        fill(dc,{10,465,990,544},RGB(15,21,34));
        const std::wstring state=replay.completed?(replay.cleared?L"过关成功":L"脚本结束（未过关）"):(replay.paused?L"已暂停":L"播放中");
        const std::wstring command=replay.lastAction<0?L"准备开始":utf8(replay.script.actions[replay.lastAction].text);
        const std::wstring rate=speedText();
        label(dc,{15,467,985,493},state+L"  "+rate+L"  帧 "+
            std::to_wstring(replay.frame)+L"/"+std::to_wstring(replay.script.totalFrames)+L"  当前: "+command,18,RGB(240,245,255));
        label(dc,{15,494,985,520},L"空格 暂停/继续 · . 单帧 · R 重播 · F7 退出 · F6 导入 · F9 显示 UI",17,RGB(150,195,230));
        fill(dc,{20,532,980,538},RGB(45,58,76));
        fill(dc,{20,532,20+960*replay.frame/replay.script.totalFrames,538},RGB(80,185,235));
    }
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
    enum class Page { Main, Levels, Settings };
    Page page=Page::Main;
    int menuItem=0, settingItem=0, rebinding=-1;
    std::optional<por2::Vec2> menuPointer;
    bool mouseNavigation=false;
    COLORREF buttonColor(RECT rect, bool keyboardSelected=false) const {
        const bool active=mouseNavigation?(menuPointer&&contains(rect,*menuPointer)):keyboardSelected;
        return active?RGB(35,110,174):RGB(31,43,61);
    }
    COLORREF levelTextColor(int index) const {
        const bool current=game.level().editorJson.empty()?levels[index]==game.level().id:levelById(levels[index]).editorJson==game.level().editorJson;
        return started&&current?RGB(139,157,183):RGB(240,245,255);
    }
    std::array<unsigned,8> bindings=DefaultBindings;
    std::wstring settingsMessage=L"选择操作后按新键；Esc 取消改键。重复键位会提示冲突。";
    std::filesystem::path bindingsPath;
    void loadBindings(const std::filesystem::path& path={}) {
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr,executable,32768);
        bindingsPath=path.empty()?std::filesystem::path(executable).parent_path()/L"keybindings.ini":path;
        std::ifstream file(bindingsPath);
        auto candidate=DefaultBindings;
        for(int i=0;i<8;++i) {
            if(!(file>>candidate[i]) || !bindable(candidate[i],i))return;
            for(int j=0;j<i;++j)if(candidate[i]==candidate[j])return;
        }
        bindings=candidate;
        std::string speedTag;int savedSpeed=10;
        if(file>>speedTag>>savedSpeed && speedTag=="speedTenths" && savedSpeed>=1 && savedSpeed<=10)speedTenths=savedSpeed;
    }
    void saveBindings() {
        if(smoke)return;
        std::ofstream file(bindingsPath);
        for(auto key:bindings)file<<key<<'\n';
        file<<"speedTenths "<<speedTenths<<'\n';
        file.close();
        if(!file)settingsMessage=L"键位已生效，但无法写入 keybindings.ini；请检查游戏目录写入权限。";
    }
    void assignBinding(unsigned key) {
        if(rebinding<0)return;
        if(!bindable(key,rebinding)){settingsMessage=L"该按键保留给菜单或系统，请选择字母、数字、方向键或空格等按键。";return;}
        for(int i=0;i<8;++i)if(i!=rebinding&&bindings[i]==key){settingsMessage=L"该按键已用于「"+ActionNames[i]+L"」，请先更改该操作。";return;}
        bindings[rebinding]=key;rebinding=-1;clearInput();
        settingsMessage=L"键位已保存。";saveBindings();
    }
    void openMenu(Page next=Page::Main) {
        menu=true;page=next;rebinding=-1;clearInput();
        if(next==Page::Levels && started){
            const auto current=std::find_if(levels.begin(),levels.end(),[&](int id){return game.level().editorJson.empty()?id==game.level().id:levelById(id).editorJson==game.level().editorJson;});
            if(current!=levels.end())selected=static_cast<int>(current-levels.begin());
        }
    }
    void resume() { if(started){menu=false;clearInput();} }
    void back() {
        if(rebinding>=0){rebinding=-1;settingsMessage=L"已取消改键。";}
        else if(page!=Page::Main)openMenu();
        else resume();
    }
    void activateMenu(HWND window) {
        if(menuItem==0){if(started)resume();else openMenu(Page::Levels);}
        if(menuItem==1)openMenu(Page::Settings);
        if(menuItem==2)openMenu(Page::Levels);
        if(menuItem==3&&stopRecording())DestroyWindow(window);
        if(menuItem==4)toggleRecording();
        if(menuItem==5)importReplay(window);
    }
    void activateSetting() {
        if(settingItem<8){rebinding=settingItem;settingsMessage=L"请按新键（传送门也可使用鼠标左右键）；Esc 取消。";}
        else if(settingItem==8){bindings=DefaultBindings;settingsMessage=L"已恢复默认键位。";saveBindings();clearInput();}
        else if(settingItem==10){
            speedTenths=speedTenths%10+1;gameClock=replayClock=0;
            settingsMessage=L"运行速度："+speedText()+L"；每个逻辑帧保持不变。";saveBindings();
        }
        else back();
    }
    bool intro=true;
    static constexpr int LevelIntroDuration=150;
    int levelIntroTicks=-1;
    unsigned suppressedKey=256;
    bool levelIntroActive() const { return levelIntroTicks>=0; }
    void beginLevelIntro() { levelIntroTicks=0; clearInput(); }
    void endLevelIntro() { levelIntroTicks=-1; clearInput(); }
    void stepLevelIntro() {
        if (levelIntroActive() && ++levelIntroTicks>=LevelIntroDuration) endLevelIntro();
    }
    void drawLevelIntro(HDC dc) const {
        fill(dc,{0,0,1000,600},RGB(15,21,34));
        const double alpha=std::clamp(std::min(levelIntroTicks/22.0,
            (LevelIntroDuration-levelIntroTicks)/22.0),0.0,1.0);
        const auto tint=[&](int r,int g,int b) {
            return RGB(15+static_cast<int>((r-15)*alpha),
                       21+static_cast<int>((g-21)*alpha),34+static_cast<int>((b-34)*alpha));
        };
        const auto entry=std::find(levels.begin(),levels.end(),game.level().id);
        const int offset=static_cast<int>((1-alpha)*12);
        label(dc,{80,155+offset,920,205+offset},game.level().editorJson.empty()?L"level "+std::to_wstring(entry-levels.begin()):L"自定义关卡",24,tint(130,194,255));
        label(dc,{60,215+offset,940,295+offset},utf8(game.level().name),52,tint(240,245,255));
        fill(dc,{450,314,550,316},tint(255,215,100));
        label(dc,{40,335+offset,960,390+offset},utf8(game.level().commentary),24,tint(190,205,225));
        label(dc,{60,505,940,550},L"按任意键跳过",17,tint(139,157,183));
    }
    por2::Game demo{0};
    std::mt19937 random{std::random_device{}()};
    int demoTicks=0, demoMove=0;
    void leaveIntro() { intro=false; openMenu(); }
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
    void keyboardShot(HWND window, int portal) {
        POINT cursor{};
        if(GetCursorPos(&cursor)&&ScreenToClient(window,&cursor)){
            const auto point=logicalPoint(window,cursor.x,cursor.y);
            if(point){previewPortal=portal;shots.push_back({portal,*point});}
        }
    }
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
        if(!stopRecording())return;
        replay.active=false;
        game=newGame(levels[selected]); menu=false; started=true; clearInput();
        beginLevelIntro();
        renderer.draw(game,debug,grid);
    }
    void menuClick(HWND window, por2::Vec2 point) {
        if(page==Page::Main){for(int i=0;i<6;++i)if(contains(menuButton(i),point)){menuItem=i;activateMenu(window);return;}return;}
        if(page==Page::Settings){
            for(int i=0;i<8;++i)if(contains(bindingRow(i),point)){settingItem=i;activateSetting();return;}
            if(contains(SpeedButton,point)){settingItem=10;activateSetting();return;}
            if(contains({190,492,490,532},point)){settingItem=8;activateSetting();}
            if(contains({510,492,810,532},point))back();
            return;
        }
        const int first=(selected/15)*15;
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i)
            if(contains(card(i-first),point)) { selected=i; startSelected(); return; }
        if(contains({60,480,240,526},point)) selected=selected>=15?selected-15:static_cast<int>(levels.size())-1;
        else if(contains({260,480,440,526},point)) selected=(first+15)%levels.size();
        else if(contains({460,480,640,526},point)) openMenu();
        else if(contains({660,480,840,526},point)) toggleFullscreen(window);
    }
    void drawMenu(HDC dc) const {
        fill(dc,{0,0,1000,600},RGB(15,21,34));
        if(page==Page::Main){
            label(dc,{60,35,940,100},started?L"游戏已暂停":L"Por2D / 主菜单",38,RGB(235,242,255));
            if(started)label(dc,{60,105,940,145},utf8(game.level().name),22,RGB(150,173,201));
            const std::array<std::wstring,6> items{started?L"继续游戏":L"开始游戏",L"设置",L"关卡选择",L"退出游戏",recorder.active||recorder.pending?L"停止录制 (F10)":L"录制 (F10)",L"回放 (F6)"};
            for(int i=0;i<6;++i){fill(dc,menuButton(i),buttonColor(menuButton(i),i==menuItem));label(dc,menuButton(i),items[i],i>=4?21:25,RGB(240,245,255));}
            label(dc,{20,540,980,580},recordingMessage.empty()?L"录制从本关起点开始 · 回放可导入操作脚本":recordingMessage,17,RGB(150,173,201));return;
        }
        if(page==Page::Settings){
            label(dc,{60,20,940,75},L"设置 / 键位与速度",34,RGB(235,242,255));
            for(int i=0;i<8;++i){fill(dc,bindingRow(i),buttonColor(bindingRow(i),i==settingItem));label(dc,bindingRow(i),ActionNames[i]+L"    "+(rebinding==i?L"[ 等待输入… ]":keyName(bindings[i])),21,RGB(240,245,255));}
            fill(dc,{190,492,490,532},buttonColor({190,492,490,532},settingItem==8));
            fill(dc,SpeedButton,buttonColor(SpeedButton,settingItem==10));
            label(dc,SpeedButton,L"运行速度："+speedText()+L"  （点击切换）",21,RGB(240,245,255));
            fill(dc,{510,492,810,532},buttonColor({510,492,810,532},settingItem==9));
            label(dc,{190,492,490,532},L"恢复默认",21,RGB(240,245,255));label(dc,{510,492,810,532},L"返回菜单 (Esc)",21,RGB(240,245,255));
            label(dc,{20,540,980,580},settingsMessage,17,RGB(255,215,100));return;
        }
        label(dc,{60,25,940,82},L"Por2D  /  选择关卡",36,RGB(235,242,255));
        label(dc,{60,85,940,120},L"点击关卡开始  ·  方向键 / Enter 开始  ·  F6 导入脚本  ·  F11 全屏",18,RGB(150,173,201));
        const int first=(selected/15)*15;
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i) {
            RECT r=card(i-first); fill(dc,r,buttonColor(r,i==selected));
            const auto level=levelById(levels[i]);
            const std::wstring caption=(level.editorJson.empty()?L"Level "+std::to_wstring(i)+L"  ":L"自定义  ")+utf8(level.name);
            label(dc,r,caption,20,levelTextColor(i));
        }
        for(const RECT r : {RECT{60,480,240,526},RECT{260,480,440,526},RECT{460,480,640,526},RECT{660,480,840,526}})
            fill(dc,r,buttonColor(r));
        label(dc,{60,480,240,526},L"上一页",20,RGB(130,194,255));
        label(dc,{260,480,440,526},L"下一页",20,RGB(130,194,255));
        label(dc,{460,480,640,526},L"返回菜单 (Esc)",20,RGB(210,221,238));
        label(dc,{660,480,840,526},fullscreen?L"切换窗口":L"切换全屏",20,RGB(255,183,100));
        label(dc,{60,540,940,580},L"第 "+std::to_wstring(selected/15+1)+L" / "+std::to_wstring((levels.size()+14)/15)+L" 页  ·  战役 / 实验地图 / levels 自定义地图",17,RGB(139,157,183));
    }

    void update(HWND window) {
        if (importing) return;
        if (intro && !smoke) {
            updateDemo();
            InvalidateRect(window,nullptr,FALSE);
            return;
        }
        if (smoke) {
            ++smokeTicks;
            const auto hover=[&](int x,int y){
                const RECT view=viewport(window);
                SendMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(view.left+x*(view.right-view.left)/1000,view.top+y*(view.bottom-view.top)/600));
            };
            if (smokeTicks == 1) {
                updateDemo();
                if (!intro || demoTicks!=1 || demo.level().id!=0)
                    throw std::runtime_error("intro demo failed");
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
                if(intro || !menu || started || page!=Page::Main) throw std::runtime_error("intro did not open main menu");
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,static_cast<LPARAM>(1LL<<30));
                if(started) throw std::runtime_error("intro key repeat started a level");
                SendMessageW(window,WM_KEYUP,VK_RETURN,0);
                SendMessageW(window, WM_KEYDOWN, VK_RETURN, 0);
                if(page!=Page::Levels || started)throw std::runtime_error("main menu did not open level selection");
                SendMessageW(window, WM_KEYUP, VK_RETURN, 0);
                SendMessageW(window, WM_KEYDOWN, VK_RETURN, 0);
                if(exitPressed) throw std::runtime_error("menu E leaked into exit action");
                SendMessageW(window, WM_KEYUP, VK_RETURN, 0);
                if (!levelIntroActive() || utf8(game.level().name)!=L"概念")
                    throw std::runtime_error("Chinese level intro did not start");
                const auto introPosition=game.player().body.position;
                smoke=false;
                for (int i=0;i<LevelIntroDuration-1;++i) update(window);
                smoke=true;
                if (!levelIntroActive() || game.player().body.position!=introPosition)
                    throw std::runtime_error("level intro did not freeze physics");
                stepLevelIntro();
                if (levelIntroActive()) throw std::runtime_error("level intro did not finish automatically");
                beginLevelIntro();
                levelIntroTicks=40;
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window, WM_KEYDOWN, 'D', 0);
                SendMessageW(window, WM_KEYDOWN, 'D', static_cast<LPARAM>(1LL<<30));
                if (levelIntroActive() || keys['D'])
                    throw std::runtime_error("intro skip leaked into movement");
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
                SendMessageW(window,WM_KEYUP,VK_ESCAPE,0);
                if(!menu || page!=Page::Main) throw std::runtime_error("menu did not pause");
                const auto frozen=game.player().body.position;
                const auto portals=game.portals();
                menuClick(window,{500,260});
                if(page!=Page::Settings)throw std::runtime_error("settings navigation failed");
                hover(500,170);
                if(buttonColor(bindingRow(1))!=RGB(35,110,174)||buttonColor(bindingRow(0),true)==RGB(35,110,174))
                    throw std::runtime_error("settings hover did not replace keyboard highlight");
                hover(300,510);
                if(buttonColor({190,492,490,532})!=RGB(35,110,174))throw std::runtime_error("reset button hover failed");
                SendMessageW(window,WM_MOUSELEAVE,0,0);
                if(buttonColor({190,492,490,532})==RGB(35,110,174))throw std::runtime_error("mouse leave retained hover");
                hover(500,170);
                smoke=false;update(window);smoke=true;
                if(game.player().body.position!=frozen || game.portals()[0].active()!=portals[0].active())throw std::runtime_error("settings did not pause game");
                SendMessageW(window,WM_PAINT,0,0);
                settingItem=0;activateSetting();
                SendMessageW(window,WM_KEYDOWN,'D',0);SendMessageW(window,WM_KEYUP,'D',0);
                if(rebinding!=0 || bindings[0]!='A')throw std::runtime_error("duplicate binding accepted");
                SendMessageW(window,WM_KEYDOWN,'J',0);SendMessageW(window,WM_KEYUP,'J',0);
                if(rebinding!=-1 || bindings[0]!='J' || keys['J'])throw std::runtime_error("rebind failed or leaked input");
                menuClick(window,{500,450});
                if(speedTenths!=1)throw std::runtime_error("speed setting click failed");
                bindingsPath=std::filesystem::temp_directory_path()/("por2-bindings-test-"+std::to_string(GetCurrentProcessId())+".ini");
                smoke=false;saveBindings();smoke=true;
                bindings=DefaultBindings;speedTenths=10;loadBindings(bindingsPath);
                std::filesystem::remove(bindingsPath);
                if(bindings[0]!='J'||speedTenths!=1)throw std::runtime_error("bindings or speed did not persist");
                speedTenths=10;
                settingItem=2;activateSetting();
                SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(window,WM_KEYUP,VK_ESCAPE,0);
                if(rebinding!=-1 || bindings[2]!='W' || page!=Page::Settings)throw std::runtime_error("cancel binding failed");
                SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(window,WM_KEYUP,VK_ESCAPE,0);
                if(page!=Page::Main || !menu)throw std::runtime_error("settings back resumed game");
                SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(window,WM_KEYUP,VK_ESCAPE,0);
                if(menu || game.player().body.position!=frozen)throw std::runtime_error("resume changed game");
                SendMessageW(window,WM_KEYDOWN,'J',0);
                smoke=false;update(window);smoke=true;
                SendMessageW(window,WM_KEYUP,'J',0);
                if(game.player().body.position.x>=frozen.x)throw std::runtime_error("new movement binding not applied");
                for(int speed=1;speed<=10;++speed){
                    speedTenths=speed;gameClock=0;
                    por2::Game reference=game;
                    por2::InputFrame expected;expected.movement.left=true;
                    expected.shots.push_back({0,{260,389}});expected.useExit=true;
                    keys['J']=true;shots=expected.shots;exitPressed=true;
                    smoke=false;
                    int frames=0;
                    for(int i=1;i<=10;++i){
                        update(window);
                        if(i*speed/10>frames){reference.tick(expected);expected.shots.clear();expected.useExit=false;++frames;}
                        if(game.player().body.position!=reference.player().body.position||game.player().velocity!=reference.player().velocity)
                            throw std::runtime_error("decimal speed changed logical frame result");
                        if(frames==0&&(shots.size()!=1||!exitPressed))throw std::runtime_error("slow mode dropped queued input");
                    }
                    smoke=true;
                    if(frames!=speed||gameClock!=0||!shots.empty()||exitPressed)throw std::runtime_error("decimal speed timing drifted");
                }
                speedTenths=10;gameClock=0;
                openMenu(Page::Settings);settingItem=8;activateSetting();
                if(bindings!=DefaultBindings)throw std::runtime_error("restore bindings failed");
                openMenu();
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
                menuClick(window,{500,340});
                if(page!=Page::Levels)throw std::runtime_error("pause menu did not open level selection");
                if(levels[selected]!=game.level().id || levelTextColor(selected)!=RGB(139,157,183))
                    throw std::runtime_error("current level text is not gray");
                const int currentSelection=selected;
                hover(400,160);
                if(buttonColor(card(1))!=RGB(35,110,174)||selected!=currentSelection)
                    throw std::runtime_error("level hover changed selection or failed to highlight");
                SendMessageW(window,WM_PAINT,0,0);
                hover(500,570);
                if(buttonColor(card(1))==RGB(35,110,174))throw std::runtime_error("empty area retained hover");
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
                smoke=false;
                std::istringstream sample("level 0\nd 2\ns 0 260 389\nz 1\n");
                startReplay(por2::ReplayScript::parse(sample));
                SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
                update(window);
                if(replay.frame!=0) throw std::runtime_error("paused replay advanced");
                speedTenths=1;
                SendMessageW(window,WM_KEYDOWN,VK_OEM_PERIOD,0);
                update(window);
                if(replay.frame!=1 || !replay.paused) throw std::runtime_error("replay single frame failed");
                speedTenths=5;
                SendMessageW(window,WM_KEYDOWN,'R',0);
                update(window);
                if(replay.frame!=0)throw std::runtime_error("replay ignored speed setting");
                update(window);
                if(replay.frame!=1)throw std::runtime_error("replay speed setting timing failed");
                speedTenths=10;
                SendMessageW(window,WM_KEYDOWN,'R',0);
                SendMessageW(window,WM_KEYDOWN,VK_F8,0);
                update(window);
                if(replay.frame!=1)throw std::runtime_error("removed F8 slow mode still affects replay");
                for(int i=0;i<3;++i) update(window);
                if(!replay.completed || replay.frame!=4 || !replayAim)
                    throw std::runtime_error("replay completion or shot visualization failed");
                const auto frozen=game.player().body.position;
                update(window);
                if(game.player().body.position!=frozen) throw std::runtime_error("finished replay did not freeze");
                smoke=true;
                SendMessageW(window,WM_PAINT,0,0);
                const RECT view=viewport(window);
                const int uiX=view.left+890*(view.right-view.left)/1000,uiY=view.top+30*(view.bottom-view.top)/600;
                SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(uiX,uiY));
                SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(uiX,uiY));
                if(!hideReplayUi()||!shots.empty()||replay.frame!=4)throw std::runtime_error("replay UI button leaked input or did not hide");
                const auto head=game.traversal().aimOrigin;
                if(renderer.pixels()[static_cast<int>(std::lround(head.y))*1000+static_cast<int>(std::lround(head.x))]!=0xFF0000)
                    throw std::runtime_error("hidden replay UI hid the red head point");
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window,WM_KEYDOWN,VK_F9,0);
                if(hideReplayUi())throw std::runtime_error("replay UI restore failed");
                SendMessageW(window,WM_KEYDOWN,VK_F7,0);
                if(replay.active) throw std::runtime_error("replay stop failed");
                SendMessageW(window,WM_KEYDOWN,VK_F10,0);
                if(!recorder.active||game.player().body.position!=game.level().spawn.position)throw std::runtime_error("recording did not restart level");
                speedTenths=3;SendMessageW(window,WM_KEYDOWN,'D',0);smoke=false;
                for(int i=0;i<20;++i)update(window);
                smoke=true;SendMessageW(window,WM_KEYUP,'D',0);
                if(recorder.script.totalFrames!=6)throw std::runtime_error("recording used wall frames instead of logical frames");
                const auto recordedPosition=game.player().body.position;
                openMenu();smoke=false;update(window);smoke=true;
                if(recorder.script.totalFrames!=6)throw std::runtime_error("menu was recorded as gameplay");
                bindingsPath=(screenshot.empty()?std::filesystem::temp_directory_path():std::filesystem::path(screenshot).parent_path())/
                    ("smoke-recording-"+std::to_string(GetCurrentProcessId()))/"keybindings.ini";
                smoke=false;SendMessageW(window,WM_KEYDOWN,VK_F10,0);smoke=true;
                if(recorder.active||recorder.pending||lastRecordingPath.empty())throw std::runtime_error("recording did not stop and save");
                por2::Replay recordedReplay;por2::Game playback(0);
                recordedReplay.start(por2::ReplayScript::load(lastRecordingPath),0,playback);
                while(!recordedReplay.completed)recordedReplay.step(playback);
                if(playback.player().body.position!=recordedPosition)throw std::runtime_error("window recording playback differs");
                loadMaps();
                if(customLevels.empty()||!mapWarnings.empty())throw std::runtime_error("editor JSON catalog failed to load");
                selected=static_cast<int>(levels.size())-1;startSelected();
                if(game.level().editorJson.empty()||game.level().id!=levels[selected])throw std::runtime_error("custom map selection did not start map");
                endLevelIntro();openMenu(Page::Levels);SendMessageW(window,WM_PAINT,0,0);
                if(menuButton(4).left!=menuRow(3).left||menuButton(5).right!=menuRow(3).right||menuButton(3).top<=menuButton(4).bottom)
                    throw std::runtime_error("record/replay menu layout failed");
                if (!paintCount) throw std::runtime_error("window smoke: paint callback was never called");
                DestroyWindow(window);
                return;
            }
        }
        por2::InputFrame input;
        input.movement = {keys[bindings[0]], keys[bindings[1]], keys[bindings[2]]};
        if (!menu && levelIntroActive()) {
            stepLevelIntro();
            InvalidateRect(window,nullptr,FALSE);
            return;
        }
        const bool advanceLogic=!menu&&(replay.active || logicDue(gameClock));
        if(advanceLogic){
            input.restart = keys[bindings[4]];
            input.useExit = exitPressed;
            exitPressed = false;
            input.shots.swap(shots);
        }
        const auto before=game.player().body.position;
        const int previousLevel=game.level().id;
        const bool previouslyFinished=game.finished();
        if(advanceLogic) {
            if (replay.active) {
                if (!replay.completed && (replaySingle || (!replay.paused && logicDue(replayClock)))) {
                    if(replaySingle)replayClock=0;
                    replay.step(game);
                    if (aimTicks>0) --aimTicks;
                    const auto& action=replay.script.actions[replay.lastAction];
                    if (!action.input.shots.empty()) { replayAim=action.input.shots.front(); aimTicks=15; }
                }
                replaySingle=false;
            } else {
                const bool recording=recorder.active;
                recorder.capture(input);game.tick(input);
                if(recording&&(!recorder.active||game.level().id!=previousLevel||game.finished()))stopRecording();
            }
        }
        if (!replay.active && (game.level().id!=previousLevel || (previouslyFinished && !game.finished())))
            beginLevelIntro();
        if(smoke && menu && (before.x!=game.player().body.position.x || before.y!=game.player().body.position.y))
            throw std::runtime_error("menu did not freeze physics");
        std::optional<por2::Shot> preview;
        if (previewEnabled && !replay.active && !menu && !smoke && GetForegroundWindow()==window) {
            POINT cursor{};
            if (GetCursorPos(&cursor) && ScreenToClient(window,&cursor)) {
                const auto point=logicalPoint(window,cursor.x,cursor.y);
                if (point) preview=por2::Shot{previewPortal,*point};
            }
        }
        if(previewEnabled && smoke && smokeTicks==10) preview=por2::Shot{0,{260,389}};
        renderer.draw(game, debug, grid&&!hideReplayUi(), preview);
        const std::string nextTitle = game.finished()
            ? "Por2D - Completed! | Esc: menu | F11: fullscreen"
            : "Por2D - " + game.level().name + " | Esc: pause / menu | F11: fullscreen";
        if (nextTitle != title) { title = nextTitle; SetWindowTextW(window, utf8(title).c_str()); }
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
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        const bool pressed = message != WM_KEYUP && message != WM_SYSKEYUP;
        unsigned key = static_cast<unsigned>(wparam);
        if(pressed&&!(lparam&(1LL<<30))&&key==VK_F10&&app->rebinding<0){app->toggleRecording();InvalidateRect(window,nullptr,FALSE);return 0;}
        if(message==WM_SYSKEYDOWN && key==VK_F4)return DefWindowProcW(window,message,wparam,lparam);
        if(app->menu && app->page==Application::Page::Settings && app->rebinding>=0){
            if(pressed && !(lparam&(1LL<<30))){
                if(key==VK_ESCAPE)app->back();else app->assignBinding(key);
                app->suppressedKey=key;
                InvalidateRect(window,nullptr,FALSE);
            }
            return 0;
        }
        if (pressed && !(lparam & (1LL<<30)) && key==VK_F6) {
            app->importReplay(window); return 0;
        }
        if (app->replay.active && !app->menu && key!=VK_ESCAPE && key!=VK_F11 && key!=VK_F1 && key!=VK_F2) {
            if (pressed && !(lparam & (1LL<<30))) {
                if (key==VK_SPACE && !app->replay.completed) app->replay.paused=!app->replay.paused;
                if (key==VK_OEM_PERIOD) { app->replay.paused=true; app->replaySingle=true; }
                if (key=='R') {
                    app->replay.restart(app->game); app->replayClock=app->aimTicks=0;
                    app->replaySingle=false; app->replayAim.reset(); app->clearInput();
                    app->renderer.draw(app->game,app->debug,app->grid&&!app->hideReplayUi());
                }
                if (key==VK_F9)app->toggleReplayUi(window);
                if (key==VK_F7) { app->replay.active=false; app->clearInput(); }
                InvalidateRect(window,nullptr,FALSE);
            }
            return 0;
        }
        if (key==app->suppressedKey) {
            if (!pressed) app->suppressedKey=256;
            return 0;
        }
        if (app->levelIntroActive() && !app->menu && key!=VK_ESCAPE) {
            if (pressed && !(lparam & (1LL<<30))) {
                app->endLevelIntro();
                app->suppressedKey=key;
                InvalidateRect(window,nullptr,FALSE);
            }
            return 0;
        }
        if (app->intro && pressed && !(lparam & (1LL<<30))) {
            app->leaveIntro();
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        if (key == VK_CONTROL) key = (lparam & (1LL << 24)) ? VK_RCONTROL : VK_LCONTROL;
        if (!app->menu && key < app->keys.size()) app->keys[key] = pressed;
        if (pressed && !(lparam & (1LL << 30))) {
            if (key == VK_F11 || (key==VK_RETURN && (lparam & (1LL<<29)))) { app->toggleFullscreen(window); return 0; }
            if (key == VK_ESCAPE) {
                if(app->menu)app->back();else app->openMenu();
                InvalidateRect(window,nullptr,FALSE);return 0;
            }
            if(key==app->bindings[3] && !app->menu) app->exitPressed=true;
            if(app->menu) {
                if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_PRIOR||key==VK_NEXT||key==VK_RETURN)
                    app->mouseNavigation=false;
                if(app->page!=Application::Page::Levels){
                    int& item=app->page==Application::Page::Main?app->menuItem:app->settingItem;
                    const std::vector<int> order=app->page==Application::Page::Main?std::vector<int>{0,1,2,4,5,3}:std::vector<int>{0,1,2,3,4,5,6,7,10,8,9};
                    const int index=static_cast<int>(std::find(order.begin(),order.end(),item)-order.begin());
                    if(key==VK_UP)item=order[(index+order.size()-1)%order.size()];
                    if(key==VK_DOWN)item=order[(index+1)%order.size()];
                    if(app->page==Application::Page::Main&&(item==4||item==5)){if(key==VK_LEFT)item=4;if(key==VK_RIGHT)item=5;}
                    if(key==VK_RETURN){if(app->page==Application::Page::Main)app->activateMenu(window);else app->activateSetting();}
                    InvalidateRect(window,nullptr,FALSE);return 0;
                }
                int delta=0;
                if(key==VK_LEFT) delta=-1;
                if(key==VK_RIGHT) delta=1;
                if(key==VK_UP) delta=-3;
                if(key==VK_DOWN) delta=3;
                if(key==VK_PRIOR) delta=-15;
                if(key==VK_NEXT) delta=15;
                app->selected=std::clamp(app->selected+delta,0,static_cast<int>(app->levels.size())-1);
                if(key==VK_RETURN) app->startSelected();
                InvalidateRect(window,nullptr,FALSE);return 0;
            }
            if (key == VK_F1) app->debug = !app->debug;
            if (key == VK_F2) app->grid = !app->grid;
            if (key == app->bindings[5]) app->previewEnabled = !app->previewEnabled;
            for(int i=0;i<2;++i)if(key==app->bindings[6+i])app->keyboardShot(window,i);
        }
        if(message==WM_SYSKEYDOWN) return DefWindowProcW(window,message,wparam,lparam);
        return 0;
    }
    case WM_MOUSEMOVE: {
        app->menuPointer=logicalPoint(window,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        app->mouseNavigation=true;
        TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT),TME_LEAVE,window,0};
        TrackMouseEvent(&tracking);
        if(app->menu||app->replay.active)InvalidateRect(window,nullptr,FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        app->menuPointer.reset();app->mouseNavigation=true;
        if(app->menu||app->replay.active)InvalidateRect(window,nullptr,FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        app->menuPointer=logicalPoint(window,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        app->mouseNavigation=true;
        if(app->replay.active&&!app->menu&&message==WM_LBUTTONDOWN&&app->menuPointer&&contains(ReplayUiButton,*app->menuPointer)){
            app->toggleReplayUi(window);return 0;
        }
        if(app->menu && app->page==Application::Page::Settings && app->rebinding>=0){
            app->assignBinding(message==WM_LBUTTONDOWN?VK_LBUTTON:VK_RBUTTON);
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        if(app->replay.active && !app->menu) return 0;
        if(app->levelIntroActive() && !app->menu) {
            app->endLevelIntro();
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        if(app->intro) {
            app->leaveIntro();
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        if(!app->menu)for(int i=0;i<2;++i)
            if(app->bindings[6+i]==(message==WM_LBUTTONDOWN?VK_LBUTTON:VK_RBUTTON))app->previewPortal=i;
        app->mousePressed[message == WM_LBUTTONDOWN ? 0 : 1] = true;
        SetCapture(window);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        const int id = message == WM_LBUTTONUP ? 0 : 1;
        const auto point=logicalPoint(window,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        if(app->mousePressed[id] && point) {
            if(app->menu) { if(id==0) app->menuClick(window,*point); }
            else for(int i=0;i<2;++i)if(app->bindings[6+i]==(id==0?VK_LBUTTON:VK_RBUTTON))app->shots.push_back({i,*point});
        }
        app->mousePressed[id] = false;
        if (!app->mousePressed[0] && !app->mousePressed[1]) ReleaseCapture();
        return 0;
    }
    case WM_CAPTURECHANGED:
        app->mousePressed = {};
        return 0;
    case WM_KILLFOCUS:
        if(app->replay.active) app->replay.paused=true;
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
        else if(app->levelIntroActive()) app->drawLevelIntro(dc);
        else if (app->game.finished()&&!app->hideReplayUi()) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            RECT area{0, 30, por2::WindowWidth, 70};
            const auto completed=L"已完成！  "+keyName(app->bindings[4])+L"：重新开始    Esc：菜单";
            DrawTextW(dc, completed.c_str(), -1, &area, DT_CENTER | DT_SINGLELINE);
        }
        if(!app->menu && !app->intro && !app->levelIntroActive() && !app->hideReplayUi()) {
            const auto entry=std::find(app->levels.begin(),app->levels.end(),app->game.level().id);
            const std::string caption = (app->game.level().editorJson.empty()?"Level" + std::to_string(entry-app->levels.begin()):"自定义") + "  " + app->game.level().name;
            const std::wstring wide=utf8(caption);
            RECT levelArea{690, 548, 975, 588};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(220, 230, 245));
            DrawTextW(dc, wide.c_str(), -1, &levelArea, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
        }
        if(!app->menu && !app->intro) app->drawReplay(dc);
        if(!app->menu&&!app->intro&&!app->replay.active&&!app->recordingMessage.empty()){
            fill(dc,{10,10,990,44},RGB(15,21,34));
            label(dc,{10,10,990,44},app->recordingMessage+(app->recorder.active?L" · "+std::to_wstring(app->recorder.script.totalFrames)+L" 帧":L""),17,RGB(255,190,120));
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
        if(app->smoke && (app->menu || app->intro || app->levelIntroActive() || app->replay.active) && !app->screenshot.empty()) {
            std::vector<std::uint32_t> pixels(1000*600);
            if(!GetDIBits(dc,bitmap,0,600,pixels.data(),&info,DIB_RGB_COLORS))
                app->failure="menu screenshot failed";
            else {
                BITMAPFILEHEADER header{}; header.bfType=0x4d42;
                header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
                header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size()*4);
                std::ofstream file(app->screenshot+(app->intro?".intro.bmp":app->menu&&app->page==Application::Page::Settings?".settings.bmp":app->menu&&app->page==Application::Page::Levels?".levels.bmp":app->hideReplayUi()?".replay-hidden.bmp":app->replay.active?".replay.bmp":app->levelIntroActive()?".level-intro.bmp":".menu.bmp"),std::ios::binary);
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
    case WM_CLOSE:
        if(app->stopRecording())DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int runWindow(int level, bool smoke, const std::string& screenshot, bool direct, bool fullscreen, const std::optional<por2::ReplayScript>& script) {
    SetProcessDPIAware();
    Application app(level);
    app.smoke = smoke;
    if(!smoke){app.loadBindings();app.loadMaps();}
    app.menu=!direct || smoke; app.started=direct && !smoke;
    app.intro=!direct || smoke;
    if (direct && !smoke) app.beginLevelIntro();
    if (script) app.startReplay(*script);
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
    if(!app.mapWarnings.empty())MessageBoxW(window,app.mapWarnings.c_str(),L"部分自定义地图未加载",MB_OK|MB_ICONWARNING);
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
    if (smoke) std::cout << "PASS native window: pause/resume, menus, bindings/speed persistence, all decimal speeds, recording/save/load/replay, replay UI/red point, keyboard, mouse, restart, focus, paint, map16 and fullscreen\n";
    return static_cast<int>(message.wParam);
}
} // namespace

int main(int argc, char** argv) {
    try {
        int level = 0;
        int frames = 60;
        bool headless = false, framesSpecified=false;
        std::string replayPath;
        bool windowSmoke = false;
        bool direct=false, fullscreen=false;
        std::string screenshot;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--headless") headless = true;
            else if (argument == "--window-smoke-test") windowSmoke = true;
            else if (argument == "--fullscreen") fullscreen=true;
            else if (argument == "--level" && i + 1 < argc) { level = std::stoi(argv[++i]); direct=true; }
            else if (argument == "--frames" && i + 1 < argc) { frames = std::stoi(argv[++i]); framesSpecified=true; }
            else if (argument == "--replay" && i + 1 < argc) replayPath=argv[++i];
            else if (argument == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
            else if (argument == "--help") {
                std::cout << "Por2D.exe [--fullscreen] [--level ID] [--headless --frames N --screenshot path.bmp]\n"
                             "Por2D.exe --window-smoke-test [--screenshot path.bmp]\n"
                             "Por2D.exe --level ID --replay script.txt [--headless]\n";
                return 0;
            } else throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
        if (frames < 0 || frames > 1000000) throw std::invalid_argument("frames must be 0..1000000");
        if (headless && windowSmoke) throw std::invalid_argument("choose headless or window smoke, not both");
        if (windowSmoke && level != 0) throw std::invalid_argument("window smoke requires level 0");
        std::optional<por2::ReplayScript> script;
        if (!replayPath.empty()) {
            if (windowSmoke) throw std::invalid_argument("replay cannot be combined with window smoke");
            script=por2::ReplayScript::load(std::filesystem::path(replayPath));
            if (script->level<0 && !script->customLevel && !direct) throw std::invalid_argument("replay needs --level ID or a level/map directive");
            if (script->level>=0) level=script->level;
        }
        if (!headless) return runWindow(level, windowSmoke, screenshot,direct,fullscreen,script);
        por2::Game game(level);
        por2::Replay replay;
        if (script) {
            replay.start(*script,level,game);
            if (!framesSpecified) frames=script->totalFrames;
        }
        int executed=0;
        for (; executed<frames; ++executed) {
            if (script) { if(replay.completed) break; replay.step(game); }
            else game.tick({});
        }
        if (script) std::cout<<"replay="<<(replay.cleared?"cleared":replay.completed?"ended":"partial")<<" ";

        por2::Renderer renderer;
        renderer.draw(game);
        if (!screenshot.empty()) renderer.saveBitmap(screenshot);
        const auto& player = game.player();
        std::cout << "level=" << game.level().id << " frames=" << executed << " position="
                  << player.body.position.x << ',' << player.body.position.y
                  << " velocity=" << player.velocity.x << ',' << player.velocity.y << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
