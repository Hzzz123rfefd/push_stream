#ifndef __MEDIAENGIN__
#define __MEDIAENGIN__
#include <iostream>
#include "librtmp/rtmp.h"
#include "librtmp/log.h"
#include <stdlib.h>
#include <queue>
#include <thread>
#include <string>
#include <memory>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
#include <mutex>
#include "Macro.h"
#include "log.hpp"
#include <filesystem>
#include <algorithm>
#define endl "\n"
namespace fs = std::filesystem;
using namespace std;

#pragma once
#include <iostream>
#include <string>
#include <cstring>
#include <queue>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <pthread.h>
template <typename T>
class SafeDeque {
private:
    std::deque<T> deque;
    mutable std::mutex mtx;
    std::condition_variable cv;

public:
    SafeDeque() = default;
    ~SafeDeque() = default;

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        deque.push_back(value);
        cv.notify_one();
    }

    void push_front(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        deque.push_front(value);
        cv.notify_one();
    }

    void pop_back() {
        std::lock_guard<std::mutex> lock(mtx);
        if (deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }
        deque.pop_back();
    }

    void pop_front() {
        std::lock_guard<std::mutex> lock(mtx);
        if (deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }
        deque.pop_front();
    }

    T front() const {
        std::lock_guard<std::mutex> lock(mtx);
        if (deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }
        return deque.front();
    }

    T back() const {
        std::lock_guard<std::mutex> lock(mtx);
        if (deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }
        return deque.back();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return deque.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return deque.empty();
    }
    T operator[](size_t index) const {
        std::lock_guard<std::mutex> lock(mtx);
        if (index >= deque.size()) {
            throw std::out_of_range("Index out of range");
        }
        return deque[index];
    }
};
 
template <typename T>
class SafeQueue {
public:
    using lock_type = std::unique_lock<std::mutex>;
public:
    SafeQueue() = default;
    SafeQueue(const SafeQueue& other)
    {
        // lock_guard对象超出作用域时，互斥锁会自动解锁，从而安全地管理互斥锁并避免死锁。
        // std::lock_guard<std::mutex> lock(other.mutex_);  
        lock_type lock{mutex_};
        queue_ = other.queue_;
    }
    ~SafeQueue() = default;
    template<typename IT>
    void push(IT &&item) {
        static_assert(std::is_same<T, std::decay_t<IT>>::value, "Item type is not convertible!!!");
        {
        lock_type lock{mutex_};
        queue_.emplace(std::forward<IT>(item));
        }
        cv_.notify_one();
    }
    void pop() {
        lock_type lock{mutex_};
        cv_.wait(lock, [&]() { return !queue_.empty(); });
        queue_.pop();
    }

    auto front() -> T{
        lock_type lock{mutex_};
        cv_.wait(lock, [&]() { return !queue_.empty(); });
        auto front = std::move(queue_.front());
        return front;
    }
 
    // - `empty()`：检查队列是否为空。
    // 通过创建互斥锁对象，锁定临界区并使用STL `queue::empty()` 成员函数检查元素是否为空来实现。
    bool empty() const {
        lock_type lock{mutex_};
        return queue_.empty();
    }
 
    // - `size()`：返回队列中元素的数量。
    // 通过创建互斥锁对象，锁定临界区并使用STL `queue::size()` 成员函数返回队列中元素的数量来实现。
    size_t size() const {
        lock_type lock{mutex_};
        return queue_.size();
    }
    
    // - `swap()`：用另一个队列对象交换这个队列对象的内容。
    // 由于交换操作涉及两个不同的队列对象，因此需要创建两个互斥锁对象分别锁定两个队列对象，
    // 然后使用STL `queue::swap()` 成员函数在两个队列对象之间交换内容来实现。
    void swap(SafeQueue<T>& other) {
        lock_type lock1{mutex_, std::defer_lock};
        lock_type lock2{other.mutex_, std::defer_lock};
        std::lock(lock1, lock2);
        std::swap(queue_, other.queue_);
    }
 
    // - `clear()`：清空队列中的所有元素。
    // 通过创建互斥锁对象，锁定临界区并使用STL `queue::swap()` 成员函数在队列对象中交换备用队列来实现。
    void clear() {
        lock_type lock{mutex_};
        std::queue<T>().swap(queue_);
    }
 
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;      //mutable允许
    std::condition_variable cv_;
};




// NALU单元:压缩的视频流（不包括初始码0001 001）
class NaluUnit{
public:
    NaluUnit(){
        data = new u_char[NALU_MAX_SIZE];
    }
    ~NaluUnit(){
        delete[] data; 
    }
public:
    int fromPkt(AVPacket* pkt);
public:
	int type;                           //nal单元类型，7:sps   8:pps    5:关键帧
	int size;                           //nal单元大小（不包括0001  001）
	unsigned char *data;                //nal单元实际二进制数据(不包括头)
};

// Raw_data_block:压缩的音频流
class RawDataBlock{
public:
    RawDataBlock(){
        data = new u_char[RAW_DATA_BLOCK_MAX_SIZE];
    }
    ~RawDataBlock(){
        delete[] data;
    }
public:
    int fromPkt(AVPacket* pkt);
public:
    u_char* data;
    int size;
};
// adts header
class ADTSHeader{
public:
    ADTSHeader(short sf_index,short channel_configuration){
        profile = 2;
        this->sf_index = sf_index;
        this->channel_configuration = channel_configuration;
    }
public:
    //int from
public:
    short syncword;
    short id;
    short layer;
    short protection_absent;
    short profile;                  // 表示aac级别  一般为2
    /**
     *  0x0 96000
        0x1 88200
        0x2 64000
        0x3 48000
        0x4 44100
        0x5 32000
        0x6 24000
        0x7 22050
        0x8 16000
        0x9 2000
        0xa 11025
        0xb 8000
        0xc reserved
        0xd reserved
        0xe reserved
        0xf reserved
    */
    short sf_index;                 // 表示采样频率index   4
    short private_bit;              
    short channel_configuration;    // 表示通道数  立体为2    //单通道为1
    short original;
    short home;
    short emphasis;
    short copyright_identification_bit;
    short copyright_identification_start;
    short aac_frame_length;
    short adts_buffer_fullness;
    short no_raw_data_blocks_in_frame;
    short crc_check;
};

// flv媒体信息
class RTMPMetadata
{
public:
    int fromSpsPps(NaluUnit& sps,NaluUnit& pps);
    bool h264_decode_sps();
public:
	// video, must be h264 type
	unsigned int	nWidth;
	unsigned int	nHeight;
	unsigned int	nFrameRate;		// fps
	unsigned int	nVideoDataRate;	// bps
	unsigned int	nSpsLen;
	unsigned char	Sps[1024];
	unsigned int	nPpsLen;
	unsigned char	Pps[1024];
 
	// audio, must be aac type
	bool	        bHasAudio;
	unsigned int	nAudioSampleRate;
	unsigned int	nAudioSampleSize;
	unsigned int	nAudioChannels;
	char		    pAudioSpecCfg;
	unsigned int	nAudioSpecCfgLen;
 
};

// flv标签
class FlvTag{
public:
    FlvTag(){
        data = new u_char[FLV_BODY_MAX_SIZE];
    }
    ~FlvTag(){
        delete[] data;
    }
public:
    int FlvType;
    int flvDataSize;
    uint64_t timeStamp;
    int timeStapeExtand;
    int StreamID;
    u_char* data;               //flv body data
    int size;                   //flv body size
    int pts;
public:
    virtual int fromCommonNalu(NaluUnit& nu){return 0;}
    virtual int fromMeteDate(RTMPMetadata& lpMetaData){return 0;}
    virtual int fromRawDataBlock(RawDataBlock& rdb){return 0;}
    virtual int fromADTSHeader(ADTSHeader& adtsh){return 0;}
};

/**
 * Flv视频标签
*/
class FlvVideoTag:public FlvTag{
public:
    FlvVideoTag(){
        FlvType = 9;
    }
    ~FlvVideoTag(){
    }
public:
    /**
     * 从普通的NaluUnit构建FlvVideoTag
    */
    int fromCommonNalu(NaluUnit& nu) override;
    /**
     * 从sps和pps的NaluUnit构建FlvVideoTag
    */
    int fromMeteDate(RTMPMetadata& metaData) override;
public:
    int frameType;
    int codecID;
    int avcPacketType;
    int compositionTime;
}; 
/**
 * Flv音频标签
*/
class FlvAudioTag:public FlvTag{
public:
    FlvAudioTag(){
        FlvType = 8;
    }
    ~FlvAudioTag(){
    }
public:
    int fromRawDataBlock(RawDataBlock& rdb) override;
    int fromADTSHeader(ADTSHeader& adtsh) override;
public:
    //flv 音频标签头
    int soundFormat;
    int soundRate;
    int soundSize;
    int soundType;
    int AACPacketType;
};
struct raw_audio_data_info{
    raw_audio_data_info(int data_size){
        data_size = data_size;
        raw_data = new u_char[data_size];
        is_merge = false;
    }
    ~raw_audio_data_info(){
        if(raw_data){
            delete[] raw_data;
            raw_data = nullptr;
        }
    }
    u_char* raw_data;
    int data_size;
    bool is_merge;//是否混音？
};

/**
 * 图片流信息
*/
struct MediaMessage{
    MediaMessage(){}
    MediaMessage(
                int m_nVideoWight,
                int m_nVideoHeight,
                int m_nVideoFrame,
                enum AVPixelFormat m_nInputFigFormat,
                enum AVCodecID m_nId,
                enum AVCodecID m_nAudioId,
                enum AVSampleFormat m_nAudioFormat,
                AVChannelLayout m_cLayout,
                int m_nChannelNum,
                int m_nSampleRate,
                string m_strServerUrl,
                uint64_t videoRate,
                uint64_t audioRate
                ):      m_nVideoHeight(m_nVideoHeight),
                        m_nVideoWight(m_nVideoWight),
                        m_nVideoFrame(m_nVideoFrame),
                        m_nInputFigFormat(m_nInputFigFormat),
                        m_nAudioFormat(m_nAudioFormat),
                        m_nId(m_nId),
                        m_nAudioId(m_nAudioId),
                        m_cLayout(m_cLayout),
                        m_nChannelNum(m_nChannelNum),
                        m_nSampleRate(m_nSampleRate),
                        m_strServerUrl(m_strServerUrl),
                        videoRate(videoRate),
                        audioRate(audioRate){}
    int m_nVideoWight;                         //图片宽度
    int m_nVideoHeight;                        //图片高度
    int m_nVideoFrame;                         //图片帧率
    enum AVPixelFormat m_nInputFigFormat;      //图片格式
    enum AVSampleFormat m_nAudioFormat;        //音频格式
    AVChannelLayout m_cLayout;                 //音频布局
    int m_nChannelNum;                         //音频通道数
    enum AVCodecID m_nId;                      //编码器ID
    enum AVCodecID m_nAudioId;                 //音频编码器ID
    int m_nSampleRate;                         //采样频率
    string m_strServerUrl;                     //服务器url
    uint64_t videoRate;                        //视频码率
    uint64_t audioRate;                        //音频码率
};

/**
 * 格式转换工具
*/
class ConVert{
private:
    struct ConvertInfo{
        ConvertInfo(){
            codec_ctx = nullptr;
            sws_ctx = nullptr;
            fmt_ctx = nullptr;
            frame = nullptr;
            targetFrame = nullptr;
            packet = nullptr;
        }
        ~ConvertInfo(){
            if(codec_ctx) avcodec_free_context(&codec_ctx);
            if(sws_ctx) sws_freeContext(sws_ctx);
            if(frame) av_frame_free(&frame);
            if(targetFrame) av_frame_free(&targetFrame);
            if(fmt_ctx) avformat_close_input(&fmt_ctx);
            if(packet) av_packet_free(&packet);

        }
        string name;
        bool status;
        int width;
        int height;
        AVCodecContext *codec_ctx;
        SwsContext *sws_ctx;
        AVFormatContext *fmt_ctx;
        AVFrame *frame;
        AVFrame *targetFrame;
        AVPacket* packet;
    };
public:
    ConVert(){}
    ~ConVert(){}
public:
    int init_jpeg2yuv420(int w,int h){
        avformat_network_init();
        jepg2yuv420Info.width = w;
        jepg2yuv420Info.height = h;
        jepg2yuv420Info.status = true;
        jepg2yuv420Info.name = "jepg2yuv420";
        const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        if (!codec) {
            printf("Codec not found\n");
            return -1;
        }

        jepg2yuv420Info.codec_ctx = avcodec_alloc_context3(codec);
        if (!jepg2yuv420Info.codec_ctx) {
            printf("Could not allocate video codec context\n");
            return -1;
        }

        if (avcodec_open2(jepg2yuv420Info.codec_ctx, codec, nullptr) < 0) {
            printf("Could not open codec\n");
            return -1;
        }
        jepg2yuv420Info.sws_ctx = sws_getContext(jepg2yuv420Info.width, jepg2yuv420Info.height, AV_PIX_FMT_YUV420P, jepg2yuv420Info.width, jepg2yuv420Info.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!jepg2yuv420Info.sws_ctx) {
            printf("Could not initialize the conversion context\n");
            return -1;
        }
        jepg2yuv420Info.packet = av_packet_alloc();
        //av_init_packet(jepg2yuv420Info.packet);
        jepg2yuv420Info.packet->data = nullptr;
        jepg2yuv420Info.packet->size = 0;

        jepg2yuv420Info.frame = av_frame_alloc();
        jepg2yuv420Info.targetFrame = av_frame_alloc();
        if (!jepg2yuv420Info.frame || !jepg2yuv420Info.targetFrame) {
            printf("Could not allocate frame\n");
        }
        jepg2yuv420Info.targetFrame->format = AV_PIX_FMT_YUV420P;
        jepg2yuv420Info.targetFrame->width = jepg2yuv420Info.width;
        jepg2yuv420Info.targetFrame->height = jepg2yuv420Info.height;
        if (av_frame_get_buffer(jepg2yuv420Info.targetFrame, 0) < 0) {
            printf("Could not allocate the YUV frame\n");
        }
        printf("finish init jpeg2yuv420\n");
        return 0;
    }
    int exit_jpeg2yuv420(){
        if(jepg2yuv420Info.codec_ctx){
            avcodec_free_context(&jepg2yuv420Info.codec_ctx);
            jepg2yuv420Info.codec_ctx = nullptr;
        }
        if (jepg2yuv420Info.sws_ctx){
            sws_freeContext(jepg2yuv420Info.sws_ctx);
            jepg2yuv420Info.sws_ctx = nullptr;
        }
        if(jepg2yuv420Info.frame){
            av_frame_free(&jepg2yuv420Info.frame);
            jepg2yuv420Info.frame = nullptr;
        }
        if(jepg2yuv420Info.targetFrame){
            av_frame_free(&jepg2yuv420Info.targetFrame);
            jepg2yuv420Info.targetFrame = nullptr;
        }
        if(jepg2yuv420Info.fmt_ctx){
            avformat_close_input(&jepg2yuv420Info.fmt_ctx);
            jepg2yuv420Info.fmt_ctx = nullptr;
        }
        if(jepg2yuv420Info.packet){
            av_packet_free(&jepg2yuv420Info.packet);
            jepg2yuv420Info.packet = nullptr;
        }
        return 0;
    }
public:
    int jepg2yuv420(string jepgPath,u_char* yuv420){
        if(jepg2yuv420Info.fmt_ctx){
            avformat_close_input(&jepg2yuv420Info.fmt_ctx);
            jepg2yuv420Info.fmt_ctx = nullptr;
        }
        if (avformat_open_input(&jepg2yuv420Info.fmt_ctx, jepgPath.c_str(), nullptr, nullptr) < 0) {
            printf("Could not open input file\n");
        }

        if (avformat_find_stream_info(jepg2yuv420Info.fmt_ctx, nullptr) < 0) {
            printf("Could not find stream information\n");
        }

        int ret = 0;
        while (av_read_frame(jepg2yuv420Info.fmt_ctx, jepg2yuv420Info.packet) >= 0) {
            if (jepg2yuv420Info.packet->stream_index != jepg2yuv420Info.fmt_ctx->streams[0]->index) {
                av_packet_unref(jepg2yuv420Info.packet);
                continue;
            }

            ret = avcodec_send_packet(jepg2yuv420Info.codec_ctx, jepg2yuv420Info.packet);
            if (ret < 0) {
                break;
            }

            ret = avcodec_receive_frame(jepg2yuv420Info.codec_ctx, jepg2yuv420Info.frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("Error while receiving frame from decoder\n");
            }

            sws_scale(jepg2yuv420Info.sws_ctx, jepg2yuv420Info.frame->data, jepg2yuv420Info.frame->linesize, 0, jepg2yuv420Info.codec_ctx->height, jepg2yuv420Info.targetFrame->data, jepg2yuv420Info.targetFrame->linesize);
            av_packet_unref(jepg2yuv420Info.packet);
        }
        int out_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, jepg2yuv420Info.width, jepg2yuv420Info.height, 1);
        av_image_copy_to_buffer(yuv420, out_size, (const uint8_t * const *)jepg2yuv420Info.targetFrame->data, jepg2yuv420Info.targetFrame->linesize, AV_PIX_FMT_YUV420P, jepg2yuv420Info.width, jepg2yuv420Info.height, 1);
        return 0;
    }
public:
    ConvertInfo jepg2yuv420Info;
};

/**
 * 混音工具
*/
// 记录了需要合并的音频信息
class AudioInfo
{
public:
    AudioInfo()
    {
        filterCtx = nullptr;
        frame = nullptr;
        raw_frame_data = nullptr;
    }
    ~AudioInfo(){
        delete[] raw_frame_data;
        if(frame) av_frame_free(&frame);
        if(filterCtx) avfilter_free(filterCtx);
    }
    int init(int raw_frame_size,int volume){
        this->volume = volume;
        this->raw_frame_size = raw_frame_size;
        this->raw_frame_data = new u_char[raw_frame_size];
        return 0;
    }
    AVFilterContext *filterCtx;
    AVFrame* frame;
    int samplerate;
    int channels;
    int bitsPerSample;
    int desp;
    int volume;
    u_char* raw_frame_data;
    int raw_frame_size;
    AVSampleFormat format;
    std::string name;
};
// 混音器
class AudioMixer{
public:
    AudioMixer();
    ~AudioMixer();
public:
    // 初始化
    int init(const char* duration = "longest");
    // 释放资源
    int exit();
    // 增加音频
    int addAudioInput(int index, int samplerate, int channels, int bitsPerSample,int volume, AVSampleFormat format);
    // 设置输出
    int setAudioOutput(int samplerate, int channels, int bitsPerSample, AVSampleFormat format);
public:
    // 发送多路（2）音频的frame到上下文
    int addFrame(bool flush);
    // 获取多路（2）音频的混音frame
    int getFrame();
public:
    bool m_bStatus;                                                         //混音器状态
    AVFilterGraph* m_cFilterGraph;                                          //滤镜
    const AVFilter* m_cAmix,* m_cAbuffersink,* m_cAbuffer,*m_cAformat;      //滤镜描述结构体
    AVFilterContext* m_cAmixContext,*m_cSinkContext;                        //滤镜上下文
    vector<AudioInfo*> m_vecAudioInfos;                                     //输入音频信息
    AudioInfo* m_cOutAudioInfo;                                             //输出音频信息
};

/**
 * 推流引擎
*/
class RtmpMediaEngine{
public:
    RtmpMediaEngine(MediaMessage mm):m_bStatus(false){
        m_cMediaMessage.m_nVideoFrame = mm.m_nVideoFrame;
        m_cMediaMessage.m_nVideoHeight = mm.m_nVideoHeight;
        m_cMediaMessage.m_nVideoWight = mm.m_nVideoWight;
        m_cMediaMessage.m_nInputFigFormat = mm.m_nInputFigFormat;
        m_cMediaMessage.m_nId = mm.m_nId;
        m_cMediaMessage.m_strServerUrl = mm.m_strServerUrl;
        m_cMediaMessage.m_nAudioFormat = mm.m_nAudioFormat;
        m_cMediaMessage.m_nAudioId = mm.m_nAudioId;
        m_cMediaMessage.m_cLayout = mm.m_cLayout;
        m_cMediaMessage.m_nSampleRate = mm.m_nSampleRate;
        m_cMediaMessage.videoRate = mm.videoRate;
        m_cMediaMessage.audioRate = mm.audioRate;
        m_cMediaMessage.m_nChannelNum = mm.m_nChannelNum;
        int nSampleSize = 4;
        if(mm.m_nSampleRate == AV_SAMPLE_FMT_S16P) nSampleSize = 2;
        m_nAudioFrameSize = 1024 * mm.m_nChannelNum * nSampleSize;
        m_dAudioFrameShowTime = (float)(1000*1024)/(float)mm.m_nSampleRate;
        video_stemp = 0;
        audio_stemp = 0;
        printf("m_dAudioFrameShowTime = %f\n",m_dAudioFrameShowTime);
    }
public:
    /**
     * 初始化视频编码器
    */ 
    int initVideoEncoderCore(int64_t bitRate,int BFrameNum,bool useHw = false);
    /**
     * 初始化音频编码器
    */ 
    int initAudioEncoderCore(int64_t bitRate);
    /**
     * 释放编码器资源
    */
    int releaseEncoderCore();
    /**
     * 连接rtmp服务器
    */
    int connect();
    /**
     * 断开与rtmp服务器的连接
    */
    void disconnect();
    /**
     * 获取视频数据
    */
    int getVideoData();
    /**
    * 获取音频数据
    */
    int getAudioData();
    /**
     * 获取第二路音频数据
    */
    int getSecondAudioData();
    /**
     * 开启引擎
    */
    bool openEngine();
    /**
     * 开始视频编码
    */
    bool beginVideoEncode();
    /**
     * 开始音频编码
    */
    bool beginAudioEncode();
    /**
     * 获取音频编码结果
    */
    int getVideoStream();
    /**
     * 获取视频编码结果
    */
    int getAudioStream();
    /**
     * 处理视频编码
    */
    int headVideoStream();
    /**
     * 处理音频编码
    */
    int headAudioStream();
    /**
     * 发送视频编码
    */
    int sendVideoTag();
    /**
     * 发送音频编码
    */
    int sendAudioTag();
    /**
     * 发送一个flv标签到rtmp服务器
    */
    bool sendFlvTag(FlvTag* ft);
    bool sendPacket(u_char* data, int dataSize, int packetType, uint32_t timestamp);
    /**
     * 从一个mainNalu中获取spsNalu、ppsNalu、iNalu
    */
    bool pushStream();
    int getSpsPpsIframe(NaluUnit& mainNalu,NaluUnit& spsNalu,NaluUnit& ppsNalu,NaluUnit& iNalu);
private:
    int encodeVideoFrame(bool flush);
    int recv_the_video_pkt();
    int encodeAudioFrame(bool flush);
    int recv_the_audio_pkt();
public:
    char m_strLogPrefix[100] = "[RtmpMediaEngine log]: ";
    bool m_bStatus;                                                  // 引擎状态            
    MediaMessage m_cMediaMessage;                                    // 引擎参数
    AVCodecContext *m_cVideo_codecContext,*m_cAudio_codecContext;    // 编码器上下文       
    AVPacket *m_cVideo_pkt,*m_cAudio_pkt;                            // 码流结构          
    AVFrame *m_cVideo_frame,*m_cAudio_frame;                         // 帧结构
    RTMP *m_cRtmp;                                                   // rtmp服务器实例 
    mutex m_cVideoContextLock,m_cAudioContextLock;                   // 上下文锁
    mutex m_cSendFlvLock;                                            // 发送数据锁
    SafeQueue<std::shared_ptr<u_char>> m_queData;
    mutex m_cRawAudiosLock; 
    SafeDeque<std::shared_ptr<raw_audio_data_info>> m_queAudioData;         
    SafeQueue<std::shared_ptr<AVPacket>> m_que_video_encode_frame;   
    SafeDeque<std::shared_ptr<AVPacket>> m_que_audio_encode_frame;
    SafeQueue<std::shared_ptr<FlvVideoTag>> m_que_video_tag;
    SafeQueue<std::shared_ptr<FlvAudioTag>> m_que_audio_tag;
    SafeQueue<string> file_names;
    int m_nAudioFrameSize;                                           //一帧原始音频的长度
    double m_dAudioFrameShowTime;                                    //一帧音频的持续时长ms
    ConVert m_cConvert;                                              // 格式转换器
    AudioMixer m_cAudioMixer;                                        // 混音器
    bool m_bBeginVideo = false;                                         //当前视频是否开始发送
    bool m_bBeginAudio = false;                                         //当前音频是否开始发送
    uint64_t video_stemp;
    uint64_t audio_stemp;
    bool m_bSendAudioHeader = false;
    double m_dPreciseTimestamp = 0.0;
    bool m_first = true;
    bool m_offset = false;
    int video_offset = 0;
};
#endif