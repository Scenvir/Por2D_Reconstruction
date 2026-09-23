#include "por2/editor_host.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string request(const std::string& url,const std::string& route,const std::string& body={},
                    const std::string& originOverride={},const std::string& extra={}){
    const auto slash=url.find('/',7);
    const auto host=url.substr(7,slash-7),origin=url.substr(0,slash);
    const auto prefix=url.substr(slash,url.rfind('/')-slash+1);
    SOCKET client=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    struct Cleanup{SOCKET value;~Cleanup(){closesocket(value);}} cleanup{client};
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    address.sin_port=htons(static_cast<unsigned short>(std::stoi(host.substr(host.find(':')+1))));
    require(connect(client,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"connect failed");
    const bool post=route.compare(0,5,"save/")==0;
    const std::string data=(post?"POST ":"GET ")+prefix+route+" HTTP/1.1\r\nHost: "+host+
        "\r\nOrigin: "+(originOverride.empty()?origin:originOverride)+
        "\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+extra+"\r\n"+body;
    std::size_t offset=0;
    while(offset<data.size()){
        const int sent=send(client,data.data()+offset,static_cast<int>(data.size()-offset),0);
        require(sent>0,"send failed");offset+=sent;
    }
    std::string result;char buffer[8192];int count;
    while((count=recv(client,buffer,sizeof(buffer),0))>0)result.append(buffer,count);
    return result;
}
void code(const std::string& response,int expected){
    require(response.find("HTTP/1.1 "+std::to_string(expected)+" ")==0,response.c_str());
}
std::string read(const std::filesystem::path& file){
    std::ifstream input(file,std::ios::binary);return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
}
int main(){
    try{
        const auto root=std::filesystem::current_path()/("editor-host-test-"+std::to_string(GetCurrentProcessId()));
        require(!std::filesystem::exists(root),"test directory already exists");
        std::filesystem::create_directories(root/L"editor");
        struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}} cleanup{root};
        {std::ofstream file(root/L"editor"/L"index.html");file<<"test editor";}
        por2::EditorHost host;const auto url=host.start(root);
        require(host.start(root)==url,"reopening must reuse session");
        const auto page=request(url,"index.html");code(page,200);require(page.find("test editor")!=std::string::npos,"asset missing");
        code(request(url,"../keybindings.ini"),404);
        code(request(url,"save/test.json","{}","https://example.com"),403);
        code(request(url,"save/%2e%2e%2fescape.json","{}"),400);
        code(request(url,"save/%5cescape.json","{}"),400);
        code(request(url,"save/%00.json","{}"),400);
        code(request(url,"save/CON.json","{}"),400);
        code(request(url,"save/invalid.exe","{}"),400);
        code(request(url,"save/empty.json"),413);
        code(request(url,"save/test.json","{}",{},"Content-Length: 2\r\n"),400);
        const std::string original="{\"name\":\"中文地图\"}";
        code(request(url,"save/%E4%B8%AD%E6%96%87.json",original),200);
        require(read(root/L"levels"/L"中文.json")==original,"UTF-8 export mismatch");
        code(request(url,"save/%E4%B8%AD%E6%96%87.json","changed"),409);
        require(read(root/L"levels"/L"中文.json")==original,"conflict overwrote file");
        code(request(url,"save/%E4%B8%AD%E6%96%87.json?replace=1","changed"),200);
        require(read(root/L"levels"/L"中文.json")=="changed","confirmed overwrite failed");
        code(request(url,"save/map.cpp","// exported source"),200);
        require(read(root/L"levels"/L"map.cpp")=="// exported source","C++ export failed");
        std::filesystem::create_directory(root/L"levels"/L"blocked.json");
        code(request(url,"save/blocked.json?replace=1","{}"),500);
        require(!std::filesystem::exists(root/L"levels"/(L".export-"+std::to_wstring(GetCurrentProcessId())+L".tmp")),"failed export left temporary file");
        host.stop();host.start(root);code(request(host.start(root),"index.html"),200);
        std::cout<<"PASS editor host: local assets, UTF-8 exports, overwrite confirmation, invalid paths/origins, failures and restart\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
