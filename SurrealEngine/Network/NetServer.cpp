
#include "Precomp.h"
#include "NetServer.h"
#include "NetChannel.h"
#include "NetClient.h"
#include "Package/Package.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Inventory/UWeapon.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Math/coords.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "VM/Frame.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
	std::map<UObject*, NetConnection*> PlayerConnections;

	std::string AddressString(const NetConnection* connection)
	{
		uint32_t a = connection->RemoteAddr;
		return std::to_string(a >> 24) + "." + std::to_string((a >> 16) & 255) + "." + std::to_string((a >> 8) & 255) + "." + std::to_string(a & 255) + ":" + std::to_string(connection->RemotePort);
	}

	void Refuse(NetConnection* connection, const std::string& text)
	{
		connection->SendText(text);
		connection->FlushNet();
		connection->State = ConnectionState::Closed;
	}

	// A value the fork has no accessor for, found by name once; none when
	// the class has no such property.
	template<typename T>
	T* FindValue(UObject* obj, const char* name, UProperty*& prop, bool& looked)
	{
		if (!looked)
		{
			looked = true;
			for (UProperty* p : obj->PropertyData.Class->Properties)
			{
				if (p->Name == name)
					prop = p;
			}
		}
		return prop && obj->PropertyData.Size > prop->DataOffset.DataOffset ? static_cast<T*>(obj->PropertyData.Ptr(prop)) : nullptr;
	}

	// Deus Ex's: the distance within which the viewer has every pawn.
	float RelevantRadius(UActor* viewer)
	{
		static UProperty* prop = nullptr;
		static bool looked = false;
		float* value = FindValue<float>(viewer, "RelevantRadius", prop, looked);
		return value ? *value : 0.0f;
	}

	// The first of a player's extra views (a remote camera, say).
	UActor* AdditionalView(UPlayerPawn* player)
	{
		static UProperty* prop = nullptr;
		static bool looked = false;
		UActor** value = FindValue<UActor*>(player, "AdditionalViews", prop, looked);
		return value ? value[0] : nullptr;
	}

	bool IsOwnedBy(UActor* actor, UActor* owner)
	{
		for (UActor* a = actor; a; a = a->Owner())
		{
			if (a == owner)
				return true;
		}
		return false;
	}

	// Whether an actor matters to a client viewing from a place
	// (AActor::IsNetRelevantFor, dx-reverse-info/network.md, relevancy).
	bool IsNetRelevantFor(UActor* actor, UPlayerPawn* realViewer, UActor* viewer, const vec3& srcLocation)
	{
		if (actor->bAlwaysRelevant())
			return true;
		for (;;)
		{
			if (IsOwnedBy(actor, viewer) || IsOwnedBy(actor, realViewer))
				return true;
			vec3 toViewer = actor->Location() - viewer->Location();
			float distSq = dot(toViewer, toViewer);
			if (actor->AmbientSound())
			{
				float reach = 25.0f * (actor->SoundRadius() + 1);
				if (distSq < reach * reach * 0.3f)
					return true;
			}
			float radius = RelevantRadius(viewer);
			if (radius > 0.0f && actor->bIsPawn() && distSq < radius * radius)
				return true;

			// A pawn's weapon matters as the pawn does.
			UPawn* owner = UObject::TryCast<UPawn>(actor->Owner());
			if (!owner || !owner->bIsPawn() || static_cast<UActor*>(owner->Weapon()) != actor)
				break;
			actor = owner;
			if (actor->bAlwaysRelevant())
				return true;
		}

		if ((actor->bHidden() || actor->bOnlyOwnerSee()) && !actor->bBlockPlayers() && !actor->AmbientSound())
			return false;

		// Otherwise in sight through the world: a pawn at its feet or eyes.
		if (actor->FastTrace(srcLocation, actor->Location()))
			return true;
		UPawn* pawn = UObject::TryCast<UPawn>(actor);
		if (!pawn || !actor->bIsPawn())
			return false;
		return actor->FastTrace(srcLocation, actor->Location() + vec3(0.0f, 0.0f, pawn->EyeHeight()));
	}

	template<typename T>
	const T& SentValue(const uint8_t* sent, PropertyDataOffset offset)
	{
		return *reinterpret_cast<const T*>(sent + offset.DataOffset);
	}

	bool SentBool(const uint8_t* sent, PropertyDataOffset offset)
	{
		return (SentValue<uint32_t>(sent, offset) & offset.BitfieldMask) != 0;
	}

	// How much an actor is worth sending: the time since it last went, by
	// its NetPriority; a walking player the more for how far it has moved
	// from where the client last had it, both predicted ahead
	// (GetNetPriority).
	float GetNetPriority(UActor* actor, const uint8_t* sent, float time, float lag)
	{
		UPawn* pawn = UObject::TryCast<UPawn>(actor);
		if (pawn && pawn->bIsPlayer() && sent && !SentBool(sent, PropOffsets_Actor.bNetOwner) &&
			pawn->Weapon() == SentValue<UWeapon*>(sent, PropOffsets_Pawn.Weapon) &&
			(bool)pawn->bHidden() == SentBool(sent, PropOffsets_Actor.bHidden) && pawn->Physics() == PHYS_Walking)
		{
			vec3 now = pawn->Location() + pawn->Velocity() * (lag * 0.5f);
			vec3 then = SentValue<vec3>(sent, PropOffsets_Actor.Location) + SentValue<vec3>(sent, PropOffsets_Actor.Velocity) * (time + lag * 0.5f);
			time = 2.0f * length(now - then) / pawn->GroundSpeed() + time * 0.5f;
		}
		return time * actor->NetPriority();
	}

	struct ActorPriority
	{
		int Priority = 0;
		UActor* Actor = nullptr;
		NetActorChannel* Channel = nullptr;
	};

	// An actor's place in this tick's order: its worth, the more the nearer
	// it lies to where the client looks; an optional one far down.
	ActorPriority Prioritize(UActor* actor, const vec3& viewLocation, const vec3& viewDir, NetConnection* connection)
	{
		ActorPriority result;
		result.Actor = actor;
		result.Channel = connection->FindActorChannel(actor);
		float time = result.Channel ? (float)(connection->Driver->Time - result.Channel->LastUpdateTime) : connection->Driver->SpawnPrioritySeconds;
		vec3 dir = actor->Location() - viewLocation;
		float lengthSq = dot(dir, dir);
		dir = lengthSq >= 1e-8f ? dir * (1.0f / std::sqrt(lengthSq)) : vec3(0.0f);
		float worth = GetNetPriority(actor, result.Channel ? result.Channel->Recent() : nullptr, time, connection->BestLag);
		result.Priority = (int)std::lrint((dot(dir, viewDir) + 3.0f) * worth * 65536.0f);
		if (actor->bNetOptional())
			result.Priority -= 100000;
		return result;
	}
}

NetConnection* NetConnectionOfPlayer(UObject* player)
{
	auto it = PlayerConnections.find(player);
	return it != PlayerConnections.end() ? it->second : nullptr;
}

NetServerLevel::NetServerLevel(Package* level) : Level(level)
{
	AddPackage(level);
}

NetServerLevel::~NetServerLevel()
{
	PlayerConnections.clear();
}

void NetServerLevel::AddPackage(Package* package)
{
	for (const NetPackageMap::PackageInfo& info : Packages)
	{
		if (info.Pkg == package)
			return;
	}
	if (((uint32_t)package->GetFlags() & (uint32_t)PackageFlags::ServerSideOnly) != 0)
		return;

	NetPackageMap::PackageInfo info;
	info.Pkg = package;
	info.Name = package->GetPackageName();
	info.Guid = package->GetGuidString();
	info.Flags = (uint32_t)package->GetFlags();
	std::error_code ec;
	info.FileSize = (int)fs::file_size(package->GetPackageFilePath(), ec);
	info.LocalGeneration = (int)package->GetGenerations().size();
	Packages.push_back(info);

	for (const NameString& name : package->GetImportedPackages())
	{
		if (engine->packages->HasPackage(name))
			AddPackage(engine->packages->GetPackage(name));
	}
}

bool NetServerLevel::NotifyAcceptingConnection()
{
	return true;
}

bool NetServerLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	// A client opens the control channel; downloads the fork does not serve.
	return channel->ChType == ChannelType::Control;
}

void NetServerLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: level server received: " + text);

	std::string rest, value;
	if (NetParseCommand(text, "HELLO"))
	{
		// A version missing counts as 219's, the first networked engine's.
		int remoteMinVer = 219, remoteVer = 219;
		if (NetParseValue(text, "MINVER=", value))
			remoteMinVer = std::atoi(value.c_str());
		if (NetParseValue(text, "VER=", value))
			remoteVer = std::atoi(value.c_str());
		if (remoteVer < 1100 || remoteMinVer > 1100)
		{
			Refuse(connection, "UPGRADE MINVER=1100 VER=1100");
			return;
		}
		connection->NegotiatedVer = std::min(remoteVer, 1100);
		connection->Challenge = (int)(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
		bool stats = false;
		if (UObject* game = engine->LevelInfo->Game())
		{
			for (UProperty* prop : game->PropertyData.Class->Properties)
			{
				if (prop->Name == "bWorldLog")
					stats = game->GetBool(prop->Name);
			}
		}
		connection->SendText("CHALLENGE VER=" + std::to_string(connection->NegotiatedVer) + " CHALLENGE=" + std::to_string(connection->Challenge) + " STATS=" + std::to_string(stats ? 1 : 0));
		connection->FlushNet();
	}
	else if (NetParseCommand(text, "NETSPEED", &rest))
	{
		int rate = std::atoi(rest.c_str());
		if (rate >= 500)
			connection->CurrentNetSpeed = std::clamp(rate, 500, connection->Driver->MaxClientRate);
		LogMessage("Net: client netspeed is " + std::to_string(connection->CurrentNetSpeed));
	}
	else if (NetParseCommand(text, "HAVE"))
	{
		// The client's generation of a package: both sides count only what
		// it has.
		std::string guid;
		if (NetParseValue(text, "GUID=", guid) && NetParseValue(text, "GEN=", value))
		{
			for (NetPackageMap::PackageInfo& info : connection->PackageMap.List)
			{
				if (StrTools::equals_ignore_case(info.Guid, guid))
					info.RemoteGeneration = std::clamp(std::atoi(value.c_str()), 1, std::max(info.LocalGeneration, 1));
			}
		}
	}
	else if (NetParseCommand(text, "LOGIN"))
	{
		std::string response, url;
		NetParseValue(text, "RESPONSE=", response);
		NetParseValue(text, "URL=", url);
		if (std::atoi(response.c_str()) != NetChallengeResponse(connection->Challenge))
		{
			Refuse(connection, "FAILURE CHALLENGE");
			return;
		}
		LogMessage("Net: login request: " + url);
		RequestURLs[connection] = UnrealURL(url);

		// The game may refuse: its message, and a code the client's menu
		// acts on.
		std::string error, failcode;
		if (!engine->PreLogin(RequestURLs[connection].GetOptions(), AddressString(connection), error, failcode))
		{
			connection->SendText("FAILURE " + error);
			if (!failcode.empty())
				connection->SendText("FAILCODE " + failcode);
			connection->FlushNet();
			connection->State = ConnectionState::Closed;
			return;
		}
		Welcome(connection);
	}
	else if (NetParseCommand(text, "JOIN"))
	{
		if (!connection->Actor)
			Join(connection);
	}
	else if (NetParseCommand(text, "USERFLAG", &rest))
	{
		connection->UserFlags = std::atoi(rest.c_str());
	}
}

void NetServerLevel::Welcome(NetConnection* connection)
{
	connection->PackageMap.List = Packages;
	for (NetPackageMap::PackageInfo& info : connection->PackageMap.List)
	{
		info.RemoteGeneration = info.LocalGeneration;
		connection->SendText("USES GUID=" + info.Guid + " PKG=" + info.Name.ToString() + " FLAGS=" + std::to_string(info.Flags) + " SIZE=" + std::to_string(info.FileSize) + " GEN=" + std::to_string(info.LocalGeneration));
	}
	connection->SendText("WELCOME LEVEL=" + Level->GetPackageName().ToString() + " LONE=" + std::to_string(engine->LevelInfo->bLonePlayer() ? 1 : 0));
	connection->SendText("DYNAMICRATE " + std::to_string(connection->Driver->DynamicUpdateRate));
	connection->SendText("STATICRATE " + std::to_string(connection->Driver->StaticUpdateRate));
	connection->FlushNet();
}

void NetServerLevel::Join(NetConnection* connection)
{
	connection->PackageMap.Compute();

	LogMessage("Net: join request: " + RequestURLs[connection].ToString());
	std::string error;
	UObject* player = engine->packages->GetTransientPackage()->NewObject("NetConnection" + std::to_string(PlayerConnections.size()), engine->packages->FindClass("Engine.NetConnection"), ObjectFlags::Transient);
	PlayerConnections[player] = connection;
	connection->PlayerObject = player;

	UPlayerPawn* pawn = engine->SpawnPlayActor(player, ROLE_AutonomousProxy, RequestURLs[connection], error);
	if (!pawn)
	{
		PlayerConnections.erase(player);
		connection->PlayerObject = nullptr;
		Refuse(connection, "FAILURE " + (error.empty() ? std::string("Could not spawn the player") : error));
		return;
	}
	connection->Actor = pawn;
	LogMessage("Net: join succeeded: " + RequestURLs[connection].GetOption("Name"));
}

void NetServerLevel::TickNetServer(float deltaSeconds)
{
	auto& connections = engine->LevelNetDriver->ClientConnections;
	for (size_t i = connections.size(); i-- > 0;)
		ServerTickClient(connections[i].get());
}

int NetServerLevel::ServerTickClient(NetConnection* connection)
{
	if (!connection->Actor || !connection->IsNetReady(false) || connection->State != ConnectionState::Open)
		return 0;
	NetDriver* driver = connection->Driver;
	NetPackageMap& map = connection->PackageMap;

	NetTag++;
	connection->TickCount++;
	for (UActor* sent : connection->SentTemporaries)
		sent->NetTag() = NetTag;

	// Where the client sees from: its player's view (PlayerCalcView)...
	UPlayerPawn* realViewer = connection->Actor;
	UActor* viewer = realViewer;
	vec3 viewLocation = realViewer->Location();
	Rotator viewRotation = realViewer->ViewRotation();
	if (UFunction* calcView = FindEventFunction(realViewer, "PlayerCalcView"))
	{
		// The out parameters in a frame of the function's own layout.
		Array<UProperty*> parms = Frame::CallParms(calcView);
		if (parms.size() >= 3)
		{
			std::unique_ptr<uint64_t[]> frame(new uint64_t[((size_t)calcView->StructSize + 7) / 8 + 1]());
			uint8_t* data = reinterpret_cast<uint8_t*>(frame.get());
			for (UProperty* prop : parms)
				prop->ConstructArray(data + prop->DataOffset.DataOffset);
			*reinterpret_cast<UObject**>(data + parms[0]->DataOffset.DataOffset) = viewer;
			*reinterpret_cast<vec3*>(data + parms[1]->DataOffset.DataOffset) = viewLocation;
			*reinterpret_cast<Rotator*>(data + parms[2]->DataOffset.DataOffset) = viewRotation;
			CallEvent(realViewer, EventName::PlayerCalcView, {
				ExpressionValue::Variable(data, parms[0]),
				ExpressionValue::Variable(data, parms[1]),
				ExpressionValue::Variable(data, parms[2])
				});
			viewer = UObject::TryCast<UActor>(*reinterpret_cast<UObject**>(data + parms[0]->DataOffset.DataOffset));
			viewLocation = *reinterpret_cast<vec3*>(data + parms[1]->DataOffset.DataOffset);
			viewRotation = *reinterpret_cast<Rotator*>(data + parms[2]->DataOffset.DataOffset);
			for (UProperty* prop : parms)
				prop->DestructArray(data + prop->DataOffset.DataOffset);
		}
	}
	if (!viewer)
		viewer = realViewer;

	// ...every other tick from where it will be: 0.9 s ahead, or 0.4 s
	// every fourth tick, along the viewer's and its base's velocity, short
	// of the world.
	if (connection->TickCount & 1)
	{
		float ahead = (connection->TickCount & 2) ? 0.4f : 0.9f;
		vec3 delta = viewer->Velocity() * ahead;
		if (UActor* base = viewer->ActorBase())
			delta += base->Velocity() * ahead;
		vec3 end = viewLocation + delta;
		TraceFlags flags;
		flags.world = true;
		CollisionHit hit = viewer->XLevel()->Collision.TraceFirstHit(viewLocation, end, nullptr, vec3(0.0f), flags);
		viewLocation = hit.Fraction < 1.0f ? viewLocation + (end - viewLocation) * hit.Fraction : end;
	}

	// The actors due: each at its NetUpdateFrequency, on a clock 0.023 s
	// further on for each actor, so they do not all fall due together.
	Array<ActorPriority> due;
	vec3 viewDir = Coords::Rotation(realViewer->ViewRotation()).XAxis;
	double lastRep = connection->LastRepTime;
	double now = driver->Time;
	const Array<UActor*>& actors = viewer->XLevel()->Actors;
	for (size_t i = 0; i < actors.size(); i++)
	{
		UActor* actor = actors[i];
		if (!actor)
			continue;
		if (((i >= 2 && !actor->bStatic()) || actor->bAlwaysRelevant()) && actor->NetTag() != NetTag && actor->RemoteRole() != ROLE_None && !actor->bDeleteMe())
		{
			float frequency = actor->NetUpdateFrequency();
			if (std::lrint((float)(frequency * lastRep)) != std::lrint((float)(frequency * now)))
			{
				actor->NetTag() = NetTag;
				due.push_back(Prioritize(actor, viewer->Location(), viewDir, connection));
			}
		}
		lastRep += 0.023;
		now += 0.023;
	}
	connection->LastRepTime = driver->Time;

	std::sort(due.begin(), due.end(), [](const ActorPriority& a, const ActorPriority& b) { return a.Priority > b.Priority; });

	// Most worth first, while the connection has room: a relevant actor is
	// replicated, on a channel opened for it if need be; one that has not
	// been relevant for RelevantTimeout loses its channel.
	int updated = 0;
	for (size_t j = 0; j < due.size() && connection->IsNetReady(false); j++)
	{
		UActor* actor = due[j].Actor;
		NetActorChannel* channel = due[j].Channel;

		bool relevant = IsNetRelevantFor(actor, realViewer, viewer, viewLocation);
		bool otherView = false;
		if (!relevant)
		{
			if (UActor* other = AdditionalView(realViewer))
				otherView = IsNetRelevantFor(actor, realViewer, other, other->Location());
		}

		if (relevant || otherView || (channel && driver->Time - channel->RelevantTime < driver->RelevantTimeout))
		{
			if (!channel)
			{
				if (map.ObjectToIndex(actor->Class) == -1)
					continue;
				channel = static_cast<NetActorChannel*>(connection->CreateChannel(ChannelType::Actor, true));
				if (!channel)
					continue;
				channel->SetChannelActor(actor);
			}
			if (relevant)
				channel->RelevantTime = driver->Time;
			if (channel->IsNetReady(false))
			{
				channel->ReplicateActor();
				updated++;
			}
		}
		else if (channel)
		{
			channel->Close();
		}
	}
	return updated;
}

void NetServerLevel::NotifyConnectionClosed(NetConnection* connection)
{
	LogMessage("Net: close " + AddressString(connection));
	RequestURLs.erase(connection);
	if (connection->PlayerObject)
		PlayerConnections.erase(connection->PlayerObject);

	// Its player goes with it.
	if (UPlayerPawn* pawn = connection->Actor)
	{
		connection->Actor = nullptr;
		pawn->Player() = nullptr;
		pawn->Destroy();
	}
}
