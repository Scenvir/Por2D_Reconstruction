#include "por2/level.hpp"
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <locale>

namespace por2 {
namespace {
struct Json {
    enum Kind { Null, Number, String, Array, Object, Boolean } kind=Null;
    double number=0;
    std::string text;
    std::vector<Json> array;
    std::map<std::string,Json> object;
    const Json& at(const std::string& key) const {
        if(kind!=Object || !object.count(key))throw std::runtime_error("Missing field: "+key);
        return object.at(key);
    }
    int integer(int low,int high) const {
        if(kind!=Number||!std::isfinite(number)||number<low||number>high||std::floor(number)!=number)
            throw std::runtime_error("Invalid integer in map");
        return static_cast<int>(number);
    }
    std::string string() const {if(kind!=String)throw std::runtime_error("Expected map text");return text;}
};
class Parser {
    const std::string& input;
    std::size_t pos=0;
    int nodes=0;
    [[noreturn]] void fail() const {throw std::runtime_error("Invalid JSON at byte "+std::to_string(pos));}
    void space(){while(pos<input.size()&&(input[pos]==' '||input[pos]=='\n'||input[pos]=='\r'||input[pos]=='\t'))++pos;}
    bool take(char c){space();if(pos<input.size()&&input[pos]==c){++pos;return true;}return false;}
    void need(char c){if(!take(c))fail();}
    unsigned hex(){unsigned n=0;for(int i=0;i<4;++i){if(pos==input.size())fail();char c=input[pos++];n*=16;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else fail();}return n;}
    static void append(std::string& out,unsigned cp){
        if(cp<0x80)out+=static_cast<char>(cp);
        else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
        else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
        else {out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
    }
    std::string string(){
        need('"');std::string out;
        while(pos<input.size()){
            const unsigned char c=input[pos++];if(c=='"')return out;if(c<32)fail();
            if(c=='\\'){
                if(pos==input.size())fail();const char e=input[pos++];
                switch(e){
                case '"':case '\\':case '/':out+=e;break;
                case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case 'u':{unsigned cp=hex();if(cp>=0xd800&&cp<=0xdbff){if(pos+2>input.size()||input.substr(pos,2)!="\\u")fail();pos+=2;unsigned tail=hex();if(tail<0xdc00||tail>0xdfff)fail();cp=0x10000+(cp-0xd800)*1024+tail-0xdc00;}else if(cp>=0xdc00&&cp<=0xdfff)fail();append(out,cp);break;}
                default:fail();}
            }else if(c<128)out+=static_cast<char>(c);
            else {
                int count=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;if(!count)fail();
                unsigned cp=c&((1u<<(6-count))-1);for(int i=0;i<count;++i){if(pos==input.size())fail();unsigned char b=input[pos++];if((b&0xc0)!=0x80)fail();cp=(cp<<6)|(b&63);}
                if(cp<(count==1?0x80u:count==2?0x800u:0x10000u)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))fail();append(out,cp);
            }
        }fail();
    }
    Json value(int depth){
        if(depth>32||++nodes>20000)fail();space();if(pos==input.size())fail();Json j;
        if(input[pos]=='"'){j.kind=Json::String;j.text=string();return j;}
        if(take('{')){j.kind=Json::Object;if(take('}'))return j;do{const auto key=string();need(':');if(!j.object.emplace(key,value(depth+1)).second)fail();}while(take(','));need('}');return j;}
        if(take('[')){j.kind=Json::Array;if(take(']'))return j;do{j.array.push_back(value(depth+1));}while(take(','));need(']');return j;}
        for(const auto* literal:{"null","true","false"}){const std::string word=literal;if(input.compare(pos,word.size(),word)==0){pos+=word.size();j.kind=word=="null"?Json::Null:Json::Boolean;return j;}}
        const auto start=pos;if(input[pos]=='-')++pos;
        auto digit=[&](){return pos<input.size()&&input[pos]>='0'&&input[pos]<='9';};
        if(!digit())fail();if(input[pos]=='0')++pos;else while(digit())++pos;
        if(pos<input.size()&&input[pos]=='.'){++pos;if(!digit())fail();while(digit())++pos;}
        if(pos<input.size()&&(input[pos]=='e'||input[pos]=='E')){++pos;if(pos<input.size()&&(input[pos]=='+'||input[pos]=='-'))++pos;if(!digit())fail();while(digit())++pos;}
        std::istringstream number(input.substr(start,pos-start));number.imbue(std::locale::classic());if(!(number>>j.number)||!std::isfinite(j.number))fail();j.kind=Json::Number;return j;
    }
public:
    explicit Parser(const std::string& text):input(text){if(input.compare(0,3,"\xef\xbb\xbf")==0)pos=3;}
    Json parse(){auto result=value(0);space();if(pos!=input.size())fail();return result;}
};
}
Level parseEditorLevel(const std::string& json,int id){
    if(json.size()>1024*1024)throw std::runtime_error("Map exceeds 1 MB");
    const auto root=Parser(json).parse();
    if(root.at("format").string()!="Por2D-map")throw std::runtime_error("Unsupported map format");
    root.at("version").integer(1,1);root.at("width").integer(MapWidth,MapWidth);root.at("height").integer(MapHeight,MapHeight);root.at("tileSize").integer(TileSize,TileSize);
    Level level;level.id=id;level.name=root.at("name").string();level.commentary=root.at("commentary").string();
    if(level.name.size()>320||level.commentary.size()>800)throw std::runtime_error("Map text too long");
    const auto& rows=root.at("tiles");if(rows.kind!=Json::Array||rows.array.size()!=MapHeight)throw std::runtime_error("Expected 30 tile rows");
    for(int y=0;y<MapHeight;++y){const auto& row=rows.array[y];if(row.kind!=Json::Array||row.array.size()!=MapWidth)throw std::runtime_error("Expected 50 tiles per row");for(int x=0;x<MapWidth;++x)level.map.set(x,y,static_cast<Tile>(row.array[x].integer(0,2)));}
    const auto body=[&](const char* key){
        const auto& item=root.at(key);Body b;
        b.position={item.at("x").integer(0,49)*20+1,item.at("y").integer(0,29)*20+1};b.direction=static_cast<Direction>(item.at("direction").integer(0,3));
        const auto r=b.bounds();if(r.left<=0||r.top<=0||r.right>=WindowWidth||r.bottom>=WindowHeight)throw std::runtime_error(std::string(key)+" touches map boundary");
        for(int y=static_cast<int>(r.top)/20;y<=static_cast<int>(r.bottom-1)/20;++y) {
            for(int x=static_cast<int>(r.left)/20;x<=static_cast<int>(r.right-1)/20;++x) {
                if(level.map.at(x,y)!=Tile::Empty)throw std::runtime_error(std::string(key)+" overlaps wall");
            }
        }
        return b;
    };
    level.spawn=body("spawn");level.exit=body("exit");
    // Keep a one-line copy for self-contained recordings; whitespace inside strings is preserved.
    bool quoted=false,escaped=false;
    for(std::size_t i=json.compare(0,3,"\xef\xbb\xbf")==0?3:0;i<json.size();++i){const char c=json[i];if(quoted||!(c==' '||c=='\r'||c=='\n'||c=='\t'))level.editorJson+=c;if(escaped){escaped=false;continue;}if(quoted&&c=='\\')escaped=true;else if(c=='"')quoted=!quoted;}
    return level;
}
Level loadEditorLevel(const std::filesystem::path& path,int id){
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open map");
    std::string text;char c;while(input.get(c)){text+=c;if(text.size()>1024*1024)throw std::runtime_error("Map exceeds 1 MB");}
    if(input.bad())throw std::runtime_error("Cannot read map");return parseEditorLevel(text,id);
}
}
