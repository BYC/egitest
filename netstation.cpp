/* 
 The MIT License
 
 Copyright (c) 2008 Andrew Butcher
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE. 
 */

#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
// #include <winsock2.h> // Already included from header for SOCKET type
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#include "netstation.h"

namespace NetStation {

	// NetStation supports "NTEL", "UNIX" and "MAC-", but "UNIX" and "MAC-" are both indicators for big endian
	const char kLittleEndian[4]	= {'N','T','E','L'};	// Inform netstation that data is in little endian format
	const char kBigEndian[4]	= {'U','N','I','X'};	// Inform netstation that data is in big endian format
    
    // EGIConnection
    const char EGIConnection::kQuery           = 'Q';
    const char EGIConnection::kExit            = 'X';
    const char EGIConnection::kBeginRecording  = 'B';
    const char EGIConnection::kEndRecording    = 'E';
    const char EGIConnection::kAttention       = 'A';
    const char EGIConnection::kTimeSynch       = 'T';
    const char EGIConnection::kEventDataStream = 'D';
    
    const char EGIConnection::kQuerySuccess    = 'I';
    const char EGIConnection::kSuccess         = 'Z';
    const char EGIConnection::kFailure         = 'F';

	// The C function "send" may not send all of the data passed to it. Loop until everything is away or an error occurs.
    size_t EGIConnection::sendComplete(const char *data, size_t dataSize) const {
        int dataSent = 0;
        int errorCode = 0;
        
        while (size_t(dataSent) < dataSize) {
            errorCode = ::send(m_socket, &data[dataSent], int(dataSize - dataSent), 0);
            if (errorCode > 0) dataSent += errorCode;
            else break;
        }
        
        return dataSent;
    }
    
    // The C function "recv" may not recv all of the data sent to it. Loop until everything is here or an error occurs.
    size_t EGIConnection::recvComplete(char *data, size_t dataSize) const {
        int dataRecv = 0;
        int errorCode = 0;
		
        while (size_t(dataRecv) < dataSize) {
            errorCode = ::recv(m_socket, &data[dataRecv], int(dataSize - dataRecv), 0);
            if (errorCode > 0) dataRecv += errorCode;
            else break;
        }
        
        return dataRecv;
    }
	
    bool EGIConnection::sendCommand(const char *command, const size_t commandSize) const {
        bool didSendCommand = true;
        
        // The response message we receive from netstation
        char responseCode = 0;
        
        // Query commands respond with a one-byte version of the protocol
        char responseVersion = 0;
        
        // Failure response with an error code
        short responseError = 0;

		if (command != 0) {
			if (this->sendComplete(command, commandSize) != commandSize) {
				didSendCommand = false;
			}
		
			// Receive the response code
			if (didSendCommand && this->recvComplete(&responseCode, sizeof(responseCode)) != sizeof(responseCode)) {
				didSendCommand = false;
			}
			
			// Receive any additional response data
			if (responseCode == kQuerySuccess) {
				if (didSendCommand && this->recvComplete((char*)&responseVersion, sizeof(responseVersion)) != sizeof(responseVersion)) {
					didSendCommand = false;
				}
			}
			else if (responseCode == kFailure) {
				if (didSendCommand && this->recvComplete((char*)&responseError, sizeof(responseError)) != sizeof(responseError)) {
					didSendCommand = false;
				}
			}
			else if (responseCode != kSuccess) {
				didSendCommand = false; // We should only see this if the NetStation protocol changes....
			}
		}
		return didSendCommand;
    }
	
	
	EGIConnection::EGIConnection() : m_socket(0) {
	}
	
	EGIConnection::~EGIConnection() {
		this->disconnect();
	}

	// Handle the connection sequence to NetStation
    bool EGIConnection::connect(const char *address, unsigned short port) {
		int noDelay = 1;
        bool didConnect = true;
        struct sockaddr_in destination = {0};
		
        // If we're already connected, disconnect cleanly first
        this->disconnect();
		
		#if defined(_WIN32) || defined(_WIN64)
		WSADATA wsaData;	
		if(WSAStartup(MAKEWORD(1,1),&wsaData) != 0){
			return false;
		}
		#endif
		
		// Try to obtain a socket from the OS, if we fail, throw an exception.
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == 0) didConnect = false;
		
		// Disable Nagleing for faster transmission... Increases amount of data being sent per moment in time, in exchange for lower latency
		setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&noDelay, sizeof(int));
		
		// Setup the destination for our socket
		destination.sin_family = AF_INET;
		destination.sin_port = htons(port);
		destination.sin_addr.s_addr = inet_addr(address);
		memset(destination.sin_zero, '\0', sizeof(destination.sin_zero));
		
		// Try to connect... if there is no one "listen"ing on the other end this will fail.
		int result = ::connect(m_socket, (struct sockaddr*) &destination, sizeof(destination));
		if (result != 0) didConnect = false;
		
        return didConnect;
    }
	
    // Cleanup after ourselves. The destructor will call this for us if we forget to, but we should do it.
    void EGIConnection::disconnect() {
        if (m_socket != 0) {
			#if defined(_WIN32) || defined(_WIN64)
			closesocket(m_socket);
			WSACleanup();
			#else
            close(m_socket);
			#endif
			m_socket = 0;
        }
    }
	
    bool EGIConnection::sendBeginSession(const char systemSpec[4]) {
        size_t offset = 0;
        
        this->m_commandBuffer[offset] = kQuery;
        offset += sizeof(kQuery);
		
        
        memcpy(&this->m_commandBuffer[offset], systemSpec, sizeof(char) * 4);
        offset += sizeof(char) * 4;

        return this->sendCommand(&this->m_commandBuffer[0], offset);
    }
    
    bool EGIConnection::sendEndSession() const {
        return this->sendCommand(&kExit, sizeof(kExit));
    }

    bool EGIConnection::sendBeginRecording() const {
        return this->sendCommand(&kBeginRecording, sizeof(kBeginRecording));
    }
    
    bool EGIConnection::sendEndRecording() const {
        return this->sendCommand(&kEndRecording, sizeof(kEndRecording));
    }
    
    bool EGIConnection::sendAttention() const {
        return this->sendCommand(&kAttention, sizeof(kAttention));
	}
    
    bool EGIConnection::sendSynch(int timeStamp) {
        size_t offset = 0;
		        
        this->m_commandBuffer[offset] = kTimeSynch;
        offset += sizeof(kTimeSynch);
   
        memcpy(&this->m_commandBuffer[offset], &timeStamp, sizeof(timeStamp));
        offset += sizeof(timeStamp);
		
        return this->sendCommand(&this->m_commandBuffer[0], offset);
    }

    bool EGIConnection::sendTrigger(const char* code, int timeStamp, int msDuration) {
        size_t offset = 0;
		
        this->m_commandBuffer[offset] = kEventDataStream;
        offset += sizeof(kEventDataStream);
	
		unsigned short dataSize = 25;
		memcpy(&this->m_commandBuffer[offset], &dataSize, sizeof(dataSize));
		offset += sizeof(dataSize);

		memcpy(&this->m_commandBuffer[offset], &timeStamp, sizeof(timeStamp));
		offset += sizeof(timeStamp);

		memcpy(&this->m_commandBuffer[offset], &msDuration, sizeof(msDuration));
		offset += sizeof(msDuration);

		memcpy(&this->m_commandBuffer[offset], code, 4);
		offset += 4;
		 
		memset(&this->m_commandBuffer[offset], 0, 13);
		offset += 13;

        return this->sendCommand(&this->m_commandBuffer[0], offset); 	
    }    
}
