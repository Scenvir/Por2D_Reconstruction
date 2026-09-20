#pragma once
#include "game.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <limits>
#include <locale>

namespace por2 {
struct ScriptAction {
    InputFrame input;
    int frames = 1;
    std::string text;
};
struct ReplayScript {
    std::vector<ScriptAction> actions;
    int level = -1;
    int totalFrames = 0;
    std::optional<Level> customLevel;
    static ReplayScript parse(std::istream& stream) {
        ReplayScript result;
        std::string line;
        int number = 0;
        while (std::getline(stream,line)) {
            ++number;
            if (number==1 && line.compare(0,3,"\xEF\xBB\xBF")==0) line.erase(0,3);
            const auto first=line.find_first_not_of(" \t\r");
            if(first!=std::string::npos&&line.compare(first,4,"map ")==0){
                if(!result.actions.empty()||result.level!=-1||result.customLevel)throw std::runtime_error("Map must precede replay actions");
                result.customLevel=parseEditorLevel(line.substr(first+4));continue;
            }
            line=line.substr(0,line.find('#'));
            std::istringstream row(line);
            row.imbue(std::locale::classic());
            std::string op,extra;
            if (!(row>>op)) continue;
            auto fail=[&]() { throw std::runtime_error("Script line "+std::to_string(number)+": invalid command or arguments"); };
            if (op=="level") {
                if (!result.actions.empty() || result.level!=-1 || result.customLevel || !(row>>result.level) ||
                    result.level<0 || result.level>16 || (row>>extra)) fail();
                continue;
            }
            ScriptAction action;
            if (op=="f") {
                int flags=0,count=0;
                if(!(row>>action.frames>>flags>>count)||action.frames<1||action.frames>1000000||flags<0||flags>63||count<0||count>256)fail();
                action.input.movement={bool(flags&1),bool(flags&2),bool(flags&4)};
                action.input.restart=flags&8;action.input.useExit=flags&16;action.input.skip=flags&32;
                for(int i=0;i<count;++i){Shot shot;if(!(row>>shot.portal>>shot.target.x>>shot.target.y)||shot.portal<0||shot.portal>1||!finite(shot.target))fail();action.input.shots.push_back(shot);}
            } else if (op=="s") {
                Shot shot;
                if (!(row>>shot.portal>>shot.target.x>>shot.target.y) ||
                    shot.portal<0 || shot.portal>1 || !finite(shot.target)) fail();
                action.input.shots.push_back(shot);
            } else {
                if (!(row>>action.frames) || action.frames<1 || action.frames>1000000) fail();
                if (op=="a" || op=="d" || op=="w" || op=="aw" || op=="dw") {
                    action.input.movement={op.find('a')!=op.npos,op.find('d')!=op.npos,op.find('w')!=op.npos};
                } else if (op=="e") action.input.useExit=true;
                else if (op=="r") action.input.restart=true;
                else if (op!="z") fail();
            }
            if ((row>>extra) || result.totalFrames>1000000-action.frames) fail();
            action.text=line;
            result.totalFrames+=action.frames;
            result.actions.push_back(action);
        }
        if (stream.bad()) throw std::runtime_error("Cannot read replay script");
        if (result.actions.empty()) throw std::runtime_error("Replay script is empty");
        return result;
    }
    static ReplayScript load(const std::filesystem::path& path) {
        std::ifstream stream(path);
        if (!stream) throw std::runtime_error("Cannot open replay script");
        return parse(stream);
    }
};
// Record complete logical inputs, including simultaneous movement and multiple shots.
struct Recorder {
    ReplayScript script;
    bool active=false, pending=false;
    void start(int level) {script={};script.level=level;active=true;pending=false;}
    void start(const Level& level){start(level.id);if(!level.editorJson.empty()){script.level=-1;script.customLevel=level;}}
    static std::string encode(const InputFrame& input) {
        std::ostringstream row;row.imbue(std::locale::classic());
        const int flags=input.movement.left+2*input.movement.right+4*input.movement.jump+8*input.restart+16*input.useExit+32*input.skip;
        row<<flags<<' '<<input.shots.size()<<std::setprecision(std::numeric_limits<double>::max_digits10);
        for(const auto& shot:input.shots)row<<' '<<shot.portal<<' '<<shot.target.x<<' '<<shot.target.y;
        return row.str();
    }
    void capture(const InputFrame& input) {
        if(!active)return;
        const auto text=encode(input);
        if(!script.actions.empty()&&script.actions.back().text==text)++script.actions.back().frames;
        else script.actions.push_back({input,1,text});
        ++script.totalFrames;pending=true;
        if(script.totalFrames>=1000000)active=false;
    }
    std::string text() const {
        std::ostringstream output;output.imbue(std::locale::classic());
        output<<"# Por2D recorded logical inputs\n";
        if(script.customLevel)output<<"map "<<script.customLevel->editorJson<<'\n';else output<<"level "<<script.level<<'\n';
        for(const auto& action:script.actions)output<<"f "<<action.frames<<' '<<action.text<<'\n';
        return output.str();
    }
};
struct Replay {
    ReplayScript script;
    bool active=false, paused=false, completed=false, cleared=false;
    int initialLevel=0, frame=0, within=0, lastAction=-1;
    std::size_t action=0;
    void start(ReplayScript value,int fallback,Game& game) {
        if(!value.customLevel&&value.level<0&&!game.level().editorJson.empty())value.customLevel=game.level();
        script=std::move(value);
        initialLevel=script.level>=0?script.level:fallback;
        restart(game);
    }
    void restart(Game& game) {
        game=script.customLevel?Game(*script.customLevel):Game(initialLevel);
        active=true; paused=false; completed=false; cleared=false;
        frame=within=0; action=0; lastAction=-1;
    }
    void step(Game& game) {
        if (!active || completed) return;
        lastAction=static_cast<int>(action);
        const int previous=game.level().id;
        game.tick(script.actions[action].input);
        ++frame;
        if (++within==script.actions[action].frames) { ++action; within=0; }
        cleared=game.level().id!=previous || game.finished();
        completed=cleared || action==script.actions.size();
        if (completed) paused=true;
    }
};
} // namespace por2
