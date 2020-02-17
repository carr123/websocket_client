//
//  tcpsocket.h
//  nettest
//
//  Created by shuai on 1/7/14.
//  Copyright (c) 2014 shuai. All rights reserved.
//

#ifndef __nettest__tcpsocket__
#define __nettest__tcpsocket__


struct ITCPSocket
{
	virtual void Release() = 0;
	virtual void close() = 0;
	virtual bool connect(const char* szIP, unsigned short nPort) = 0;
	virtual bool connectHost(const char* szHostURL, unsigned short nPort= 80) = 0;
	virtual bool write(const char* inbuff, unsigned int nLen) = 0;
	virtual int  read(char* outbuff, unsigned int nLen) = 0;
	virtual bool isConnected() = 0;
};


ITCPSocket* createTCPSocket();
ITCPSocket* createTLSSocket();



#endif /* defined(__nettest__tcpsocket__) */


/*

ITCPSocket* pSock = createTCPSocket();

bool bOK = pSock->connect("127.0.0.1", 80); //用一个ITCPSocket* 可以多次connect
bOK = pSock->connect("www.baidu.com");


*/
