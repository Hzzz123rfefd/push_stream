#include "RtmpMediaEngine.h"
extern Log errorlog;
extern Log video_send_context_log;
extern Log video_recv_context_log;
extern Log video_send_rtmp_log;
extern Log audio_send_context_log;
extern Log audio_recv_context_log;
extern Log audio_send_rtmp_log;
extern Log info_log;

UINT Ue(BYTE *pBuff, UINT nLen, UINT &nStartBit)
{
	//计算0bit的个数
	UINT nZeroNum = 0;
	while (nStartBit < nLen * 8)
	{
		if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:按位与，%取余
		{
			break;
		}
		nZeroNum++;
		nStartBit++;
	}
	nStartBit ++;
 
 
	//计算结果
	DWORD dwRet = 0;
	for (UINT i=0; i<nZeroNum; i++)
	{
		dwRet <<= 1;
		if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
		{
			dwRet += 1;
		}
		nStartBit++;
	}
	return (1 << nZeroNum) - 1 + dwRet;
}
 
 
int Se(BYTE *pBuff, UINT nLen, UINT &nStartBit)
{
	int UeVal=Ue(pBuff,nLen,nStartBit);
	double k=UeVal;
	int nValue=ceil(k/2);//ceil函数：ceil函数的作用是求不小于给定实数的最小整数。ceil(2)=ceil(1.2)=cei(1.5)=2.00
	if (UeVal % 2==0)
		nValue=-nValue;
	return nValue;
}
 
 
DWORD u(UINT BitCount,BYTE * buf,UINT &nStartBit)
{
	DWORD dwRet = 0;
	for (UINT i=0; i<BitCount; i++)
	{
		dwRet <<= 1;
		if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
		{
			dwRet += 1;
		}
		nStartBit++;
	}
	return dwRet;
}




int RtmpMediaEngine::initVideoEncoderCore(int64_t bitRate, int BFrameNum, bool useHw)
{
    // 1 查找编码器   AV_CODEC_ID_H264
    const AVCodec* m_cVideo_codec = avcodec_find_encoder(m_cMediaMessage.m_nId);
    if (!m_cVideo_codec)
    {
        cout << "无法找到H.264编码器" << endl;
        return -1;
    }

    // 2 分派上下文
    m_cVideo_codecContext = avcodec_alloc_context3(m_cVideo_codec);
    if (!m_cVideo_codecContext)
    {
        cout << "无法分配编码器上下文" << endl;
        return -2;
    }
    // 3 设置编码器参数
    m_cVideo_codecContext->profile = FF_PROFILE_H264_HIGH;                             // 表示使用H.264的高级别配置
    m_cVideo_codecContext->bit_rate = bitRate;                                         // 设置码率
    m_cVideo_codecContext->width = m_cMediaMessage.m_nVideoWight;                      // 设置视频宽度
    m_cVideo_codecContext->height = m_cMediaMessage.m_nVideoHeight;                    // 设置视频高度
    m_cVideo_codecContext->time_base = (AVRational){1, m_cMediaMessage.m_nVideoFrame}; // 表示每个时间单位（帧）的持续时间是1/25秒，帧率为25
    m_cVideo_codecContext->framerate = (AVRational){m_cMediaMessage.m_nVideoFrame, 1}; // 设置帧率
    m_cVideo_codecContext->gop_size = 2*m_cMediaMessage.m_nVideoFrame;                                              // 设置关键帧间隔
    m_cVideo_codecContext->max_b_frames = 0;                                   // 设置I P帧之间的最大B帧数量
    m_cVideo_codecContext->pix_fmt = m_cMediaMessage.m_nInputFigFormat;                // 设置输入图像为YUV420格式  AV_PIX_FMT_YUV420P
    // 设置时间基准
    //codecContext->time_base = av_make_q(1, 90000);  // 设置时间基准为1/90000秒
    if (m_cVideo_codec->id == AV_CODEC_ID_H264)
    {
        av_opt_set(m_cVideo_codecContext->priv_data, "preset", "veryfast", 0);
        av_opt_set(m_cVideo_codecContext->priv_data, "tune", "zerolatency", 0);
    }
    // 4 打开编码器
    if (avcodec_open2(m_cVideo_codecContext, m_cVideo_codec, nullptr) < 0)
    {
        cout << "无法打开编码器\n";
        return -3;
    }

    // 5 创建AVPacket并分配内存
    m_cVideo_pkt = av_packet_alloc();
    if (!m_cVideo_pkt)
    {
        cout << "创建AVPacket错误" << endl;
        return -4;
    }

    // 6 创建AVFrame并分配内存
    m_cVideo_frame = av_frame_alloc();
    if (!m_cVideo_frame)
    {
        cout << "无法分配AVFrame\n";
        return -5;
    }
    m_cVideo_frame->width = m_cVideo_codecContext->width;
    m_cVideo_frame->height = m_cVideo_codecContext->height;
    m_cVideo_frame->format = m_cVideo_codecContext->pix_fmt; // 设置像素格式
    if (av_frame_get_buffer(m_cVideo_frame, 0) < 0)
    {
        cout << "无法分配帧缓冲区\n";
        return -6;
    }
    printf("%s finish init video encoder,video resolution:%d * %d,video frame rate:%d\n",
                m_strLogPrefix,
                m_cMediaMessage.m_nVideoWight,
                m_cMediaMessage.m_nVideoHeight,
                m_cMediaMessage.m_nVideoFrame);
    return 0;
}

int RtmpMediaEngine::initAudioEncoderCore(int64_t bitRate)
{
    // 查找编码器   AV_CODEC_ID_MP3
    const AVCodec* mAudio_codec = avcodec_find_encoder(m_cMediaMessage.m_nAudioId);
    if (!mAudio_codec)
    {
        errorlog << "无法找到编码器" << endl;
        return -1;
    }

    // 1 分派上下文
    m_cAudio_codecContext = avcodec_alloc_context3(mAudio_codec);
    if (!m_cAudio_codecContext)
    {
        errorlog << "无法分配编码器上下文" << endl;
        return -1;
    }
    
    // 2 设置编码器参数
    printf("bit_rate = %ld\n",bitRate);
    m_cAudio_codecContext->bit_rate = bitRate;                                // 设置码率
    m_cAudio_codecContext->sample_fmt = m_cMediaMessage.m_nAudioFormat;       // 设置音频格式
    m_cAudio_codecContext->sample_rate = m_cMediaMessage.m_nSampleRate;       // 设置采样频率
    m_cAudio_codecContext->ch_layout = m_cMediaMessage.m_cLayout;             // 设置声道布局为立体
    //codecContext->ch_layout.nb_channels = 2;                                // 设置声道数为2
    cout << "采样频率:"<<m_cAudio_codecContext->sample_rate<<endl;
    cout << "每个采样值占用的字节数:" << av_get_bytes_per_sample(m_cAudio_codecContext->sample_fmt) << endl;
    // 打开编码器
    if (avcodec_open2(m_cAudio_codecContext, mAudio_codec, nullptr) < 0)
    {
        cout << "无法打开编码器\n";
        return -1;
    }
    // 创建AVPacket并分配内存
    m_cAudio_pkt = av_packet_alloc();
    if (!m_cAudio_pkt)
    {
        cout << "创建AVPacket错误" << endl;
    }

    // 创建AVFrame并分配内存
    m_cAudio_frame = av_frame_alloc();
    if (!m_cAudio_frame)
    {
        cout << "无法分配AVFrame\n";
        return -1;
    }
    m_cAudio_frame->nb_samples = m_cAudio_codecContext->frame_size;
    cout << "每一帧的采样数量:" << m_cAudio_frame->nb_samples << endl;
    cout << "采样通道数:"<< m_cAudio_codecContext->ch_layout.nb_channels<<endl;
    m_cAudio_frame->ch_layout = m_cAudio_codecContext->ch_layout;
    m_cAudio_frame->format = m_cAudio_codecContext->sample_fmt; // 设置采样格式
    m_cAudio_frame->sample_rate = m_cAudio_codecContext->sample_rate;
    if (av_frame_get_buffer(m_cAudio_frame, 0) < 0)
    {
        cout << "无法分配帧缓冲区\n";
        return -1;
    }
    printf("%s finish init audio encoder\n",m_strLogPrefix);
    return 0;
}

int RtmpMediaEngine::releaseEncoderCore()
{
    avcodec_free_context(&m_cVideo_codecContext);
    avcodec_free_context(&m_cAudio_codecContext);
    av_frame_free(&m_cVideo_frame);
    av_frame_free(&m_cAudio_frame);
    av_packet_free(&m_cVideo_pkt);
    av_packet_free(&m_cAudio_pkt);
    return 0;
}

int RtmpMediaEngine::connect()
{

    // 1 初始化librtmp环境
    RTMP_LogSetLevel(RTMP_LOGINFO); // 设置日志级别

    // 2 创建一个RTMP实例
    m_cRtmp = RTMP_Alloc();
    if (m_cRtmp == NULL)
    {
        fprintf(stderr, "Failed to allocate RTMP instance\n");
        return EXIT_FAILURE;
    }
    RTMP_Init(m_cRtmp);

    // 3 设置RTMP连接URL
    if (!RTMP_SetupURL(m_cRtmp, (char *)m_cMediaMessage.m_strServerUrl.c_str()))
    {
        fprintf(stderr, "Failed to set up RTMP URL: %s\n", (char *)m_cMediaMessage.m_strServerUrl.c_str());
        RTMP_Free(m_cRtmp);
        return EXIT_FAILURE;
    }

    // 4 设置为发布模式
    int bufferSize = 5000; // 设置为5000毫秒（5秒）
    RTMP_EnableWrite(m_cRtmp);
    RTMP_SetBufferMS(m_cRtmp, bufferSize);

    // 5 连接到RTMP服务器
    if (!RTMP_Connect(m_cRtmp, NULL))
    {
        fprintf(stderr, "Failed to connect to RTMP server\n");
        RTMP_Free(m_cRtmp);
        return EXIT_FAILURE;
    }

    // 6 连接到RTMP流
    if (!RTMP_ConnectStream(m_cRtmp, 1))
    {
        fprintf(stderr, "Failed to connect to RTMP stream\n");
        RTMP_Close(m_cRtmp);
        RTMP_Free(m_cRtmp);
        return EXIT_FAILURE;
    }
    RTMP_SendCtrl(m_cRtmp, 0x01, 65535, 0);
    printf("%s connect to rtmp server,rtmp server url:%s\n",m_strLogPrefix,m_cMediaMessage.m_strServerUrl.c_str());
    return 0;
}

void RtmpMediaEngine::disconnect()
{
    RTMP_Close(m_cRtmp);
    RTMP_Free(m_cRtmp);
}

struct FileNameInfo
{
    string filename;
    long long timestamp;
    int frame_id;
};


int RtmpMediaEngine::getVideoData()
{
    int index = 1;
    string foldpath = "../base_datas/frames/";
    vector<FileNameInfo> readfilenames;
    string filename;
    long long filesize = (long long)m_cMediaMessage.m_nVideoWight*m_cMediaMessage.m_nVideoHeight*1.5;
    m_cConvert.init_jpeg2yuv420(m_cMediaMessage.m_nVideoWight,m_cMediaMessage.m_nVideoHeight);
    bool flag = false;
    while(m_bStatus){
        // 读取文件名
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(foldpath)) {
            const auto& p = entry.path(); // 获取路径
            if (fs::is_regular_file(p)) { // 确保它是一个文件
                FileNameInfo fni;
                fni.filename = p.filename().string();
                int parsedItems = sscanf(fni.filename.c_str(), "1_%lld_%06d.jpg", &fni.timestamp, &fni.frame_id);
                if (parsedItems == 2) {
                    fni.filename = foldpath + fni.filename;
                    readfilenames.push_back(fni); // 将文件名加入向量
                }
            }
        }
        if(readfilenames.size() == 0){
            if(flag == false){
                printf("wait video data ...\n");
                //info<<"wait video data ..."<<endl;
                flag = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        flag = false;
        printf("get video frame : frame number:%ld\n",readfilenames.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //info<<"get video frame : frame number:"<<readfilenames.size()<<endl;
        // 排序文件名
        sort(readfilenames.begin(),readfilenames.end(), [](const FileNameInfo& a, const FileNameInfo& b) {
            if (a.timestamp != b.timestamp) {
                return a.timestamp > b.timestamp;
            }
            return a.frame_id > b.frame_id; 
        });
        if(m_first == true){
            int id = readfilenames[readfilenames.size()-1].frame_id;
            cout<<"first id:"<<id<<endl;
            video_offset = (id - 1)* 40 * m_nAudioFrameSize/64;
            m_first = false;
            m_offset = true;
        }
        // 读取文件到内存
        while(readfilenames.size()){
            if(m_que_video_tag.size() >= m_cMediaMessage.m_nVideoFrame*2){
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            filename = readfilenames[readfilenames.size()-1].filename;
            //cout<<"filename:"<<filename<<endl;
            readfilenames.erase(readfilenames.end());
            index++;
            std::shared_ptr<u_char> data(new u_char[filesize], std::default_delete<u_char[]>());
            //u_char* data = new u_char[filesize];
            m_cConvert.jepg2yuv420(filename,data.get());
            m_queData.push(data);
            file_names.push(filename);
            if (remove(filename.c_str()) != 0) {
                printf("File delete failed\n");
            }
        }
        // while(file_names.size()){
        //     if(remove(file_names.front().c_str()) !=0){
        //         printf("File delete failed\n");
        //     }
        //     file_names.pop();
        // }
    }
    m_cConvert.exit_jpeg2yuv420();
    return 0;
}
int RtmpMediaEngine::getAudioData()
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    int data_size = this->m_nAudioFrameSize; 
    string foldpath = "../base_datas/audios/";
    vector<FileNameInfo> readfilenames;
    string filename;
    bool flag = false;
    FILE* ptr = nullptr;
    int i = 0;
    while(m_bStatus){
        // 读取文件名
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(foldpath)) {
            const auto& p = entry.path(); // 获取路径
            if (fs::is_regular_file(p)) { // 确保它是一个文件
                FileNameInfo fni;
                fni.filename = p.filename().string();
                int parsedItems = sscanf(fni.filename.c_str(), "1_%lld_%06d.wav", &fni.timestamp, &fni.frame_id);
                if (parsedItems == 2) {
                    fni.filename = foldpath + fni.filename;
                    readfilenames.push_back(fni); // 将文件名加入向量
                    //break;
                }
            }
        }
        if(readfilenames.size() == 0){
            if(flag == false){
                printf("wait audio data ...\n");
                info_log<<"wait audio data ...\n";
                flag = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        printf("get audio data , audio file name = %s,audio file num = %ld\n",readfilenames[readfilenames.size()-1].filename.c_str(),readfilenames.size());
        while(m_offset == false){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        flag = false;
        // 排序文件名
        sort(readfilenames.begin(),readfilenames.end(), [](const FileNameInfo& a, const FileNameInfo& b) {
            if (a.timestamp != b.timestamp) {
                return a.timestamp > b.timestamp;
            }
            return a.frame_id > b.frame_id; 
        });
        while(readfilenames.size()){
            if(m_que_audio_tag.size() >= 214){
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            filename = readfilenames[readfilenames.size()-1].filename;
            readfilenames.pop_back();
            std::this_thread::sleep_for(std::chrono::milliseconds(int(100)));
            ptr = fopen(filename.c_str(),"rb");  
            if(ptr == nullptr){
                cout<<"open wav file fail"<<endl;
            }
            i = 0; 
            if(video_offset!=0){
                cout<<"需要音频对齐"<<video_offset<<endl;
                char* offset = new char[video_offset];
                int size = fread(offset,1,video_offset,ptr);  
                delete[] offset;
                video_offset = 0;
            }
            //cout<<"header size:"<<size<<endl;
            //info<<"header size:"<<size<<endl;
            while(1){
                shared_ptr<raw_audio_data_info> raw_data(new raw_audio_data_info(data_size),std::default_delete<raw_audio_data_info>());
                //raw_audio_data_info* raw_data = new raw_audio_data_info(data_size);
                int size = fread(raw_data.get()->raw_data,1,data_size,ptr);   
                if(size!=data_size){
                    break;
                }
                while(m_que_audio_tag.size() >= (int)((2*1000)/m_dAudioFrameShowTime)){
                    std::this_thread::sleep_for(std::chrono::milliseconds(int(m_dAudioFrameShowTime)));
                }
                m_queAudioData.push_back(raw_data);
                std::this_thread::sleep_for(std::chrono::milliseconds(int(10)));
                i++;
            } 
            printf("get audio frame : frame number:%d\n",i);
            fclose(ptr);
            ptr == nullptr;
            //std::this_thread::sleep_for(std::chrono::milliseconds(int(5000)));
            if (remove(filename.c_str()) != 0) {
                printf("File delete failed\n");
            }
        }
    }
    return 0;
}

int RtmpMediaEngine::getSecondAudioData()
{
    // 监听文件夹中是否出现第二路音频
    int data_size = 1024 * 2 * 4;    //一帧采样点 * 双通道 * 一个采样点所占大小
    string foldpath = "../base_datas/audios2/";
    vector<FileNameInfo> readfilenames;
    string filename;
    bool flag = false;
    while(m_bStatus){
        // 读取文件名
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(foldpath)) {
            const auto& p = entry.path(); // 获取路径
            if (fs::is_regular_file(p)) { // 确保它是一个文件
                FileNameInfo fni;
                fni.filename = p.filename().string();
                int parsedItems = sscanf(fni.filename.c_str(), "1_%lld_%06d.pcm", &fni.timestamp, &fni.frame_id);
                if (parsedItems == 2) {
                    fni.filename = foldpath + fni.filename;
                    readfilenames.push_back(fni); // 将文件名加入向量
                }
            }
        }
        if(readfilenames.size() == 0){
            if(flag == false){
                printf("wait second audio data ...\n");
                flag = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        flag = false;
        printf("get second audio data\n");
        // 混音初始化
        m_cAudioMixer.addAudioInput(1,44100,2,32,1,AV_SAMPLE_FMT_FLT);
        m_cAudioMixer.addAudioInput(2,44100,2,32,5,AV_SAMPLE_FMT_FLT);
        m_cAudioMixer.setAudioOutput(44100,2,32,AV_SAMPLE_FMT_FLT);
        m_cAudioMixer.init();
        while(readfilenames.size()){
            if(m_queAudioData.size() == 0){
                printf("error : main audio is empty!\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            filename = readfilenames[readfilenames.size()-1].filename;
            readfilenames.erase(readfilenames.end());
            FILE* ptr = fopen(filename.c_str(),"rb");   
            while(1){
                // 读取一帧数据
                u_char* data = new u_char[data_size];//1024*2*4:一帧数据量
                int size = fread(data,1,data_size,ptr);   
                if(size!=data_size){
                    delete[] data;
                    m_cAudioMixer.addFrame(true);
                    break;
                }
                // 读取数据到m_vecAudioInfos的raw_frame_data
                // 读取主音频流
                int mainIndex = 0;
                while(1){
                    m_cRawAudiosLock.lock();
                    //找到第一个没有混音的位置
                    u_char* frame1 = nullptr;
                    for(mainIndex;mainIndex!=m_queAudioData.size();mainIndex++){
                        if(m_queAudioData[mainIndex]->is_merge == false){
                            m_queAudioData[mainIndex]->is_merge = true;
                            frame1 = m_queAudioData[mainIndex]->raw_data;
                            memcpy(m_cAudioMixer.m_vecAudioInfos[0]->raw_frame_data,frame1,m_cAudioMixer.m_vecAudioInfos[0]->raw_frame_size);
                            break;
                        }
                    }
                    if(frame1 == nullptr) m_cRawAudiosLock.unlock();
                    else break;
                }
                // 读取插入音频流
                memcpy(m_cAudioMixer.m_vecAudioInfos[1]->raw_frame_data,data,m_cAudioMixer.m_vecAudioInfos[1]->raw_frame_size);
                // 混音
                m_cAudioMixer.addFrame(false);
                int ret = 0;
                while ((ret = m_cAudioMixer.getFrame()) < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                //写回队列
                memcpy(m_queAudioData[mainIndex]->raw_data,m_cAudioMixer.m_cOutAudioInfo->frame->data[0],m_cAudioMixer.m_cOutAudioInfo->raw_frame_size);
                m_cRawAudiosLock.unlock();
            } 
            fclose(ptr);
            int ret = m_cAudioMixer.getFrame();
            if (remove(filename.c_str()) != 0) {
                printf("File delete failed");
            }
        }
        m_cAudioMixer.exit();
    }

    return 0;
}

bool RtmpMediaEngine::openEngine()
{
    m_bStatus = true;
    // 连接服务器
    connect();
    // 初始化
    initVideoEncoderCore(m_cMediaMessage.videoRate, 0);
    initAudioEncoderCore(m_cMediaMessage.audioRate);
    // 开启线程获取推流数据
    thread t7(&RtmpMediaEngine::getVideoData,this);
    thread t8(&RtmpMediaEngine::getAudioData,this);
    thread t9(&RtmpMediaEngine::getSecondAudioData,this);
    // 开启线程编码
    thread t1(&RtmpMediaEngine::beginVideoEncode, this);
    thread t2(&RtmpMediaEngine::beginAudioEncode, this);
    // 开启线程获取编码数据
    thread t3(&RtmpMediaEngine::getVideoStream, this);
    thread t4(&RtmpMediaEngine::getAudioStream, this);
    // 开启线程发送数据
    thread t10(&RtmpMediaEngine::pushStream,this);
    // // Get the native handle
    // pthread_t threadHandle = t10.native_handle();

    // // Set thread scheduling parameters
    // struct sched_param schedParam;
    // schedParam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    // if (pthread_setschedparam(threadHandle, SCHED_FIFO, &schedParam) != 0) {
    //     std::cerr << "Failed to set thread priority" << endl;
    // }
    t7.join();
    t8.join();
    t9.join();
    m_bStatus = false;
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    //t5.join();
    //t6.join();
    t10.join();
    //释放资源
    releaseEncoderCore();
    disconnect();
    return true;
}

bool RtmpMediaEngine::beginVideoEncode()
{
    int nNowFrameId = 0;
    while (m_bStatus == true || m_queData.size() != 0)
    {
        if(m_queData.size() == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // 1 将yuv数据存入frame
        int yuvH = m_cMediaMessage.m_nVideoHeight;
        int yuvW = m_cMediaMessage.m_nVideoWight;
        shared_ptr<u_char> yuv_buffer = m_queData.front();
        for (int i=0; i<yuvH; i++) {
            memcpy(m_cVideo_frame->data[0] + i*m_cVideo_frame->linesize[0], yuv_buffer.get() + yuvW*i, yuvW);
        }
        for (int i=0; i<yuvH/2; i++) {
            memcpy(m_cVideo_frame->data[1] + (i*m_cVideo_frame->linesize[1]), yuv_buffer.get()+yuvH*yuvW + yuvW/2*i, yuvW/2);
        }
        for (int i=0; i<yuvH/2; i++) {
            memcpy(m_cVideo_frame->data[2] + (i*m_cVideo_frame->linesize[2]), yuv_buffer.get()+yuvH*yuvW*5/4 + yuvW/2*i, yuvW/2);
        }
        m_queData.pop();
        // delete[] yuv_buffer;
        // 2 送入编码器上下文
        m_cVideo_frame->pts = nNowFrameId * (m_cVideo_codecContext->time_base.den / m_cMediaMessage.m_nVideoFrame);
        int ret = AVERROR(EAGAIN);
        while(ret == AVERROR(EAGAIN)){
            ret = encodeVideoFrame(false);
        }
        nNowFrameId++;
    }
    encodeVideoFrame(true);
    return true;
}

bool RtmpMediaEngine::beginAudioEncode()
{
    int nNowFrameId = 0;
    int data_size = av_get_bytes_per_sample(m_cAudio_codecContext->sample_fmt); // 每个采样值占用的字节数
    if (data_size < 0)
    {
        cout << "fail to calculate data_size!" << endl;
        return false;
    }
    while(m_bStatus == true || m_queAudioData.size() != 0){
        if(m_queAudioData.size() == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        m_cRawAudiosLock.lock();
        shared_ptr<raw_audio_data_info> raw_audio_data = m_queAudioData.front();
        u_char *cData = raw_audio_data.get()->raw_data;
        m_queAudioData.pop_front();
        m_cRawAudiosLock.unlock();
        int index = 0;
        for (int i = 0; i < m_cAudio_frame->nb_samples; i++){
            for(int ch = 0;ch<m_cAudio_frame->ch_layout.nb_channels;ch++){
                for(int j = 0;j<4;j++){
                    m_cAudio_frame->data[ch][i*data_size+j] =  cData[index++];
                }
            }
        }
        //delete raw_audio_data;
        m_cAudio_frame->pts = nNowFrameId++;
        // int ret = AVERROR(EAGAIN);
        // while(ret == AVERROR(EAGAIN)){
        //     ret = encodeAudioFrame(false);
        // }
        encodeAudioFrame(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    encodeAudioFrame(true);
    return true;
}

int RtmpMediaEngine::getVideoStream()
{
    int ret = -100;
    RTMPMetadata metaData;
    NaluUnit nalu,spsNalu,ppsNalu,iNalu;
    uint64_t timestemp = 40;
    //FlvVideoTag flv;
    int num = 1;
    //AVPacket *pTem;
    while(1){
        ret = recv_the_video_pkt();
        if (ret == 0)
        { 
            // std::shared_ptr<AVPacket> pTem(av_packet_alloc(), [](AVPacket* pkt) {
            //     av_packet_free(&pkt);
            //     pkt = nullptr;
            // });
            // pTem.get()->size = m_cVideo_pkt->size;
            // pTem.get()->data = (u_int8_t*)av_malloc(m_cVideo_pkt->size);
            // memcpy(pTem.get()->data,m_cVideo_pkt->data,m_cVideo_pkt->size);
            // m_que_video_encode_frame.push(pTem);
            std::shared_ptr<FlvVideoTag> flv(new FlvVideoTag(),std::default_delete<FlvVideoTag>());
            nalu.fromPkt(m_cVideo_pkt);
            if(nalu.type == 7){      //7 8 5 都在这个帧里面   sps pps 关键帧
                getSpsPpsIframe(nalu,spsNalu,ppsNalu,iNalu);
                metaData.fromSpsPps(spsNalu,ppsNalu);
                flv->fromMeteDate(metaData);
                flv->timeStamp = 0;
                flv->pts = m_cVideo_pkt->pts;
                //sendFlvTag(&flv);
                m_que_video_tag.push(flv);
                std::shared_ptr<FlvVideoTag> flv2(new FlvVideoTag(),std::default_delete<FlvVideoTag>());
                flv2->fromCommonNalu(iNalu);
                flv2->timeStamp = timestemp;
                flv2->pts = m_cVideo_pkt->pts;
                timestemp = timestemp + (1000/m_cMediaMessage.m_nVideoFrame);
                m_que_video_tag.push(flv2);
                //sendFlvTag(&flv);
            }else{  //普通帧
                flv->fromCommonNalu(nalu);
                flv->timeStamp = timestemp;
                flv->pts = m_cVideo_pkt->pts;
                timestemp = timestemp + (1000/m_cMediaMessage.m_nVideoFrame);
                m_que_video_tag.push(flv);
                //sendFlvTag(&flv);
            }
        }
        else if (ret == -1)
        { // 读取发生错误
            releaseEncoderCore();
            return -1;
        }
        else if (ret == AVERROR_EOF)
            break;
    }
    return 0;
}

int RtmpMediaEngine::getAudioStream()
{
    int ret = -100;
    //double preciseTimestamp = 0.0; // 精确时间戳，以毫秒为单位
    int lastSentTimestamp = 0; // 上一次发送的RTMP时间戳
    //FlvAudioTag audioFlv;
    RawDataBlock audioData;
    short sf_index = 4;
    short channel_configuration = 2;
    if(m_cMediaMessage.m_nSampleRate == 16000) sf_index = 8;
    if(m_cMediaMessage.m_nChannelNum == 1) channel_configuration = 1;
    ADTSHeader adtsh(sf_index,channel_configuration);
    int num = 1;
    AVPacket* pTem;
    while(1){
        ret = recv_the_audio_pkt();
        if(ret == 0){
            m_dPreciseTimestamp += m_dAudioFrameShowTime;
            int roundedTimestamp = (int)round(m_dPreciseTimestamp);
            int timestampDelta = roundedTimestamp - lastSentTimestamp;
            std::shared_ptr<FlvAudioTag> audioFlv(new FlvAudioTag(),std::default_delete<FlvAudioTag>());
            if(m_bSendAudioHeader == false){
                std::shared_ptr<FlvAudioTag> audioFlvHeader(new FlvAudioTag(),std::default_delete<FlvAudioTag>());
                m_bSendAudioHeader = true;
                audioFlvHeader->fromADTSHeader(adtsh);
                audioFlvHeader->pts = m_cAudio_pkt->pts;
                audioFlvHeader->timeStamp = 0;
                sendFlvTag(audioFlvHeader.get());
            }
            audioData.fromPkt(m_cAudio_pkt);
            audioFlv->fromRawDataBlock(audioData);
            audioFlv->timeStamp = lastSentTimestamp + timestampDelta;
            audioFlv->pts = m_cAudio_pkt->pts;
            lastSentTimestamp = roundedTimestamp;
            m_que_audio_tag.push(audioFlv);
        }
        else if (ret == -1)
        { // 读取发生错误
            releaseEncoderCore();
            return -1;
        }
        else if (ret == AVERROR_EOF)
            break;
    }
    return 0;
}

int RtmpMediaEngine::headVideoStream()
{
    int timestemp = 0;
    RTMPMetadata metaData;
    NaluUnit nalu,spsNalu,ppsNalu,iNalu;
    //FlvVideoTag flv;
    int num = 1;
    while(m_bStatus || m_que_video_encode_frame.size()!=0)
    {
        if(m_que_video_encode_frame.size() == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        std::shared_ptr<FlvVideoTag> flv(new FlvVideoTag(),std::default_delete<FlvVideoTag>());
        auto start = std::chrono::high_resolution_clock::now();
        shared_ptr<AVPacket> pkt = m_que_video_encode_frame.front();
        m_que_video_encode_frame.pop();
        nalu.fromPkt(pkt.get());
        //cout<<"video frame id:"<<num++<<endl;
        if(nalu.type == 7){      //7 8 5 都在这个帧里面   sps pps 关键帧
            getSpsPpsIframe(nalu,spsNalu,ppsNalu,iNalu);
            metaData.fromSpsPps(spsNalu,ppsNalu);
            flv->fromMeteDate(metaData);
            flv->timeStamp = timestemp;
            //sendFlvTag(&flv);
            m_que_video_tag.push(flv);
            std::shared_ptr<FlvVideoTag> flv2(new FlvVideoTag(),std::default_delete<FlvVideoTag>());
            flv2->fromCommonNalu(iNalu);
            flv2->timeStamp = timestemp;
            timestemp = timestemp + (1000/m_cMediaMessage.m_nVideoFrame);
            m_que_video_tag.push(flv2);
            //sendFlvTag(&flv);
        }else{  //普通帧
            flv->fromCommonNalu(nalu);
            flv->timeStamp = timestemp;
            timestemp = timestemp + (1000/m_cMediaMessage.m_nVideoFrame);
            m_que_video_tag.push(flv);
            //sendFlvTag(&flv);
        }
        //av_packet_free(&pkt);
        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // std::this_thread::sleep_for(std::chrono::milliseconds(40-duration.count()-1)); 
        // 输出程序运行时间
        // std::cout << "程序运行时间: " << duration.count() << " 毫秒" << endl;
        // std::cout <<"m_que_video_encode_frame size:"<<m_que_video_encode_frame.size()<<endl;
    }
    return 0;
}

int RtmpMediaEngine::headAudioStream()
{
    double preciseTimestamp = 0.0; // 精确时间戳，以毫秒为单位
    int lastSentTimestamp = 0; // 上一次发送的RTMP时间戳
    //FlvAudioTag audioFlv;
    RawDataBlock audioData;
    short sf_index = 4;
    short channel_configuration = 2;
    if(m_cMediaMessage.m_nSampleRate == 16000) sf_index = 8;
    if(m_cMediaMessage.m_nChannelNum == 1) channel_configuration = 1;
    ADTSHeader adtsh(sf_index,channel_configuration);
    bool sendAudioHeader = false;
    int num = 1;
    printf("m_dAudioFrameShowTime = %2f\n",m_dAudioFrameShowTime);
    while(m_bStatus || m_que_audio_encode_frame.size() !=0 ){
        if(m_que_audio_encode_frame.size() == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        std::shared_ptr<FlvAudioTag> audioFlv(new FlvAudioTag(),std::default_delete<FlvAudioTag>());
        shared_ptr<AVPacket> pkt = m_que_audio_encode_frame.front();
        m_que_audio_encode_frame.pop_front();
        if(sendAudioHeader == false){
            std::shared_ptr<FlvAudioTag> audioFlvHeader(new FlvAudioTag(),std::default_delete<FlvAudioTag>());
            sendAudioHeader = true;
            audioFlvHeader->fromADTSHeader(adtsh);
            m_que_audio_tag.push(audioFlvHeader);
            //sendFlvTag(&audioFlv);
        }
        audioData.fromPkt(pkt.get());
        audioFlv->fromRawDataBlock(audioData);
        preciseTimestamp += m_dAudioFrameShowTime;
        int roundedTimestamp = (int)round(preciseTimestamp);
        int timestampDelta = roundedTimestamp - lastSentTimestamp;
        audioFlv->timeStamp = lastSentTimestamp + timestampDelta;;
        lastSentTimestamp = roundedTimestamp;
        m_que_audio_tag.push(audioFlv);
        //sendFlvTag(&audioFlv);
        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // std::this_thread::sleep_for(std::chrono::milliseconds((int)(m_dAudioFrameShowTime) - duration.count()));
        // 输出程序运行时间
        // std::cout << "程序运行时间: " << duration.count() << " 毫秒" << endl;
        // std::cout <<"m_que_audio_encode_frame size:"<<m_que_audio_encode_frame.size()<<endl; 
        //av_packet_free(&pkt);
    }
    return 0;
}

int RtmpMediaEngine::sendVideoTag()
{
    int accumulated_delay = 0;  //us
    int other = 0;  //ms
    while(m_bStatus || m_que_video_tag.size()!=0){
        if(m_que_video_tag.size() == 0){
        //if(m_que_video_tag.size() == 0 || video_stemp - audio_stemp > 500){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        // beginVideo = true;
        // while(beginVideo == false || beginAudio == false){
        //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //     continue;
        // }
        auto start = std::chrono::high_resolution_clock::now();
        shared_ptr<FlvVideoTag> flv = m_que_video_tag.front();
        sendFlvTag(flv.get());
        if(flv->avcPacketType != 0x00){
            this->video_stemp = flv->timeStamp;
        }
        m_que_video_tag.pop();
        if(flv->avcPacketType == 0x00){
            flv = m_que_video_tag.front();
            sendFlvTag(flv.get());
            this->video_stemp = flv->timeStamp;
            m_que_video_tag.pop();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        accumulated_delay += duration.count();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000/m_cMediaMessage.m_nVideoFrame - accumulated_delay/1000));
        accumulated_delay = accumulated_delay%1000;
        // other = other + 5;
        // if(other == 5000){
        //     std::this_thread::sleep_for(std::chrono::milliseconds(other - 1000));
        //     other = 0;
        // }
    }
    return 0;
}

int RtmpMediaEngine::sendAudioTag()
{
    int accumulated_delay = 0;  //us
    int other = 0;
    while(m_bStatus || m_que_audio_tag.size()!=0){
        if(m_que_audio_tag.size() == 0){
        //if(m_que_audio_tag.size() == 0 || audio_stemp - video_stemp > 500){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        m_bBeginAudio = true;
        while(m_bBeginVideo == false || m_bBeginAudio == false){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        auto start = std::chrono::high_resolution_clock::now();
        shared_ptr<FlvAudioTag> flv = m_que_audio_tag.front();
        sendFlvTag(flv.get());
        this->audio_stemp = flv->timeStamp;
        m_que_audio_tag.pop();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        accumulated_delay += duration.count();
        //cout<<(int)(m_dAudioFrameShowTime)- accumulated_delay/1000 <<endl;
        std::this_thread::sleep_for(std::chrono::milliseconds((int)(m_dAudioFrameShowTime)- accumulated_delay/1000));
        // other = other + 5;
        // if(other == 5000){
        //     std::this_thread::sleep_for(std::chrono::milliseconds(other - 1000));
        //     other = 0;
        // }
        accumulated_delay = accumulated_delay%1000;
    }
    return 0;
}

bool RtmpMediaEngine::sendPacket(u_char* data, int dataSize, int packetType, uint32_t timestamp) {
    RTMPPacket packet;
    RTMPPacket_Alloc(&packet, dataSize);
    RTMPPacket_Reset(&packet);

    packet.m_nChannel = 0x04; // Video channel
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_nTimeStamp = timestamp;
    packet.m_packetType = packetType;
    packet.m_nBodySize = dataSize;
    packet.m_nInfoField2 = m_cRtmp->m_stream_id;
    memcpy(packet.m_body, data, dataSize);

    bool result = RTMP_SendPacket(m_cRtmp, &packet, 0);

    RTMPPacket_Free(&packet);
    return result;
}


bool RtmpMediaEngine::sendFlvTag(FlvTag *ft)
{
    RTMPPacket packet;
	RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet,ft->size); 
    FlvVideoTag* fvt = nullptr;
    FlvAudioTag* fat = nullptr;
    if(ft->FlvType == 9){                       //发送视频数据
        fvt = (FlvVideoTag*)ft; 
        packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
        packet.m_nChannel = 0x04;  
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;  
        packet.m_nTimeStamp = fvt->timeStamp;  
        packet.m_hasAbsTimestamp = 0;
        packet.m_nInfoField2 = m_cRtmp->m_stream_id;
        packet.m_nBodySize = fvt->size;
        memcpy(packet.m_body,fvt->data,fvt->size);
        audio_send_rtmp_log<<"["<<Log::getCurrentTime()<< "]"<<"send video flv tag,flv body size:"<<ft->size<<" ,timestemp:"<<ft->timeStamp<<",pts:"<<ft->pts<<endl;
    }else if(ft->FlvType == 8){                            //发送音频数据
        fat = (FlvAudioTag*)ft;
        packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
        packet.m_nChannel = 0x04;
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;      //RTMP_PACKET_SIZE_MEDIUM
        packet.m_nTimeStamp = fat->timeStamp;
        packet.m_nInfoField2 = m_cRtmp->m_stream_id;
        packet.m_hasAbsTimestamp = 0;
        packet.m_nBodySize = fat->size;
        memcpy(packet.m_body,fat->data,fat->size);
        audio_send_rtmp_log<<"["<<Log::getCurrentTime()<< "]"<<"send audio flv tag,flv body size:"<<ft->size<<" ,timestemp:"<<ft->timeStamp<<",pts:"<<ft->pts<<endl;
    }  
    //cout<<"tyep:"<<packet.m_hasAbsTimestamp<<endl;
    auto start = std::chrono::high_resolution_clock::now();
    //m_cSendFlvLock.lock();
    int nRet = 1;
    if (!m_cRtmp || !RTMP_IsConnected(m_cRtmp)) {
        cout<<"RTMP connection is not established or is invalid."<<endl;
    }
	nRet = RTMP_SendPacket(m_cRtmp,&packet,1);
    //m_cSendFlvLock.unlock();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //cout<<"pack size:"<<packet.m_nBodySize<<endl;
    //cout<<"send time:(ws)"<<duration.count()<<endl;
    if(nRet != 1){
        cout<<"["<<Log::getCurrentTime()<< "]"<<"send rtmp server fail!"<<endl;
        RTMPPacket_Free(&packet);
        return false;
    }
    RTMPPacket_Free(&packet);
    return true;
}

bool RtmpMediaEngine::pushStream()
{
    int accumulated_delay = 0;         //us
    int other = 0;                     //ms
    shared_ptr<FlvVideoTag> vflv;
    shared_ptr<FlvAudioTag> aflv;
    bool begin_flag = false;
    while(m_bStatus || m_que_video_tag.size()!=0){
        if(m_que_video_tag.size() < 8 || m_que_audio_tag.size()< 5){
            if(begin_flag == true){
                while(m_que_video_tag.size()) m_que_video_tag.pop();
                while(m_que_audio_tag.size()) m_que_audio_tag.pop();
            }
            if(m_que_video_tag.size() == 0 && m_que_audio_tag.size() == 0) begin_flag = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        begin_flag = true;
        auto start = std::chrono::high_resolution_clock::now();
        // 1
        aflv = m_que_audio_tag.front();
        sendFlvTag(aflv.get());
        m_que_audio_tag.pop();
        // 2
        vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }
        // 3
        aflv = m_que_audio_tag.front();
        sendFlvTag(aflv.get());
        m_que_audio_tag.pop();
        // 4
        vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }
        // 5
       vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }
        // 6
        aflv = m_que_audio_tag.front();
        sendFlvTag(aflv.get());
        m_que_audio_tag.pop();
        // 7
       vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }
        // 8
        aflv = m_que_audio_tag.front();
        sendFlvTag(aflv.get());
        m_que_audio_tag.pop();
        // 9
       vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }
        // 10
        vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }        
        // 11
        aflv = m_que_audio_tag.front();
        sendFlvTag(aflv.get());
        m_que_audio_tag.pop();
        // 12
        vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }   
        // 13
       vflv = m_que_video_tag.front();
        sendFlvTag(vflv.get());
        m_que_video_tag.pop();
        if(vflv->avcPacketType == 0x00){
            vflv = m_que_video_tag.front();
            sendFlvTag(vflv.get());
            m_que_video_tag.pop();
        }   
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        accumulated_delay = accumulated_delay + 320*1000 - duration.count();
        //cout<<"accumulated_delay:"<<accumulated_delay<<endl;
        if(accumulated_delay > 0){
            std::this_thread::sleep_for(std::chrono::microseconds(accumulated_delay));
            accumulated_delay = 0;
        }else{
        }
        //accumulated_delay += duration.count();
        //std::this_thread::sleep_for(std::chrono::microseconds(320*1000 - duration.count()));
        //cout<<"duration:"<<320*1000 - duration.count()<<endl;
        //accumulated_delay = accumulated_delay%1000;
    }
    return 0;
}

int RtmpMediaEngine::getSpsPpsIframe(NaluUnit &mainNalu, NaluUnit &spsNalu, NaluUnit &ppsNalu, NaluUnit &iNalu)
{
    int flag = 0;
    int pos = 0;
    int starsize = 0;
    for(int i = 0;i<mainNalu.size - 4; ++i){
        if ((mainNalu.data[i] == 0x00 && mainNalu.data[i+1] == 0x00 && mainNalu.data[i+2] == 0x01) || (mainNalu.data[i] == 0x00 && mainNalu.data[i+1] == 0x00 && mainNalu.data[i+2] == 0x00 && mainNalu.data[i+3] == 0x01)) {
            int startCodeLength = (mainNalu.data[i + 2] == 0x01) ? 3 : 4;
            //cout<<"startCodeLength:"<<startCodeLength<<endl;
            if(flag == 0 && (mainNalu.data[i + startCodeLength] & 0x1F) == 8){  //找到sps
                //cout<<"flag == 0 && mainNalu.data[i + startCodeLength] & 0x1F == 8"<<endl;
                spsNalu.size = i - pos;
                spsNalu.type = mainNalu.data[pos] & 0x1F;
                memcpy(spsNalu.data,mainNalu.data,spsNalu.size);
                flag++;
                pos = i;
                starsize = startCodeLength;
            }else if(flag == 1 && (mainNalu.data[i + startCodeLength] & 0x1F) == 5){  //找到pps
                //cout<<"flag == 1 && mainNalu.data[i + startCodeLength] & 0x1F == 5"<<endl;
                ppsNalu.size = i - pos - starsize;
                ppsNalu.type = mainNalu.data[pos + starsize] & 0x1F;
                memcpy(ppsNalu.data,mainNalu.data+pos+starsize,ppsNalu.size);   
                pos = i;
                starsize = startCodeLength;
                iNalu.size = mainNalu.size - pos - starsize;
                iNalu.type = mainNalu.data[pos + starsize] & 0x1F;
                memcpy(iNalu.data,mainNalu.data+pos+starsize,iNalu.size);   
                return 0; 
            }
            i = i+3;
        }
    }
    return 0;
}


int RtmpMediaEngine::encodeVideoFrame(bool flush)
{
    int ret = 0;
    if (!flush)
    {
        video_send_context_log<<"["<<Log::getCurrentTime()<< "]"<< "send frame to video encoder with pts:" << m_cVideo_frame->pts << "\n";
    }
    else
    {
        video_send_context_log <<"["<<Log::getCurrentTime()<< "]"<< "send last video frame" << "\n";
    }
    // 传入nullptr表示编码结束，此时刷新AVPacket中的缓存空间；否则继续编码frame
    m_cVideoContextLock.lock();
    ret = avcodec_send_frame(m_cVideo_codecContext, flush ? nullptr : m_cVideo_frame);
    m_cVideoContextLock.unlock();
    /**
     *  * @retval AVERROR_EOF       the encoder has been flushed, and no new frames can
        *                           be sent to it
        * @retval AVERROR(EINVAL)   codec not opened, it is a decoder, or requires flush
        * @retval AVERROR(ENOMEM)   failed to add packet to internal queue, or similar
    */
    if (ret != 0)
    {
        if(ret == AVERROR(EINVAL)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"codec not opened, it is a decoder, or requires flush" << "\n";
        }else if(ret == AVERROR(ENOMEM)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"failed to add packet to internal queue, or similar" << "\n";
        }else if(ret == AVERROR_EOF){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"the encoder has been flushed, and no new frames can be sent to it" << "\n";
        }else if(ret != AVERROR(EAGAIN)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"legitimate encoding errors" << "\n";
        }
        return ret;
    }
    return ret;
}

int RtmpMediaEngine::recv_the_video_pkt()
{
    m_cVideoContextLock.lock();
    int ret = avcodec_receive_packet(m_cVideo_codecContext, m_cVideo_pkt);
    m_cVideoContextLock.unlock();
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        // cout << "AVERROR_EOF:上下文中没有码流；AVERROR(EAGAIN)：输出不可用\n";
        return ret;
    }
    else if (ret != 0)
    {
        cout <<"["<<Log::getCurrentTime()<< "] "<< "recv pkt fail!!" << endl;
        return 1;
    }
    video_recv_context_log <<"["<<Log::getCurrentTime()<< "] "<<"get video stream package with dts:" << m_cVideo_pkt->dts << ",pts:" << m_cVideo_pkt->pts << "\n";
    return 0;
}

int RtmpMediaEngine::encodeAudioFrame(bool flush)
{
    int ret = 0;
    if (!flush)
    {
        audio_send_context_log <<"["<<Log::getCurrentTime()<< "] "<< "send frame to audio encoder with pts:" << m_cAudio_frame->pts << endl;
    }
    else
    {
        audio_send_context_log <<"["<<Log::getCurrentTime()<< "] "<< "send last audio frame" << endl;
    }
    // 传入nullptr表示编码结束，此时刷新AVPacket中的缓存空间；否则继续编码frame
    m_cAudioContextLock.lock();
    ret = avcodec_send_frame(m_cAudio_codecContext, flush ? nullptr : m_cAudio_frame);
    m_cAudioContextLock.unlock();
    if (ret != 0)
    {
        if(ret == AVERROR(EINVAL)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"codec not opened, it is a decoder, or requires flush" << "\n";
        }else if(ret == AVERROR(ENOMEM)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"failed to add packet to internal queue, or similar" << "\n";
        }else if(ret == AVERROR_EOF){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"the encoder has been flushed, and no new frames can be sent to it" << "\n";
        }else if(ret != AVERROR(EAGAIN)){
            errorlog << "["<<Log::getCurrentTime()<< "]"<<"legitimate encoding errors(audio)" << "\n";
        }
        return ret;
    }
    return ret;
}

int RtmpMediaEngine::recv_the_audio_pkt()
{
    m_cAudioContextLock.lock();
    int ret = avcodec_receive_packet(m_cAudio_codecContext, m_cAudio_pkt);
    m_cAudioContextLock.unlock();
    //cout << "ret = " << ret << endl;
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        // cout << "AVERROR_EOF:上下文中没有码流；AVERROR(EAGAIN)：输出不可用\n";
        return ret;
    }
    else if (ret != 0)
    {
        errorlog << "recevice audio pkt fail!" << endl;
        return 1;
    }
    audio_recv_context_log <<"["<<Log::getCurrentTime()<< "] "<<"recv audio stream package with pts:" << m_cAudio_pkt->pts << endl;
    //cout << "get encoded package with dts:" << pkt->dts << ",pts:" << pkt->pts << endl;
    return 0;
}

int FlvVideoTag::fromCommonNalu(NaluUnit &nu)
{
    int i = 0;
    size = nu.size+9;
    if(nu.type == 5){
        data[i++] = 0x17;// 1:Iframe  7:AVC
        frameType = 1;
    }
    else{
        data[i++] = 0x27;// 2:Pframe  7:AVC
        frameType = 2;
    }
    codecID = 7;

	data[i++] = 0x01;// AVC NALU 
    avcPacketType = 0x01;


	data[i++] = 0x00;
	data[i++] = 0x00;
	data[i++] = 0x00;// compositiontime
    compositionTime = 0;
 
	// NALU size
	data[i++] = nu.size>>24;
	data[i++] = nu.size>>16;
	data[i++] = nu.size>>8;
	data[i++] = nu.size&0xff;;
 
	// NALU data
	memcpy(&data[i],nu.data,nu.size);
    return 0;
}

int FlvVideoTag::fromMeteDate(RTMPMetadata &metaData)
{
	int i = 0;
	data[i++] = 0x17; // 1:keyframe  7:AVC
    frameType = 1;
    codecID = 7;

	data[i++] = 0x00; // AVC sequence header
    avcPacketType = 0x00;
 
	data[i++] = 0x00;
	data[i++] = 0x00;
	data[i++] = 0x00; // fill in 0;
    compositionTime = 0;
 
	// AVCDecoderConfigurationRecord.
	data[i++] = 0x01; // configurationVersion
	data[i++] = metaData.Sps[1]; // AVCProfileIndication
	data[i++] = metaData.Sps[2]; // profile_compatibility
	data[i++] = metaData.Sps[3]; // AVCLevelIndication 
    data[i++] = 0xff; // lengthSizeMinusOne  
 
    // sps nums
	data[i++] = 0xE1; //&0x1f
	// sps data length
	data[i++] = metaData.nSpsLen>>8;
	data[i++] = metaData.nSpsLen&0xff;
	// sps data
	memcpy(&data[i],metaData.Sps,metaData.nSpsLen);
	i= i+metaData.nSpsLen;
 
	// pps nums
	data[i++] = 0x01; //&0x1f
	// pps data length 
	data[i++] = metaData.nPpsLen>>8;
	data[i++] = metaData.nPpsLen&0xff;
	// sps data
	memcpy(&data[i],metaData.Pps,metaData.nPpsLen);
	i= i+metaData.nPpsLen;
    size = i;
    return 0;
}

int NaluUnit::fromPkt(AVPacket *pkt)
{
    int startCodeLength = 0;
    uint8_t* pktdata = pkt->data;
    // 检查起始码长度
    if (pktdata[0] == 0x00 && pktdata[1] == 0x00 && pktdata[2] == 0x00 && pktdata[3] == 0x01) {
        startCodeLength = 4; // 起始码是 0x00000001
    } else if (pktdata[0] == 0x00 && pktdata[1] == 0x00 && pktdata[2] == 0x01) {
        startCodeLength = 3; // 起始码是 0x000001
    } else {
        // 无效的起始码，可能需要更复杂的逻辑来处理不同的情况或格式
        cout<<"pkt format error"<<endl;
        return -1;
    }
    // 跳过起始码，获取 NALU 头部的第一个字节
    type = pktdata[startCodeLength] & 0x1F;
    size = pkt->size - startCodeLength;
    memcpy(data,pkt->data+startCodeLength,size);
    return 0;
}

int RTMPMetadata::fromSpsPps(NaluUnit &sps, NaluUnit &pps)
{
	nSpsLen = sps.size;
	memcpy(Sps,sps.data,sps.size);
	nPpsLen = pps.size;
	memcpy(Pps,pps.data,pps.size);
    h264_decode_sps();
    nFrameRate = 25;
    return 0;
}

bool RTMPMetadata::h264_decode_sps()
{
	UINT StartBit=0; 
	int forbidden_zero_bit=u(1,Sps,StartBit);
	int nal_ref_idc=u(2,Sps,StartBit);
	int nal_unit_type=u(5,Sps,StartBit);
	if(nal_unit_type==7)
	{
		int profile_idc=u(8,Sps,StartBit);
		int constraint_set0_flag=u(1,Sps,StartBit);//(buf[1] & 0x80)>>7;
		int constraint_set1_flag=u(1,Sps,StartBit);//(buf[1] & 0x40)>>6;
		int constraint_set2_flag=u(1,Sps,StartBit);//(buf[1] & 0x20)>>5;
		int constraint_set3_flag=u(1,Sps,StartBit);//(buf[1] & 0x10)>>4;
		int reserved_zero_4bits=u(4,Sps,StartBit);
		int level_idc=u(8,Sps,StartBit);
 
		int seq_parameter_set_id=Ue(Sps,nSpsLen,StartBit);
 
		if( profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 144 )
		{
			int chroma_format_idc=Ue(Sps,nSpsLen,StartBit);
			if( chroma_format_idc == 3 )
				int residual_colour_transform_flag=u(1,Sps,StartBit);
			int bit_depth_luma_minus8=Ue(Sps,nSpsLen,StartBit);
			int bit_depth_chroma_minus8=Ue(Sps,nSpsLen,StartBit);
			int qpprime_y_zero_transform_bypass_flag=u(1,Sps,StartBit);
			int seq_scaling_matrix_present_flag=u(1,Sps,StartBit);
 
			int seq_scaling_list_present_flag[8];
			if( seq_scaling_matrix_present_flag )
			{
				for( int i = 0; i < 8; i++ ) {
					seq_scaling_list_present_flag[i]=u(1,Sps,StartBit);
				}
			}
		}
		int log2_max_frame_num_minus4=Ue(Sps,nSpsLen,StartBit);
		int pic_order_cnt_type=Ue(Sps,nSpsLen,StartBit);
		if( pic_order_cnt_type == 0 )
			int log2_max_pic_order_cnt_lsb_minus4=Ue(Sps,nSpsLen,StartBit);
		else if( pic_order_cnt_type == 1 )
		{
			int delta_pic_order_always_zero_flag=u(1,Sps,StartBit);
			int offset_for_non_ref_pic=Se(Sps,nSpsLen,StartBit);
			int offset_for_top_to_bottom_field=Se(Sps,nSpsLen,StartBit);
			int num_ref_frames_in_pic_order_cnt_cycle=Ue(Sps,nSpsLen,StartBit);
 
			int *offset_for_ref_frame=new int[num_ref_frames_in_pic_order_cnt_cycle];
			for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
				offset_for_ref_frame[i]=Se(Sps,nSpsLen,StartBit);
			delete [] offset_for_ref_frame;
		}
		int num_ref_frames=Ue(Sps,nSpsLen,StartBit);
		int gaps_in_frame_num_value_allowed_flag=u(1,Sps,StartBit);
		int pic_width_in_mbs_minus1=Ue(Sps,nSpsLen,StartBit);
		int pic_height_in_map_units_minus1=Ue(Sps,nSpsLen,StartBit);
 
		nWidth=(pic_width_in_mbs_minus1+1)*16;
		nHeight=(pic_height_in_map_units_minus1+1)*16;
 
		return true;
	}
	else
		return false;
}



int RawDataBlock::fromPkt(AVPacket *pkt)
{
    size = pkt->size;
    memcpy(data,pkt->data,size);
    return 0;
}

int FlvAudioTag::fromRawDataBlock(RawDataBlock &rdb)
{
    int i=0;
    size = rdb.size + 2; //2是标签头长度
    //填充标签头
	data[i++] = 0xAF;
	data[i++] = 0x01;
	//Aac raw data without 7bytes frame header
	memcpy(&data[i],rdb.data,rdb.size);
    return 0;
}

int FlvAudioTag::fromADTSHeader(ADTSHeader &adtsh)
{
    int i=0;
    size = 4; //2是标签头长度 2是配置长度
    data[0]=0xAF;
	data[1]=0x00;

	unsigned char tmp = 0;
	tmp |= (((unsigned char)adtsh.profile) << 3);	
	tmp |= ((((unsigned char)adtsh.sf_index) >> 1) & 0x07);

	data[2]=tmp;
	
	tmp = 0;
	tmp |= (((unsigned char)adtsh.sf_index) << 7);	
	tmp |= (((unsigned char)adtsh.channel_configuration) << 3) & 0x78; 
	
	data[3]=tmp;
    return 0;
}

AudioMixer::AudioMixer()
{
    m_bStatus = false;
    m_cOutAudioInfo = new AudioInfo();
}

AudioMixer::~AudioMixer()
{
    delete m_cOutAudioInfo;
    for(auto& iter:m_vecAudioInfos){
        if(iter) delete iter;
    }
}

int AudioMixer::init(const char *duration)
{
    // 创建滤镜图
    m_cFilterGraph = avfilter_graph_alloc();
    if(!m_cFilterGraph){
        printf("unable to create avfilter graph\n");
        return -1;
    }
    // 混音初始化
    m_cAmix = avfilter_get_by_name("amix");
    if(!m_cAmix){
        printf("unable to create amix filter\n");
        return -1;
    }
    m_cAmixContext = avfilter_graph_alloc_filter(m_cFilterGraph,m_cAmix,"amix");
    if(!m_cAmixContext){
        printf("unable to create amix context\n");
        return -1;
    }
    char args[512];
    snprintf(args, sizeof(args), "inputs=%ld:duration=%s:dropout_transition=0",
            m_vecAudioInfos.size(),duration);
    if(avfilter_init_str(m_cAmixContext,args)<0){
        printf("unable to init amix filter");
        return -1;
    }

    // 格式转换初始化
    m_cAbuffersink = avfilter_get_by_name("abuffersink");
    m_cSinkContext = avfilter_graph_alloc_filter(m_cFilterGraph, m_cAbuffersink, "sink");
    if(avfilter_init_str(m_cSinkContext, nullptr) != 0)
    {
        printf("unable to init sink filter!\n");
        return -1;
    }
    // 输入初始化
    int i = 0;
    for(auto& iter : m_vecAudioInfos)
    {
        m_cAbuffer = avfilter_get_by_name("abuffer");
        snprintf(args, sizeof(args),
                 "sample_rate=%d:sample_fmt=%s:channel_layout=0x3",
                 iter->samplerate,
                 av_get_sample_fmt_name(iter->format)
                 );
 
        iter->filterCtx = avfilter_graph_alloc_filter(m_cFilterGraph, m_cAbuffer, iter->name.c_str());
 
        if(avfilter_init_str(iter->filterCtx, args) != 0)
        {
            printf("unable to init input filter!\n");
            return -1;
        }
        // if(avfilter_link(iter->filterCtx, 0, m_cAmixContext, iter->desp) != 0)
        // {
        //     printf("unable to link input filter! \n");
        //     return -1;
        // }
        if(1){
            // 设置音量过滤器的参数 
            snprintf(args, sizeof(args),"volume=%d",iter->volume);
            AVFilterContext* volume_filter_ctx = avfilter_graph_alloc_filter(m_cFilterGraph, avfilter_get_by_name("volume"), "volume");
            if(avfilter_init_str(volume_filter_ctx, args) != 0)
            {
                printf("unable to init output filter context !");
                return -1;
            }
            // 将过滤器连接起来，创建一个简单的过滤器链iter->filterCtx
            avfilter_link(iter->filterCtx, 0, volume_filter_ctx, 0);
            //audio_input_infos[index] -> audio_min_info_ptr[index]
            avfilter_link(volume_filter_ctx, 0, m_cAmixContext, iter->desp);
        }

        i++;
    }
    // 输出初始化
    m_cAformat = avfilter_get_by_name("aformat");
    snprintf(args, sizeof(args),
                "sample_rates=%d:sample_fmts=%s:channel_layouts=0x3",
                m_cOutAudioInfo->samplerate,
                av_get_sample_fmt_name(m_cOutAudioInfo->format));
    m_cOutAudioInfo->filterCtx = avfilter_graph_alloc_filter(m_cFilterGraph,
                                                                    m_cAformat,
                                                                    "aformat");


    if(avfilter_init_str(m_cOutAudioInfo->filterCtx, args) != 0)
    {
        printf("unable to init output filter context !");
        return -1;
    }

    //audio_mix_info_ptr -> audio_output_info_ptr
    if(avfilter_link(m_cAmixContext, 0, m_cOutAudioInfo->filterCtx, 0) != 0)
    {
        printf("unable to link output filter!");
        return -1;
    }

    //audio_output_info_ptr -> audio_sink_info_ptr
    if(avfilter_link(m_cOutAudioInfo->filterCtx, 0, m_cSinkContext, 0) != 0)
    {
        printf("unable to link sink filter!");
        return -1;
    }
    m_cOutAudioInfo->frame = av_frame_alloc();
    if (!m_cOutAudioInfo->frame)
    {
        printf("unable to alloc mix frame !\n");
        return -1;
    }
    if(avfilter_graph_config(m_cFilterGraph, NULL) < 0)
    {
        printf("avfilter_graph_config failed!\n" );
        return -1;
    }
 
    m_bStatus = true;
    printf("audiomixer init finished!\n");
    return 0;
}

int AudioMixer::exit()
{
    if(m_cFilterGraph) avfilter_graph_free(&m_cFilterGraph);
    // if(m_cOutAudioInfo) delete m_cOutAudioInfo;
    // for(auto& iter:m_vecAudioInfos){
    //     if(iter) delete iter;
    // }
    return 0;
}

int AudioMixer::addAudioInput(int index, int samplerate, int channels, int bitsPerSample, int volume,AVSampleFormat format)
{
    if(m_bStatus){
        printf("add audio input error,engine has been opend!\n");
        return -1;
    }

    AudioInfo* cInfo = new AudioInfo(); 
    cInfo->init(4*2*1024,volume);
    cInfo->desp = index-1;
    cInfo->samplerate = samplerate;
    cInfo->channels = channels;
    cInfo->bitsPerSample = bitsPerSample;
    cInfo->format = format;
    cInfo->name = std::string("input") + std::to_string(index);
    m_vecAudioInfos.push_back(cInfo);
    return 0;
}

int AudioMixer::setAudioOutput(int samplerate, int channels, int bitsPerSample, AVSampleFormat format)
{
    if(m_bStatus){
        printf("set audio output error,engine has been opend!\n");
    }
    m_cOutAudioInfo->init(4*2*1024,1);
    m_cOutAudioInfo->samplerate = samplerate;
    m_cOutAudioInfo->channels = channels;
    m_cOutAudioInfo->bitsPerSample = bitsPerSample;
    m_cOutAudioInfo->format = format;
    m_cOutAudioInfo->name = std::string("output");
    return 0;
}



int AudioMixer::addFrame(bool flush)
{
    if(flush == false)
    {
        printf("add a frame to context \n");
    }
    else{
        printf("add a end frame\n");
    }
    for(auto& iter : m_vecAudioInfos){
        //创建AVFrame
        iter->frame = av_frame_alloc();
        if (!iter->frame)
        {
            printf("unable to alloc frame !\n");
            return -1;
        }
        iter->frame->nb_samples = 1024;
        iter->frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        iter->frame->format = iter->format; // 设置采样格式
        iter->frame->sample_rate = iter->samplerate;
        if (av_frame_get_buffer(iter->frame, 0) < 0)
        {
            printf("unable to get frame !\n");
            return -1;
        }
        // 将数据存入frame
        memcpy(iter->frame->data[0], iter->raw_frame_data, iter->raw_frame_size);
        if(av_buffersrc_add_frame(iter->filterCtx, flush?nullptr:iter->frame) != 0)
        {
            printf("add a frame error\n ");
            return -1;
        }
    }
    return 0;
}

int AudioMixer::getFrame()
{
    int ret = av_buffersink_get_frame(m_cSinkContext, m_cOutAudioInfo->frame);
    if(ret == AVERROR(EAGAIN)){
        return ret;
    }
    else if(ret == AVERROR_EOF){
        printf("get end mix frame !\n");
        return ret;
    }
    else if(ret < 0)
    {
        printf("unable to get mix frame!\n");
        return -1;
    }
    printf("get a mix frame\n");
    return 0;
}
