
#include "Precomp.h"
#include "NetChannel.h"
#include "NetDriver.h"
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
#include "Packages/Engine/UViewport.h"
#include "Utils/Logger.h"
#include "VM/Frame.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include <cmath>

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

NetFileChannel::NetFileChannel(NetConnection* connection, int chIndex, bool openedLocally) :
	NetChannel(connection, chIndex, openedLocally, ChannelType::File)
{
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

	bool ObjectIsA(UObject* obj, UClass* cls)
	{
		for (UStruct* c = obj ? obj->Class : nullptr; c; c = c->BaseStruct)
		{
			if (c == cls)
				return true;
		}
		return false;
	}

	int CeilLogTwo(uint32_t value)
	{
		int bits = 0;
		while (bits < 32 && (1u << bits) < value)
			bits++;
		return bits;
	}

	// One replicated value into memory (the original's NetSerializeItem,
	// loading): docs/re/network.md has the forms.
	void ReadItem(UProperty* prop, NetBitReader& reader, NetPackageMap& map, void* data)
	{
		if (auto boolProp = UObject::TryCast<UBoolProperty>(prop))
		{
			boolProp->SetBool(data, reader.ReadBit());
		}
		else if (auto byteProp = UObject::TryCast<UByteProperty>(prop))
		{
			uint8_t value = 0;
			if (byteProp->EnumType)
				reader.ReadBits(&value, CeilLogTwo((uint32_t)byteProp->EnumType->ElementNames.size()));
			else
				value = reader.ReadByte();
			*static_cast<uint8_t*>(data) = value;
		}
		else if (UObject::TryCast<UIntProperty>(prop))
		{
			*static_cast<int32_t*>(data) = reader.ReadInt32();
		}
		else if (UObject::TryCast<UFloatProperty>(prop))
		{
			*static_cast<float*>(data) = reader.ReadFloat();
		}
		else if (auto objProp = UObject::TryCast<UObjectProperty>(prop))
		{
			UObject* obj = map.ReadObject(reader);
			if (obj && objProp->ObjectClass && !ObjectIsA(obj, objProp->ObjectClass))
				obj = nullptr;
			*static_cast<UObject**>(data) = obj;
		}
		else if (UObject::TryCast<UNameProperty>(prop))
		{
			*static_cast<NameString*>(data) = map.ReadName(reader);
		}
		else if (UObject::TryCast<UStrProperty>(prop) || UObject::TryCast<UStringProperty>(prop))
		{
			*static_cast<std::string*>(data) = reader.ReadString();
		}
		else if (auto structProp = UObject::TryCast<UStructProperty>(prop))
		{
			UStruct* s = structProp->Struct;
			if (s->Name == "Vector")
			{
				uint32_t bits = reader.ReadInt(16);
				int bias = 1 << (bits + 1);
				uint32_t max = 1u << (bits + 2);
				int x = (int)reader.ReadInt(max) - bias;
				int y = (int)reader.ReadInt(max) - bias;
				int z = (int)reader.ReadInt(max) - bias;
				*static_cast<vec3*>(data) = vec3((float)x, (float)y, (float)z);
			}
			else if (s->Name == "Rotator")
			{
				int values[3];
				for (int& value : values)
				{
					uint8_t b = reader.ReadBit() ? reader.ReadByte() : 0;
					value = b << 8;
				}
				*static_cast<Rotator*>(data) = Rotator(values[0], values[1], values[2]);
			}
			else if (s->Name == "Plane")
			{
				float* plane = static_cast<float*>(data);
				for (int i = 0; i < 4; i++)
				{
					uint8_t bytes[2];
					reader.ReadBytes(bytes, 2);
					plane[i] = (float)(int16_t)(bytes[0] | (bytes[1] << 8));
				}
			}
			else
			{
				for (UField* field = s->Children; field; field = field->Next)
				{
					UProperty* member = UObject::TryCast<UProperty>(field);
					if (!member || map.ObjectToIndex(member) == -1)
						continue;
					for (int i = 0; i < member->ArrayDimension; i++)
						ReadItem(member, reader, map, member->GetElement(static_cast<uint8_t*>(data) + member->DataOffset.DataOffset, i));
				}
			}
		}
		else
		{
			LogMessage("Net: cannot receive a " + prop->Class->Name.ToString() + " (" + prop->Name.ToString() + ")");
			reader.SetError();
		}
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
		float SimAnim[4];
		vec3 SimInterpolate;
	};

	float* SimAnim(UActor* actor)
	{
		return static_cast<float*>(actor->PropertyData.Ptr(PropOffsets_Actor.SimAnim.DataOffset));
	}

	ReceivedState PreNetReceive(UActor* actor)
	{
		ReceivedState saved;
		saved.Location = actor->Location();
		saved.Rotation = actor->Rotation();
		saved.Base = actor->ActorBase();
		saved.CollideActors = actor->bCollideActors();
		saved.CollisionRadius = actor->CollisionRadius();
		saved.CollisionHeight = actor->CollisionHeight();
		for (int i = 0; i < 4; i++)
			saved.SimAnim[i] = SimAnim(actor)[i];
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

		float* sim = SimAnim(actor);
		if (sim[0] != old.SimAnim[0] || sim[1] != old.SimAnim[1] || sim[2] != old.SimAnim[2] || sim[3] != old.SimAnim[3])
		{
			actor->AnimFrame() = sim[0] * 0.0001f;
			actor->AnimRate() = sim[1] * 0.0002f;
			actor->TweenRate() = sim[2] * 0.001f;
			actor->AnimLast() = sim[3] * 0.0001f;
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

void NetActorChannel::SetChannelActor(UActor* actor)
{
	Actor = actor;
	ActorClass = actor->Class;
	Connection->ActorChannels[actor] = this;
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

	// Owned here: its top owner a player pawn a viewport of this machine
	// plays.
	Actor->bNetOwner() = false;
	UActor* top = Actor;
	while (top->Owner())
		top = top->Owner();
	if (UPlayerPawn* topPawn = UObject::TryCast<UPlayerPawn>(top))
	{
		if (topPawn->Player() && UObject::TryCast<UViewport>(topPawn->Player()))
			Actor->bNetOwner() = true;
	}

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

			// A value older than one already taken is read and dropped.
			auto it = Retirement.find({ prop, element });
			int lastPacketId = it != Retirement.end() ? it->second : -1;
			bool take = bunch.PacketId >= lastPacketId && element < prop->ArrayDimension;
			if (take)
			{
				Retirement[{ prop, element }] = bunch.PacketId;
				ReadItem(prop, reader, map, prop->GetElement(Actor->PropertyData.Ptr(prop), element));
			}
			else
			{
				ScratchElement scratch(prop);
				ReadItem(prop, reader, map, scratch.Get());
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
			ReadItem(prop, reader, map, data + prop->DataOffset.DataOffset);
	}

	if (!reader.IsError() && !Actor->bDeleteMe())
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
	// once to keep (bNetTemporary). A map's actor stays either way.
	if (Connection->Driver->ServerConnection.get() == Connection && !Actor->bNetTemporary())
		Actor->Destroy();
	Actor = nullptr;
}
