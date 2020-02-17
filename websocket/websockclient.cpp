#include "websockclient.h"
#include "tcpsocket.h"
#include <string>
#include <iterator>
#include <string>
#include <stdlib.h>
#include <condition_variable>
#include "deelx.h"

namespace WS_CLIENT_SS
{


template<typename T>
T reverseByteOrder(T n)
{
	T m;
	U8* pSrc = (U8*)&n;
	U8* pDest = (U8*)&m;
	for (int i = 0; i < sizeof(T); i++)
	{
		pDest[i] = pSrc[sizeof(T) - 1 - i];
	}
	return m;
}


class CWSFrame : public IWSFrame
{
public:
	virtual void Release();
	virtual int Type(); // 1文本  2二进制
	virtual int Len();
	virtual const char* Data();

public:
	int m_nType;
	std::string m_buff;
};

void CWSFrame::Release()
{
	delete this;
}

int CWSFrame::Type()
{
	return m_nType;
}

int CWSFrame::Len()
{
	return m_buff.size();
}

const char*CWSFrame::Data()
{
	return &m_buff[0];
}


struct WSHeader_T
{
	U8 FIN;
	U8 opcode;
	U8 hasMask;
	U64 dataLen;
	U8 mask[4];
};


class  CWebSockClientImpl : public IWebSockClient
{
public:
	 CWebSockClientImpl();
	~ CWebSockClientImpl();

	virtual void Release();
	virtual void close();
	virtual bool parseURL(const char* wsURL, const char* szOrigin);
	virtual bool connect();
	virtual bool isConnected();
	virtual bool sendBytes(const char* buff, int nLen);
	virtual const char* errInfo();
	virtual IWSFrame* readFrame();
	virtual bool readFrameTo(IWSFrameWriter* pWriter);

private:
	bool handShake();
	bool _sendData(const char* buff, int nLen, U8 opcode);
	bool _readHeader(WSHeader_T* pHeader);
	bool _sendPong(const char* buff, int nLen);
	bool _readN(int nCount, std::string& v);
	bool _readNTo(int nCount, IWSFrameWriter* pWriter);


private:
	ITCPSocket* m_pSock;
	bool m_bTLS;
	unsigned short m_nPort;
	std::string m_szHostOrIP;
	std::string m_szPath;
	std::string m_szorigin;
	std::string m_szRawURL;
	bool m_bConnected;

private:
	std::string m_szErr;	
	mutable std::mutex _mutex;
};

CWebSockClientImpl::CWebSockClientImpl()
{
	m_pSock = 0;
	m_nPort = 0;
	m_bTLS = false;
	m_bConnected = false;
}

CWebSockClientImpl::~ CWebSockClientImpl()
{

}

void CWebSockClientImpl::Release()
{
	if (m_pSock)
	{
		m_pSock->close();
		m_pSock->Release();
		m_pSock = 0;
	}

	delete this;
}

void CWebSockClientImpl::close()
{
	if (m_pSock)
	{
		m_pSock->close();		
	}

	m_bConnected = false;
}

bool CWebSockClientImpl::isConnected()
{
	return m_bConnected;
}

const char* CWebSockClientImpl::errInfo()
{
	return m_szErr.c_str();
}

bool CWebSockClientImpl::parseURL(const char* wsURL, const char* szOrigin)
{
	m_szRawURL = wsURL;
	m_szorigin = szOrigin;
	m_bTLS = false;
	m_nPort = 80;
	m_szPath.clear();
	m_szHostOrIP.clear();

	CRegexpT<char> regexp;
	regexp.Compile("^(ws://|wss://)([^/:]+)(:\\d+)?(.*)$");
    MatchResult result = regexp.Match(wsURL, -1);

    if(result.IsMatched())
    {
		std::string szScheme;
		std::copy(wsURL+result.GetGroupStart(1), wsURL+result.GetGroupEnd(1), std::back_inserter(szScheme));
		if(szScheme == "wss://") {
			m_bTLS = true;
			m_nPort = 443;
		}

		std::copy(wsURL+result.GetGroupStart(2), wsURL+result.GetGroupEnd(2), std::back_inserter(m_szHostOrIP));
		
		std::string szPort;
		std::copy(wsURL+result.GetGroupStart(3), wsURL+result.GetGroupEnd(3), std::back_inserter(szPort));
		if(szPort.size() > 0) {
			m_nPort = atoi( szPort.c_str() + 1 );
		}

		std::copy(wsURL+result.GetGroupStart(4), wsURL+result.GetGroupEnd(4), std::back_inserter(m_szPath));
    }else{
		m_szErr = "URL错误";
		return false;
	}

	if (m_pSock){
		m_pSock->Release();		
	}

	m_pSock = 0;

	if(!m_bTLS) {
		m_pSock = createTCPSocket();
	}else{
		m_pSock = createTLSSocket();
	}

	return true;
}

bool CWebSockClientImpl::connect()
{
	if (!m_pSock){
		m_szErr = "未完成URL解析";
		return false;
	}

	bool bOK = false;

	CRegexpT<char> regexp;
	regexp.Compile("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)($|(?!\\.$)\\.)){4}$");
	MatchResult result = regexp.Match(m_szHostOrIP.c_str(), -1);
    if(result.IsMatched())
    {
		bOK = m_pSock->connect(m_szHostOrIP.c_str(), m_nPort);
	}else{

		bOK = m_pSock->connectHost(m_szHostOrIP.c_str(), m_nPort);
	}

	if(!bOK) {
		m_szErr = "TCP连接失败";
		return false;
	}

	if(!handShake()){
		m_szErr = "握手失败";
		return false;
	}

	m_bConnected = true;

	return true;
}

bool CWebSockClientImpl::handShake()
{
	std::string szHost;
	CRegexpT<char> regexp;
	regexp.Compile("^(ws://|wss://)([^/]+)(.*)$");
	MatchResult result = regexp.Match(m_szRawURL.c_str(), -1);
    if(result.IsMatched())
    {
		std::copy(m_szRawURL.c_str()+result.GetGroupStart(2), m_szRawURL.c_str()+result.GetGroupEnd(2), std::back_inserter(szHost));
	}

	std::string szHandShake;
	szHandShake.append("GET ").append(m_szPath).append(" HTTP/1.1").append("\r\n");
	szHandShake.append("Host:").append(szHost).append("\r\n");
	szHandShake.append("Origin:").append(m_szorigin).append("\r\n");
	szHandShake.append("Upgrade:").append("websocket").append("\r\n");
	szHandShake.append("Connection:").append("Upgrade").append("\r\n");
	szHandShake.append("Sec-WebSocket-Key:").append("dGhlIHNhbXBsZSBub25jZQ==").append("\r\n");
	szHandShake.append("Sec-WebSocket-Version:13").append("\r\n");
	szHandShake.append("\r\n");

	bool bOK = m_pSock->write(szHandShake.c_str(), szHandShake.size());
	if (!bOK)
	{
		return false;
	}

	std::string respHeader;
	char ch;
    int nCount;
    bool bRecvHeader = false;    
    while(true)
    {
		nCount = m_pSock->read(&ch, 1);
        if (nCount == 1)
        {
            respHeader += ch;
            if (respHeader.size() > 4)
            {
                const char* pEnd = respHeader.c_str() + respHeader.size() - 4;
                if (pEnd[0] == '\r' && pEnd[1] == '\n' && pEnd[2] == '\r' && pEnd[3] == '\n')
                {//响应头结束标志
                    bRecvHeader = true;
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }

	if (!bRecvHeader)
	{
		m_pSock->close();
		return false;
	}

	regexp.Compile("^HTTP.*\\s101");
	result = regexp.Match(respHeader.c_str(), -1);
    if(result.IsMatched())
    {
		return true;
	}

	return false;
}


bool CWebSockClientImpl::_sendData(const char* buff, int nLen, U8 opcode)
{
	U8 header[10] = { 0 };
	header[0] = 0x80;  //1帧
	header[1] |= 0x80; //有掩码

	header[0] |= opcode;

	if (nLen <= 125){
		header[1] |= (U8)nLen;
		bool bOK = m_pSock->write((const char*)header, 2);
		if (!bOK) return false;
	}
	else if (nLen <= 0xFFFF){
		header[1] |= (U8)126;
		U16* p = (U16*)(&header[2]);
		*p = reverseByteOrder((U16)nLen);
		bool bOK = m_pSock->write((const char*)header, 4);
		if (!bOK) return false;
	}
	else {
		header[1] |= (U8)127;
		U64* p = (U64*)(&header[2]);
		*p = reverseByteOrder((U64)nLen);
		bool bOK = m_pSock->write((const char*)header, 10);
		if (!bOK) return false;
	}

	U8 mask[4] = { 0xD4, 0xF5, 0x72, 0x6F };
	bool bOK = m_pSock->write((const char*)mask, 4);
	if (!bOK) return false;

	std::string szSendBuff;
	szSendBuff.reserve(4090);

	U8 oneByte;
	for (int i = 0; i < nLen; i++){
		oneByte = buff[i] ^ mask[i % 4];
		szSendBuff.push_back(oneByte);
		if (szSendBuff.size() >= 4000)
		{
			bOK = m_pSock->write(szSendBuff.c_str(), szSendBuff.size());
			if (!bOK) {
				return false;
			}
			szSendBuff.clear();
		}
	}

	if (szSendBuff.size() > 0)
	{
		bOK = m_pSock->write(szSendBuff.c_str(), szSendBuff.size());
		if (!bOK) {
			return false;
		}
	}

	return true;
}

bool CWebSockClientImpl::sendBytes(const char* buff, int nLen)
{
	if (!m_bConnected){
		m_szErr = "未连接";
		return false;
	}

	std::lock_guard<std::mutex> lock (_mutex);
	return _sendData(buff, nLen, 0x02);
}

bool CWebSockClientImpl::_sendPong(const char* buff, int nLen)
{
	std::lock_guard<std::mutex> lock (_mutex);
	return _sendData(buff, nLen, 0x0A);
}

bool CWebSockClientImpl::_readHeader(WSHeader_T* pHeader)
{
	U8 header[10] = { 0 };
	int nCount = 0;

	for (int i = 0; i<2; ++i){
		nCount = m_pSock->read((char*)&header[i], 1); 
		if(nCount != 1) {
			return false;
		}
	}

	pHeader->FIN = (header[0] >> 7);
	pHeader->opcode = (header[0] & 0x0F);
	pHeader->hasMask = (header[1] >> 7);

	U8 len = (header[1] & 0x7F);
	if (len <= 125)
	{
		pHeader->dataLen = len;
	}else if(len == 126){
		for (int i = 0; i<2; ++i){
			nCount = m_pSock->read((char*)&header[i+2], 1); 
			if(nCount != 1) {
				return false;
			}
		}

		pHeader->dataLen = reverseByteOrder(*(U16*)(header+2));
	}else{
		for (int i = 0; i<8; ++i){
			nCount = m_pSock->read((char*)&header[i+2], 1); 
			if(nCount != 1) {
				return false;
			}
		}

		pHeader->dataLen = reverseByteOrder(*(U64*)(header+2));
	}

	if(pHeader->hasMask){
		char ch = 0;
		for (int i = 0; i<4; ++i){
			nCount = m_pSock->read(&ch, 1); 
			if(nCount != 1) {
				return false;
			}
			pHeader->mask[i] = ch;
		}		
	}

	return true;
}

#define MIN(a,b) (a>b?b:a)

bool CWebSockClientImpl::_readN(int nCount, std::string& v){
	v.reserve(v.size() + nCount);

	int nRecvd = 0;
	int nRead = 0;
	int nMinSize = 4000;

	char* pBuff = new char[nMinSize];

	while (nRecvd < nCount)
	{
		nRead = m_pSock->read(pBuff, MIN(nCount-nRecvd, nMinSize));
		if (nRead <= 0)
		{
			break;
		}

		nRecvd += nRead;
		std::copy(pBuff, pBuff+nRead, std::back_inserter(v));
	}

	delete [] pBuff;

	return (nRecvd == nCount);
}

bool CWebSockClientImpl::_readNTo(int nCount, IWSFrameWriter* pWriter){

	int nRecvd = 0;
	int nRead = 0;
	int nMinSize = 4000;
	bool bOK = false;

	char* pBuff = new char[nMinSize];

	while (nRecvd < nCount)
	{
		nRead = m_pSock->read(pBuff, MIN(nCount-nRecvd, nMinSize));
		if (nRead <= 0)
		{
			break;
		}

		bOK = pWriter->Write(pBuff, nRead);
		if (!bOK){
			return false;
		}

		nRecvd += nRead;
	}

	delete [] pBuff;

	return (nRecvd == nCount);
}

IWSFrame* CWebSockClientImpl::readFrame()
{
	if (!m_bConnected){
		m_szErr = "未连接";
		return 0;
	}

	WSHeader_T header;	
	CWSFrame* pFrame = new CWSFrame();
	bool bOK;

	while (true)
	{
		memset(&header, 0, sizeof(header));

		bOK = _readHeader(&header);
		if (!bOK) {
			break;
		}

		if (header.opcode == 0x08)
		{//关闭消息
			close();
			break;
		}

		if (header.opcode == 0x09)
		{//ping消息
			std::string dataBuff;
			dataBuff.reserve(header.dataLen);

			bOK = _readN(header.dataLen, dataBuff);
			if (!bOK) {
				break;
			}

			_sendPong(dataBuff.c_str(), dataBuff.size());
			continue;
		}

		if (header.opcode == 0x01 || header.opcode == 0x02)
		{
			pFrame->m_nType = header.opcode;
		}

		if (header.dataLen > 0)
		{
			bOK = _readN(header.dataLen, pFrame->m_buff);
			if (!bOK) {
				break;
			}
		}

		if (header.FIN)
		{
			return pFrame;
		}
	}

	pFrame->Release();

	return 0;
}

bool CWebSockClientImpl::readFrameTo(IWSFrameWriter* pWriter)
{
	if (!m_bConnected){
		m_szErr = "未连接";
		return false;
	}

	WSHeader_T header;	
	bool bOK;

	while (true)
	{
		memset(&header, 0, sizeof(header));

		bOK = _readHeader(&header);
		if (!bOK) {
			break;
		}

		if (header.opcode == 0x08)
		{//关闭消息
			close();			
			break;
		}

		if (header.opcode == 0x09)
		{//ping消息
			std::string dataBuff;
			bOK = _readN(header.dataLen, dataBuff);
			if (!bOK) {
				break;
			}

			_sendPong(dataBuff.c_str(), dataBuff.size());
			continue;
		}

		if (header.dataLen > 0)
		{
			bOK = _readNTo(header.dataLen, pWriter);
			if (!bOK) {
				break;
			}
		}

		if (header.FIN)
		{
			return true;
		}
	}

	return false;	
}


} //namespace WS_CLIENT_SS

IWebSockClient* createWebSockClient()
{
	return new WS_CLIENT_SS::CWebSockClientImpl();
}