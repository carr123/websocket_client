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

static_assert(sizeof(U8) == 1, "sizeof U8����1���ֽ�");
static_assert(sizeof(U16) == 2, "sizeof U16����2���ֽ�");
static_assert(sizeof(U64) == 8, "sizeof U64����8���ֽ�");


struct IWSFrame
{
	virtual void Release() = 0;
	virtual int Type() = 0; // 1�ı�  2������
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
	virtual bool parseURL(const char* wsURL, const char* szOrigin="") = 0; //���߳��н���URL
	virtual bool connect() = 0;									//���߳��з�������, ͬһ��ʵ�����Զ�η�������
	virtual bool isConnected() = 0;
	virtual bool sendBytes(const char* buff, int nLen) = 0;  //���̷߳�������
	virtual const char* errInfo() = 0;
	virtual IWSFrame* readFrame() = 0;                         //���̶߳�ȡ����, һ�η���һ֡
	virtual bool readFrameTo(IWSFrameWriter* pWriter) = 0;     //���̶߳�ȡ����, ��һ֡��������ԴԴ����д��pWriter
};




IWebSockClient* createWebSockClient();











#endif //_WEBSOCKET_CLIENT_85SDGSDGSDSD12S1D2G1SDGSDG1SD2G1SDG1D2FDS2