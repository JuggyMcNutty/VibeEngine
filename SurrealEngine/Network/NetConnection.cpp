
#include "Precomp.h"
#include "NetConnection.h"
#include "NetChannel.h"
#include "NetDriver.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <cmath>

NetOutBunch::NetOutBunch(NetChannel* channel, bool close) :
	Data(channel->Connection->MaxPacket * 8 - NetConnection::MaxBunchHeaderBits - NetConnection::MaxPacketTrailerBits - NetConnection::MaxPacketHeaderBits),
	ChIndex(channel->ChIndex), ChType(channel->ChType), bClose(close)
{
	// A reliable bunch needs room to wait for its ack.
	if ((int)channel->OutRec.size() >= NetConnection::ReliableBuffer - 1 + (close ? 1 : 0))
		Data = NetBitWriter(0);
}

/////////////////////////////////////////////////////////////////////////////

NetConnection::NetConnection(NetDriver* driver, uint32_t remoteAddr, int remotePort, int netSpeed) :
	Driver(driver), RemoteAddr(remoteAddr), RemotePort(remotePort), CurrentNetSpeed(netSpeed)
{
	PackageMap.Connection = this;
	LastReceiveTime = Driver->Time;
	LastSendTime = Driver->Time;
	LastTickTime = Driver->Time;
	StatUpdateTime = Driver->Time;
	LastTime = Driver->Time;
	InitOut();
}

NetConnection::~NetConnection()
{
	for (NetChannel* channel : OpenChannels)
		delete channel;
}

void NetConnection::InitOut()
{
	Out = NetBitWriter(MaxPacket * 8);
}

void NetConnection::ReceivedRawPacket(const uint8_t* data, int count)
{
	if (count <= 0)
		return;

	// The last byte ends in the trailing 1 bit; the bits after it are
	// padding.
	uint8_t last = data[count - 1];
	if (last == 0)
	{
		LogMessage("Net: packet missing its trailing bit");
		return;
	}
	int numBits = count * 8 - 1;
	while (!(last & 0x80))
	{
		last <<= 1;
		numBits--;
	}
	NetBitReader reader(data, numBits);
	ReceivedPacket(reader);
}

void NetConnection::ReceivedPacket(NetBitReader& reader)
{
	LastReceiveTime = Driver->Time;

	int packetId = MakeRelative((int)reader.ReadInt(MaxPacketId), InPacketId, MaxPacketId);
	if (reader.IsError())
		return;
	if (packetId > InPacketId)
		InPacketId = packetId;

	SendAck(packetId);

	while (!reader.AtEnd() && State != ConnectionState::Closed)
	{
		bool isAck = reader.ReadBit();
		if (reader.IsError())
			return;

		if (isAck)
		{
			int ackPacketId = MakeRelative((int)reader.ReadInt(MaxPacketId), OutAckPacketId, MaxPacketId);
			if (reader.IsError())
				return;

			// The numbers between the last ack and this one were lost.
			if (ackPacketId > OutAckPacketId)
			{
				for (int nakPacketId = OutAckPacketId + 1; nakPacketId < ackPacketId; nakPacketId++)
					ReceivedNak(nakPacketId);
				OutAckPacketId = ackPacketId;
			}

			int index = ackPacketId & 255;
			if (OutLagPacketId[index] == ackPacketId)
			{
				float lag = (float)(Driver->Time - OutLagTime[index]);
				LagAcc = (LagCount == 0) ? lag : LagAcc + lag;
				BestLagAcc = std::min(BestLagAcc, lag);
				LagCount++;
			}

			for (int i = (int)OpenChannels.size() - 1; i >= 0; i--)
			{
				NetChannel* channel = OpenChannels[i];
				if (channel->OpenPacketId == ackPacketId)
					channel->OpenAcked = true;
				for (NetOutBunch& out : channel->OutRec)
				{
					if (out.PacketId == ackPacketId)
					{
						out.ReceivedAck = true;
						if (out.bOpen)
							channel->OpenAcked = true;
					}
				}
				channel->ReceivedAcks();
			}
		}
		else
		{
			NetInBunch bunch;
			bool control = reader.ReadBit();
			bunch.PacketId = packetId;
			bunch.bOpen = control ? reader.ReadBit() : false;
			bunch.bClose = control ? reader.ReadBit() : false;
			bunch.bReliable = reader.ReadBit();
			bunch.ChIndex = (int)reader.ReadInt(MaxChannels);
			bunch.ChSequence = bunch.bReliable ? MakeRelative((int)reader.ReadInt(MaxChSequence), InReliable[bunch.ChIndex], MaxChSequence) : 0;
			bunch.ChType = (bunch.bReliable || bunch.bOpen) ? (ChannelType)reader.ReadInt((int)ChannelType::Max) : ChannelType::None;
			int dataBits = (int)reader.ReadInt(MaxPacket * 8);
			if (reader.IsError())
			{
				LogMessage("Net: bunch header overflowed");
				return;
			}
			bunch.Data = reader.ReadSub(dataBits);
			if (reader.IsError())
			{
				LogMessage("Net: bunch data overflowed");
				return;
			}

			// A reliable bunch already had.
			if (bunch.bReliable && bunch.ChSequence <= InReliable[bunch.ChIndex])
				continue;

			NetChannel* channel = Channels[bunch.ChIndex];

			// An unreliable bunch opens a channel only when it closes it too
			// (an actor sent once, bNetTemporary).
			if (!channel && !bunch.bReliable && (!bunch.bOpen || !bunch.bClose))
				continue;

			if (!channel)
			{
				if (bunch.ChType != ChannelType::Control && bunch.ChType != ChannelType::Actor && bunch.ChType != ChannelType::File)
				{
					LogMessage("Net: unknown channel type " + std::to_string((int)bunch.ChType));
					return;
				}

				channel = CreateChannel(bunch.ChType, false, bunch.ChIndex);
				if (!Driver->Notify || !Driver->Notify->NotifyAcceptingChannel(channel))
				{
					// Refused: closed, flushed and gone.
					NetOutBunch closeBunch(channel, true);
					closeBunch.bReliable = true;
					channel->SendBunch(closeBunch, false);
					FlushNet();
					DestroyChannel(channel);
					if (bunch.ChIndex == 0)
					{
						LogMessage("Net: channel 0 refused");
						State = ConnectionState::Closed;
					}
					continue;
				}
			}

			channel->ReceivedRawBunch(bunch);
		}
	}
}

void NetConnection::ReceivedNak(int nakPacketId)
{
	for (int i = (int)OpenChannels.size() - 1; i >= 0; i--)
	{
		NetChannel* channel = OpenChannels[i];
		channel->ReceivedNak(nakPacketId);
		if (channel->OpenPacketId == nakPacketId)
			channel->ReceivedAcks();
	}
}

void NetConnection::SendAck(int packetId, bool firstTime)
{
	if (firstTime)
	{
		PurgeAcks();
		QueuedAcks.push_back(packetId);
	}
	PreSend(1 + 14);
	Out.WriteBit(true);
	Out.WriteInt(packetId, MaxPacketId);
	PostSend();
}

void NetConnection::PurgeAcks()
{
	// Each ack goes twice: again with the next packet's first new one.
	Array<int> resend;
	resend.swap(ResendAcks);
	for (int packetId : resend)
		SendAck(packetId, false);
}

void NetConnection::PreSend(int sizeBits)
{
	if (Out.GetNumBits() && Out.GetNumBits() + sizeBits + MaxPacketTrailerBits > MaxPacket * 8)
		FlushNet();

	if (Out.GetNumBits() == 0)
		Out.WriteInt(OutPacketId, MaxPacketId);
}

void NetConnection::PostSend()
{
	if (Out.GetNumBits() >= MaxPacket * 8)
		FlushNet();
}

int NetConnection::SendRawBunch(NetOutBunch& bunch, bool allowMerge)
{
	NetBitWriter header(MaxBunchHeaderBits);
	header.WriteBit(false);
	header.WriteBit(bunch.bOpen || bunch.bClose);
	if (bunch.bOpen || bunch.bClose)
	{
		header.WriteBit(bunch.bOpen);
		header.WriteBit(bunch.bClose);
	}
	header.WriteBit(bunch.bReliable);
	header.WriteInt(bunch.ChIndex, MaxChannels);
	if (bunch.bReliable)
		header.WriteInt(bunch.ChSequence, MaxChSequence);
	if (bunch.bReliable || bunch.bOpen)
		header.WriteInt((uint32_t)bunch.ChType, (uint32_t)ChannelType::Max);
	header.WriteInt(bunch.Data.GetNumBits(), MaxPacket * 8);

	PreSend(header.GetNumBits() + bunch.Data.GetNumBits());
	bunch.Time = Driver->Time;
	TimeSensitive = true;
	Out.WriteBits(header.GetData(), header.GetNumBits());
	Out.WriteBits(bunch.Data.GetData(), bunch.Data.GetNumBits());
	bunch.PacketId = OutPacketId;
	int packetId = OutPacketId;
	PostSend();
	return packetId;
}

void NetConnection::FlushNet()
{
	TimeSensitive = false;
	if (Out.GetNumBits() || Driver->Time - LastSendTime > Driver->KeepAliveTime)
	{
		// A keepalive is a packet with its number only.
		if (Out.GetNumBits() == 0)
			PreSend(0);

		Out.WriteBit(true);
		while (Out.GetNumBits() & 7)
			Out.WriteBit(false);

		Driver->LowLevelSend(this, Out.GetData(), Out.GetNumBytes());

		int index = OutPacketId & 255;
		OutLagPacketId[index] = OutPacketId;
		OutLagTime[index] = Driver->Time;
		OutPacketId++;
		LastSendTime = Driver->Time;
		QueuedBytes += Out.GetNumBytes() + PacketOverhead;
		InitOut();
	}

	for (int packetId : QueuedAcks)
		ResendAcks.push_back(packetId);
	QueuedAcks.clear();
}

void NetConnection::Tick()
{
	// The client's frame time, averaged each second.
	CumulativeTime += Driver->Time - LastTime;
	LastTime = Driver->Time;
	CountedFrames++;
	if (CumulativeTime > 1.0)
	{
		AverageFrameTime = CumulativeTime / CountedFrames;
		CumulativeTime = 0.0;
		CountedFrames = 0;
	}

	UpdateStats();

	// The long wait lasts until the connection has its player.
	float timeout = (State != ConnectionState::Pending && Actor) ? Driver->ConnectionTimeout : Driver->InitialConnectTimeout;
	if (State != ConnectionState::Closed && Driver->Time - LastReceiveTime > timeout)
	{
		LogMessage("Net: connection timed out after " + std::to_string(Driver->Time - LastReceiveTime) + " s");
		State = ConnectionState::Closed;
	}
	else
	{
		for (int i = (int)OpenChannels.size() - 1; i >= 0; i--)
			OpenChannels[i]->Tick();

		// With the control channel gone, so is the connection.
		if (!Channels[0] && (OutReliable[0] != 0 || InReliable[0] != 0))
			State = ConnectionState::Closed;
	}

	// A packet goes out at the end of a tick that sent a bunch, or with the
	// second copies of the acks when nothing has gone for a while.
	PurgeAcks();
	if (TimeSensitive || Driver->Time - LastSendTime > Driver->KeepAliveTime)
		FlushNet();

	// The rate: what has gone out drains at the connection's speed.
	double deltaBytes = CurrentNetSpeed * (Driver->Time - LastTickTime);
	QueuedBytes -= (int)std::floor(deltaBytes);
	double allowedLag = 2.0 * deltaBytes;
	if (QueuedBytes < -allowedLag)
		QueuedBytes = (int)std::floor(-allowedLag);
	LastTickTime = Driver->Time;
}

void NetConnection::UpdateStats()
{
	double period = Driver->Time - StatUpdateTime;
	if (period < 1.0)
		return;
	if (LagCount > 0)
	{
		AvgLag = LagAcc / LagCount;
		BestLag = BestLagAcc;
	}
	LagAcc = 0.0f;
	BestLagAcc = 9999.0f;
	LagCount = 0;
	StatUpdateTime = Driver->Time;
}

bool NetConnection::IsNetReady(bool saturate)
{
	if (saturate)
		QueuedBytes = -Out.GetNumBytes();
	return QueuedBytes + Out.GetNumBytes() <= 0;
}

NetChannel* NetConnection::CreateChannel(ChannelType type, bool openedLocally, int chIndex)
{
	if (chIndex == -1)
	{
		// The first free one past the control channel.
		for (chIndex = 1; chIndex < MaxChannels; chIndex++)
		{
			if (!Channels[chIndex])
				break;
		}
		if (chIndex == MaxChannels)
			return nullptr;
	}

	NetChannel* channel = nullptr;
	if (type == ChannelType::Control)
		channel = new NetControlChannel(this, chIndex, openedLocally);
	else if (type == ChannelType::Actor)
		channel = new NetActorChannel(this, chIndex, openedLocally);
	else
		channel = new NetFileChannel(this, chIndex, openedLocally);

	Channels[chIndex] = channel;
	OpenChannels.push_back(channel);
	return channel;
}

void NetConnection::DestroyChannel(NetChannel* channel)
{
	channel->CleanUp();
	if (Channels[channel->ChIndex] == channel)
		Channels[channel->ChIndex] = nullptr;
	auto it = std::find(OpenChannels.begin(), OpenChannels.end(), channel);
	if (it != OpenChannels.end())
		OpenChannels.erase(it);
	delete channel;
}

NetControlChannel* NetConnection::GetControlChannel()
{
	NetChannel* channel = Channels[0];
	if (channel && channel->ChType == ChannelType::Control && !channel->Closing)
		return static_cast<NetControlChannel*>(channel);
	return nullptr;
}

NetActorChannel* NetConnection::FindActorChannel(UActor* actor)
{
	auto it = ActorChannels.find(actor);
	return it != ActorChannels.end() ? it->second : nullptr;
}

void NetConnection::SendText(const std::string& text)
{
	if (NetControlChannel* control = GetControlChannel())
		control->SendText(text);
}
