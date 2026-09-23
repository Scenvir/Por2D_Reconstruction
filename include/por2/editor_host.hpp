#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace por2 {
// A loopback-only host lets the bundled browser editor save without a download
// directory, external runtime, or browser-specific file system permissions.
class EditorHost {
    SOCKET listener_=INVALID_SOCKET;
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    bool winsock_=false;
    std::filesystem::path root_;
    std::string origin_, prefix_;
    struct Socket {
        SOCKET value;
        ~Socket(){if(value!=INVALID_SOCKET)closesocket(value);}
    };
    static constexpr std::size_t Limit=1024*1024;
    static std::string readFile(const std::filesystem::path& path){
        std::ifstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("Cannot read editor asset");
        return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    }
    static void reply(SOCKET client,int code,const std::string& body,const std::string& type="text/plain; charset=utf-8"){
        const auto response="HTTP/1.1 "+std::to_string(code)+" Result\r\nContent-Type: "+type+
            "\r\nContent-Length: "+std::to_string(body.size())+
            "\r\nConnection: close\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nX-Content-Type-Options: nosniff\r\n\r\n"+body;
        std::size_t sent=0;
        while(sent<response.size()){
            const int count=send(client,response.data()+sent,static_cast<int>(response.size()-sent),0);
            if(count<=0)return;
            sent+=count;
        }
    }
    static std::filesystem::path filename(const std::string& encoded){
        std::string decoded;
        auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
        for(std::size_t i=0;i<encoded.size();++i){
            if(encoded[i]!='%'){decoded+=encoded[i];continue;}
            if(i+2>=encoded.size())throw std::runtime_error("Invalid filename");
            const int high=hex(encoded[i+1]),low=hex(encoded[i+2]);
            if(high<0||low<0)throw std::runtime_error("Invalid filename");
            decoded+=static_cast<char>(high*16+low);i+=2;
        }
        if(decoded.empty()||decoded.size()>240)throw std::runtime_error("Invalid filename length");
        for(unsigned char c:decoded)if(c<32||std::string("<>:\"/\\|?*").find(static_cast<char>(c))!=std::string::npos)
            throw std::runtime_error("Invalid filename");
        const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,decoded.data(),static_cast<int>(decoded.size()),nullptr,0);
        if(!count)throw std::runtime_error("Invalid UTF-8 filename");
        std::wstring wide(count,L'\0');
        MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,decoded.data(),static_cast<int>(decoded.size()),wide.data(),count);
        const std::filesystem::path name(wide);
        if(name.extension()!=L".json"&&name.extension()!=L".cpp")throw std::runtime_error("Only JSON and C++ exports are supported");
        auto stem=decoded.substr(0,decoded.find('.'));
        std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
        if(stem.empty()||stem=="CON"||stem=="PRN"||stem=="AUX"||stem=="NUL"||
           (stem.size()==4&&(stem.substr(0,3)=="COM"||stem.substr(0,3)=="LPT")&&stem[3]>='0'&&stem[3]<='9'))
            throw std::runtime_error("Reserved filename");
        return name;
    }
    void serve(SOCKET client){
        DWORD timeout=1500;
        setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
        setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
        std::string request;char buffer[8192];std::size_t boundary;
        while((boundary=request.find("\r\n\r\n"))==std::string::npos){
            if(stopping_)return;
            const int count=recv(client,buffer,sizeof(buffer),0);if(count<=0)return;
            request.append(buffer,count);
            if(request.size()>16384){reply(client,413,"Request headers too large");return;}
        }
        std::istringstream input(request.substr(0,boundary));
        std::string method,path,version,line;input>>method>>path>>version;std::getline(input,line);
        std::map<std::string,std::string> headers;
        while(std::getline(input,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto colon=line.find(':');if(colon==std::string::npos)continue;
            auto key=line.substr(0,colon),value=line.substr(colon+1);
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            value.erase(0,value.find_first_not_of(" \t"));
            if(!headers.emplace(key,value).second){reply(client,400,"Duplicate header");return;}
        }
        if(headers["host"]!=origin_.substr(7)||path.compare(0,prefix_.size(),prefix_)!=0){reply(client,403,"Invalid editor session");return;}
        path.erase(0,prefix_.size());
        if(method=="GET"){
            if(path!="index.html"&&path!="editor.js"&&path!="model.js"){reply(client,404,"Not found");return;}
            reply(client,200,readFile(root_/L"editor"/path),path=="index.html"?"text/html; charset=utf-8":"text/javascript; charset=utf-8");return;
        }
        if(method!="POST"||path.compare(0,5,"save/")!=0){reply(client,405,"Unsupported operation");return;}
        if(headers["origin"]!=origin_||headers.count("transfer-encoding")){reply(client,403,"Invalid save origin");return;}
        const auto lengthText=headers["content-length"];
        if(lengthText.empty()||lengthText.size()>7||lengthText.find_first_not_of("0123456789")!=std::string::npos){reply(client,400,"Invalid content length");return;}
        const auto length=static_cast<std::size_t>(std::stoul(lengthText));
        if(!length||length>Limit){reply(client,413,"Export must be between 1 byte and 1 MB");return;}
        std::string body=request.substr(boundary+4);
        while(body.size()<length){
            const int count=recv(client,buffer,static_cast<int>(std::min(sizeof(buffer),length-body.size())),0);if(count<=0)return;
            body.append(buffer,count);
            if(stopping_)return;
        }
        if(body.size()!=length){reply(client,400,"Invalid body length");return;}
        const bool replace=path.size()>10&&path.substr(path.size()-10)=="?replace=1";
        if(replace)path.resize(path.size()-10);
        const auto name=filename(path.substr(5));
        const auto directory=root_/L"levels";
        std::filesystem::create_directories(directory);
        const auto destination=directory/name;
        for(const auto& entry:{directory,destination}){
            const auto attributes=GetFileAttributesW(entry.c_str());
            if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)){reply(client,403,"Linked export paths are not supported");return;}
        }
        if(!replace&&std::filesystem::exists(destination)){reply(client,409,"File already exists");return;}
        const auto temporary=directory/(L".export-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
        // CREATE_NEW avoids following an existing temporary file or link.
        HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){reply(client,500,"Cannot create export file; check game folder permissions");return;}
        DWORD written=0;
        const bool saved=WriteFile(file,body.data(),static_cast<DWORD>(body.size()),&written,nullptr)&&written==body.size()&&FlushFileBuffers(file);
        CloseHandle(file);
        if(!saved||!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0))){
            const auto error=GetLastError();DeleteFileW(temporary.c_str());
            reply(client,(!replace&&(error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS))?409:500,"Cannot save export; check game folder permissions");return;
        }
        reply(client,200,"Saved to levels");
    }
    void run(){
        while(!stopping_){
            fd_set readers;FD_ZERO(&readers);FD_SET(listener_,&readers);timeval wait{0,100000};
            if(select(0,&readers,nullptr,nullptr,&wait)<=0)continue;
            Socket client{accept(listener_,nullptr,nullptr)};
            if(client.value==INVALID_SOCKET)continue;
            try{serve(client.value);}catch(const std::exception& error){reply(client.value,400,error.what());}
        }
    }
public:
    EditorHost()=default;
    EditorHost(const EditorHost&)=delete;
    EditorHost& operator=(const EditorHost&)=delete;
    ~EditorHost(){stop();}
    void stop(){
        stopping_=true;
        if(worker_.joinable())worker_.join();
        if(listener_!=INVALID_SOCKET){closesocket(listener_);listener_=INVALID_SOCKET;}
        if(winsock_){WSACleanup();winsock_=false;}
    }
    std::string start(const std::filesystem::path& root){
        if(worker_.joinable())return origin_+prefix_+"index.html";
        try{
            root_=root;WSADATA data{};
            if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("Cannot initialize editor host");
            winsock_=true;listener_=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(listener_==INVALID_SOCKET)throw std::runtime_error("Cannot create editor socket");
            BOOL exclusive=TRUE;setsockopt(listener_,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
            sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
            if(bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||listen(listener_,8)!=0)
                throw std::runtime_error("Cannot start local editor host");
            int size=sizeof(address);
            if(getsockname(listener_,reinterpret_cast<sockaddr*>(&address),&size)!=0)throw std::runtime_error("Cannot find editor port");
            GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot create editor session");
            const char digits[]="0123456789abcdef";prefix_="/";
            const auto* bytes=reinterpret_cast<const unsigned char*>(&id);
            for(std::size_t i=0;i<sizeof(id);++i){prefix_+=digits[bytes[i]>>4];prefix_+=digits[bytes[i]&15];}
            prefix_+='/';origin_="http://127.0.0.1:"+std::to_string(ntohs(address.sin_port));
            stopping_=false;worker_=std::thread([this]{run();});
            return origin_+prefix_+"index.html";
        }catch(...){stop();throw;}
    }
};
} // namespace por2
