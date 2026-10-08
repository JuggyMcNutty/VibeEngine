#pragma once

#include "ULevelBase.h"
#include "Math/bbox.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Collision/TopLevel/CollisionHit.h"
#include "Light/LightSystem.h"

class UNavigationPoint;
class UModel;

enum EReachSpecFlags
{
	R_WALK = 1,
	R_FLY = 2,
	R_SWIM = 4,
	R_JUMP = 8,
	R_DOOR = 16,
	R_SPECIAL = 32,
	R_PLAYERONLY = 64
};

class LevelReachSpec
{
public:
	int32_t distance = 0;
	UNavigationPoint* startActor = nullptr;
	UNavigationPoint* endActor = nullptr;
	int32_t collisionRadius = 0;
	int32_t collisionHeight = 0;
	int32_t reachFlags = 0;
	int8_t bPruned = 0;
};

class ULevel : public ULevelBase
{
public:
	ULevel(NameString name, UClass* base, ObjectFlags flags);

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	void Tick(float elapsed, bool gamePaused);

	// Deus Ex: the actor list in the original's order, at every load after
	// PostPostBeginPlay -- its first two slots, then the static actors, then
	// the rest, empty slots dropped -- and its tick from the first dynamic
	// actor: a static actor is never ticked (dx-reverse-info engine-dll.md,
	// the actor list's order). 0 for a level never sorted.
	void SortActors();
	size_t FirstDynamicActor = 0;

	// Deus Ex: the mark an actor ticked this frame carries (bTicked), which
	// flips after each tick; a spawned actor takes its opposite, not yet
	// ticked.
	bool TickMark() const { return ticked; }

	// Deus Ex: the start of an actor's tick (UActor::StartTick) tells the
	// level how it went: an actor whose owner has not ticked this frame
	// waits, and is ticked after the pass once its owner has.
	enum class TickStart { Ticked, Stasis, Waiting };
	void TickStarted(UActor* actor, TickStart start);

	// The reach specs' ends, the BSP model and the destroyed actors' chain.
	// The collision hash and the light caches hold only actors of Actors,
	// kept there.
	void Mark(GCMarker& marker) override;

	Array<LevelReachSpec> ReachSpecs;
	UModel* Model = nullptr;

	CollisionSystem Collision;
	LightSystem Light;
	std::map<std::string, std::string> TravelInfo;

	// The actors destroyed, last first, linked through their Deleted, and how
	// many: freed in a batch, as the original's (dx-reverse-info
	// engine-dll.md, destroyed actors).
	UActor* FirstDeleted = nullptr;
	int DeletedCount = 0;
	// Once 128 wait, or forced (before a save): every live actor's
	// references to them made None, the event manager told, and each flagged
	// to be freed at the next collection.
	void CleanupDestroyed(bool force);

private:
	void TickActor(float elapsed, UActor* actor);
	void TickDeusEx(float elapsed, bool gamePaused);
	bool TickActorDeusEx(float elapsed, UActor* actor);

	bool ticked = false;
	TickStart lastStart = TickStart::Ticked;
	// The actors waiting for their owners' ticks, the last to wait last.
	Array<UActor*> ownerWait;
};
