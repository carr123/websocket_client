//
//  tcpsocket.cpp
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

namespace TCP_SOCKET_WIN32
{
	class CTCPSocket : public ITCPSocket
	{
	public:
		CTCPSocket();
		~CTCPSocket();
		virtual void Release() ;
		virtual void close();
		virtual bool connect(const char* szIP, unsigned short nPort);
		virtual bool connectHost(const char* szHostURL, unsigned short nPort) ;
		virtual bool write(const char* inbuff, unsigned int nLen);
		virtual int  read(char* outbuff, unsigned int nLen);
		virtual bool isConnected();

	private:
		SOCKET socket;
		bool m_bConnected;
	};

	CTCPSocket::CTCPSocket()
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2), &wsaData);
		socket = INVALID_SOCKET;
		m_bConnected = false;
	}

	CTCPSocket::~CTCPSocket()
	{
		close();
		WSACleanup();
	}

	void CTCPSocket::Release()
	{
		delete this;
	}

	void CTCPSocket::close()
	{
		if (socket != INVALID_SOCKET)
		{
			closesocket(socket);
			socket = INVALID_SOCKET;			
		}
		m_bConnected = false;
	}

	bool  CTCPSocket::isConnected()
	{
		return m_bConnected;
	}

	bool CTCPSocket::connect(const char* szIP, unsigned short nPort)
	{
		close();
		socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		sockaddr_in AddrPeer; 
		AddrPeer.sin_family = AF_INET;
		AddrPeer.sin_addr.s_addr = inet_addr(szIP);
		AddrPeer.sin_port = htons(nPort);

		int nRet = ::connect(socket, (SOCKADDR*)&AddrPeer, sizeof(AddrPeer)); 
		if (nRet != SOCKET_ERROR){
			m_bConnected = true;
			return true;
		}

		close();
		return false;
	}

	bool CTCPSocket::connectHost(const char* szHostURL, unsigned short nPort)
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

	bool CTCPSocket::write(const char* inbuff, unsigned int nLen)
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
			nRet = ::send(socket, inbuff+nTotalSend, nBytesToSend, 0);
			if (SOCKET_ERROR == nRet){
				close();
				return false;
			}

			nTotalSend += nRet;
		}

		return (nTotalSend == nLen);
	}

	int  CTCPSocket::read(char* outbuff, unsigned int nLen)
	{
		if (!m_bConnected){
			return 0;
		}

		int n =  ::recv(socket, outbuff, nLen, 0);
		if (n == SOCKET_ERROR)
		{
			close();
			return 0;
		}

		return n;
	}


}//TCP_SOCKET_WIN32

ITCPSocket* createTCPSocket()
{
	return static_cast<ITCPSocket*>(new TCP_SOCKET_WIN32::CTCPSocket());
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

namespace TCP_SOCKET_POSIX
{
    class CTCPSocket : public ITCPSocket
    {
    public:
        CTCPSocket();
        ~CTCPSocket();
        virtual void Release();
        virtual void close();
        virtual bool connect(const char* szIP, unsigned short nPort);
        virtual bool connectHost(const char* szHostURL, unsigned short nPort);
        virtual bool write(const char* inbuff, unsigned int nLen);
        virtual int read(char* outbuff, unsigned int nLen);
		virtual bool isConnected();
        
    private:
        int m_fd;
		bool m_bConnected;
    };
    
    CTCPSocket::CTCPSocket()
    {
		m_fd = -1;
		m_bConnected = false;
    }
    
    CTCPSocket::~CTCPSocket()
    {
        close();
    }
    
    void CTCPSocket::Release()
    {
        delete this;
    }
    
    void CTCPSocket::close()
    {
        if (m_fd != -1)
        {
            ::shutdown(m_fd, SHUT_RDWR);
            ::close(m_fd);			
        }

        m_fd = -1;
		m_bConnected = false;
    }

	bool CTCPSocket::isConnected()
	{
		return m_bConnected;
	}
    
    bool CTCPSocket::connect(const char* szIP, unsigned short nPort)
    {
		close();
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
			m_bConnected = true;
			return true;
		}

		close();       
        return false;
    }
    
    bool CTCPSocket::connectHost(const char* szHostURL, unsigned short nPort)
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
    
    bool CTCPSocket::write(const char* inbuff, unsigned int nLen)
    {
        int nTotalSend = 0;
        int nRet = 0;
        int nBytesToSend = 0;
        
        while(nTotalSend < nLen)
        {
            nBytesToSend = ((nLen - nTotalSend) < 1024 ? (nLen - nTotalSend) : 1024);
            
            nRet = ::send(m_fd, inbuff+nTotalSend, nBytesToSend, 0);
            if (nRet <= 0){
				close();
                return false;
			}
            nTotalSend += nRet;
        }
        
        return (nTotalSend == nLen);
    }
    
    int CTCPSocket::read(char* outbuff, unsigned int nLen)
    {
        int nRead = ::recv(m_fd, outbuff, nLen, 0);
        
        if (nRead > 0){
            return nRead;
		}
        else{
			close();
            return 0;
		}
    }
    
}//namespace TCP_SOCKET_POSIX

ITCPSocket* createTCPSocket()
{
	return static_cast<ITCPSocket*>(new TCP_SOCKET_POSIX::CTCPSocket());
}

#endif//WIN32








