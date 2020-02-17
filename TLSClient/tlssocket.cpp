//
//  tlssocket.cpp
//  
//
//  Created by shuai on 1/7/14.
//  Copyright (c) 2014 shuai. All rights reserved.
//

#ifdef WIN32

#include "tcpsocket.h"
#include <stdio.h>
#include <WINSOCK2.H>
#pragma  comment (lib, "WS2_32")

#include "openssl/ssl.h"


namespace TLS_SOCKET_WIN32
{
	class CTLSSocket : public ITCPSocket
	{
	public:
		CTLSSocket();
		~CTLSSocket();
		virtual void Release() ;
		virtual void close();
		virtual bool connect(const char* szIP, unsigned short nPort);
		virtual bool connectHost(const char* szHostURL, unsigned short nPort) ;
		virtual bool write(const char* inbuff, unsigned int nLen);
		virtual int  read(char* outbuff, unsigned int nLen);
		virtual bool isConnected();
		void clean();

	private:
		SOCKET socket;
		SSL_CTX* m_ctx;
		SSL*     m_ssl;
		bool m_bConnected;
	};

	CTLSSocket::CTLSSocket()
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2), &wsaData);
		socket = INVALID_SOCKET;
		m_ctx = 0;
		m_ssl = 0;
		m_bConnected = false;
	}

	CTLSSocket::~CTLSSocket()
	{
		clean();
		WSACleanup();
	}

	void CTLSSocket::Release()
	{
		delete this;
	}

	void CTLSSocket::close()
	{
		if (socket != INVALID_SOCKET)
		{
			closesocket(socket);
			socket = INVALID_SOCKET;			
		}
		m_bConnected = false;
	}

	void CTLSSocket::clean()
	{
		if (socket != INVALID_SOCKET)
		{
			closesocket(socket);
			socket = INVALID_SOCKET;
		}

		if (m_ssl)
		{
			SSL_free(m_ssl);
			m_ssl = 0;
		}

		if (m_ctx)
		{
			SSL_CTX_free(m_ctx);
			m_ctx = 0;
		}

		m_bConnected = false;
	}

	bool  CTLSSocket::isConnected()
	{
		return m_bConnected;
	}

	bool CTLSSocket::connect(const char* szIP, unsigned short nPort)
	{
		clean();

		OpenSSL_add_ssl_algorithms(); //初始化

		SSL_METHOD *meth = TLSv1_client_method();
		m_ctx = SSL_CTX_new(meth);
		SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, NULL); //不验证证书
		
		socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		sockaddr_in AddrPeer; 
		AddrPeer.sin_family = AF_INET;
		AddrPeer.sin_addr.s_addr = inet_addr(szIP);
		AddrPeer.sin_port = htons(nPort);

		int nRet = ::connect(socket, (SOCKADDR*)&AddrPeer, sizeof(AddrPeer)); 
		if (nRet != SOCKET_ERROR){
			m_ssl = SSL_new(m_ctx);
			SSL_set_fd(m_ssl, socket);
			nRet = SSL_connect(m_ssl);
			if (nRet == -1){
				return false;
			}

			m_bConnected = true;
			return true;
		}

		clean();
		return false;
	}

	bool CTLSSocket::connectHost(const char* szHostURL, unsigned short nPort)
	{
		struct hostent* host_entry = gethostbyname(szHostURL);
		if (!host_entry)
			return false;

		char szIP[20] = {0};
		sprintf(szIP, "%d.%d.%d.%d", (host_entry->h_addr_list[0][0] & 0x00ff),
			(host_entry->h_addr_list[0][1] & 0x00ff),
			(host_entry->h_addr_list[0][2] & 0x00ff),
			(host_entry->h_addr_list[0][3] & 0x00ff));

		return connect(szIP, nPort);
	}

	bool CTLSSocket::write(const char* inbuff, unsigned int nLen)
	{
		if (!m_bConnected){
			return false;
		}

		int nTotalSend = 0;
		int nRet = 0;
		int nBytesToSend = 0;

		while(nTotalSend < nLen)
		{
			nBytesToSend = min(1024, nLen-nTotalSend);
			nRet = SSL_write(m_ssl, inbuff + nTotalSend, nBytesToSend);
			if (SOCKET_ERROR == nRet){
				close();
				return false;
			}

			nTotalSend += nRet;
		}

		return (nTotalSend == nLen);
	}

	int  CTLSSocket::read(char* outbuff, unsigned int nLen)
	{
		if (!m_bConnected){
			return 0;
		}

		int n = SSL_read(m_ssl, outbuff, nLen);
		if (n == SOCKET_ERROR){
			close();
			return 0;
		}

		return n;
	}


}//TLS_SOCKET_WIN32

ITCPSocket* createTLSSocket()
{
	return static_cast<ITCPSocket*>(new TLS_SOCKET_WIN32::CTLSSocket());
}

#else //WIN32

//POSIX implement
#include "tcpsocket.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "openssl/ssl.h"

namespace TLS_SOCKET_POSIX
{
    class CTLSSocket : public ITCPSocket
    {
    public:
        CTLSSocket();
        ~CTLSSocket();
        virtual void Release();
        virtual void close();
        virtual bool connect(const char* szIP, unsigned short nPort);
        virtual bool connectHost(const char* szHostURL, unsigned short nPort);
        virtual bool write(const char* inbuff, unsigned int nLen);
        virtual int read(char* outbuff, unsigned int nLen);
		virtual bool isConnected();
		void clean();
        
    private:
        int m_fd;
		SSL_CTX* m_ctx;
		SSL*     m_ssl;
		bool m_bConnected;
    };
    
    CTLSSocket::CTLSSocket()
    {
		m_fd = -1;
		m_ctx = 0;
		m_ssl = 0;
		m_bConnected = false;
    }
    
    CTLSSocket::~CTLSSocket()
    {
		clean();
    }
    
    void CTLSSocket::Release()
    {
        delete this;
    }
    
    void CTLSSocket::close()
    {
        if (m_fd != -1)
        {
            ::shutdown(m_fd, SHUT_RDWR);
            ::close(m_fd);			
        }

        m_fd = -1;
		m_bConnected = false;
    }

	void CTLSSocket::clean()
	{
		if (m_fd != -1)
		{
			::shutdown(m_fd, SHUT_RDWR);
			::close(m_fd);			
		}

		if (m_ssl)
		{
			SSL_free(m_ssl);
			m_ssl = 0;
		}

		if (m_ctx)
		{
			SSL_CTX_free(m_ctx);
			m_ctx = 0;
		}

		m_fd = -1;
		m_bConnected = false;
	}

	bool CTLSSocket::isConnected()
	{
		return m_bConnected;
	}
    
    bool CTLSSocket::connect(const char* szIP, unsigned short nPort)
    {
		clean();

		OpenSSL_add_ssl_algorithms(); //初始化

		SSL_METHOD *meth = TLSv1_client_method();
		m_ctx = SSL_CTX_new(meth);
		SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, NULL); //不验证证书

		m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_fd == -1)	{
			return false;
		}
        
		struct sockaddr_in AddrPeer = {0};
        AddrPeer.sin_family = AF_INET;
        AddrPeer.sin_port = htons(nPort);
        AddrPeer.sin_addr.s_addr = inet_addr(szIP);
        
        int nRet = ::connect(m_fd, (struct sockaddr*)&AddrPeer, sizeof(struct sockaddr_in));
		if(nRet == 0){
			m_ssl = SSL_new(m_ctx);
			SSL_set_fd(m_ssl, m_fd);
			nRet = SSL_connect(m_ssl);
			if (nRet == -1){
				return false;
			}

			m_bConnected = true;
			return true;
		}

		clean();      
        return false;
    }
    
    bool CTLSSocket::connectHost(const char* szHostURL, unsigned short nPort)
    {
		struct hostent* host_entry = gethostbyname(szHostURL);
		if (!host_entry)
			return false;

		char szIP[20] = {0};
		sprintf(szIP, "%d.%d.%d.%d", (host_entry->h_addr_list[0][0] & 0x00ff),
			(host_entry->h_addr_list[0][1] & 0x00ff),
			(host_entry->h_addr_list[0][2] & 0x00ff),
			(host_entry->h_addr_list[0][3] & 0x00ff));

		return connect(szIP, nPort);
    }
    
    bool CTLSSocket::write(const char* inbuff, unsigned int nLen)
    {
        int nTotalSend = 0;
        int nRet = 0;
        int nBytesToSend = 0;
        
        while(nTotalSend < nLen)
        {
            nBytesToSend = ((nLen - nTotalSend) < 1024 ? (nLen - nTotalSend) : 1024);       
			nRet = SSL_write(m_ssl, (const void *)(inbuff + nTotalSend), nBytesToSend);

            if (nRet <= 0){
				close();
                return false;
			}
            nTotalSend += nRet;
        }
        
        return (nTotalSend == nLen);
    }
    
    int CTLSSocket::read(char* outbuff, unsigned int nLen)
    {
		int nRead = SSL_read(m_ssl, outbuff, nLen);       
        if (nRead > 0){
            return nRead;
		}else{
			close();
            return 0;
		}
    }
    
}//namespace TLS_SOCKET_POSIX

ITCPSocket* createTLSSocket()
{
	return static_cast<ITCPSocket*>(new TLS_SOCKET_POSIX::CTLSSocket());
}

#endif//WIN32








