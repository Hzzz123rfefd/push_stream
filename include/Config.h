#ifndef __CONFIG__
#define __CONFIG__
#include "Macro.h"
#include "RtmpMediaEngine.h"
class MediaEngineConfigInfo{
private:
    MediaEngineConfigInfo(){}
    MediaEngineConfigInfo(const MediaEngineConfigInfo& ) = delete;   
    MediaEngineConfigInfo& operator=(const MediaEngineConfigInfo&) = delete;
    CLASS_PEIVATE(int,wight,Wight);                                    // 视频宽度
    CLASS_PEIVATE(int,height,Height);                                  // 视频高度
    CLASS_PEIVATE(int,frame,Frame);                                    // 视频帧率
    CLASS_PEIVATE(AVPixelFormat,videoformat,VideoFormat);              // 输入图片格式
    CLASS_PEIVATE(AVCodecID,videoCodeID,VideoCodeID);                  // 视频编码器格式
    CLASS_PEIVATE(AVSampleFormat,audioFormat,AudioFormat);             // 音频格式
    CLASS_PEIVATE(AVCodecID,audioCodeID,AudioCodeID);                  // 音频编码器格式
    CLASS_PEIVATE(int,channelNum,ChannelNum)                           // 音频通道数量
    CLASS_PEIVATE(AVChannelLayout,layout,Layout);                      // 音频通道布局
    CLASS_PEIVATE(int,simpleRate,SimpleRate);                          // 音频采样率
    CLASS_PEIVATE(string,rtmpServerIp,RtmpServerIp);                   // rtmp服务器url
    CLASS_PEIVATE(uint64_t,videoRate,VideoRate);                       // 视频码率
    CLASS_PEIVATE(uint64_t,audioRate,AudioRate);                       // 音频码率
public:
    static MediaEngineConfigInfo* getInstance(){
        static MediaEngineConfigInfo instance;
        return &instance;
    }
};
void config(int w,int h,uint64_t videoRate,uint64_t audioRate,string rtmp_url){
    MediaEngineConfigInfo::getInstance()->setWight(w);
    MediaEngineConfigInfo::getInstance()->setHeight(h);
    MediaEngineConfigInfo::getInstance()->setFrame(25);
    MediaEngineConfigInfo::getInstance()->setVideoFormat(AV_PIX_FMT_YUV420P);
    MediaEngineConfigInfo::getInstance()->setVideoCodeID(AV_CODEC_ID_H264);
    /**
     * f32le:AV_SAMPLE_FMT_FLTP
     * s16le:AV_SAMPLE_FMT_S16P
     * s32le:AV_SAMPLE_FMT_S32P
    */
    MediaEngineConfigInfo::getInstance()->setAudioFormat(AV_SAMPLE_FMT_FLTP);
    MediaEngineConfigInfo::getInstance()->setAudioCodeID(AV_CODEC_ID_AAC);
    /**
     *   单通道宏：AV_CHANNEL_LAYOUT_MONO
     *   双通道宏：AV_CHANNEL_LAYOUT_STEREO
    */
    MediaEngineConfigInfo::getInstance()->setLayout(AV_CHANNEL_LAYOUT_MONO);
    MediaEngineConfigInfo::getInstance()->setChannelNum(1);
    MediaEngineConfigInfo::getInstance()->setSimpleRate(16000);
    MediaEngineConfigInfo::getInstance()->setRtmpServerIp(rtmp_url);//"rtmp://10.60.146.107/live/123"
    MediaEngineConfigInfo::getInstance()->setVideoRate(videoRate);
    MediaEngineConfigInfo::getInstance()->setAudioRate(audioRate);
}
#endif