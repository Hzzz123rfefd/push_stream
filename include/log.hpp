#ifndef __LOG__
#define __LOG__
#include <mutex>
#include <string>
#include <iostream>
#include <sstream>
#include <mutex>
#include <ctime>
#include <cstdio>
#include <cstring>

using namespace std;
//#define __DEBUGE__
//#define __APPEND__

class Log{
public:
    Log(string logFilePath,string logType,bool write){
        pfile = fopen(logFilePath.c_str(),"wb");
        if(pfile == nullptr){
            printf("open log file error\n");
            return;
        } 
        this->logType = logType;
        this->write = write;
    }
    ~Log(){
        if(pfile){
        fclose(pfile);
        pfile = nullptr;
    }
    }
public:
    static string getCurrentTime(){
        std::time_t now = std::time(nullptr);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }
    string getlogType(){
        return this->logType;
    }
public:
    template<typename T>
    Log& operator <<(const T& message){
        if(pfile == nullptr){
            printf("error:not exist log file\n");
        }
        logLock.lock();
        std::ostringstream msgStream;
        msgStream << message;
        std::string msgStr = msgStream.str();
    #ifdef __APPEND__
        if(write){
            int size = fwrite(msgStr.c_str(),1,msgStr.size(),pfile);
        }
    #endif
    #ifdef __DEBUGE__
        printf("%s",msgStr.c_str());
    #endif
        logLock.unlock();
        return *this;
    }
private:
    FILE* pfile;
    string logType;
    mutex logLock;
    bool write;
};




#endif

