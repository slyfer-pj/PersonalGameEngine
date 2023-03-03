#pragma once
#include "Engine/Network/TCPSocket.hpp"

struct NetAddress;

class TCPConnection : public TCPSocket
{
public:
	bool Connect(const NetAddress& address);
	size_t Send(const void* data, const size_t dataSize);
	size_t Receive(void* buffer, size_t maxBytesToRead);

public:
	char m_buffer[1024] = { 0 };
	size_t m_bytesReceived = 0;
};