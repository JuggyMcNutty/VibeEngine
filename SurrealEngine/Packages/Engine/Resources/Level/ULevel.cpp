
#include "Precomp.h"
#include "ULevel.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/UEventManager.h"
#include "Packages/Engine/UViewport.h"
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
	bool deusEx = engine->LaunchInfo.IsDeusEx();

	// Each tick marks what it ticks with a fresh value, so the first tick of
	// a level ticks every actor loaded with it (their bTicked false) --
	// marked with the old value, the first tick after a load passed over
	// them all and the first frame was drawn before any had ticked. Deus
	// Ex's flips the mark after its tick, as the original's: its pass skips
	// no actor for its own mark (TickDeusEx).
	if (!deusEx)
		ticked = !ticked;

	// The actors' step is at most 0.4 s, as the original's level tick holds
	// it, the level's clock taking the whole time: a long frame -- a load, a
	// hitch -- moves nothing further. Its floor of 5 ms is not carried: above
	// 200 frames a second it would run the game fast.
	if (deusEx)
		elapsed = std::min(elapsed, 0.4f);

	if (deusEx)
	{
		TickDeusEx(elapsed, gamePaused);
	}
	else if (gamePaused)
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
			if (Actors[i] && (UObject::TryCast<UPlayerPawn>(Actors[i]) || Actors[i]->bAlwaysTick()))
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
	}

	// Without a nulled slot the compacted list is the same list; the first
	// dynamic actor moves down by the static slots emptied before it.
	if (ActorsHaveHoles)
	{
		Array<UActor*> newActorList;
		newActorList.reserve(Actors.size());
		size_t firstDynamic = 0;
		for (size_t i = 0; i < Actors.size(); i++)
		{
			if (i == FirstDynamicActor)
				firstDynamic = newActorList.size();
			if (UActor* actor = Actors[i])
			{
				actor->Index = (int)newActorList.size();
				newActorList.push_back(actor);
			}
		}
		FirstDynamicActor = FirstDynamicActor < Actors.size() ? firstDynamic : newActorList.size();
		Actors.swap(newActorList);
		ActorsHaveHoles = false;
		ActorsVersion++;
	}

	// The original's mark flips after the actors' tick.
	if (deusEx)
		ticked = !ticked;

	// The original's last step of a level tick.
	CleanupDestroyed(false);
}

// Deus Ex's tick of the level's actors, as the original's ULevel::Tick
// (dx-reverse-info engine-dll.md, a level's tick): every pass from the first
// dynamic actor, the static ones never ticked.
void ULevel::TickDeusEx(float elapsed, bool gamePaused)
{
	ownerWait.clear();

	// Paused: the players' input, and the actors with bAlwaysTick.
	if (gamePaused)
	{
		for (size_t i = FirstDynamicActor; i < Actors.size(); i++)
		{
			UActor* actor = Actors[i];
			if (auto playerPawn = UObject::TryCast<UPlayerPawn>(actor))
				playerPawn->PausedInput(elapsed);
			else if (actor && actor->bAlwaysTick())
				TickActorDeusEx(elapsed, actor);
		}
		ownerWait.clear();
		return;
	}

	// The fork's own: with bPlayersOnly, the players and the actors with
	// bAlwaysTick.
	if (engine->LevelInfo->bPlayersOnly())
	{
		for (size_t i = FirstDynamicActor; i < Actors.size(); i++)
		{
			UActor* actor = Actors[i];
			if (actor && (UObject::TryCast<UPlayerPawn>(actor) || actor->bAlwaysTick()))
				TickActorDeusEx(elapsed, actor);
		}
		ownerWait.clear();
		return;
	}

	// Each dynamic actor's distance to the local player, before any ticks, in
	// single player.
	if (engine->LevelInfo->NetMode() == NM_Standalone)
	{
		UPlayerPawn* player = engine->viewport ? UObject::TryCast<UPlayerPawn>(engine->viewport->Actor()) : nullptr;
		if (player && player->Player())
		{
			vec3 at = player->Location();
			for (size_t i = FirstDynamicActor; i < Actors.size(); i++)
			{
				if (UActor* actor = Actors[i])
					actor->DistanceFromPlayer() = length(actor->Location() - at);
			}
		}
	}

	// Each dynamic actor's tick, in the list's order as it grows: one spawned
	// in the pass is ticked in it. One whose owner has not ticked waits; the
	// waiting are ticked after the pass, the last to wait first, again until
	// a round ticks none.
	bool progress = false;
	for (size_t i = FirstDynamicActor; i < Actors.size(); i++)
	{
		if (UActor* actor = Actors[i])
			progress = TickActorDeusEx(elapsed, actor) || progress;
	}
	while (!ownerWait.empty() && progress)
	{
		Array<UActor*> waiting;
		waiting.swap(ownerWait);
		progress = false;
		for (size_t i = waiting.size(); i-- > 0;)
		{
			if (waiting[i]->bTicked() != ticked)
				progress = TickActorDeusEx(elapsed, waiting[i]) || progress;
		}
	}
	ownerWait.clear();

	// The AI event manager is ticked after the actors, on a full tick with
	// the game not paused, as the original's ULevel::Tick does.
	if (this == engine->Level && engine->LevelInfo && engine->LevelInfo->Pauser().empty())
	{
		if (UEventManager* manager = UEventManager::Get())
			manager->Tick();
	}
}

// One actor's tick in Deus Ex's level tick: whether it counted as ticked (in
// stasis too), not waiting for its owner. Its class's tick starts with
// UActor::StartTick, which says how it went (TickStarted).
bool ULevel::TickActorDeusEx(float elapsed, UActor* actor)
{
	if (actor->bDeleteMe())
		return false;

	lastStart = TickStart::Ticked;
	actor->Tick(elapsed);
	if (lastStart == TickStart::Waiting)
		return false;
	if (lastStart == TickStart::Stasis || actor->bDeleteMe())
		return true;

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
	return true;
}

void ULevel::TickStarted(UActor* actor, TickStart start)
{
	lastStart = start;
	if (start == TickStart::Waiting)
		ownerWait.push_back(actor);
}

void ULevel::SortActors()
{
	Array<UActor*> sorted;
	sorted.reserve(Actors.size());
	for (size_t i = 0; i < Actors.size() && i < 2; i++)
		sorted.push_back(Actors[i]);
	for (size_t i = 2; i < Actors.size(); i++)
	{
		if (Actors[i] && Actors[i]->bStatic())
			sorted.push_back(Actors[i]);
	}
	FirstDynamicActor = sorted.size();
	for (size_t i = 2; i < Actors.size(); i++)
	{
		if (Actors[i] && !Actors[i]->bStatic())
			sorted.push_back(Actors[i]);
	}

	ActorsHaveHoles = false;
	for (size_t i = 0; i < sorted.size(); i++)
	{
		if (sorted[i])
			sorted[i]->Index = (int)i;
		else
			ActorsHaveHoles = true;
	}
	Actors.swap(sorted);
	ActorsVersion++;
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
	// The destroyed actors waiting to be let go, each linked to the next by
	// its Deleted: kept until CleanupDestroyed, as the original's level keeps
	// its FirstDeleted.
	marker.SetField("FirstDeleted");
	marker.MarkConst(FirstDeleted);
}

void ULevel::CleanupDestroyed(bool force)
{
	if (!FirstDeleted || (!force && DeletedCount < 128))
		return;

	for (UActor* actor : Actors)
	{
		if (actor && actor->PropertyData.Class && actor->PropertyData.Data)
			actor->PropertyData.Class->CleanupDestroyed(actor->PropertyData.Data);
	}

	UEventManager* manager = engine->LaunchInfo.IsDeusEx() && this == engine->Level ? UEventManager::Get() : nullptr;
	while (UActor* actor = FirstDeleted)
	{
		FirstDeleted = actor->Deleted();
		actor->Deleted() = nullptr;
		if (manager)
			manager->ActorDestroyed(actor);
		// A decal attached again after its Destroyed stays on its surfaces
		// until here; the original's then points at the freed decal.
		if (UDecal* decal = UObject::TryCast<UDecal>(actor))
			decal->DetachDecal();
		// The original deletes it here; the fork frees it at the next
		// collection, which makes every other reference to it None (a
		// window's, a conversation's): one deleted now would leave those
		// pointing at freed memory.
		actor->Flags |= ObjectFlags::EliminateObject;
		engine->GarbageDestroyedBacklog++;
	}
	DeletedCount = 0;

	if (engine->GarbageDestroyedBacklog >= Engine::GarbageDestroyedThreshold)
		engine->RequestGarbageCollection("destroyed actors");
}
