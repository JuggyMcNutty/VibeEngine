#pragma once

#include "NetConnection.h"
#include <list>
#include <map>

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
// (or name, when the map has it), its properties and remote calls.
class NetActorChannel : public NetChannel
{
public:
	NetActorChannel(NetConnection* connection, int chIndex, bool openedLocally);

	void ReceivedBunch(NetInBunch& bunch) override;
	void CleanUp() override;

	void SetChannelActor(UActor* actor);

	UActor* Actor = nullptr;
	UClass* ActorClass = nullptr;

private:
	void ReceiveFunction(UFunction* function, NetInBunch& bunch);

	// The packet each property element was last taken from, so an older
	// value arriving late is read and dropped.
	std::map<std::pair<UProperty*, int>, int> Retirement;
};
