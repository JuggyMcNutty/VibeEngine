
#include "Precomp.h"
#include "UUdpLink.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "Engine.h"

#ifdef WIN32
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

UUdpLink::~UUdpLink()
{
	if (handle != invalid_socket_value)
		CloseSocket(handle);
}

void UUdpLink::Tick(float elapsed)
{
	UInternetLink::Tick(elapsed);

	if (handle == invalid_socket_value)
		return;

	if (ReceiveMode() == RMODE_Event)
	{
		// One datagram a tick, as the original's, each its own event.
		char data[4096];
		IpAddr from = { 0, 0 };
		int count = Receive(from, data, sizeof(data) - 1);
		if (count < 0)
			return;
		data[count] = 0;

		UStructProperty prop({}, nullptr, ObjectFlags::NoFlags);
		if (LinkMode() == MODE_Text)
		{
			CallEvent(this, "ReceivedText", { IpAddrArg("ReceivedText", from, prop), ExpressionValue::StringValue(std::string(data)) });
		}
		else if (LinkMode() == MODE_Line)
		{
			// The whole datagram is the line.
			CallEvent(this, "ReceivedLine", { IpAddrArg("ReceivedLine", from, prop), ExpressionValue::StringValue(std::string(data)) });
		}
		else if (LinkMode() == MODE_Binary)
		{
			UFunction* func = FindEventFunction(this, "ReceivedBinary");
			if (func && func->Properties.size() >= 3)
			{
				uint8_t bytes[255] = {};
				memcpy(bytes, data, std::min(count, 255));
				CallEvent(this, "ReceivedBinary", { IpAddrArg("ReceivedBinary", from, prop), ExpressionValue::IntValue(count), ExpressionValue::Variable(bytes, func->Properties[2]) });
			}
		}
	}
	else
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(handle, &readable);
		timeval timeout = {};
		int result = select((int)handle + 1, &readable, nullptr, nullptr, &timeout);
		DataPending() = (result > 0) ? 1 : 0;
	}
}

int UUdpLink::BindPort(int port, bool bUseNextAvailable)
{
	if (handle == invalid_socket_value)
	{
		handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (handle == invalid_socket_value)
		{
			LastError = LastSocketError();
			return 0;
		}
		int on = 1;
		setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on));
		Socket() = (int)handle;
	}

	int bound = BindSocket(handle, port, bUseNextAvailable);
	if (bound == 0)
	{
		LastError = LastSocketError();
		return 0;
	}
	SetNonBlocking(handle);
	Port() = bound;
	return bound;
}

int UUdpLink::Receive(IpAddr& from, char* data, int size)
{
	sockaddr_in addr = {};
#ifdef WIN32
	int addrlen = sizeof(addr);
#else
	socklen_t addrlen = sizeof(addr);
#endif
	int count = (int)recvfrom(handle, data, size, 0, (sockaddr*)&addr, &addrlen);
	if (count < 0)
		return -1;
	from.Addr = (int32_t)ntohl(addr.sin_addr.s_addr);
	from.Port = ntohs(addr.sin_port);
	return count;
}

int UUdpLink::ReadBinary(IpAddr& Addr, int Count, uint8_t* B)
{
	if (handle == invalid_socket_value)
		return 0;
	char data[255];
	int count = Receive(Addr, data, std::clamp(Count, 0, 255));
	if (count <= 0)
		return 0;
	memcpy(B, data, count);
	return count;
}

bool UUdpLink::SendBinary(const IpAddr& Addr, int Count, const uint8_t* B)
{
	if (handle == invalid_socket_value)
		return false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	int result = (int)sendto(handle, (const char*)B, std::clamp(Count, 0, 255), 0, (const sockaddr*)&addr, sizeof(addr));
	return result != 0;
}

int UUdpLink::ReadText(IpAddr& Addr, std::string& Str)
{
	if (handle == invalid_socket_value)
		return 0;
	char data[4096];
	int count = Receive(Addr, data, sizeof(data) - 1);
	if (count <= 0)
		return 0;
	data[count] = 0;
	Str = data;
	return count;
}

bool UUdpLink::SendText(const IpAddr& Addr, const std::string& Str)
{
	// The text as it is, no line end in any mode; the original counts only
	// a send of nothing as failing.
	if (handle == invalid_socket_value)
		return false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	int result = (int)sendto(handle, Str.data(), (int)Str.size(), 0, (const sockaddr*)&addr, sizeof(addr));
	if (result < 0)
		LastError = LastSocketError();
	return result != 0;
}
