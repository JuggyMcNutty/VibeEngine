
#include "Precomp.h"
#include "NetChannel.h"
#include "NetDriver.h"
#include "NetRemote.h"
#include "NetSerialize.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UEnum.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UByteProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Core/Properties/UIntProperty.h"
#include "Packages/Core/Properties/UNameProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStrProperty.h"
#include "Packages/Core/Properties/UStringProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/UViewport.h"
#include "Package/PackageManager.h"
#include "Package/PackageFlags.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "VM/Frame.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

NetChannel::NetChannel(NetConnection* connection, int chIndex, bool openedLocally, ChannelType type) :
	Connection(connection), ChIndex(chIndex), OpenedLocally(openedLocally), ChType(type)
{
}

void NetChannel::ReceivedRawBunch(NetInBunch& bunch)
{
	if (bunch.bReliable && bunch.ChSequence != Connection->InReliable[ChIndex] + 1)
	{
		// Held back, in sequence, until the gap before it fills.
		auto it = InRec.begin();
		for (; it != InRec.end(); ++it)
		{
			if (bunch.ChSequence == it->ChSequence)
				return;
			if (bunch.ChSequence < it->ChSequence)
				break;
		}
		if ((int)InRec.size() >= NetConnection::ReliableBuffer)
		{
			LogMessage("Net: incoming reliable buffer overflow on channel " + std::to_string(ChIndex));
			return;
		}
		InRec.insert(it, std::move(bunch));
		return;
	}

	if (ReceivedSequencedBunch(bunch))
		return;

	while (!InRec.empty() && InRec.front().ChSequence == Connection->InReliable[ChIndex] + 1)
	{
		NetInBunch release = std::move(InRec.front());
		InRec.pop_front();
		if (ReceivedSequencedBunch(release))
			return;
	}
}

bool NetChannel::ReceivedSequencedBunch(NetInBunch& bunch)
{
	if (bunch.bReliable)
		Connection->InReliable[ChIndex] = bunch.ChSequence;

	if (!Closing)
		ReceivedBunch(bunch);

	if (bunch.bClose)
	{
		Connection->DestroyChannel(this);
		return true;
	}
	return false;
}

void NetChannel::ReceivedNak(int nakPacketId)
{
	for (NetOutBunch& out : OutRec)
	{
		if (out.PacketId == nakPacketId && !out.ReceivedAck)
			Connection->SendRawBunch(out, false);
	}
}

void NetChannel::Tick()
{
	// Until the other side acknowledges the control channel, what it has not
	// acknowledged goes again each second: the first packets of a join.
	if (ChIndex != 0 || OpenAcked)
		return;
	for (NetOutBunch& out : OutRec)
	{
		if (!out.ReceivedAck && Connection->Driver->Time - out.Time > 1.0)
			Connection->SendRawBunch(out, false);
	}
}

void NetChannel::ReceivedAcks()
{
	bool doClose = false;
	while (!OutRec.empty() && OutRec.front().ReceivedAck)
	{
		doClose = doClose || OutRec.front().bClose;
		OutRec.pop_front();
	}

	// A close acknowledged in sequence, or a temporary open acknowledged.
	if (doClose || (OpenTemporary && OpenAcked))
		Connection->DestroyChannel(this);
}

int NetChannel::SendBunch(NetOutBunch& bunch, bool merge)
{
	if (OpenPacketId == -1 && OpenedLocally)
	{
		bunch.bOpen = true;
		OpenTemporary = !bunch.bReliable;
	}

	NetOutBunch* outBunch = &bunch;
	if (bunch.bReliable)
	{
		if ((int)OutRec.size() >= NetConnection::ReliableBuffer - 1 + (bunch.bClose ? 1 : 0))
		{
			LogMessage("Net: outgoing reliable buffer overflow on channel " + std::to_string(ChIndex));
			Connection->State = ConnectionState::Closed;
			return Connection->OutPacketId;
		}
		bunch.ChSequence = ++Connection->OutReliable[ChIndex];
		OutRec.push_back(bunch);
		outBunch = &OutRec.back();
	}

	outBunch->ReceivedAck = false;
	int packetId = Connection->SendRawBunch(*outBunch, merge);
	if (OpenPacketId == -1 && OpenedLocally)
		OpenPacketId = packetId;
	if (outBunch->bClose)
		SetClosingFlag();
	return packetId;
}

void NetChannel::Close()
{
	if (!Closing && (Connection->State == ConnectionState::Open || Connection->State == ConnectionState::Pending))
	{
		NetOutBunch closeBunch(this, true);
		closeBunch.bReliable = true;
		SendBunch(closeBunch, false);
	}
}

bool NetChannel::IsNetReady(bool saturate)
{
	if ((int)OutRec.size() >= NetConnection::ReliableBuffer - 1)
		return false;
	return Connection->IsNetReady(saturate);
}

/////////////////////////////////////////////////////////////////////////////

NetControlChannel::NetControlChannel(NetConnection* connection, int chIndex, bool openedLocally) :
	NetChannel(connection, chIndex, openedLocally, ChannelType::Control)
{
}

void NetControlChannel::ReceivedBunch(NetInBunch& bunch)
{
	while (!bunch.Data.AtEnd())
	{
		std::string text = bunch.Data.ReadString();
		if (bunch.Data.IsError())
			break;
		if (Connection->Driver->Notify)
			Connection->Driver->Notify->NotifyReceivedText(Connection, text);
	}
}

void NetControlChannel::SendText(const std::string& text)
{
	NetOutBunch bunch(this, false);
	bunch.bReliable = true;
	bunch.Data.WriteString(text);
	if (!bunch.Data.IsError())
		SendBunch(bunch, true);
	else
		LogMessage("Net: control channel bunch overflowed");
}

/////////////////////////////////////////////////////////////////////////////

namespace
{
	// A GUID's text, as the USES lines and the cache's names have it, and the
	// four words a bunch carries (Core's FGuid).
	void GuidToWords(const std::string& guid, uint32_t words[4])
	{
		for (int i = 0; i < 4; i++)
			words[i] = (uint32_t)std::strtoul(guid.substr(std::min<size_t>(i * 8, guid.size()), 8).c_str(), nullptr, 16);
	}

	std::string WordsToGuid(const uint32_t words[4])
	{
		char text[33];
		snprintf(text, sizeof(text), "%08X%08X%08X%08X", words[0], words[1], words[2], words[3]);
		return text;
	}

	// Core's appCreateTempFilename: the first NNNN.tmp in the folder not
	// already holding something.
	std::string CreateTempFilename(const fs::path& folder)
	{
		static int counter = 0;
		std::error_code ec;
		for (int tries = 0; tries < 0x10000; tries++)
		{
			char name[16];
			snprintf(name, sizeof(name), "%04X.tmp", counter++ & 0xffff);
			fs::path path = folder / name;
			if (!fs::exists(path, ec) || fs::file_size(path, ec) == 0)
				return path.string();
		}
		return {};
	}
}

NetFileChannel::NetFileChannel(NetConnection* connection, int chIndex, bool openedLocally) :
	NetChannel(connection, chIndex, openedLocally, ChannelType::File)
{
}

NetFileChannel::~NetFileChannel()
{
	// Gone with its connection: a download not finished is dropped.
	if (File)
	{
		fclose(File);
		File = nullptr;
		if (OpenedLocally && !Filename.empty())
		{
			std::error_code ec;
			fs::remove(Filename, ec);
		}
	}
}

void NetFileChannel::Request(NetConnection* connection, int packageIndex, const std::string& name, const std::string& guid, int fileSize)
{
	auto channel = static_cast<NetFileChannel*>(connection->CreateChannel(ChannelType::File, true));
	if (!channel)
	{
		if (connection->Driver->Notify)
			connection->Driver->Notify->NotifyReceivedFile(connection, packageIndex, LocalizeMessage("Engine", "Errors", "ChAllocate", "Couldn't allocate channel"));
		return;
	}
	channel->PackageIndex = packageIndex;
	channel->PackageName = name;
	channel->Guid = guid;
	channel->FileSize = fileSize;

	uint32_t words[4];
	GuidToWords(guid, words);
	NetOutBunch bunch(channel, false);
	bunch.bReliable = true;
	for (uint32_t word : words)
		bunch.Data.WriteInt32((int32_t)word);
	channel->SendBunch(bunch, false);
}

void NetFileChannel::ReceivedBunch(NetInBunch& bunch)
{
	NetNotify* notify = Connection->Driver->Notify;
	if (OpenedLocally)
	{
		// The client: the file's bytes, into a file of its own in the cache.
		if (Transferred == 0 && !File)
		{
			LogMessage("Net: receiving package '" + PackageName + "'");
			fs::path cache = engine->packages->GetCacheFolderPath();
			std::error_code ec;
			fs::create_directories(cache, ec);
			Filename = CreateTempFilename(cache);
			File = !Filename.empty() ? fopen(Filename.c_str(), "wb") : nullptr;
		}
		if (!File)
		{
			Error = LocalizeMessage("Engine", "Errors", "NetOpen", "Error opening file");
			Close();
			return;
		}

		int numBytes = (bunch.Data.GetNumBits() - bunch.Data.GetPosBits()) / 8;
		std::vector<uint8_t> data(std::max(numBytes, 0));
		if (numBytes > 0)
			bunch.Data.ReadBytes(data.data(), numBytes);
		if (numBytes > 0 && fwrite(data.data(), 1, numBytes, File) != (size_t)numBytes)
		{
			Error = LocalizeMessage("Engine", "Errors", "NetWrite", "Error writing to file", { Filename });
			Close();
			return;
		}
		Transferred += std::max(numBytes, 0);
		if (notify)
		{
			std::string line1 = LocalizeMessage("Engine", "Progress", "ReceiveFile", "Receiving '%s'", { PackageName });
			std::string line2 = LocalizeMessage("Engine", "Progress", "ReceiveSize", "Size %iK, Complete %3.1f%%", { FileSize / 1024, FileSize > 0 ? Transferred * 100.0 / FileSize : 100.0 });
			notify->NotifyProgress(line1, line2, 4.0f);
		}
	}
	else
	{
		// The server: a client asks for a package by its GUID. One it may
		// download (AllowDownload) goes if the level agrees; anything else,
		// the channel closes empty.
		uint32_t words[4] = {};
		for (uint32_t& word : words)
			word = (uint32_t)bunch.Data.ReadInt32();
		if (!bunch.Data.IsError())
		{
			std::string guid = WordsToGuid(words);
			const Array<NetPackageMap::PackageInfo>& list = Connection->PackageMap.List;
			for (size_t i = 0; i < list.size(); i++)
			{
				const NetPackageMap::PackageInfo& info = list[i];
				if (!info.Pkg || (info.Flags & (uint32_t)PackageFlags::AllowDownload) == 0 || !StrTools::equals_ignore_case(info.Guid, guid))
					continue;
				Filename = info.Pkg->GetPackageFilePath();
				if (notify && notify->NotifySendingFile(Connection, guid))
				{
					File = fopen(Filename.c_str(), "rb");
					if (File)
					{
						LogMessage("Net: " + LocalizeMessage("Engine", "Progress", "NetSend", "Sending '%s'", { Filename }));
						PackageIndex = (int)i;
						PackageName = info.Name.ToString();
						FileSize = info.FileSize;
						return;
					}
				}
			}
		}
		LogMessage("Net: " + LocalizeMessage("Engine", "Errors", "NetInvalid", "Received invalid file request"));
		Close();
	}
}

void NetFileChannel::Tick()
{
	NetChannel::Tick();

	// Each tick sends a packet while a file goes: the client's acks keep
	// the server's reliable buffer draining.
	Connection->SetTimeSensitive();

	// The server: a reliable bunch as big as the packet has room for, and
	// the packet sent, until the reliable buffer is full; the file's end
	// closes the channel.
	while (File && !OpenedLocally && IsNetReady(true))
	{
		int size = Connection->MaxSendBytes();
		if (size == 0)
			break;
		int remaining = FileSize - Transferred;
		NetOutBunch bunch(this, size >= remaining);
		size = std::max(std::min(size, remaining), 0);
		std::vector<uint8_t> data(size);
		if (size > 0 && fread(data.data(), 1, size, File) != (size_t)size)
			LogMessage("Net: error reading " + Filename);
		Transferred += size;
		if (size > 0)
			bunch.Data.WriteBytes(data.data(), size);
		bunch.bReliable = true;
		SendBunch(bunch, false);
		Connection->FlushNet();
		if (bunch.bClose)
		{
			fclose(File);
			File = nullptr;
		}
	}
}

void NetFileChannel::CleanUp()
{
	if (File)
	{
		fclose(File);
		File = nullptr;
	}
	if (!OpenedLocally)
		return;

	// The client: the whole file, moved into the cache under its GUID; or
	// why not, the server having sent nothing, too little or too much.
	std::error_code ec;
	if (Error.empty() && Transferred == 0)
		Error = LocalizeMessage("Engine", "Errors", "NetRefused", "Server refused to send '%s'", { PackageName });
	if (Error.empty() && (int64_t)fs::file_size(Filename, ec) != FileSize)
		Error = LocalizeMessage("Engine", "Errors", "NetSize", "File size mismatch");
	if (Error.empty())
	{
		fs::rename(Filename, engine->packages->GetCachedPackagePath(Guid), ec);
		if (ec)
			Error = LocalizeMessage("Engine", "Errors", "NetMove", "Error moving file");
	}
	if (!Error.empty() && !Filename.empty())
		fs::remove(Filename, ec);
	Filename.clear();

	NetNotify* notify = Connection->Driver->Notify;
	if (!notify)
		return;
	if (Error.empty())
		notify->NotifyProgress("Success", "Received '" + PackageName + "'", 4.0f);
	notify->NotifyReceivedFile(Connection, PackageIndex, Error);
}

/////////////////////////////////////////////////////////////////////////////

namespace
{
	bool IsChildOf(UStruct* cls, const NameString& baseName)
	{
		for (UStruct* c = cls; c; c = c->BaseStruct)
		{
			if (c->Name == baseName)
				return true;
		}
		return false;
	}

	// Storage for one element, to read a value into and drop.
	struct ScratchElement
	{
		explicit ScratchElement(UProperty* prop) : Prop(prop), Data(new uint64_t[(prop->ElementSize() + 7) / 8 + 1]())
		{
			Prop->ConstructElement(Data.get());
		}
		~ScratchElement() { Prop->DestructElement(Data.get()); }
		void* Get() { return Data.get(); }

		UProperty* Prop;
		std::unique_ptr<uint64_t[]> Data;
	};

	// What arrives replaces an actor's place, turn, base and collision only
	// through the calls that keep the level's bookkeeping right, and its
	// animation and a mover's interpolation are unpacked from their packed
	// forms (the original's PreNetReceive and PostNetReceive).
	struct ReceivedState
	{
		vec3 Location;
		Rotator Rotation;
		UActor* Base;
		bool CollideActors;
		float CollisionRadius;
		float CollisionHeight;
		vec4 SimAnim;
		vec3 SimInterpolate;
	};

	ReceivedState PreNetReceive(UActor* actor)
	{
		ReceivedState saved;
		saved.Location = actor->Location();
		saved.Rotation = actor->Rotation();
		saved.Base = actor->ActorBase();
		saved.CollideActors = actor->bCollideActors();
		saved.CollisionRadius = actor->CollisionRadius();
		saved.CollisionHeight = actor->CollisionHeight();
		saved.SimAnim = actor->SimAnim();
		if (UMover* mover = UObject::TryCast<UMover>(actor))
			saved.SimInterpolate = mover->SimInterpolate();
		return saved;
	}

	void PostNetReceive(UActor* actor, const ReceivedState& old, NetConnection* connection)
	{
		// The received values, and the old ones back in place for the calls
		// that move from them.
		vec3 newLocation = actor->Location();
		Rotator newRotation = actor->Rotation();
		UActor* newBase = actor->ActorBase();
		bool newCollideActors = actor->bCollideActors();
		float newRadius = actor->CollisionRadius();
		float newHeight = actor->CollisionHeight();
		actor->Location() = old.Location;
		actor->Rotation() = old.Rotation;
		actor->ActorBase() = old.Base;
		actor->bCollideActors() = old.CollideActors;
		actor->CollisionRadius() = old.CollisionRadius;
		actor->CollisionHeight() = old.CollisionHeight;

		if (UMover* mover = UObject::TryCast<UMover>(actor))
		{
			vec3 sim = mover->SimInterpolate();
			if (sim != old.SimInterpolate)
			{
				mover->PhysAlpha() = sim.x * 0.01f;
				mover->PhysRate() = sim.y * 0.01f;
				mover->OldPos() = mover->SimOldPos();
				mover->OldRot() = Rotator(mover->SimOldRotPitch(), mover->SimOldRotYaw(), mover->SimOldRotRoll());
				int keys = (int)sim.z;
				mover->KeyNum() = (uint8_t)keys;
				mover->PrevKeyNum() = (uint8_t)(keys >> 8);
				mover->SetPhysics(PHYS_MovingBrush);
				mover->bInterpolating() = true;
			}
		}

		vec4 sim = actor->SimAnim();
		if (!(sim == old.SimAnim))
		{
			actor->AnimFrame() = sim.x * 0.0001f;
			actor->AnimRate() = sim.y * 0.0002f;
			actor->TweenRate() = sim.z * 0.001f;
			actor->AnimLast() = sim.w * 0.0001f;
			if (actor->AnimLast() < 0.0f)
			{
				actor->AnimLast() = -actor->AnimLast();
				actor->bAnimLoop() = true;
				if (UObject::TryCast<UPawn>(actor) && actor->AnimMinRate() < 0.5f)
					actor->AnimMinRate() = 0.5f;
			}
			else
			{
				actor->bAnimLoop() = false;
			}
		}

		if (newLocation != actor->Location())
		{
			// A moving simulated pawn eases toward where the server has it,
			// unless far off.
			vec3 velocity = actor->Velocity();
			bool moving = !(std::abs(velocity.x) < 0.0001f && std::abs(velocity.y) < 0.0001f && std::abs(velocity.z) < 0.0001f);
			if (UObject::TryCast<UPawn>(actor) && actor->Role() == ROLE_SimulatedProxy && moving)
			{
				vec3 d = actor->Location() - newLocation;
				float distSq = dot(d, d);
				if (distSq < 10000.0f)
				{
					if (distSq > 1600.0f)
					{
						actor->MoveSmooth((newLocation - actor->Location()) * 0.35f);
						vec3 left = actor->Location() - newLocation;
						if (distSq * 0.75f < dot(left, left))
							actor->SetLocation(actor->Location() + (newLocation - actor->Location()) * 0.5f, true);
					}
					else
					{
						actor->SetLocation(actor->Location() + (newLocation - actor->Location()) * 0.15f, true);
					}
				}
				else
				{
					actor->SetLocation(newLocation, true);
				}
			}
			else
			{
				actor->SetLocation(newLocation, true);
			}
		}

		if (newRotation != actor->Rotation())
			actor->SetRotation(newRotation);

		if (newRadius != actor->CollisionRadius() || newHeight != actor->CollisionHeight())
			actor->SetCollisionSize(newRadius, newHeight);

		if (newCollideActors != (bool)actor->bCollideActors())
			actor->SetCollision(newCollideActors, actor->bBlockActors(), actor->bBlockPlayers());

		if (newBase != actor->ActorBase())
		{
			CallEvent(actor, EventName::Bump, { ExpressionValue::ObjectValue(newBase) });
			if (newBase)
				CallEvent(newBase, EventName::Bump, { ExpressionValue::ObjectValue(actor) });
			actor->SetBase(newBase, true);
		}

		actor->bJustTeleported() = false;

		// The local player's ping, less half the client's frame time.
		if (auto pri = UObject::TryCast<UPlayerReplicationInfo>(actor))
		{
			UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
			if (player && player->PlayerReplicationInfo() == pri && actor->Level()->NetMode() == NM_Client)
			{
				pri->Ping() += (int)(connection->AverageFrameTime * 1000.0 * -0.5);
				if (pri->Ping() < 0)
					pri->Ping() = 0;
			}
		}
	}
}

NetActorChannel::NetActorChannel(NetConnection* connection, int chIndex, bool openedLocally) :
	NetChannel(connection, chIndex, openedLocally, ChannelType::Actor)
{
}

NetActorChannel::~NetActorChannel()
{
	if (uint8_t* recent = Recent())
	{
		for (UProperty* prop : ActorClass->Properties)
			prop->DestructArray(recent + prop->DataOffset.DataOffset);
	}
}

void NetActorChannel::SetChannelActor(UActor* actor)
{
	Actor = actor;
	ActorClass = actor->Class;
	Connection->ActorChannels[actor] = this;

	// A server keeps what this client last got of each replicated value.
	// It starts as the class's defaults, as a client's new copy does,
	// less the config values, which may differ from the client's own and
	// so always go.
	if (Connection->Driver->ServerConnection)
		return;
	NetPackageMap::ClassNetCache* cache = Connection->PackageMap.GetClassNetCache(ActorClass);
	Sent.resize(cache ? cache->RepElements.size() : 0);
	if (actor->bNetTemporary())
		return;
	RecentData.reset(new uint64_t[(ActorClass->StructSize + 7) / 8 + 1]());
	uint8_t* recent = Recent();
	for (UProperty* prop : ActorClass->Properties)
	{
		void* value = recent + prop->DataOffset.DataOffset;
		if (AnyFlags(prop->PropFlags, PropertyFlags::Config | PropertyFlags::GlobalConfig))
			prop->ConstructArray(value);
		else
			prop->CopyConstructArray(value, ActorClass->PropertyData.Ptr(prop));
	}
}

void NetActorChannel::ReplicateActor()
{
	NetPackageMap& map = Connection->PackageMap;
	UActor* actor = Actor;

	// No room for another reliable bunch: nothing goes this tick.
	NetOutBunch bunch(this, false);
	if (bunch.Data.GetMaxBits() == 0)
		return;

	NetPackageMap::ClassNetCache* cache = map.GetClassNetCache(ActorClass);
	if (!cache)
		return;

	// The first bunch opens the channel with the actor's state, in order
	// (reliable) -- unless the actor is sent once to keep, whose bunch closes
	// the channel as it opens it.
	bool initial = OpenPacketId == -1;
	actor->bNetInitial() = initial;
	if (initial)
	{
		bunch.bClose = actor->bNetTemporary();
		bunch.bReliable = !actor->bNetTemporary();
	}
	else if (!SpawnAcked && OpenAcked)
	{
		// What went unreliably before the client had the actor goes again:
		// it may have come first and been dropped.
		SpawnAcked = true;
		for (int i = (int)Sent.size() - 1; i >= 0; i--)
		{
			if (Sent[i].OutPacketId != -1 && !Sent[i].Reliable)
				AddDirty(i);
		}
	}

	actor->bNetOwner() = NetOwnedHere(actor, Connection);

	// The opening bunch names the actor: a map's by reference, any other by
	// class and place.
	if (initial && OpenedLocally)
	{
		if (actor->bStatic() || actor->bNoDelete())
		{
			map.WriteObject(bunch.Data, actor);
		}
		else
		{
			map.WriteObject(bunch.Data, ActorClass);
			vec3 location = actor->Location();
			bunch.Data.WriteFloat(location.x);
			bunch.Data.WriteFloat(location.y);
			bunch.Data.WriteFloat(location.z);
			if (uint8_t* recent = Recent())
				*reinterpret_cast<vec3*>(recent + PropOffsets_Actor.Location.DataOffset) = location;
		}
	}

	// A player's pawn is autonomous on its own client only; the others
	// simulate it (so does an actor whose instigator is not this client's).
	uint8_t remoteRole = actor->RemoteRole();
	if (remoteRole == ROLE_AutonomousProxy)
	{
		UPawn* instigator = actor->Instigator();
		if (instigator && !instigator->bNetOwner() && !actor->bNetOwner())
			actor->RemoteRole() = ROLE_SimulatedProxy;
	}
	actor->bSimulatedPawn() = actor->bIsPawn() && actor->RemoteRole() == ROLE_SimulatedProxy;

	// An always-relevant inventory item sends the values of the classes
	// the engine does not replicate itself only in its first bunch.
	bool scriptClassesToo = !(UObject::TryCast<UInventory>(actor) && actor->bAlwaysRelevant()) || initial;

	// What goes: each replicated element whose value is not what this
	// client last got and whose replication statement holds, each
	// statement evaluated once. A reference to an actor the client has no
	// channel for yet counts as none until it has one.
	const uint8_t* recent = Recent() ? Recent() : static_cast<const uint8_t*>(ActorClass->PropertyData.Data);
	std::map<std::pair<UClass*, uint16_t>, bool> conditions;
	Array<int> reps;
	for (NetPackageMap::FieldNetCache* field : cache->RepProperties)
	{
		UProperty* prop = static_cast<UProperty*>(field->Field);
		UClass* owner = UObject::TryCast<UClass>(prop->Outer());
		if (!scriptClassesToo && !(owner && (owner->ClsFlags & ClassFlags::NativeReplication) != 0))
			continue;
		bool objectProp = UObject::TryCast<UObjectProperty>(prop) != nullptr;
		for (int i = 0; i < prop->ArrayDimension; i++)
		{
			const void* value = prop->GetElement(actor->PropertyData.Ptr(prop), i);
			const void* last = prop->GetElement(recent + prop->DataOffset.DataOffset, i);
			bool same;
			if (objectProp && !map.CanSerializeObject(*static_cast<UObject* const*>(value)))
				same = *static_cast<UObject* const*>(last) == nullptr;
			else
				same = prop->CompareElement(last, value);
			if (same)
				continue;

			auto key = std::make_pair(owner, prop->ReplicationOffset);
			auto it = conditions.find(key);
			if (it == conditions.end())
				it = conditions.emplace(key, NetReplicationCondition(owner, prop->ReplicationOffset, actor)).first;
			if (it->second)
				reps.push_back(field->RepIndex + i);
		}
	}

	// Then those whose packet was lost, not going already.
	for (int i = (int)Dirty.size() - 1; i >= 0; i--)
	{
		if (std::find(reps.begin(), reps.end(), Dirty[i]) == reps.end())
			reps.push_back(Dirty[i]);
	}

	// Written until the bunch is full. Role and RemoteRole each go as the
	// other, the roles as the client has them.
	static UProperty* roleProp = nullptr;
	static UProperty* remoteRoleProp = nullptr;
	if (!roleProp)
	{
		roleProp = ActorClass->GetProperty("Role");
		remoteRoleProp = ActorClass->GetProperty("RemoteRole");
	}
	bool complete = true;
	size_t count = 0;
	for (; count < reps.size(); count++)
	{
		auto [field, element] = cache->RepElements[reps[count]];
		UProperty* prop = static_cast<UProperty*>(field->Field);
		NetPackageMap::FieldNetCache* sendAs = field;
		if (prop == roleProp)
			sendAs = cache->GetFromField(remoteRoleProp);
		else if (prop == remoteRoleProp)
			sendAs = cache->GetFromField(roleProp);
		if (!sendAs)
			sendAs = field;

		int mark = bunch.Data.GetNumBits();
		bunch.Data.WriteInt(sendAs->FieldNetIndex, cache->GetMaxIndex());
		if (prop->ArrayDimension != 1)
			bunch.Data.WriteByte((uint8_t)element);
		void* value = prop->GetElement(actor->PropertyData.Ptr(prop), element);
		bool mapped = NetWriteItem(prop, bunch.Data, map, value);
		if (bunch.Data.IsError())
		{
			bunch.Data.SetNumBits(mark);
			complete = false;
			break;
		}

		// A reference the client could not resolve yet goes again.
		if (uint8_t* recentValues = Recent())
		{
			void* last = prop->GetElement(recentValues + prop->DataOffset.DataOffset, element);
			if (mapped)
			{
				prop->CopyElement(last, value);
			}
			else
			{
				prop->DestructElement(last);
				prop->ConstructElement(last);
			}
		}
	}
	reps.resize(count);

	if (bunch.Data.GetNumBits() > 0)
	{
		int packetId = SendBunch(bunch, true);
		for (int rep : reps)
		{
			Dirty.erase(std::remove(Dirty.begin(), Dirty.end(), rep), Dirty.end());
			if (rep < (int)Sent.size())
				Sent[rep] = { packetId, bunch.bReliable };
		}
		if (actor->bNetTemporary())
			Connection->SentTemporaries.push_back(actor);
	}

	if (complete)
		LastUpdateTime = Connection->Driver->Time;
	actor->bNetOwner() = false;
	actor->RemoteRole() = remoteRole;
}

bool NetActorChannel::WantedFromClient(UClass* cls, uint16_t replicationOffset)
{
	std::swap(Actor->Role(), Actor->RemoteRole());
	bool wanted = NetReplicationCondition(cls, replicationOffset, Actor);
	std::swap(Actor->Role(), Actor->RemoteRole());
	return wanted && Actor->bNetOwner();
}

void NetActorChannel::AddDirty(int repIndex)
{
	if (std::find(Dirty.begin(), Dirty.end(), repIndex) == Dirty.end())
		Dirty.push_back(repIndex);
}

void NetActorChannel::ReceivedNak(int nakPacketId)
{
	NetChannel::ReceivedNak(nakPacketId);

	// Values lost in an unreliable bunch go again.
	for (int i = (int)Sent.size() - 1; i >= 0; i--)
	{
		if (Sent[i].OutPacketId == nakPacketId && !Sent[i].Reliable)
			AddDirty(i);
	}
}

void NetActorChannel::Close()
{
	NetChannel::Close();
	Actor = nullptr;
}

void NetActorChannel::SetClosingFlag()
{
	// A closing channel is no longer the actor's here.
	if (Actor)
	{
		auto it = Connection->ActorChannels.find(Actor);
		if (it != Connection->ActorChannels.end() && it->second == this)
			Connection->ActorChannels.erase(it);
	}
	NetChannel::SetClosingFlag();
}

void NetActorChannel::ReceivedBunch(NetInBunch& bunch)
{
	NetBitReader& reader = bunch.Data;
	NetPackageMap& map = Connection->PackageMap;
	bool spawnedNewActor = false;

	if (!Actor)
	{
		if (!bunch.bOpen)
			LogMessage("Net: actor channel " + std::to_string(ChIndex) + " began without its open");

		// A map's actor by reference; any other by class and place, spawned.
		UObject* obj = map.ReadObject(reader);
		UActor* actor = UObject::TryCast<UActor>(obj);
		if (!actor)
		{
			UClass* cls = UObject::TryCast<UClass>(obj);
			if (!cls || !IsChildOf(cls, "Actor"))
			{
				LogMessage("Net: actor channel " + std::to_string(ChIndex) + " received an invalid actor class");
				return;
			}
			vec3 location;
			location.x = reader.ReadFloat();
			location.y = reader.ReadFloat();
			location.z = reader.ReadFloat();
			actor = engine->LevelInfo->Spawn(cls, nullptr, NameString(), location, Rotator(0, 0, 0), true, true);
			if (!actor)
			{
				LogMessage("Net: could not spawn a " + cls->Name.ToString());
				return;
			}
			spawnedNewActor = true;
		}
		SetChannelActor(actor);
	}

	NetPackageMap::ClassNetCache* cache = map.GetClassNetCache(ActorClass);
	if (!cache)
	{
		LogMessage("Net: no class net cache for " + ActorClass->Name.ToString());
		return;
	}

	// Owned here: on a client, its top owner the pawn a viewport of this
	// machine plays; on a server, the pawn this client plays.
	Actor->bNetOwner() = NetOwnedHere(Actor, Connection);
	bool server = Connection->Driver->ServerConnection.get() != Connection;

	auto nextField = [&]() -> NetPackageMap::FieldNetCache* {
		uint32_t repIndex = reader.ReadInt(cache->GetMaxIndex());
		return reader.IsError() ? nullptr : cache->GetFromIndex(repIndex);
	};

	NetPackageMap::FieldNetCache* field = nextField();
	while (field)
	{
		ReceivedState saved = PreNetReceive(Actor);
		while (field)
		{
			UProperty* prop = UObject::TryCast<UProperty>(field->Field);
			if (!prop)
				break;

			int element = 0;
			if (prop->ArrayDimension != 1)
				element = reader.ReadByte();

			// A value older than one already taken is read and dropped; so
			// is one a server does not take from this client: the value's
			// replication statement, with the roles as the client has them,
			// must hold for an actor the client owns.
			auto it = Retirement.find({ prop, element });
			int lastPacketId = it != Retirement.end() ? it->second : -1;
			bool take = bunch.PacketId >= lastPacketId && element < prop->ArrayDimension;
			if (take && server && !WantedFromClient(UObject::TryCast<UClass>(prop->Outer()), prop->ReplicationOffset))
			{
				LogMessage("Net: received unwanted property value " + prop->Name.ToString() + " in " + Actor->Name.ToString());
				take = false;
			}
			if (take)
			{
				Retirement[{ prop, element }] = bunch.PacketId;
				NetReadItem(prop, reader, map, prop->GetElement(Actor->PropertyData.Ptr(prop), element));
			}
			else
			{
				ScratchElement scratch(prop);
				NetReadItem(prop, reader, map, scratch.Get());
			}
			field = nextField();
		}
		PostNetReceive(Actor, saved, Connection);

		if (spawnedNewActor)
		{
			CallEvent(Actor, "PostNetBeginPlay");
			spawnedNewActor = false;
		}

		if (!field)
			break;

		UFunction* function = UObject::TryCast<UFunction>(field->Field);
		if (!function)
		{
			LogMessage("Net: actor channel " + std::to_string(ChIndex) + " received an invalid field");
			return;
		}
		ReceiveFunction(function, bunch);
		field = nextField();
	}

	// The server's pawn for this client: the join is done.
	if (Connection->Driver->ServerConnection.get() == Connection && Connection->State == ConnectionState::Pending && Actor->bNetOwner())
	{
		if (UPlayerPawn* pawn = UObject::TryCast<UPlayerPawn>(Actor))
		{
			if (Connection->Driver->Notify)
				Connection->Driver->Notify->NotifyClientPlayer(Connection, pawn);
		}
	}
}

void NetActorChannel::ReceiveFunction(UFunction* declared, NetInBunch& bunch)
{
	NetBitReader& reader = bunch.Data;
	NetPackageMap& map = Connection->PackageMap;

	// The actor's own version of it, its state's first.
	UFunction* function = FindEventFunction(Actor, declared->Name);
	if (!function)
		function = declared;

	// A server runs a client's call only as its first declaration's
	// replication statement allows it, on an actor the client owns; the
	// parameters are read either way.
	bool call = true;
	if (Connection->Driver->ServerConnection.get() != Connection)
	{
		UFunction* root = function;
		while (UFunction* super = UObject::TryCast<UFunction>(root->BaseField))
			root = super;
		if (!WantedFromClient(UObject::TryCast<UClass>(root->Outer()), root->ReplicationOffset))
		{
			LogMessage("Net: received unwanted function " + root->Name.ToString() + " in " + Actor->Name.ToString());
			call = false;
		}
	}

	// The parameters in the order declared, until the first that is not
	// one; a bool is its bit, any other a bit for whether it was sent.
	size_t words = ((size_t)function->StructSize + 7) / 8 + 1;
	std::unique_ptr<uint64_t[]> parms(new uint64_t[words]());
	uint8_t* data = reinterpret_cast<uint8_t*>(parms.get());
	Array<UProperty*> parmProps;
	for (UField* field = function->Children; field; field = field->Next)
	{
		UProperty* prop = UObject::TryCast<UProperty>(field);
		if (!prop)
			continue;
		if ((prop->PropFlags & (PropertyFlags::Parm | PropertyFlags::ReturnParm)) != PropertyFlags::Parm)
			break;
		parmProps.push_back(prop);
	}
	for (UProperty* prop : parmProps)
		prop->ConstructArray(data + prop->DataOffset.DataOffset);
	for (UProperty* prop : parmProps)
	{
		if (map.ObjectToIndex(prop) == -1)
			continue;
		if (UObject::TryCast<UBoolProperty>(prop) || reader.ReadBit())
			NetReadItem(prop, reader, map, data + prop->DataOffset.DataOffset);
	}

	if (call && !reader.IsError() && !Actor->bDeleteMe())
	{
		Array<ExpressionValue> args;
		for (UProperty* prop : Frame::CallParms(function))
		{
			if (prop == function->ReturnParm)
				continue;
			args.push_back(ExpressionValue::Variable(data, prop));
		}
		Frame::Call(function, Actor, std::move(args));
	}

	for (UProperty* prop : parmProps)
		prop->DestructArray(data + prop->DataOffset.DataOffset);
}

void NetActorChannel::CleanUp()
{
	if (!Actor)
		return;
	auto it = Connection->ActorChannels.find(Actor);
	if (it != Connection->ActorChannels.end() && it->second == this)
		Connection->ActorChannels.erase(it);

	// A client lets go of the actor with its channel, unless it was sent
	// once to keep (bNetTemporary). A map's actor stays either way. A
	// server whose one-time send never arrived sends it again.
	if (Connection->Driver->ServerConnection.get() == Connection)
	{
		if (!Actor->bNetTemporary())
			Actor->Destroy();
	}
	else if (!OpenAcked)
	{
		auto& sent = Connection->SentTemporaries;
		sent.erase(std::remove(sent.begin(), sent.end(), Actor), sent.end());
	}
	Actor = nullptr;
}
