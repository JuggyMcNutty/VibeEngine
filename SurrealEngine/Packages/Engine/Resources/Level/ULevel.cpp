
#include "Precomp.h"
#include "ULevel.h"
#include "UModel.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/UEventManager.h"
#include "Engine.h"
#include "VM/ScriptCall.h"

ULevel::ULevel(NameString name, UClass* base, ObjectFlags flags) : ULevelBase(name, base, flags)
{
	Collision.SetLevel(this);
	Light.SetLevel(this);
}

void ULevel::Load(ObjectStream* stream)
{
	ULevelBase::Load(stream);

	Model = stream->ReadObject<UModel>();

	if (Model)
		Model->LoadNow();

	int count = stream->ReadIndex();
	for (int i = 0; i < count; i++)
	{
		LevelReachSpec spec;
		spec.distance = stream->ReadInt32();
		spec.startActor = stream->ReadObject<UNavigationPoint>();
		spec.endActor = stream->ReadObject<UNavigationPoint>();
		spec.collisionRadius = stream->ReadInt32();
		spec.collisionHeight = stream->ReadInt32();
		spec.reachFlags = stream->ReadInt32();
		spec.bPruned = stream->ReadInt8();
		ReachSpecs.push_back(spec);
	}
}

void ULevel::Save(PackageStreamWriter* stream)
{
	ULevelBase::Save(stream);
	stream->WriteObject(Model);
	stream->WriteIndex((int)ReachSpecs.size());
	for (const LevelReachSpec& spec : ReachSpecs)
	{
		stream->WriteInt32(spec.distance);
		stream->WriteObject(spec.startActor);
		stream->WriteObject(spec.endActor);
		stream->WriteInt32(spec.collisionRadius);
		stream->WriteInt32(spec.collisionHeight);
		stream->WriteInt32(spec.reachFlags);
		stream->WriteInt8(spec.bPruned);
	}

	// The rest of the original's ULevel::Serialize (Engine.dll 0x1039d560),
	// which its load of a save reads on: the level's time as a float, the
	// first deleted actor, sixteen text blocks and the travel info. The fork
	// wrote none of it, and the original's load read on into the next export.
	ULevelInfo* levelInfo = Actors.empty() ? nullptr : UObject::TryCast<ULevelInfo>(Actors[0]);
	stream->WriteFloat(levelInfo ? levelInfo->TimeSeconds() : 0.0f);
	stream->WriteObject(nullptr);
	for (int i = 0; i < 16; i++)
		stream->WriteObject(nullptr);
	stream->WriteIndex((int)TravelInfo.size());
	for (const auto& info : TravelInfo)
	{
		stream->WriteString(info.first);
		stream->WriteString(info.second);
	}
}

void ULevel::TickActor(float elapsed, UActor* actor)
{
	if (!actor)
		return;

	// If we have an owner, tick it first
	if (!actor->bDeleteMe() && actor->Owner())
	{
		TickActor(elapsed, actor->Owner());
	}

	// Do we have an actor? is it deleted? did it already tick?
	if (actor->bDeleteMe() || actor->bTicked() == ticked)
		return;

	// Mark actor as ticked
	actor->bTicked() = ticked;

	// Tick the actor for this turn
	actor->Tick(elapsed);

	// Destroy the actor if its time
	if (actor->Role() >= ROLE_SimulatedProxy && actor->LifeSpan() != 0.0f)
	{
		actor->LifeSpan() = std::max(actor->LifeSpan() - elapsed, 0.0f);
		if (actor->LifeSpan() == 0.0f)
		{
			CallEvent(actor, EventName::Expired);
			actor->Destroy();
		}
	}
}

void ULevel::Tick(float elapsed, bool gamePaused)
{
	// Each tick marks what it ticks with a fresh value, so the first tick of
	// a level ticks every actor loaded with it (their bTicked false), as the
	// original's first tick does -- marked with the old value, the first
	// tick after a load passed over them all and the first frame was drawn
	// before any had ticked.
	ticked = !ticked;

	// The actors' step is at most 0.4 s, as the original's level tick holds
	// it, the level's clock taking the whole time: a long frame -- a load, a
	// hitch -- moves nothing further. Its floor of 5 ms is not carried: above
	// 200 frames a second it would run the game fast.
	if (engine->LaunchInfo.IsDeusEx())
		elapsed = std::min(elapsed, 0.4f);

	if (gamePaused)
	{
		for (size_t i = 0; i < Actors.size(); i++)
		{
			if (auto playerPawn = UObject::TryCast<UPlayerPawn>(Actors[i]))
				playerPawn->PausedInput(elapsed);
			else if (Actors[i] && Actors[i]->bAlwaysTick()) // Should this happen?
				TickActor(elapsed, Actors[i]);
		}
	}
	else if (engine->LevelInfo->bPlayersOnly())
	{
		for (size_t i = 0; i < Actors.size(); i++)
		{
			if (UObject::TryCast<UPlayerPawn>(Actors[i]) || Actors[i]->bAlwaysTick())
				TickActor(elapsed, Actors[i]);
		}
	}
	else
	{
		for (size_t i = 0; i < Actors.size(); i++)
		{
			if (Actors[i])
				TickActor(elapsed, Actors[i]);
		}

		// The AI event manager is ticked after the actors, on a full tick
		// with the game not paused, as the original's ULevel::Tick does.
		if (engine->LaunchInfo.IsDeusEx() && this == engine->Level && engine->LevelInfo && engine->LevelInfo->Pauser().empty())
		{
			if (UEventManager* manager = UEventManager::Get())
				manager->Tick();
		}
	}

	// Without a nulled slot the compacted list is the same list
	if (ActorsHaveHoles)
	{
		Array<UActor*> newActorList;
		newActorList.reserve(Actors.size());
		for (UActor* actor : Actors)
		{
			if (actor)
			{
				actor->Index = (int)newActorList.size();
				newActorList.push_back(actor);
			}
		}
		Actors.swap(newActorList);
		ActorsHaveHoles = false;
		ActorsVersion++;
	}
}

void ULevel::Mark(GCMarker& marker)
{
	ULevelBase::Mark(marker);
	marker.SetField("ReachSpecs");
	for (LevelReachSpec& spec : ReachSpecs)
	{
		marker.Mark(spec.startActor);
		marker.Mark(spec.endActor);
	}
	marker.SetField("Model");
	marker.Mark(Model);
}
