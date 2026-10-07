#pragma once

class GCMarker;

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
class UObject;
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

// One end of a game over UDP (Engine's UNetConnection, dx-reverse-info/network.md):
// numbered packets of acks and bunches, up to 1,023 channels, reliable bunches
// delivered in order and sent again when lost.
// An address in host order as IpDrv writes one: a.b.c.d, then :port when
// the port is not 0 (IpString).
std::string NetAddressString(uint32_t addr, int port = 0);

class NetConnection
{
public:
	// The player and its connection object, the actors its channels
	// replicate and the classes they cache, the packages of its map: kept,
	// never made None, while the connection is open.
	void Mark(GCMarker& marker);

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
	// Whether a channel's bunch can go into the last one sent, as the
	// original merges (UChannel::SendBunch): that one is the same channel's
	// and still ends the packet being built, no ack written since, and the
	// two fit the packet with one header.
	bool CanMerge(const NetOutBunch& bunch) const;
	// The last bunch sent, taken back out of the packet being built with the
	// bunch's data and flags added: what goes in their place.
	NetOutBunch& MergeIntoLast(const NetOutBunch& bunch);
	// A channel's bunch just sent, for the next to merge into; `record` its
	// reliable record, or none.
	void SentBunch(const NetOutBunch& bunch, NetOutBunch* record);
	// The last bunch's reliable record while it can be merged into.
	NetOutBunch* LastOutRecord() const { return LastOutBunch; }
	// The bytes a bunch can still carry in the packet being built (Engine's
	// UChannel::MaxSendBytes).
	int MaxSendBytes() const;
	void SendAck(int packetId, bool firstTime = true);
	// A packet goes at the end of this tick, whatever it holds: a download's
	// acks go each tick.
	void SetTimeSensitive() { TimeSensitive = true; }

	NetDriver* Driver = nullptr;
	uint32_t RemoteAddr = 0;
	int RemotePort = 0;
	// The other end, a.b.c.d:port (IpDrv's LowLevelGetRemoteAddress).
	std::string RemoteAddressString() const { return NetAddressString(RemoteAddr, RemotePort); }
	ConnectionState State = ConnectionState::Pending;
	NetPackageMap PackageMap;

	int MaxPacket = 512;
	int PacketOverhead = 32;
	int Challenge = 0;
	int NegotiatedVer = 1100;
	int UserFlags = 0;
	int CurrentNetSpeed = 0;
	// The server's DYNAMICRATE and STATICRATE as intervals, which pace the
	// player's moves (Deus Ex's).
	float DynamicUpdateInterval = 0.0f;
	float StaticUpdateInterval = 0.0f;
	UPlayerPawn* Actor = nullptr;
	// On a server, the Player object the scripts see for this client.
	UObject* PlayerObject = nullptr;

	double LastReceiveTime = 0.0;
	double LastSendTime = 0.0;
	double LastTickTime = 0.0;
	// A server's: when this client was last replicated to, its count of
	// those ticks, and the actors sent to it once to keep (bNetTemporary).
	double LastRepTime = 0.0;
	int TickCount = 0;
	Array<UActor*> SentTemporaries;
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
	bool TimeSensitive = false;

	// The last bunch a channel sent, where it starts and ends in Out, and
	// whether it may be merged into (no ack written since).
	NetOutBunch LastOut;
	NetOutBunch* LastOutBunch = nullptr;
	int LastStart = 0;
	int LastEnd = 0;
	bool AllowMerge = false;
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
