#pragma once

#include "UInternetLink.h"

class UUdpLink : public UInternetLink
{
public:
	using UInternetLink::UInternetLink;
	~UUdpLink();

	void Tick(float elapsed) override;

	int BindPort(int Port, bool bUseNextAvailable);

	int ReadBinary(IpAddr& Addr, int Count, uint8_t* B);
	bool SendBinary(const IpAddr& Addr, int Count, const uint8_t* B);

	int ReadText(IpAddr& Addr, std::string& Str);
	bool SendText(const IpAddr& Addr, const std::string& Str);

	int& BroadcastAddr() { return Value<int>(PropOffsets_UdpLink.BroadcastAddr); }

private:
	int Receive(IpAddr& from, char* data, int size);

	socket_t handle = invalid_socket_value;
};
