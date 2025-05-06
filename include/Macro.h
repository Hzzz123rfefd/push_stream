#ifndef __MACRO__
#define __MACRO__
#define CLASS_PEIVATE(varType,varName,funName)\
private:varType varName;\
public: void set##funName(varType var){varName = var;}\
public: varType get##funName(){return varName;}
#define UINT unsigned int
#define BYTE unsigned char
#define DWORD unsigned short
#define NALU_MAX_SIZE 1024 * 1024 
#define RAW_DATA_BLOCK_MAX_SIZE 1024 * 1024 
#define FLV_BODY_MAX_SIZE 1024 * 1024 
#define PCM1_FRAME_SIZE (1024*2*4)
#define PCM2_FRAME_SIZE (1024*2*4)
#define PCM_OUT_FRAME_SIZE (1024*2*4)
#endif