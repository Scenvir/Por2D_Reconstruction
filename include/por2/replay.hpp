#pragma once
#include "game.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
    static ReplayScript parse(std::istream& stream) {
        ReplayScript result;
        std::string line;
        int number = 0;
        while (std::getline(stream,line)) {
            ++number;
            if (number==1 && line.compare(0,3,"\xEF\xBB\xBF")==0) line.erase(0,3);
            line=line.substr(0,line.find('#'));
            std::istringstream row(line);
            std::string op,extra;
            if (!(row>>op)) continue;
            auto fail=[&]() { throw std::runtime_error("Script line "+std::to_string(number)+": invalid command or arguments"); };
            if (op=="level") {
                if (!result.actions.empty() || result.level!=-1 || !(row>>result.level) ||
                    result.level<0 || result.level>16 || (row>>extra)) fail();
                continue;
            }
            ScriptAction action;
            if (op=="s") {
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
struct Replay {
    ReplayScript script;
    bool active=false, paused=false, completed=false, cleared=false;
    int initialLevel=0, frame=0, within=0, lastAction=-1;
    std::size_t action=0;
    void start(ReplayScript value,int fallback,Game& game) {
        script=std::move(value);
        initialLevel=script.level>=0?script.level:fallback;
        restart(game);
    }
    void restart(Game& game) {
        game=Game(initialLevel);
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
