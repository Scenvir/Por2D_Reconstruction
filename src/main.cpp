#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "por2/editor_host.hpp"
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include "por2/replay.hpp"
#include "por2/render.hpp"
#include "por2/tutorial.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <cwctype>
#include <cwchar>
#include <random>
#include <shellapi.h>

namespace {
std::filesystem::path executableFolder() {
    wchar_t executable[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length || length>=32768)throw std::runtime_error("Cannot locate game folder");
    return std::filesystem::path(executable).parent_path();
}
std::filesystem::path resourceFolder() {
    auto folder=executableFolder();
    // Support both build/Por2D.exe and CMake's build/Release/Por2D.exe.
    if(folder.parent_path().filename()==L"build")folder=folder.parent_path();
    return folder.filename()==L"build"?folder.parent_path():folder;
}
std::filesystem::path userDataFolder() {
    // A source build and a released package both keep data beside their resources.
    return resourceFolder();
}
void enableDpiAwareness() {
    using SetAwareness=BOOL(WINAPI*)(HANDLE);
    const auto setAwareness=reinterpret_cast<SetAwareness>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));
    if(!setAwareness || !setAwareness(reinterpret_cast<HANDLE>(-4)))SetProcessDPIAware();
}
RECT fitToWorkArea(RECT bounds, const RECT& work) {
    const LONG width=std::min(bounds.right-bounds.left,work.right-work.left);
    const LONG height=std::min(bounds.bottom-bounds.top,work.bottom-work.top);
    bounds.left=std::clamp(bounds.left,work.left,work.right-width);
    bounds.top=std::clamp(bounds.top,work.top,work.bottom-height);
    bounds.right=bounds.left+width;bounds.bottom=bounds.top+height;
    return bounds;
}
struct DesktopWindowState {
    RECT bounds{}; // Screen coordinates, never WINDOWPLACEMENT workspace coordinates.
    bool maximized=false;
};
std::optional<DesktopWindowState> readWindowState(const std::filesystem::path& path) {
    std::ifstream file(path);
    DesktopWindowState state;int version=0,maximized=0;
    auto& r=state.bounds;
    if(!(file>>version>>r.left>>r.top>>r.right>>r.bottom>>maximized) || version!=1 ||
       (maximized!=0&&maximized!=1))return {};
    for(const LONG coordinate:{r.left,r.top,r.right,r.bottom})
        if(coordinate < -1000000 || coordinate > 1000000)return {};
    if(r.right-r.left<320 || r.bottom-r.top<240 || r.right-r.left>32768 || r.bottom-r.top>32768)return {};
    state.maximized=maximized!=0;
    return state;
}
bool writeWindowState(const std::filesystem::path& path,const DesktopWindowState& state) {
    const auto temporary=std::filesystem::path(path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");
    std::ofstream file(temporary);
    const auto& r=state.bounds;
    file<<1<<' '<<r.left<<' '<<r.top<<' '<<r.right<<' '<<r.bottom<<' '<<state.maximized<<'\n';
    file.close();
    if(file && MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    std::error_code error;std::filesystem::remove(temporary,error);return false;
}
void checkDataFolders() {
    const auto root=std::filesystem::temp_directory_path()/
        (L"por2-data-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    const auto target=root/L"游戏目录";
    struct Cleanup {
        std::vector<std::filesystem::path> paths;
        ~Cleanup(){for(const auto& path:paths){std::error_code error;std::filesystem::remove(path,error);}}
    } cleanup{{target/L"window.ini",target,root}};
    std::filesystem::create_directories(target);
    if(userDataFolder()!=resourceFolder())throw std::runtime_error("data must stay in game folder");
    const DesktopWindowState remembered{{-1200,80,-300,680},true};
    if(!writeWindowState(target/L"window.ini",remembered))throw std::runtime_error("window state write failed");
    const auto loaded=readWindowState(target/L"window.ini");
    if(!loaded||!loaded->maximized||!EqualRect(&loaded->bounds,&remembered.bounds))
        throw std::runtime_error("window position/maximized persistence failed");
    {std::ofstream file(target/L"window.ini");file<<"1 0 0 2147483647 600 0";}
    if(readWindowState(target/L"window.ini"))throw std::runtime_error("invalid window state accepted");
    const RECT fit=fitToWorkArea({-200,-200,1800,1000},{0,0,800,560});
    if(fit.left!=0||fit.top!=0||fit.right!=800||fit.bottom!=560)
        throw std::runtime_error("small desktop window fitting failed");
}
static_assert(L"中文"[0] == 0x4E2D && L"中文"[1] == 0x6587, "Source must be compiled as UTF-8");
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
constexpr RECT SpeedButton{190,430,490,474};
constexpr RECT FullscreenButton{510,430,810,474};
constexpr RECT ReplayUiButton{735,12,920,48};
constexpr RECT GearButton{936,10,986,62};
constexpr RECT HelpButton{400,492,600,532};
constexpr RECT ResetButton{190,492,390,532};
constexpr RECT SettingsBack{610,492,810,532};
constexpr RECT MapEditorButton{60,534,340,570}, MapFolderButton{360,534,660,570}, MapRefreshButton{680,534,940,570};
constexpr RECT LessonPrevious{40,540,215,582}, LessonSettings{230,540,410,582}, LessonSkip{570,540,740,582}, LessonNext{755,540,960,582};
constexpr RECT LessonPause{425,540,555,582};
constexpr RECT LessonPractice{755,120,960,160}, CrownButton{350,536,650,568};
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
void label(HDC dc, RECT r, const std::wstring& text, int size, COLORREF color,
           UINT format=DT_CENTER|DT_VCENTER|DT_SINGLELINE) {
    HFONT font = CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei");
    auto old = SelectObject(dc,font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    DrawTextW(dc,text.c_str(),-1,&r,format|DT_NOPREFIX);
    SelectObject(dc,old); DeleteObject(font);
}
void fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush);
}
int paragraph(HDC dc,RECT area,const std::wstring& text,int size,COLORREF color,bool measure=false,const wchar_t* face=L"Microsoft YaHei"){
    HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
    auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
    DrawTextW(dc,text.c_str(),-1,&area,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|(measure?DT_CALCRECT:0));
    SelectObject(dc,old);DeleteObject(font);return area.bottom-area.top;
}
struct Application {
    explicit Application(int level) : game(level), levels(por2::Campaign.begin(),por2::Campaign.end()) {
        auto found=std::find(levels.begin(),levels.end(),level);
        if(found!=levels.end()) selected=static_cast<int>(found-levels.begin());
    }
    por2::Game game;
    por2::Renderer renderer;
    por2::Game demo{0};
    std::mt19937 random{std::random_device{}()};
    int demoTicks=0, demoMove=0;
    por2::Replay replay;
    por2::Tutorial tutorial;
    bool tutorialActive=false, tutorialPending=false, tutorialPaused=false;
    bool tutorialPractice=false;
    void togglePractice(){tutorialPractice=!tutorialPractice;tutorialPaused=false;clearInput();tutorial.aim.reset();if(!tutorialPractice)tutorial.reset(tutorial.stage);}
    void finishTutorial(){tutorialActive=tutorialPending=false;clearInput();renderer.draw(game,debug,grid);}
    void tutorialStep(int delta){
        if(tutorial.stage+delta>=6){finishTutorial();return;}
        tutorialPractice=false;tutorial.reset(std::clamp(tutorial.stage+delta,0,5));clearInput();
        renderer.draw(tutorial.scene,true,true,tutorial.aim,false);
    }
    void drawGear(HDC dc) const {
        fill(dc,GearButton,buttonColor(GearButton));
        HPEN pen=CreatePen(PS_SOLID,2,RGB(220,233,250));auto old=SelectObject(dc,pen);auto brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
        Ellipse(dc,952,20,972,40);Ellipse(dc,958,26,966,34);
        for(int i=0;i<8;++i){double a=i*3.141592653589793/4;MoveToEx(dc,962+static_cast<int>(10*std::cos(a)),30+static_cast<int>(10*std::sin(a)),nullptr);LineTo(dc,962+static_cast<int>(15*std::cos(a)),30+static_cast<int>(15*std::sin(a)));}
        SelectObject(dc,brush);SelectObject(dc,old);DeleteObject(pen);label(dc,{936,43,986,61},L"菜单",12,RGB(220,233,250));
    }
    std::wstring helpText;
    struct HelpBlock{std::wstring text;int size;bool code;};
    std::vector<HelpBlock> helpBlocks;
    int helpScroll=0;
    mutable int helpHeight=0;
    void openHelp(){
        openMenu(Page::Help);helpScroll=0;
        try{
            std::ifstream file(resourceFolder()/L"新手教程.md",std::ios::binary);if(!file)throw std::runtime_error("missing tutorial");
            std::string text((std::istreambuf_iterator<char>(file)),{});helpText=utf8(text);
        }catch(const std::exception&){helpText=L"无法读取新手教程.md。请将教程文件与程序放在一起（开发版本放在项目目录）。";}
        helpBlocks.clear();std::wistringstream lines(helpText);std::wstring line;bool code=false;
        while(std::getline(lines,line)){
            if(!line.empty()&&line.back()==L'\r')line.pop_back();
            if(line.rfind(L"```",0)==0){code=!code;continue;}
            if(code){helpBlocks.push_back({line.empty()?L" ":line,18,true});continue;}
            if(line.empty()){helpBlocks.push_back({L" ",8,false});continue;}
            int size=19;
            if(line.front()==L'#'){const auto start=line.find_first_not_of(L"# ");line=start==std::wstring::npos?L" ":line.substr(start);size=25;}
            if(line.front()==L'|'&&line.find_first_not_of(L"|-: \t")==std::wstring::npos)continue;
            if(line.front()==L'|'){
                line.erase(0,1);if(!line.empty()&&line.back()==L'|')line.pop_back();
                std::size_t at=0;while((at=line.find(L'|',at))!=std::wstring::npos){line.replace(at,1,L" — ");at+=3;}
            }
            for(const auto* mark:{L"**",L"`"}){std::size_t at;while((at=line.find(mark))!=std::wstring::npos)line.erase(at,std::wcslen(mark));}
            std::size_t at=0;
            while((at=line.find(L'[',at))!=std::wstring::npos){const auto end=line.find(L"](",at),close=end==std::wstring::npos?end:line.find(L')',end+2);if(end==std::wstring::npos||close==std::wstring::npos)break;const auto text=line.substr(at+1,end-at-1);line.replace(at,close-at+1,text);at+=text.size();}
            helpBlocks.push_back({line,size,false});
        }
    }
    void scrollHelp(int amount){helpScroll=std::clamp(helpScroll+amount,0,std::max(0,helpHeight-400));}
    void drawHelp(HDC dc) {
        fill(dc,{0,0,1000,600},RGB(15,21,34));label(dc,{40,20,960,78},L"帮助 / 新手教程",32,RGB(240,245,255));
        const int saved=SaveDC(dc);IntersectClipRect(dc,45,100,935,500);
        std::vector<int> heights;helpHeight=0;
        for(const auto& block:helpBlocks){
            const int height=paragraph(dc,{55,0,925,0},block.text,block.size,RGB(220,230,245),true,block.code?L"Consolas":L"Microsoft YaHei");
            heights.push_back(height);helpHeight+=height+(block.code?0:7);
        }
        helpScroll=std::clamp(helpScroll,0,std::max(0,helpHeight-400));
        int offset=0;
        for(std::size_t index=0;index<helpBlocks.size();++index){
            const auto& block=helpBlocks[index];
            const auto font=block.code?L"Consolas":L"Microsoft YaHei";
            const int height=heights[index];
            const int y=100+offset-helpScroll;
            if(y+height>=100&&y<500)paragraph(dc,{55,y,925,y+height},block.text,block.size,block.size>19?RGB(130,194,255):RGB(220,230,245),false,font);
            offset+=height+(block.code?0:7);
        }
        RestoreDC(dc,saved);
        fill(dc,{950,100,958,500},RGB(45,58,76));
        const int thumb=std::max(20,400*400/std::max(400,helpHeight));
        const int y=100+(400-thumb)*helpScroll/std::max(1,helpHeight-400);fill(dc,{950,y,958,y+thumb},RGB(90,170,225));
        label(dc,{40,502,960,538},L"鼠标滚轮 / ↑ ↓ 滚动 · PageUp / PageDown 翻页 · Home / End",16,RGB(150,173,201));
        fill(dc,{375,545,625,585},buttonColor({375,545,625,585}));label(dc,{375,545,625,585},L"返回设置 (Esc)",20,RGB(240,245,255));
    }
    void drawTutorial(HDC dc) const {
        const auto& t=tutorial;
        const std::array<std::wstring,6> titles{L"键位与移动",L"哪些墙可以射门？",L"必须击中三格的中间格",L"头朝向决定门朝向",L"门朝向与传送对应关系",L"跨门时的锁定与换门"};
        fill(dc,{0,0,1000,112},RGB(15,21,34));
        label(dc,{30,10,910,48},L"新手教学  "+std::to_wstring(t.stage+1)+L" / 6 · "+titles[t.stage],27,RGB(240,245,255));
        std::wstring detail;
        if(t.stage==0)detail=L"身体按屏幕方向移动。点击下方“键位设置”可改键；返回菜单后继续教学。";
        if(t.stage==1)detail=t.phase==0?L"白色墙：连续三格可放门，绿色射线与箭头表示当前可以放置。":L"深灰墙：不能放门。红色射线和叉号表示放置失败。";
        if(t.stage==2)detail=t.phase==1?L"命中第 2 格：它是连续三格的中间，能够放门。":L"命中边上的格子：不会自动吸附到中间，三格墙段容不下新门，放置失败。";
        if(t.stage==3)detail=L"红点是角色头部；四面白墙上的灰色箭头表示当前在此处开门的朝向，随角色朝向和位置变化。";
        if(t.stage==4)detail=t.phase==2?L"蓝门与橙门朝向相反，仍然亮对亮、暗对暗。角色从蓝门进入、橙门出来后，头朝下了。":L"传送始终亮对亮、暗对暗；观察两端标记和跨门的身体。红点标出角色头部。";
        if(t.stage==5)detail=t.phase==0?L"身体较大的一侧锁定（金框）。尝试移动这扇门会失败，原门保持不变。":t.phase==1?L"身体较小的一侧可以换门：射击后，另一端露出的身体也移动到新的门口。":L"继续穿过门，观察金框随本体侧变化；完全离开后解除锁定。";
        paragraph(dc,{40,58,900,106},detail,18,RGB(176,201,227));
        if(t.stage==0){
            const std::array<bool,3> pressed{t.movement.left,t.movement.right,t.movement.jump};
            for(int i=0;i<3;++i){RECT r{365+i*90,160,440+i*90,206};fill(dc,r,pressed[i]?RGB(35,110,174):RGB(31,43,61));label(dc,r,keyName(bindings[i]),22,RGB(240,245,255));}
            label(dc,{280,215,730,247},L"演示按键：左移 / 右移 / 跳跃",17,RGB(180,200,225));
        }
        if(t.stage==2){
            for(int i=0;i<3;++i)label(dc,{185,300+i*20,235,320+i*20},std::to_wstring(i+1),17,i==t.phase?RGB(255,215,100):RGB(170,180,200));
        }
        if(t.stage==3)label(dc,{360,220,690,255},L"当前头朝"+std::array<std::wstring,4>{L"上",L"右",L"下",L"左"}[t.phase],23,RGB(255,215,100));
        if(t.stage==4){
            label(dc,{260,220,740,255},t.phase==0?L"同向门：头朝上":t.phase==1?L"同向门：头朝下":L"反向蓝门 → 橙门：角色倒过来了",23,RGB(255,215,100));
            const auto& portals=t.scene.portals();
            if(portals[0].active()&&portals[1].active())for(int end=0;end<2;++end){
                const auto a=por2::decode(portals[0]);
                const auto source=a.anchor+a.tangent*(end==0?8:52);
                const auto target=por2::transformPoint(source,portals[0],portals[1]);
                const auto color=end==0?RGB(145,165,195):RGB(255,239,170);
                for(int i=0;i<2;++i){
                    const auto p=i==0?source:target;
                    const LONG x=static_cast<LONG>(p.x)+(i==0?-70:15),y=static_cast<LONG>(p.y);
                    label(dc,{x,y-13,x+55,y+13},end==0?L"暗端":L"亮端",18,color);
                }
                const auto p=source+(target-source)*((t.frame%120)/119.0);
                fill(dc,{static_cast<LONG>(p.x)-4,static_cast<LONG>(p.y)-4,static_cast<LONG>(p.x)+5,static_cast<LONG>(p.y)+5},color);
            }
        }
        if(t.stage==5){
            const auto& view=t.scene.traversal();const double total=view.bodyAreas[0]+view.bodyAreas[1];
            for(int i=0;i<2;++i){
                const RECT area=i==0?RECT{10,250,230,310}:RECT{765,310,995,370};
                const auto share=total>0?std::to_wstring(static_cast<int>(std::lround(view.bodyAreas[i]*100/total)))+L"%":L"";
                paragraph(dc,area,(i==0?L"蓝门 ":L"橙门 ")+share+(view.lockedPortal==i?L"\n金框：当前锁定":L"\n未锁定"),18,i==0?RGB(80,175,255):RGB(255,170,80));
            }
        }
        if(t.aim&&!tutorialPractice){
            const auto hit=por2::castShot(t.scene.level().map,t.scene.traversal().aimOrigin,t.aim->target);
            if(hit){
                const auto p=t.scene.traversal().aimOrigin+(hit->point-t.scene.traversal().aimOrigin)*((t.frame%60)/60.0);
                fill(dc,{static_cast<LONG>(p.x)-2,static_cast<LONG>(p.y)-2,static_cast<LONG>(p.x)+3,static_cast<LONG>(p.y)+3},RGB(255,220,120));
            }
        }
        fill(dc,{0,450,1000,600},RGB(15,21,34));
        fill(dc,LessonPractice,buttonColor(LessonPractice));
        label(dc,LessonPractice,tutorialPractice?L"返回自动演示":L"亲自试试",19,RGB(240,245,255));
        std::wstring outcome=t.stage==0?L"射门："+keyName(bindings[6])+L" / "+keyName(bindings[7])+L" · 交互："+keyName(bindings[3])+L" · 重开："+keyName(bindings[4]):t.attempted?(t.accepted?L"实际射击结果：放置成功":L"实际射击结果：放置失败，原门保留"):L"观察预览，随后自动尝试射击";
        if(t.stage==4)outcome=L"亮对亮，暗对暗 · 移动标记表示对应位置，身体按真实传送规则映射";
        if(tutorialPractice)outcome=L"自由练习：使用当前键位"+std::wstring(t.attempted?(t.accepted?L" · 射门成功":L" · 射门失败，原门保留"):L" · 移动、跳跃、射门或重开");
        label(dc,{30,458,970,493},outcome,21,RGB(255,215,100));
        label(dc,{30,498,970,527},tutorialPractice?L"点击下方按钮暂停或换段 · 返回自动演示会重置本段 · 不影响正式关卡":L"实时演示循环播放 · 空格 / → 下一段 · ← 上一段 · 教学不会改变关卡进度",16,RGB(150,173,201));
        for(const auto r:{LessonPrevious,LessonSettings,LessonPause,LessonSkip,LessonNext})fill(dc,r,buttonColor(r));
        label(dc,LessonPause,tutorialPaused?L"继续 (P)":L"暂停 (P)",19,RGB(240,245,255));
        label(dc,LessonPrevious,L"上一段",19,RGB(240,245,255));label(dc,LessonSettings,L"键位设置",19,RGB(240,245,255));
        label(dc,LessonSkip,L"跳过教学",19,RGB(240,245,255));label(dc,LessonNext,t.stage==5?L"开始 Level 0":L"下一段",19,RGB(240,245,255));
    }
    std::vector<por2::Level> customLevels;
    std::vector<std::filesystem::path> customPaths;
    std::wstring mapWarnings;
    std::wstring mapMessage;
    por2::EditorHost editorHost;
    por2::Level levelById(int id) const {
        for(const auto& level:customLevels)if(level.id==id)return level;
        return por2::makeLevel(id);
    }
    por2::Game newGame(int id) const {const auto level=levelById(id);return level.editorJson.empty()?por2::Game(id):por2::Game(level);}
    std::filesystem::path userMapsFolder() const {return bindingsPath.parent_path()/L"levels";}
    void loadMaps(const std::filesystem::path& bundled={},const std::filesystem::path& user={}){
        const int oldId=levels.empty()?0:levels[std::clamp(selected,0,static_cast<int>(levels.size())-1)];
        const int oldCustom=selected-static_cast<int>(por2::Campaign.size());
        const auto oldPath=oldCustom>=0&&oldCustom<static_cast<int>(customPaths.size())?customPaths[oldCustom]:std::filesystem::path{};
        customLevels.clear();customPaths.clear();mapWarnings.clear();
        levels.assign(por2::Campaign.begin(),por2::Campaign.end());
        const auto package=bundled.empty()?resourceFolder()/L"levels":bundled;
        const auto personal=user.empty()?userMapsFolder():user;
        std::vector<std::filesystem::path> directories{package};
        std::error_code error;
        if(!std::filesystem::equivalent(package,personal,error))directories.push_back(personal);
        for(const auto& directory:directories)try{
            if(!std::filesystem::exists(directory))continue;
            std::vector<std::filesystem::path> paths;
            for(const auto& entry:std::filesystem::directory_iterator(directory)){
                auto extension=entry.path().extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
                if(entry.is_regular_file()&&extension==L".json")paths.push_back(entry.path());
            }
            std::sort(paths.begin(),paths.end());
            for(const auto& path:paths){try{const int id=1000+static_cast<int>(customLevels.size());customLevels.push_back(por2::loadEditorLevel(path,id));customPaths.push_back(path);levels.push_back(id);}
                catch(const std::exception& error){mapWarnings+=path.wstring()+L": "+utf8(error.what())+L"\n";}}
        }catch(const std::exception& error){mapWarnings+=directory.wstring()+L": "+utf8(error.what())+L"\n";}
        selected=std::clamp(selected,0,static_cast<int>(levels.size())-1);
        if(!oldPath.empty()){
            const auto found=std::find(customPaths.begin(),customPaths.end(),oldPath);
            if(found!=customPaths.end())selected=static_cast<int>(por2::Campaign.size()+std::distance(customPaths.begin(),found));
        }else{
            const auto found=std::find(levels.begin(),levels.end(),oldId);
            if(found!=levels.end())selected=static_cast<int>(std::distance(levels.begin(),found));
        }
        mapMessage=L"已加载 "+std::to_wstring(customLevels.size())+L" 张自定义地图"+(mapWarnings.empty()?L"":L"；部分文件未加载");
    }
    void refreshMaps(HWND window){
        loadMaps();clearInput();
        if(!mapWarnings.empty()&&!smoke)MessageBoxW(window,mapWarnings.c_str(),L"部分自定义地图未加载",MB_OK|MB_ICONWARNING);
        InvalidateRect(window,nullptr,FALSE);
    }
    void openMapsFolder(HWND window){
        try{
            const auto folder=userMapsFolder();std::filesystem::create_directories(folder);
            if(!smoke && reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
                throw std::runtime_error("Cannot open map folder");
            mapMessage=L"将 JSON 放入地图文件夹，然后点击刷新列表 (F5)";
        }catch(const std::exception&){mapMessage=L"无法打开地图文件夹，请检查目录权限。";}
        InvalidateRect(window,nullptr,FALSE);
    }
    void openMapEditor(HWND window){
        try{
            const auto folder=resourceFolder()/L"editor";
            const auto entry=folder/L"index.html";
            if(!std::filesystem::is_regular_file(entry)||!std::filesystem::is_regular_file(folder/L"editor.js")||
               !std::filesystem::is_regular_file(folder/L"model.js")){
                mapMessage=L"地图编辑器文件缺失，请完整解压游戏压缩包。";
            }else{
                if(!smoke){
                    const auto url=editorHost.start(resourceFolder());
                    const std::wstring wideUrl(url.begin(),url.end());
                    if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",wideUrl.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
                        throw std::runtime_error("Cannot open editor browser");
                }
                mapMessage=L"编辑器已打开：直接保存到 levels，按 F5 刷新；请保持游戏运行。";
            }
        }catch(const std::exception&){mapMessage=L"无法读取地图编辑器，请检查游戏目录权限。";}
        clearInput();InvalidateRect(window,nullptr,FALSE);
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
            const auto entry=std::find(por2::Campaign.begin(),por2::Campaign.end(),recorder.script.level);
            const auto name=recorder.script.customLevel?L"custom":entry!=por2::Campaign.end()?L"level-"+std::to_wstring(entry-por2::Campaign.begin()):L"experimental-"+std::to_wstring(recorder.script.level);
            const auto path=directory/(name+L"-"+std::to_wstring(GetTickCount64())+L".txt");
            std::ofstream output(path);output<<recorder.text();output.close();
            if(!output)throw std::runtime_error("Cannot write recording");
            lastRecordingPath=path;
            recorder.pending=false;recordingMessage=L"录制已保存；按 F6 打开录制文件夹。";
            return true;
        }catch(const std::exception&){
            recordingMessage=L"录制保存失败，数据仍保留；按 F10 重试。";
            MessageBoxW(nullptr,recordingMessage.c_str(),L"录制保存失败",MB_OK|MB_ICONERROR);return false;
        }
    }
    void toggleRecording() {
        if(recorder.active||recorder.pending){stopRecording();return;}
        if(replay.active){recordingMessage=L"请先按 F7 退出回放，再开始录制。";return;}
        if(!started)return;
        game=game.level().editorJson.empty()?por2::Game(game.level().id):por2::Game(game.level());recorder.start(game.level());
        intro=false;menu=false;tutorialPending=tutorialActive=false;endLevelIntro();gameClock=0;
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
        intro=false; menu=false; started=true; tutorialPending=tutorialActive=false;endLevelIntro();
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
        const auto recordings=bindingsPath.parent_path()/L"recordings";
        const auto initialFolder=std::filesystem::is_directory(recordings)?recordings:bindingsPath.parent_path();
        dialog.lpstrInitialDir=initialFolder.c_str();
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
    bool background=false, changingWindowMode=false, windowStateReady=false;
    DesktopWindowState windowState;
    enum class Page { Main, Levels, Settings, Help };
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
        bindingsPath=path.empty()?userDataFolder()/L"keybindings.ini":path;
        std::ifstream file(bindingsPath);
        auto candidate=DefaultBindings;
        for(int i=0;i<8;++i) {
            if(!(file>>candidate[i]) || !bindable(candidate[i],i))return;
            for(int j=0;j<i;++j)if(candidate[i]==candidate[j])return;
        }
        bindings=candidate;
        std::string speedTag;int savedSpeed=10;
        if(file>>speedTag>>savedSpeed && speedTag=="speedTenths" && savedSpeed>=1 && savedSpeed<=10)speedTenths=savedSpeed;
        int value;while(file>>speedTag>>value){if(speedTag=="crownUnlocked")renderer.crownUnlocked=value==1;if(speedTag=="crownVisible")renderer.crownVisible=value==1;}
    }
    void saveBindings() {
        if(smoke)return;
        const auto temporary=std::filesystem::path(bindingsPath.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");
        std::ofstream file(temporary);
        for(auto key:bindings)file<<key<<'\n';
        file<<"speedTenths "<<speedTenths<<'\n';
        file<<"crownUnlocked "<<renderer.crownUnlocked<<'\n'<<"crownVisible "<<renderer.crownVisible<<'\n';
        file.close();
        if(!file || !MoveFileExW(temporary.c_str(),bindingsPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            settingsMessage=L"设置已生效，但保存失败；请检查数据目录的写入权限与剩余空间。";
            std::error_code error;std::filesystem::remove(temporary,error);
        }
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
    void pauseForFocus(HWND window) {
        background=true;
        if(replay.active)replay.paused=true;
        clearInput();menuPointer.reset();
        if(!importing && started && !intro && !menu)openMenu();
        InvalidateRect(window,nullptr,FALSE);
    }
    void saveWindow() {
        if(smoke||!windowStateReady||bindingsPath.empty())return;
        if(!writeWindowState(bindingsPath.parent_path()/L"window.ini",windowState))
            settingsMessage=L"窗口位置未能保存，请检查数据目录权限。";
    }
    void back() {
        if(rebinding>=0){rebinding=-1;settingsMessage=L"已取消改键。";}
        else if(page==Page::Help)openMenu(Page::Settings);
        else if(page!=Page::Main)openMenu();
        else resume();
    }
    void activateMenu(HWND window) {
        if(menuItem==0){if(started)resume();else openMenu(Page::Levels);}
        if(menuItem==1)openMenu(Page::Settings);
        if(menuItem==2)openMenu(Page::Levels);
        if(menuItem==3&&stopRecording()){saveWindow();DestroyWindow(window);}
        if(menuItem==4)toggleRecording();
        if(menuItem==5)importReplay(window);
    }
    void activateSetting(HWND window=nullptr) {
        if(settingItem<8){rebinding=settingItem;settingsMessage=L"请按新键（传送门也可使用鼠标左右键）；Esc 取消。";}
        else if(settingItem==8){bindings=DefaultBindings;settingsMessage=L"已恢复默认键位。";saveBindings();clearInput();}
        else if(settingItem==10){
            speedTenths=speedTenths%10+1;gameClock=replayClock=0;
            settingsMessage=L"运行速度："+speedText()+L"；每个逻辑帧保持不变。";saveBindings();
        }
        else if(settingItem==11&&window)toggleFullscreen(window);
        else if(settingItem==12)openHelp();
        else if(settingItem==13){renderer.crownVisible=!renderer.crownVisible;saveBindings();}
        else back();
    }
    bool intro=true;
    static constexpr int LevelIntroDuration=150;
    int levelIntroTicks=-1;
    unsigned suppressedKey=256;
    bool levelIntroActive() const { return levelIntroTicks>=0; }
    void beginLevelIntro() { levelIntroTicks=0;tutorialActive=false;tutorialPending=game.level().id==0&&game.level().editorJson.empty()&&!replay.active&&!recorder.active;clearInput(); }
    void endLevelIntro() {
        levelIntroTicks=-1;clearInput();
        if(tutorialPending){tutorialPending=false;tutorialActive=true;tutorialPaused=false;tutorialPractice=false;tutorial.reset(0);renderer.draw(tutorial.scene,true,true);}
    }
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
    void leaveIntro() { intro=false; openMenu(); }
    void drawIntro(HDC dc) const {
        fill(dc,{310,30,690,135},RGB(15,21,34));
        label(dc,{310,35,690,125},L"Por2D",68,RGB(235,242,255));
        fill(dc,{250,470,750,570},RGB(15,21,34));
        label(dc,{250,475,750,525},L"按空格跳过",30,RGB(255,215,100));
        label(dc,{250,530,750,565},L"Level 0 配有实时新手教学",18,RGB(150,173,201));
    }
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
            changingWindowMode=true;
            SetWindowLongPtrW(window,GWL_STYLE,savedStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,
                monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
                SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        } else {
            changingWindowMode=true;
            SetWindowLongPtrW(window,GWL_STYLE,savedStyle);
            if(smoke) placement.showCmd=SW_HIDE;
            SetWindowPlacement(window,&placement);
            SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
        fullscreen=!fullscreen;changingWindowMode=false;InvalidateRect(window,nullptr,FALSE);
    }
    void startSelected() {
        if(!stopRecording())return;
        replay.active=false;
        game=newGame(levels[selected]); menu=false; started=true; clearInput();
        beginLevelIntro();
        renderer.draw(game,debug,grid);
    }
    void menuClick(HWND window, por2::Vec2 point) {
        if(page==Page::Help){if(contains({375,545,625,585},point))back();return;}
        if(page==Page::Main){for(int i=0;i<6;++i)if(contains(menuButton(i),point)){menuItem=i;activateMenu(window);return;}return;}
        if(page==Page::Settings){
            for(int i=0;i<8;++i)if(contains(bindingRow(i),point)){settingItem=i;activateSetting();return;}
            if(contains(SpeedButton,point)){settingItem=10;activateSetting();return;}
            if(contains(FullscreenButton,point)){settingItem=11;activateSetting(window);return;}
            if(contains(ResetButton,point)){settingItem=8;activateSetting();}
            if(contains(HelpButton,point)){settingItem=12;activateSetting();}
            if(contains(SettingsBack,point))back();
            if(contains(CrownButton,point)){settingItem=13;activateSetting();}
            return;
        }
        const int first=(selected/15)*15;
        if(contains(MapEditorButton,point)){openMapEditor(window);return;}
        if(contains(MapFolderButton,point)){openMapsFolder(window);return;}
        if(contains(MapRefreshButton,point)){refreshMaps(window);return;}
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i)
            if(contains(card(i-first),point)) { selected=i; startSelected(); return; }
        if(contains({210,480,390,526},point)) selected=selected>=15?selected-15:static_cast<int>(levels.size())-1;
        else if(contains({410,480,590,526},point)) selected=(first+15)%levels.size();
        else if(contains({610,480,790,526},point)) openMenu();
    }
    void drawMenu(HDC dc) {
        if(page==Page::Help){drawHelp(dc);return;}
        fill(dc,{0,0,1000,600},RGB(15,21,34));
        if(page==Page::Main){
            label(dc,{60,35,940,100},started?L"游戏已暂停":L"Por2D / 主菜单",38,RGB(235,242,255));
            if(started)label(dc,{60,105,940,145},utf8(game.level().name),22,RGB(150,173,201));
            const std::array<std::wstring,6> items{started?L"继续游戏":L"开始游戏",L"设置",L"关卡选择",L"退出游戏",recorder.active||recorder.pending?L"停止录制 (F10)":L"录制 (F10)",L"回放 (F6)"};
            for(int i=0;i<6;++i){
                const bool disabled=i==4&&!started;
                fill(dc,menuButton(i),disabled?RGB(25,33,46):buttonColor(menuButton(i),i==menuItem));
                label(dc,menuButton(i),items[i],i>=4?21:25,disabled?RGB(100,112,128):RGB(240,245,255));
            }
            label(dc,{20,540,980,580},recordingMessage.empty()?L"录制从本关起点开始 · 回放可导入操作脚本":recordingMessage,17,RGB(150,173,201));return;
        }
        if(page==Page::Settings){
            label(dc,{60,20,940,75},L"设置",34,RGB(235,242,255));
            for(int i=0;i<8;++i){fill(dc,bindingRow(i),buttonColor(bindingRow(i),i==settingItem));label(dc,bindingRow(i),ActionNames[i]+L"    "+(rebinding==i?L"[ 等待输入… ]":keyName(bindings[i])),21,RGB(240,245,255));}
            fill(dc,ResetButton,buttonColor(ResetButton,settingItem==8));
            fill(dc,SpeedButton,buttonColor(SpeedButton,settingItem==10));
            label(dc,SpeedButton,L"运行速度："+speedText(),21,RGB(240,245,255));
            fill(dc,FullscreenButton,buttonColor(FullscreenButton,settingItem==11));
            label(dc,FullscreenButton,fullscreen?L"全屏：开（切换窗口）":L"全屏：关（切换全屏）",20,RGB(240,245,255));
            fill(dc,SettingsBack,buttonColor(SettingsBack,settingItem==9));
            fill(dc,HelpButton,buttonColor(HelpButton,settingItem==12));
            label(dc,ResetButton,L"恢复默认",21,RGB(240,245,255));label(dc,SettingsBack,L"返回菜单 (Esc)",19,RGB(240,245,255));
            label(dc,HelpButton,L"帮助 / 新手教程",19,RGB(240,245,255));
            fill(dc,CrownButton,buttonColor(CrownButton,settingItem==13));
            label(dc,CrownButton,renderer.crownVisible?L"皇冠：显示（通关 Level 14 解锁）":L"皇冠：隐藏",16,RGB(240,245,255));
            label(dc,{20,570,980,599},settingsMessage,14,RGB(255,215,100));return;
        }
        label(dc,{60,25,940,82},L"Por2D  /  选择关卡",36,RGB(235,242,255));
        label(dc,{60,85,940,120},L"点击关卡开始  ·  方向键 / Enter 开始  ·  F6 导入脚本",18,RGB(150,173,201));
        const int first=(selected/15)*15;
        for(int i=first;i<std::min(first+15,static_cast<int>(levels.size()));++i) {
            RECT r=card(i-first); fill(dc,r,buttonColor(r,i==selected));
            const auto level=levelById(levels[i]);
            const bool campaign=i<static_cast<int>(por2::Campaign.size());
            const std::wstring prefix=campaign?L"Level "+std::to_wstring(i)+L"  ":
                L"自定义  ";
            label(dc,r,prefix+utf8(level.name),20,levelTextColor(i),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        for(const RECT r : {RECT{210,480,390,526},RECT{410,480,590,526},RECT{610,480,790,526}})
            fill(dc,r,buttonColor(r));
        label(dc,{210,480,390,526},L"上一页",20,RGB(130,194,255));
        label(dc,{410,480,590,526},L"下一页",20,RGB(130,194,255));
        label(dc,{610,480,790,526},L"返回菜单 (Esc)",20,RGB(210,221,238));
        fill(dc,MapEditorButton,buttonColor(MapEditorButton));
        label(dc,MapEditorButton,L"打开地图编辑器 (Ctrl+E)",18,RGB(210,221,238));
        fill(dc,MapFolderButton,buttonColor(MapFolderButton));fill(dc,MapRefreshButton,buttonColor(MapRefreshButton));
        label(dc,MapFolderButton,L"打开地图文件夹 (Ctrl+O)",18,RGB(210,221,238));
        label(dc,MapRefreshButton,L"刷新列表 (F5)",18,RGB(210,221,238));
        label(dc,{20,574,980,599},L"第 "+std::to_wstring(selected/15+1)+L" / "+std::to_wstring((levels.size()+14)/15)+L" 页 · "+mapMessage,14,RGB(139,157,183));
    }

    void update(HWND window) {
        if (importing) return;
        if(background&&!smoke)return;
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
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
                SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(500,400));
                if(!intro)throw std::runtime_error("title screen must require Space");
                SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
                SendMessageW(window,WM_KEYUP,VK_SPACE,0);
                if(intro || !menu || started || page!=Page::Main) throw std::runtime_error("intro did not open main menu");
                SendMessageW(window,WM_KEYDOWN,VK_F10,0);
                if(!recordingMessage.empty()||recorder.active)throw std::runtime_error("disabled recording left a persistent message");
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
                if(!tutorialActive)throw std::runtime_error("level 0 did not start live tutorial");
                for(int stage=0;stage<6;++stage){
                    tutorial.reset(stage);smoke=false;
                    for(int i=0;i<(stage>=4?170:80);++i)update(window);
                    smoke=true;
                    if(game.player().body.position!=introPosition||!shots.empty()||recorder.active)throw std::runtime_error("tutorial changed live game");
                    if(stage==4){
                        smoke=false;while(tutorial.frame<3*por2::Tutorial::MappingPhaseFrames-10)update(window);smoke=true;
                        if(tutorial.phase!=2||tutorial.scene.player().body.direction!=por2::Direction::Down)throw std::runtime_error("reversed portal lesson did not invert the actor");
                    }
                    SendMessageW(window,WM_PAINT,0,0);
                    if(stage==0){
                        tutorial.reset(0);
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(850,140));
                        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(850,140));
                        if(!tutorialPractice)throw std::runtime_error("practice button did not enable control");
                        const auto initial=tutorial.scene.player().body.position;
                        const int animationFrame=tutorial.frame;
                        SendMessageW(window,WM_KEYDOWN,bindings[1],0);
                        smoke=false;for(int i=0;i<8;++i)update(window);smoke=true;
                        SendMessageW(window,WM_KEYUP,bindings[1],0);
                        if(tutorial.scene.player().body.position.x<=initial.x||tutorial.frame!=animationFrame||game.player().body.position!=introPosition)throw std::runtime_error("practice movement or isolation failed");
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(260,389));
                        smoke=false;update(window);smoke=true;
                        if(!tutorial.attempted||!tutorial.accepted)throw std::runtime_error("practice mouse shot failed");
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(490,560));
                        const auto paused=tutorial.scene.player().body.position;
                        smoke=false;update(window);smoke=true;
                        if(!tutorialPaused||tutorial.scene.player().body.position!=paused)throw std::runtime_error("practice pause failed");
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(850,140));
                        if(tutorialPractice||tutorialPaused||!shots.empty())throw std::runtime_error("return to automatic demo failed");
                    }
                    if(stage==2){
                        const int frozen=tutorial.frame;
                        SendMessageW(window,WM_KEYDOWN,'P',0);
                        SendMessageW(window,WM_KEYDOWN,'P',static_cast<LPARAM>(1LL<<30));
                        SendMessageW(window,WM_KEYUP,'P',0);
                        smoke=false;update(window);smoke=true;
                        if(!tutorialPaused||tutorial.frame!=frozen)throw std::runtime_error("tutorial pause failed");
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(490,560));
                        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(490,560));
                        smoke=false;update(window);smoke=true;
                        if(tutorialPaused||tutorial.frame==frozen)throw std::runtime_error("tutorial resume failed");
                        const int pausedFrame=tutorial.frame;openMenu(Page::Settings);
                        smoke=false;update(window);smoke=true;
                        if(tutorial.frame!=pausedFrame)throw std::runtime_error("settings did not pause teaching scene");
                        resume();
                    }
                }
                SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
                SendMessageW(window,WM_KEYDOWN,VK_SPACE,static_cast<LPARAM>(1LL<<30));
                if(tutorialActive||keys[VK_SPACE])throw std::runtime_error("tutorial completion leaked input");
                SendMessageW(window,WM_KEYUP,VK_SPACE,0);
                beginLevelIntro();
                levelIntroTicks=40;
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window, WM_KEYDOWN, 'D', 0);
                SendMessageW(window, WM_KEYDOWN, 'D', static_cast<LPARAM>(1LL<<30));
                if (levelIntroActive() || keys['D'])
                    throw std::runtime_error("intro skip leaked into movement");
                finishTutorial();
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
                const auto position=game.player().body.position;
                smoke=false;update(window);smoke=true;
                if(!menu||!background||game.player().body.position!=position)
                    throw std::runtime_error("focus loss did not pause gameplay");
                SendMessageW(window,WM_SETFOCUS,0,0);
                smoke=false;update(window);smoke=true;
                if(!menu||background||game.player().body.position!=position)
                    throw std::runtime_error("focus return resumed gameplay automatically");
                resume();SendMessageW(window,WM_SIZE,SIZE_MINIMIZED,0);
                if(!menu||!background)throw std::runtime_error("minimize did not pause gameplay");
                SendMessageW(window,WM_SETFOCUS,0,0);
            }
            if (smokeTicks == 13) {
                SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(962,30));
                SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(962,30));
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
                menuClick(window,{500,510});
                if(page!=Page::Help||helpText.find(L"射击必须命中门的中间方块")==std::wstring::npos)
                    throw std::runtime_error("help did not read tutorial markdown");
                SendMessageW(window,WM_PAINT,0,0);
                SendMessageW(window,WM_KEYDOWN,VK_NEXT,0);
                if(helpScroll<=0)throw std::runtime_error("help scrolling failed");
                SendMessageW(window,WM_KEYDOWN,VK_END,0);
                if(helpScroll!=std::max(0,helpHeight-400))throw std::runtime_error("help cannot reach end of tutorial");
                SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
                if(page!=Page::Settings)throw std::runtime_error("help back did not restore settings");
                settingItem=0;activateSetting();
                SendMessageW(window,WM_KEYDOWN,'D',0);SendMessageW(window,WM_KEYUP,'D',0);
                if(rebinding!=0 || bindings[0]!='A')throw std::runtime_error("duplicate binding accepted");
                SendMessageW(window,WM_KEYDOWN,'J',0);SendMessageW(window,WM_KEYUP,'J',0);
                if(rebinding!=-1 || bindings[0]!='J' || keys['J'])throw std::runtime_error("rebind failed or leaked input");
                menuClick(window,{300,450});
                if(speedTenths!=1)throw std::runtime_error("speed setting click failed");
                bindingsPath=std::filesystem::temp_directory_path()/("por2-bindings-test-"+std::to_string(GetCurrentProcessId())+".ini");
                renderer.crownUnlocked=true;menuClick(window,{500,550});
                if(renderer.crownVisible)throw std::runtime_error("crown setting did not hide crown");
                smoke=false;saveBindings();smoke=true;
                renderer.crownUnlocked=false;renderer.crownVisible=true;
                bindings=DefaultBindings;speedTenths=10;loadBindings(bindingsPath);
                std::filesystem::remove(bindingsPath);
                if(bindings[0]!='J'||speedTenths!=1)throw std::runtime_error("bindings or speed did not persist");
                if(!renderer.crownUnlocked||renderer.crownVisible)throw std::runtime_error("crown unlock and visibility did not persist");
                renderer.crownUnlocked=false;renderer.crownVisible=true;
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
                openMenu(Page::Settings);
                menuClick(window,{660,450});
                if(!EqualRect(&before,&windowState.bounds))throw std::runtime_error("fullscreen overwrote remembered bounds");
                const RECT r=viewport(window);
                auto point=logicalPoint(window,(r.left+r.right)/2,(r.top+r.bottom)/2);
                if(!fullscreen || !point || std::abs(point->x-500)>1 || std::abs(point->y-300)>1)
                    throw std::runtime_error("fullscreen coordinate mapping failed");
                settingItem=11;activateSetting(window);
                RECT after{}; GetWindowRect(window,&after);
                if(!EqualRect(&before,&after)) throw std::runtime_error("window bounds not restored");
                openMenu(Page::Levels);menuClick(window,{700,500});
                if(fullscreen)throw std::runtime_error("level selection retained fullscreen button");
                openMenu();
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
                resume();SendMessageW(window,WM_KILLFOCUS,0,0);
                smoke=false;update(window);smoke=true;
                SendMessageW(window,WM_SETFOCUS,0,0);
                smoke=false;update(window);smoke=true;
                if(!menu||recorder.script.totalFrames!=6||!recorder.active)
                    throw std::runtime_error("focus pause advanced or stopped recording");
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
                const auto mapCount=customLevels.size();const auto activeMap=game.level().editorJson;
                menuClick(window,{200,550});
                if(mapMessage.find(L"编辑器已打开")==std::wstring::npos)throw std::runtime_error("map editor entry could not locate packaged assets");
                menuClick(window,{510,550});
                if(!std::filesystem::is_directory(userMapsFolder()))throw std::runtime_error("map folder button failed");
                const auto imported=userMapsFolder()/L"窗口检查地图.json";
                std::filesystem::copy_file(resourceFolder()/L"levels"/L"example.json",imported);
                SendMessageW(window,WM_KEYDOWN,VK_F5,0);SendMessageW(window,WM_KEYUP,VK_F5,0);
                if(customLevels.size()!=mapCount+1||game.level().editorJson!=activeMap)
                    throw std::runtime_error("F5 did not discover user map or reset active game");
                std::filesystem::remove(imported);menuClick(window,{810,550});
                if(customLevels.size()!=mapCount)throw std::runtime_error("refresh button retained removed map");
                if(menuButton(4).left!=menuRow(3).left||menuButton(5).right!=menuRow(3).right||menuButton(3).top<=menuButton(4).bottom)
                    throw std::runtime_error("record/replay menu layout failed");
                const auto remembered=windowState;
                SendMessageW(window,WM_SIZE,SIZE_MAXIMIZED,0);
                SendMessageW(window,WM_SIZE,SIZE_MINIMIZED,0);
                if(!windowState.maximized||!EqualRect(&remembered.bounds,&windowState.bounds))
                    throw std::runtime_error("minimizing lost window state");
                SendMessageW(window,WM_SIZE,SIZE_RESTORED,0);SendMessageW(window,WM_SETFOCUS,0,0);
                const auto baseScreenshot=screenshot;
                for(const int scale:{125,200}){
                    RECT large{0,0,1000*scale/100,600*scale/100};
                    AdjustWindowRect(&large,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE);
                    SetWindowPos(window,nullptr,0,0,large.right-large.left,large.bottom-large.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                    RECT client{};GetClientRect(window,&client);
                    if(client.right!=1000*scale/100||client.bottom!=600*scale/100)
                        throw std::runtime_error("high resolution client size failed");
                    if(!baseScreenshot.empty())screenshot=baseScreenshot+".scale"+std::to_string(scale);
                    openMenu(Page::Settings);SendMessageW(window,WM_PAINT,0,0);
                    openHelp();SendMessageW(window,WM_PAINT,0,0);
                    SendMessageW(window,WM_KEYDOWN,VK_END,0);
                    if(helpScroll!=std::max(0,helpHeight-400))throw std::runtime_error("scaled help scrolling failed");
                    openMenu(Page::Levels);SendMessageW(window,WM_PAINT,0,0);
                    const auto center=logicalPoint(window,client.right/2,client.bottom/2);
                    if(!center||std::abs(center->x-500)>1||std::abs(center->y-300)>1)
                        throw std::runtime_error("high resolution hit testing failed");
                }
                screenshot=baseScreenshot;
                if (!paintCount) throw std::runtime_error("window smoke: paint callback was never called");
                DestroyWindow(window);
                return;
            }
        }
        if(!menu&&tutorialActive){
            if(!tutorialPaused){
                if(tutorialPractice){
                    if(keys[bindings[4]]){tutorial.reset(tutorial.stage);clearInput();}
                    por2::InputFrame practice;
                    practice.movement={keys[bindings[0]],keys[bindings[1]],keys[bindings[2]]};
                    tutorial.movement=practice.movement;tutorial.scene.tick(practice);
                    for(const auto& shot:shots){tutorial.attempted=true;tutorial.accepted=tutorial.scene.shoot(shot);}
                    shots.clear();
                    tutorial.aim=menuPointer?std::optional<por2::Shot>{{previewPortal,*menuPointer}}:std::nullopt;
                }else tutorial.update();
            }
            renderer.draw(tutorial.scene,true,true,tutorialPractice&&!previewEnabled?std::nullopt:tutorial.aim,false,tutorial.stage==3);
            InvalidateRect(window,nullptr,FALSE);return;
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
        if(game.crowned()&&!renderer.crownUnlocked){renderer.crownUnlocked=true;saveBindings();}
        renderer.draw(game, debug, grid&&!hideReplayUi(), preview);
        const std::string nextTitle = game.finished()
            ? "Por2D - Completed! | Esc: menu | F11: fullscreen"
            : "Por2D - " + game.level().name + " | Esc: pause / menu | F11: fullscreen";
        if (nextTitle != title) { title = nextTitle; SetWindowTextW(window, utf8(title).c_str()); }
        InvalidateRect(window, nullptr, FALSE);
        if (smoke) SendMessageW(window, WM_PAINT, 0, 0);
    }
};

void checkMapCatalog() {
    const auto root=std::filesystem::temp_directory_path()/
        (L"por2-maps-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    const auto bundled=root/L"附带地图",user=root/L"用户地图";
    struct Cleanup {
        std::vector<std::filesystem::path> paths;
        ~Cleanup(){for(const auto& path:paths){std::error_code error;std::filesystem::remove(path,error);}}
    } cleanup{{bundled/L"same.json",user/L"same.json",user/L"broken.json",user,bundled,root}};
    std::filesystem::create_directories(bundled);std::filesystem::create_directories(user);
    const auto example=resourceFolder()/L"levels"/L"example.json";
    std::filesystem::copy_file(example,bundled/L"same.json");std::filesystem::copy_file(example,user/L"same.json");
    {std::ofstream file(user/L"broken.json");file<<"invalid map";}
    Application catalog(0);catalog.smoke=true;
    catalog.loadMaps(bundled,user);
    if(catalog.customLevels.size()!=2||catalog.mapWarnings.empty())throw std::runtime_error("two-source map loading/error isolation failed");
    catalog.selected=static_cast<int>(catalog.levels.size())-1;
    catalog.startSelected();const auto activeJson=catalog.game.level().editorJson;
    const auto position=catalog.game.player().body.position;
    catalog.loadMaps(bundled,user);
    if(catalog.customLevels.size()!=2||catalog.customPaths[catalog.selected-por2::Campaign.size()]!=user/L"same.json")
        throw std::runtime_error("map refresh duplicated maps or lost selection");
    std::filesystem::remove(user/L"same.json");std::filesystem::remove(user/L"broken.json");
    catalog.loadMaps(bundled,user);
    if(catalog.customLevels.size()!=1||!catalog.mapWarnings.empty()||catalog.game.level().editorJson!=activeJson||catalog.game.player().body.position!=position)
        throw std::runtime_error("refresh changed active map or retained removed files");
    catalog.loadMaps(bundled,bundled);
    if(catalog.customLevels.size()!=1)throw std::runtime_error("portable map directory was loaded twice");
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_GETMINMAXINFO:
        // Hidden CI desktops can be smaller than the off-screen sizes exercised by
        // the native-resolution rendering smoke test. Do not let the work area cap it.
        if(app->smoke){
            auto* limits=reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMaxTrackSize={4096,4096};
            return 0;
        }
        return DefWindowProcW(window,message,wparam,lparam);
    case WM_DPICHANGED: {
        MONITORINFO monitor{sizeof(MONITORINFO),{},{},0};
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) {
            const RECT bounds=app->fullscreen?monitor.rcMonitor:
                fitToWorkArea(*reinterpret_cast<const RECT*>(lparam),monitor.rcWork);
            SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,
                         bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
        }
        app->menuPointer.reset();InvalidateRect(window,nullptr,FALSE);return 0;
    }
    case WM_SIZE:
        if(wparam==SIZE_MINIMIZED)app->pauseForFocus(window);
        if(app->windowStateReady&&!app->fullscreen&&!app->changingWindowMode&&wparam!=SIZE_MINIMIZED)
            app->windowState.maximized=wparam==SIZE_MAXIMIZED;
        app->menuPointer.reset();InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_WINDOWPOSCHANGED:
        if(app->windowStateReady&&!app->fullscreen&&!app->changingWindowMode&&!IsIconic(window)&&!IsZoomed(window))
            GetWindowRect(window,&app->windowState.bounds);
        return DefWindowProcW(window,message,wparam,lparam);
    case WM_EXITSIZEMOVE:
        app->saveWindow();return 0;
    case WM_SETFOCUS:
        app->background=false;return 0;
    case WM_ACTIVATEAPP:
        if(!wparam)app->pauseForFocus(window);
        else app->background=false;
        return 0;
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
        if(message==WM_SYSKEYDOWN && key==VK_F4)return DefWindowProcW(window,message,wparam,lparam);
        if(app->intro){
            if(pressed&&!(lparam&(1LL<<30))){
                if(key==VK_SPACE){app->leaveIntro();app->suppressedKey=key;}
                else if(key==VK_F11)app->toggleFullscreen(window);
                InvalidateRect(window,nullptr,FALSE);
            }
            return 0;
        }
        if(pressed&&!(lparam&(1LL<<30))&&key==VK_F10&&app->rebinding<0){app->toggleRecording();InvalidateRect(window,nullptr,FALSE);return 0;}
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
        if(app->menu&&app->page==Application::Page::Help){
            if(pressed){
                if(key==VK_ESCAPE)app->back();
                if(key==VK_DOWN)app->scrollHelp(48);if(key==VK_UP)app->scrollHelp(-48);
                if(key==VK_NEXT)app->scrollHelp(360);if(key==VK_PRIOR)app->scrollHelp(-360);
                if(key==VK_HOME)app->scrollHelp(-app->helpHeight);if(key==VK_END)app->scrollHelp(app->helpHeight);
                InvalidateRect(window,nullptr,FALSE);
            }return 0;
        }
        if(app->tutorialActive&&!app->menu){
            if(app->tutorialPractice&&key!=VK_ESCAPE&&key!=VK_F11){
                if(key==VK_CONTROL)key=(lparam&(1LL<<24))?VK_RCONTROL:VK_LCONTROL;
                if(key<app->keys.size())app->keys[key]=pressed&&!app->tutorialPaused;
                if(pressed&&!(lparam&(1LL<<30))&&key==app->bindings[5])app->previewEnabled=!app->previewEnabled;
                if(pressed&&!app->tutorialPaused&&!(lparam&(1LL<<30)))for(int i=0;i<2;++i)if(key==app->bindings[6+i])app->keyboardShot(window,i);
                return 0;
            }
            if(pressed&&!(lparam&(1LL<<30))){
                if(key=='P'){app->tutorialPaused=!app->tutorialPaused;app->clearInput();}
                if(key==VK_SPACE||key==VK_RIGHT||key==VK_RETURN)app->tutorialStep(1);
                if(key==VK_LEFT)app->tutorialStep(-1);
                if(key==VK_ESCAPE)app->openMenu();
                if(key==VK_F11)app->toggleFullscreen(window);
                app->suppressedKey=key;InvalidateRect(window,nullptr,FALSE);
            }return 0;
        }
        if (app->levelIntroActive() && !app->menu && key!=VK_ESCAPE) {
            if (pressed && !(lparam & (1LL<<30))) {
                app->endLevelIntro();
                app->suppressedKey=key;
                InvalidateRect(window,nullptr,FALSE);
            }
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
                if(app->page==Application::Page::Levels){
                    if(key==VK_F5){app->refreshMaps(window);return 0;}
                    if(key=='E'&&(GetKeyState(VK_CONTROL)&0x8000)){app->openMapEditor(window);return 0;}
                    if(key=='O'&&(GetKeyState(VK_CONTROL)&0x8000)){app->openMapsFolder(window);return 0;}
                }
                if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_PRIOR||key==VK_NEXT||key==VK_RETURN)
                    app->mouseNavigation=false;
                if(app->page!=Application::Page::Levels){
                    int& item=app->page==Application::Page::Main?app->menuItem:app->settingItem;
                    const std::vector<int> order=app->page==Application::Page::Main?(app->started?std::vector<int>{0,1,2,4,5,3}:std::vector<int>{0,1,2,5,3}):std::vector<int>{0,1,2,3,4,5,6,7,10,11,8,12,13,9};
                    const int index=static_cast<int>(std::find(order.begin(),order.end(),item)-order.begin());
                    if(key==VK_UP)item=order[(index+order.size()-1)%order.size()];
                    if(key==VK_DOWN)item=order[(index+1)%order.size()];
                    if(app->page==Application::Page::Main&&(item==4||item==5)){if(key==VK_LEFT)item=4;if(key==VK_RIGHT)item=5;}
                    if(key==VK_RETURN){if(app->page==Application::Page::Main)app->activateMenu(window);else app->activateSetting(window);}
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
    case WM_MOUSEWHEEL:
        if(app->menu&&app->page==Application::Page::Help){app->scrollHelp(-GET_WHEEL_DELTA_WPARAM(wparam)*96/WHEEL_DELTA);InvalidateRect(window,nullptr,FALSE);}
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        app->menuPointer=logicalPoint(window,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        app->mouseNavigation=true;
        if(app->intro)return 0;
        if(!app->menu&&message==WM_LBUTTONDOWN&&app->menuPointer&&contains(GearButton,*app->menuPointer)){
            app->openMenu();InvalidateRect(window,nullptr,FALSE);return 0;
        }
        if(app->tutorialActive&&!app->menu){
            if(app->tutorialPractice&&!app->tutorialPaused&&app->menuPointer&&app->menuPointer->y>160&&app->menuPointer->y<450){
                for(int i=0;i<2;++i)if(app->bindings[6+i]==(message==WM_LBUTTONDOWN?VK_LBUTTON:VK_RBUTTON)){app->previewPortal=i;app->shots.push_back({i,*app->menuPointer});}
            }
            if(message==WM_LBUTTONDOWN&&app->menuPointer){const auto p=*app->menuPointer;
                if(contains(LessonPractice,p))app->togglePractice();
                if(contains(LessonPrevious,p))app->tutorialStep(-1);
                if(contains(LessonSettings,p))app->openMenu(Application::Page::Settings);
                if(contains(LessonPause,p)){app->tutorialPaused=!app->tutorialPaused;app->clearInput();}
                if(contains(LessonSkip,p))app->finishTutorial();
                if(contains(LessonNext,p))app->tutorialStep(1);
                InvalidateRect(window,nullptr,FALSE);
            }return 0;
        }
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
        app->pauseForFocus(window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        ++app->paintCount;
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(window, &paint);
        RECT client{};GetClientRect(window,&client);
        const RECT view=viewport(window);
        if(client.right<=0||client.bottom<=0||view.right<=view.left||view.bottom<=view.top){EndPaint(window,&paint);return 0;}
        HDC dc=CreateCompatibleDC(target);
        HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);
        auto oldBitmap=SelectObject(dc,bitmap);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = por2::WindowWidth;
        info.bmiHeader.biHeight = -por2::WindowHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        // Scale only the game pixels. GDI rasterizes UI and TrueType fonts directly
        // into the full-resolution back buffer through this logical coordinate map.
        fill(dc,client,RGB(0,0,0));SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,view.left,view.top,view.right-view.left,view.bottom-view.top,
                      0,0,1000,600,app->renderer.pixels(),&info,DIB_RGB_COLORS,SRCCOPY);
        const int drawingState=SaveDC(dc);
        SetMapMode(dc,MM_ANISOTROPIC);
        SetWindowExtEx(dc,1000,600,nullptr);
        SetViewportExtEx(dc,view.right-view.left,view.bottom-view.top,nullptr);
        SetViewportOrgEx(dc,view.left,view.top,nullptr);
        IntersectClipRect(dc,0,0,1000,600);
        if(app->intro) app->drawIntro(dc);
        else if(app->menu) app->drawMenu(dc);
        else if(app->levelIntroActive()) app->drawLevelIntro(dc);
        else if(app->tutorialActive)app->drawTutorial(dc);
        else if (app->game.finished()&&!app->hideReplayUi()) {
            const auto completed=L"已完成！  "+keyName(app->bindings[4])+L"：重新开始    Esc：菜单";
            label(dc,{0,30,1000,70},completed,20,RGB(255,255,255));
        }
        if(!app->menu && !app->intro && !app->levelIntroActive() && !app->tutorialActive && !app->hideReplayUi()) {
            const auto level=app->replay.active?(app->replay.script.customLevel?*app->replay.script.customLevel:por2::makeLevel(app->replay.initialLevel)):app->game.level();
            const auto entry=std::find(por2::Campaign.begin(),por2::Campaign.end(),level.id);
            const std::string prefix=!level.editorJson.empty()?"自定义":entry!=por2::Campaign.end()?"Level"+std::to_string(entry-por2::Campaign.begin()):"实验地图";
            const std::string caption = prefix + "  " + level.name;
            const std::wstring wide=utf8(caption);
            if(level.id==0&&!app->game.finished())
                label(dc,{200,510,800,550},L"到达目标点后按 "+keyName(app->bindings[3])+L" 过关",22,RGB(190,190,190));
            label(dc,{690,548,975,588},wide,18,RGB(220,230,245),DT_RIGHT|DT_BOTTOM|DT_SINGLELINE);
        }
        if(!app->menu && !app->intro) app->drawReplay(dc);
        if(!app->menu&&!app->intro&&!app->tutorialActive&&!app->replay.active&&!app->recordingMessage.empty()){
            fill(dc,{10,10,925,44},RGB(15,21,34));
            label(dc,{10,10,925,44},app->recordingMessage+(app->recorder.active?L" · "+std::to_wstring(app->recorder.script.totalFrames)+L" 帧":L""),17,RGB(255,190,120));
        }
        if(!app->intro&&!app->menu)app->drawGear(dc);
        RestoreDC(dc,drawingState);
        // Present the completed UI, scene and black bars together to avoid flicker.
        BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,oldBitmap);
        if(app->smoke && (app->menu || app->intro || app->levelIntroActive() || app->tutorialActive || app->replay.active) && !app->screenshot.empty()) {
            info.bmiHeader.biWidth=client.right;info.bmiHeader.biHeight=-client.bottom;
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(client.right)*client.bottom);
            if(!GetDIBits(dc,bitmap,0,client.bottom,pixels.data(),&info,DIB_RGB_COLORS))
                app->failure="menu screenshot failed";
            else {
                BITMAPFILEHEADER header{}; header.bfType=0x4d42;
                header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
                header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size()*4);
                std::ofstream file(app->screenshot+(app->intro?".intro.bmp":app->menu&&app->page==Application::Page::Help?".help.bmp":app->menu&&app->page==Application::Page::Settings?".settings.bmp":app->menu&&app->page==Application::Page::Levels?".levels.bmp":app->tutorialActive&&!app->menu?".tutorial"+std::to_string(app->tutorial.stage)+".bmp":app->hideReplayUi()?".replay-hidden.bmp":app->replay.active?".replay.bmp":app->levelIntroActive()?".level-intro.bmp":".menu.bmp"),std::ios::binary);
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
        if(app->stopRecording()){app->saveWindow();DestroyWindow(window);}
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
    enableDpiAwareness();
    if(smoke){checkDataFolders();checkMapCatalog();}
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
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT bounds{0, 0, por2::WindowWidth, por2::WindowHeight};
    AdjustWindowRect(&bounds, style, FALSE);
    HWND window = CreateWindowExW(0, type.lpszClassName, L"Por2D", style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, &app);
    if (!window) throw std::runtime_error("window creation failed");
    if(smoke) {
        RECT resized{0,0,800,480};
        SendMessageW(window,WM_DPICHANGED,MAKELONG(144,144),reinterpret_cast<LPARAM>(&resized));
        RECT client{};GetClientRect(window,&client);
        const auto center=logicalPoint(window,client.right/2,client.bottom/2);
        if(!center || std::abs(center->x-500)>2 || std::abs(center->y-300)>2 ||
           logicalPoint(window,-1,-1) || client.right>=1000)
            throw std::runtime_error("DPI resize/input mapping failed");
        RECT original{0,0,1000,600};AdjustWindowRect(&original,style,FALSE);
        SetWindowPos(window,nullptr,0,0,original.right-original.left,original.bottom-original.top,
                     SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    bool restoreMaximized=false;
    if(!smoke) {
        MONITORINFO monitor{sizeof(MONITORINFO),{},{},0};
        RECT bounds{};GetWindowRect(window,&bounds);
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) {
            using GetDpi=UINT(WINAPI*)(HWND);
            const auto getDpi=reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
            const UINT dpi=getDpi?getDpi(window):96;
            bounds.right=bounds.left+MulDiv(bounds.right-bounds.left,dpi,96);
            bounds.bottom=bounds.top+MulDiv(bounds.bottom-bounds.top,dpi,96);
            bounds=fitToWorkArea(bounds,monitor.rcWork);
            SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,
                         bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
        }
        if(const auto saved=readWindowState(app.bindingsPath.parent_path()/L"window.ini")) {
            bounds=saved->bounds;
            if(GetMonitorInfoW(MonitorFromRect(&bounds,MONITOR_DEFAULTTONEAREST),&monitor)) {
                bounds=fitToWorkArea(bounds,monitor.rcWork);
                SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,
                             bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
                restoreMaximized=saved->maximized;
            }
        }
    }
    GetWindowRect(window,&app.windowState.bounds);
    app.windowState.maximized=restoreMaximized;app.windowStateReady=true;
    if(!app.mapWarnings.empty())MessageBoxW(window,app.mapWarnings.c_str(),L"部分自定义地图未加载",MB_OK|MB_ICONWARNING);
    ShowWindow(window,smoke?SW_HIDE:restoreMaximized?SW_SHOWMAXIMIZED:SW_SHOW);
    if(fullscreen)app.toggleFullscreen(window);
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
    if (smoke) std::cout << "PASS native window: focus/minimize pause, window memory, native-resolution UI at 125/200%, user/bundled map refresh, game-local data, DPI resize/input, Space title, gear menu, scrollable help, six live tutorial scenes/pause/skip, bindings/speed, recording/replay, maps and fullscreen\n";
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
        if (!headless) {
            try {return runWindow(level, windowSmoke, screenshot,direct,fullscreen,script);}
            catch(const std::exception& error) {
                if(!windowSmoke)MessageBoxW(nullptr,utf8(error.what()).c_str(),L"Por2D 启动或运行失败",MB_OK|MB_ICONERROR);
                throw;
            }
        }
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
