#pragma once

#include "NetBits.h"
#include "NetPackageMap.h"
#include "Utils/Array.h"
#include <cstdint>
#include <map>
#include <string>

class NetDriver;
class NetChannel;
class NetControlChannel;
class NetActorChannel;
class UActor;
class UPlayerPawn;

enum class ChannelType : int
{
	None = 0,
	Control = 1,
	Actor = 2,
	File = 3,
	Max = 8
};

enum class ConnectionState
{
	Invalid,
	Closed,
	Pending,
	Open
};

// A channel's data for one packet, and the header SendRawBunch writes before
// it (Engine's FOutBunch). Room for the headers is left out of its size.
struct NetOutBunch
{
	NetOutBunch() = default;
	NetOutBunch(NetChannel* channel, bool close);

	NetBitWriter Data;
	double Time = 0.0;
	bool ReceivedAck = false;
	int ChIndex = 0;
	ChannelType ChType = ChannelType::None;
	int ChSequence = 0;
	int PacketId = 0;
	bool bOpen = false;
	bool bClose = false;
	bool bReliable = false;
};

struct NetInBunch
{
	NetBitReader Data;
	int PacketId = 0;
	int ChIndex = 0;
	ChannelType ChType = ChannelType::None;
	int ChSequence = 0;
	bool bOpen = false;
	bool bClose = false;
	bool bReliable = false;
};

// One end of a game over UDP (Engine's UNetConnection, docs/re/network.md):
// numbered packets of acks and bunches, up to 1,023 channels, reliable bunches
// delivered in order and sent again when lost.
class NetConnection
{
public:
	static constexpr int MaxChannels = 1023;
	static constexpr int MaxPacketId = 16384;
	static constexpr int MaxChSequence = 1024;
	static constexpr int ReliableBuffer = 128;
	static constexpr int MaxBunchHeaderBits = 64;
	static constexpr int MaxPacketHeaderBits = 16;
	static constexpr int MaxPacketTrailerBits = 1;

	NetConnection(NetDriver* driver, uint32_t remoteAddr, int remotePort, int netSpeed);
	~NetConnection();

	void ReceivedRawPacket(const uint8_t* data, int count);
	void FlushNet();
	void Tick();
	bool IsNetReady(bool saturate);

	NetChannel* CreateChannel(ChannelType type, bool openedLocally, int chIndex = -1);
	void DestroyChannel(NetChannel* channel);
	NetControlChannel* GetControlChannel();
	NetActorChannel* FindActorChannel(UActor* actor);

	// A line of text on the control channel (the original's Logf on the
	// connection).
	void SendText(const std::string& text);

	int SendRawBunch(NetOutBunch& bunch, bool allowMerge);
	void SendAck(int packetId, bool firstTime = true);

	NetDriver* Driver = nullptr;
	uint32_t RemoteAddr = 0;
	int RemotePort = 0;
	ConnectionState State = ConnectionState::Pending;
	NetPackageMap PackageMap;

	int MaxPacket = 512;
	int PacketOverhead = 32;
	int Challenge = 0;
	int NegotiatedVer = 1100;
	int UserFlags = 0;
	int CurrentNetSpeed = 0;
	// The server's rates for what it sends, from DYNAMICRATE and STATICRATE.
	int DynamicUpdateRate = 40;
	int StaticUpdateRate = 12;
	UPlayerPawn* Actor = nullptr;

	double LastReceiveTime = 0.0;
	double LastSendTime = 0.0;
	double LastTickTime = 0.0;
	int QueuedBytes = 0;

	int InPacketId = -1;
	int OutPacketId = 0;
	int OutAckPacketId = -1;

	float BestLag = 9999.0f;
	float AvgLag = 9999.0f;
	double AverageFrameTime = 0.0;

	NetChannel* Channels[MaxChannels] = {};
	int OutReliable[MaxChannels] = {};
	int InReliable[MaxChannels] = {};
	Array<NetChannel*> OpenChannels;
	std::map<UActor*, NetActorChannel*> ActorChannels;

private:
	void ReceivedPacket(NetBitReader& reader);
	void ReceivedNak(int nakPacketId);
	void PurgeAcks();
	void PreSend(int sizeBits);
	void PostSend();
	void InitOut();
	void UpdateStats();

	NetBitWriter Out;
	Array<int> QueuedAcks;
	Array<int> ResendAcks;

	double OutLagTime[256] = {};
	int OutLagPacketId[256] = {};
	double StatUpdateTime = 0.0;
	double LastTime = 0.0;
	double CumulativeTime = 0.0;
	int CountedFrames = 0;
	float LagAcc = 9999.0f;
	float BestLagAcc = 9999.0f;
	int LagCount = 0;
};

inline int MakeRelative(int value, int reference, int max)
{
	return reference + (((value - reference + max / 2) & (max - 1)) - max / 2);
}
