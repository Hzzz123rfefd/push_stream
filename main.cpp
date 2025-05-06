#include "RtmpMediaEngine.h"
#include "Config.h"
#include "log.hpp"

Log errorlog("../log/error.log","error",true);
Log info_log("../log/info.log","info",true);
Log video_send_context_log("../log/video_log/send_context.log","video",true);
Log video_recv_context_log("../log/video_log/recv_context.log","video",true);
Log video_send_rtmp_log("../log/video_log/send_rtmp.log","video",true);
Log audio_send_context_log("../log/audio_log/send_context.log","video",true);
Log audio_recv_context_log("../log/audio_log/recv_context.log","video",true);
Log audio_send_rtmp_log("../log/audio_log/send_rtmp.log","video",true);
namespace fs = std::filesystem;
void deleteFilesInDirectory(const fs::path& dirPath) {
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            fs::remove_all(entry.path());
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error deleting files in directory " << dirPath << ": " << e.what() << '\n';
    }
}

int main(int argc, char *argv[])
{
    if(argc != 6){
        printf("check parameter number,please begin with *.exe width height videoRate audioRate rtmp_url\n");
        return -1;
    }
    config(atoi(argv[1]),atoi(argv[2]),strtoull(argv[3], nullptr, 10),strtoull(argv[4], nullptr, 10),string(argv[5]));
    printf("rtmp push engine args: \nwidth = %d\nheight = %d\nvideoRate = %ld\naudioRate =  %ld\nrtmp server url:%s\n",
         MediaEngineConfigInfo::getInstance()->getWight(),
         MediaEngineConfigInfo::getInstance()->getHeight(),
         MediaEngineConfigInfo::getInstance()->getVideoRate(),
         MediaEngineConfigInfo::getInstance()->getAudioRate(),
         MediaEngineConfigInfo::getInstance()->getRtmpServerIp().c_str());

    fs::path audioDir = "../base_datas/audios/";
    fs::path frameDir = "../base_datas/frames";

    // Check if the audio directory exists and is a directory
    if (fs::exists(audioDir) && fs::is_directory(audioDir)) {
        // Check if there are any files in the audio directory
        auto it = fs::directory_iterator(audioDir);
        bool isEmpty = (it == fs::end(it));
        
        if (isEmpty) {
            std::cout << "No files in audio directory. Deleting files in audio and frame directories." << endl;
            deleteFilesInDirectory(audioDir);
            deleteFilesInDirectory(frameDir);
        } else {
            std::cout << "Audio directory contains files." << endl;
        }
    } else {
        std::cerr << "Audio directory does not exist or is not a directory." << endl;
    }


    RtmpMediaEngine rme(
        MediaMessage(
        MediaEngineConfigInfo::getInstance()->getWight(),
        MediaEngineConfigInfo::getInstance()->getHeight(),
        MediaEngineConfigInfo::getInstance()->getFrame(),
        MediaEngineConfigInfo::getInstance()->getVideoFormat(), 
        MediaEngineConfigInfo::getInstance()->getVideoCodeID(),
        MediaEngineConfigInfo::getInstance()->getAudioCodeID(),
        MediaEngineConfigInfo::getInstance()->getAudioFormat(), 
        MediaEngineConfigInfo::getInstance()->getLayout(),
        MediaEngineConfigInfo::getInstance()->getChannelNum(),
        MediaEngineConfigInfo::getInstance()->getSimpleRate(),
        MediaEngineConfigInfo::getInstance()->getRtmpServerIp(),
        MediaEngineConfigInfo::getInstance()->getVideoRate(),
        MediaEngineConfigInfo::getInstance()->getAudioRate()
        ));
    rme.openEngine();
    return 0;
}