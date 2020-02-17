#ifndef _WEBSOCKET_CLIENT_85SDGSDGSDSD12S1D2G1SDGSDG1SD2G1SDG1D2FDS2
#define _WEBSOCKET_CLIENT_85SDGSDGSDSD12S1D2G1SDGSDG1SD2G1SDG1D2FDS2

#if defined(__ANDROID__)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef long long      U64;
#elif defined(WIN32)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef __int64        U64;
#endif

static_assert(sizeof(U8) == 1, "sizeof U8不是1个字节");
static_assert(sizeof(U16) == 2, "sizeof U16不是2个字节");
static_assert(sizeof(U64) == 8, "sizeof U64不是8个字节");


struct IWSFrame
{
	virtual void Release() = 0;
	virtual int Type() = 0; // 1文本  2二进制
	virtual int Len() = 0;
	virtual const char* Data() = 0;
};

struct IWSFrameWriter
{
	virtual bool Write(const char* buff, int nCount) = 0;
};



struct IWebSockClient
{
	virtual void Release() = 0;
	virtual void close() = 0;
	virtual bool parseURL(const char* wsURL, const char* szOrigin="") = 0; //主线程中解析URL
	virtual bool connect() = 0;									//子线程中发起连接, 同一个实例可以多次发起连接
	virtual bool isConnected() = 0;
	virtual bool sendBytes(const char* buff, int nLen) = 0;  //子线程发送数据
	virtual const char* errInfo() = 0;
	virtual IWSFrame* readFrame() = 0;                         //子线程读取数据, 一次返回一帧
	virtual bool readFrameTo(IWSFrameWriter* pWriter) = 0;     //子线程读取数据, 将一帧数据内容源源不断写进pWriter
};




IWebSockClient* createWebSockClient();











#endif //_WEBSOCKET_CLIENT_85SDGSDGSDSD12S1D2G1SDGSDG1SD2G1SDG1D2FDS2