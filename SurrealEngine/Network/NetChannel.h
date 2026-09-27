#pragma once

#include "NetConnection.h"
#include <list>
#include <map>
#include <memory>

class UActor;
class UClass;
class UProperty;
class UFunction;

// A numbered stream of bunches on a connection (Engine's UChannel): reliable
// ones arrive in order, a gap held back until it fills.
class NetChannel
{
public:
	NetChannel(NetConnection* connection, int chIndex, bool openedLocally, ChannelType type);
	virtual ~NetChannel() = default;

	virtual void ReceivedBunch(NetInBunch& bunch) = 0;
	virtual void ReceivedNak(int nakPacketId);
	virtual void Tick();
	virtual void Close();
	virtual void SetClosingFlag() { Closing = true; }
	// What goes with the channel when it is cleaned up (Destroy).
	virtual void CleanUp() {}

	void ReceivedRawBunch(NetInBunch& bunch);
	void ReceivedAcks();
	int SendBunch(NetOutBunch& bunch, bool merge);
	bool IsNetReady(bool saturate);

	NetConnection* Connection = nullptr;
	bool OpenAcked = false;
	bool Closing = false;
	int ChIndex = 0;
	bool OpenedLocally = false;
	int OpenPacketId = -1;
	bool OpenTemporary = false;
	ChannelType ChType = ChannelType::None;
	std::list<NetInBunch> InRec;
	std::list<NetOutBunch> OutRec;

protected:
	// True when the channel is gone.
	bool ReceivedSequencedBunch(NetInBunch& bunch);
};

// Lines of text: the handshake, and the few commands after it.
class NetControlChannel : public NetChannel
{
public:
	NetControlChannel(NetConnection* connection, int chIndex, bool openedLocally);

	void ReceivedBunch(NetInBunch& bunch) override;
	void SendText(const std::string& text);
};

// Downloads, which the fork does not do: a server that opens one is refused.
class NetFileChannel : public NetChannel
{
public:
	NetFileChannel(NetConnection* connection, int chIndex, bool openedLocally);

	void ReceivedBunch(NetInBunch& bunch) override {}
};

// An actor's replication: on a client, the actor the server's bunches spawn
// (or name, when the map has it), its properties and remote calls; on a
// server, what of the actor this client is sent (ReplicateActor).
class NetActorChannel : public NetChannel
{
public:
	NetActorChannel(NetConnection* connection, int chIndex, bool openedLocally);
	~NetActorChannel();

	void ReceivedBunch(NetInBunch& bunch) override;
	void ReceivedNak(int nakPacketId) override;
	void Close() override;
	void SetClosingFlag() override;
	void CleanUp() override;

	void SetChannelActor(UActor* actor);
	void ReplicateActor();

	UActor* Actor = nullptr;
	UClass* ActorClass = nullptr;

	// A server's: when the actor was last relevant to this client, when it
	// was last sent in full, and whether the client has had its first bunch.
	double RelevantTime = 0.0;
	double LastUpdateTime = 0.0;
	bool SpawnAcked = false;

	// A server's copy of the values this client last got, laid out as the
	// actor's own; none for an actor sent once (bNetTemporary).
	uint8_t* Recent() { return RecentData.get() ? reinterpret_cast<uint8_t*>(RecentData.get()) : nullptr; }

private:
	void ReceiveFunction(UFunction* function, NetInBunch& bunch);
	void AddDirty(int repIndex);
	// A server's test of what a client sends: the replication statement,
	// with the roles as the client has them, holds for an actor it owns.
	bool WantedFromClient(UClass* cls, uint16_t replicationOffset);

	// The packet each property element was last taken from, so an older
	// value arriving late is read and dropped.
	std::map<std::pair<UProperty*, int>, int> Retirement;

	// A server's, by element (the class cache's RepIndex): the packet it
	// last went in and whether that bunch was reliable; and the elements to
	// send again, their unreliable packet lost.
	struct SentElement
	{
		int OutPacketId = -1;
		bool Reliable = false;
	};
	Array<SentElement> Sent;
	Array<int> Dirty;
	std::unique_ptr<uint64_t[]> RecentData;
};
