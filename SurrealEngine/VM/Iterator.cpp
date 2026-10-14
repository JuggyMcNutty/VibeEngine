
#include "Precomp.h"
#include "Iterator.h"
#include "Packages/Core/UClass.h"
#include "Utils/File.h"
#include "Utils/StrTools.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"

#include <algorithm>

// The slots of the level's actors that pass test, rebuilt only when the
// actor list has changed. The actor iterators tested every actor in the level
// for its class -- Liberty Island has ~2,500 -- at a cache miss each on the
// handheld.
template<typename Key, typename Test>
static const Array<int>& ActorSlots(std::map<Key, ULevelBase::ClassSlots>& indexes, ULevel* level, Key key, Test test)
{
	ULevelBase::ClassSlots& index = indexes[key];
	if (index.Version != level->ActorsVersion)
	{
		index.Slots.clear();
		const Array<UActor*>& actors = level->Actors;
		for (size_t i = 0; i < actors.size(); i++)
		{
			if (actors[i] && test(actors[i]))
				index.Slots.push_back((int)i);
		}
		index.Version = level->ActorsVersion;
	}
	return index.Slots;
}

// The slots of actors whose class, or a parent of it, has the name
static const Array<int>& ActorSlotsByName(ULevel* level, const NameString& className)
{
	return ActorSlots(level->ActorsByClassName, level, className.GetCompareIndex(), [&](UActor* actor) { return actor->IsA(className); });
}

AllObjectsIterator::AllObjectsIterator(UObject* BaseClass, UObject** ReturnValue)
	: BaseClass(BaseClass), ReturnValue(ReturnValue), m_Objects(GC::GetObjects()), m_Iterator(m_Objects.begin())
{
}

bool AllObjectsIterator::Next()
{
	while (m_Iterator != m_Objects.end())
	{
		auto currentObject = dynamic_cast<UObject*>(*m_Iterator);

		++m_Iterator;

		// An object deleted (CriticalDelete) is gone to the script, as the
		// original's leaves the object table.
		if (currentObject && !AnyFlags(currentObject->Flags, ObjectFlags::EliminateObject) && currentObject->IsA(BaseClass->Name))
		{
			*ReturnValue = currentObject;
			return true;
		}
	}
	*ReturnValue = nullptr;
	return false;
}

/////////////////////////////////////////////////////////////////////////////

AllActorsIterator::AllActorsIterator(UObject* BaseClass, UObject** ReturnValue, NameString MatchTag)
	: BaseClass(BaseClass), ReturnValue(ReturnValue), MatchTag(MatchTag)
{
}

bool AllActorsIterator::Next()
{
	bool matchTag = !MatchTag.IsNone();

	const Array<UActor*>& actors = engine->Level->Actors;
	const Array<int>& slots = ActorSlotsByName(engine->Level, BaseClass->Name);
	for (auto it = std::lower_bound(slots.begin(), slots.end(), (int)index); it != slots.end(); ++it)
	{
		UActor* actor = actors[*it];
		index = (size_t)*it + 1;
		if (actor && (!matchTag || actor->Tag() == MatchTag))
		{
			*ReturnValue = actor;
			return true;
		}
	}
	index = actors.size();
	*ReturnValue = nullptr;
	return false;
}

/////////////////////////////////////////////////////////////////////////////

BasedActorsIterator::BasedActorsIterator(UActor* Caller, UObject* BaseClass, UObject** Actor) : BaseClass(BaseClass), Actor(Actor)
{
	for (UActor* based : Caller->BasedActors)
	{
		if (!based || !based->Class) continue;
		if (based->IsA(BaseClass->Name))
			BasedActors.push_back(based);
	}
	iterator = BasedActors.begin();
}

bool BasedActorsIterator::Next()
{
	if (iterator == BasedActors.end())
	{
		*Actor = nullptr;
		return false;
	}

	*Actor = *iterator;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////

ChildActorsIterator::ChildActorsIterator(UActor* Caller, UObject* BaseClass, UObject** Actor) : BaseClass(BaseClass), Actor(Actor)
{
	for (UActor* levelActor : Caller->ChildActors)
	{
		if (levelActor->IsA(BaseClass->Name))
			ChildActors.push_back(levelActor);
	}

	iterator = ChildActors.begin();
}

bool ChildActorsIterator::Next()
{
	if (iterator == ChildActors.end())
	{
		*Actor = nullptr;
		return false;
	}

	*Actor = *iterator;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////
CycleActorsIterator::CycleActorsIterator(UObject* BaseClass, UObject** Actor, int* outIndex) : BaseClass(UObject::TryCast<UStruct>(BaseClass)), Actor(Actor), outIndex(outIndex)
{
	size_t size = engine->Level->Actors.size();
	position = (outIndex && *outIndex >= 0 && (size_t)*outIndex < size) ? (size_t)*outIndex : 0;
}

// Every NPC's CheckEnemyPresence cycles through the pawns this way each tick,
// and a scan of the level's actors for the next one cost a cache miss per
// actor -- about a sixth of the game tick on the handheld. The level keeps
// the slots holding each class asked for, rebuilt when its actor list
// changes, so each step jumps to the next such slot. The order, the lap and
// the indexes are the scan's.
bool CycleActorsIterator::Next()
{
	ULevel* level = engine->Level;
	const Array<UActor*>& actors = level->Actors;
	size_t size = actors.size();
	if (!BaseClass || size == 0 || position >= size)
		return NextByScan();

	// Class pointers along the BaseStruct chain, as the scan tests them
	const Array<int>& slots = ActorSlots(level->ActorsByClass, level, BaseClass, [&](UActor* actor) {
		for (UStruct* cls = actor->Class; cls; cls = cls->BaseStruct)
		{
			if (cls == BaseClass)
				return true;
		}
		return false;
	});

	while (visited < size && !slots.empty())
	{
		// The next slot of the class after position, going round: the scan
		// would reach it after step more slots, the start's own slot last.
		auto it = std::upper_bound(slots.begin(), slots.end(), (int)position);
		size_t slot = (it != slots.end()) ? (size_t)*it : (size_t)slots.front();
		size_t step = (slot + size - position) % size;
		if (step == 0)
			step = size;
		if (visited + step > size)
			break;
		visited += step;
		position = slot;

		UActor* candidate = actors[slot];
		if (!candidate || candidate->bDeleteMe())
			continue;
		*Actor = candidate;
		if (outIndex)
			*outIndex = (int)position;
		return true;
	}
	visited = size;
	*Actor = nullptr;
	return false;
}

// No class, an empty level or a start past the end: the scan itself
bool CycleActorsIterator::NextByScan()
{
	const Array<UActor*>& actors = engine->Level->Actors;
	size_t size = actors.size();
	while (BaseClass && size > 0 && visited < size)
	{
		visited++;
		position = (position + 1) % size;
		UActor* candidate = actors[position];
		if (!candidate || candidate->bDeleteMe())
			continue;

		// Class pointers along the BaseStruct chain: the same test as
		// IsA(BaseClass->Name) without a name comparison per level.
		for (UStruct* cls = candidate->Class; cls; cls = cls->BaseStruct)
		{
			if (cls == BaseClass)
			{
				*Actor = candidate;
				if (outIndex)
					*outIndex = (int)position;
				return true;
			}
		}
	}
	*Actor = nullptr;
	return false;
}

/////////////////////////////////////////////////////////////////////////////

RadiusActorsIterator::RadiusActorsIterator(UActor* Caller, UObject* BaseClass, UObject** Actor, float Radius, vec3 Location) : BaseClass(BaseClass), Actor(Actor), Radius(Radius), Location(Location)
{
	// Deus Ex's reaches each actor's own collision radius further, as the
	// original's does (Engine.dll 0x103e4fa0, dx-reverse-info engine-dll.md,
	// traces): its location strictly within Radius + CollisionRadius
	bool deusEx = engine->LaunchInfo.IsDeusEx();
	const Array<UActor*>& actors = engine->Level->Actors;
	for (int slot : ActorSlotsByName(engine->Level, BaseClass->Name))
	{
		UActor* levelActor = actors[slot];
		vec3 d = levelActor->Location() - Location;
		bool within = deusEx ? dot(d, d) < (Radius + levelActor->CollisionRadius()) * (Radius + levelActor->CollisionRadius()) : length(d) <= Radius;
		if (within)
			RadiusActors.push_back(levelActor);
	}

	iterator = RadiusActors.begin();
}

bool RadiusActorsIterator::Next()
{
	if (iterator == RadiusActors.end())
	{
		*Actor = nullptr;
		return false;
	}

	*Actor = *iterator;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////

TouchingActorsIterator::TouchingActorsIterator(UActor* Caller, UObject* BaseClass, UObject** outActor) : BaseClass(BaseClass), outActor(outActor)
{
	CollisionHitList hitList = Caller->XLevel()->Collision.OverlapTest(Caller->Location(), Caller->CollisionHeight(), Caller->CollisionRadius(), true, false, false);
	for (auto& hit : hitList)
	{
		// Only allow the Actors of type BaseClass
		if (hit.Actor && hit.Actor->IsA(BaseClass->Name))
			TouchingActors.push_back(hit.Actor);
	}

	iterator = TouchingActors.begin();
}

bool TouchingActorsIterator::Next()
{
	if (iterator == TouchingActors.end())
	{
		*outActor = nullptr;
		return false;
	}
	
	*outActor = *iterator;
	iterator++;

	return *outActor;
}

/////////////////////////////////////////////////////////////////////////////

static CollisionHitList DeusExMultiLineCheck(const vec3& start, const vec3& end, const vec3& extent, bool visibilityOnly);
static vec3 HitLocationOf(const vec3& start, const vec3& end, float fraction);

// As the Deus Ex trace iterators: the original's MultiLineCheck from Start
// to End, every hit nearest first, the level's included as the LevelInfo,
// nothing past the first wall; BaseClass is not used (TraceConsole).
TraceActorsIterator::TraceActorsIterator(UActor* SelfActor, UObject* BaseClass, UObject** Actor, vec3* HitLoc, vec3* HitNorm, const vec3& End, const vec3& Start, const vec3& Extent) : SelfActor(SelfActor), BaseClass(BaseClass), Actor(Actor), HitLoc(HitLoc), HitNorm(HitNorm), End(End), Start(Start), Extent(Extent)
{
	for (const CollisionHit& hit : DeusExMultiLineCheck(Start, End, Extent, false))
		tracedActors.push_back({ hit.Actor, HitLocationOf(Start, End, hit.Fraction), hit.Normal });

	iterator = tracedActors.begin();
}

bool TraceActorsIterator::Next()
{
	if (iterator == tracedActors.end() || tracedActors.empty())
	{
		*Actor = nullptr;
		*HitLoc = vec3(0.0f);
		*HitNorm = vec3(0.0f);
		return false;
	}

	*Actor = iterator->tracedActor;
	*HitLoc = iterator->HitLoc;
	*HitNorm = iterator->HitNorm;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////

VisibleActorsIterator::VisibleActorsIterator(UActor* Caller, UObject* BaseClass, UObject** Actor, float Radius, const vec3& Location) : BaseClass(BaseClass), Actor(Actor), Radius(Radius), Location(Location)
{
	// Deus Ex's as the original's: within the radius strictly, a radius of 0
	// no limit.
	bool deusEx = engine->LaunchInfo.IsDeusEx();
	const Array<UActor*>& actors = engine->Level->Actors;
	for (int slot : ActorSlotsByName(engine->Level, BaseClass->Name))
	{
		// Our checks (the slots are actors of the class of BaseClass):
		// * Whether the actor we're dealing with is not hidden
		// * Then whether the distance of the actor from our given Location is no more than Radius
		UActor* levelActor = actors[slot];
		float dist = length(levelActor->Location() - Location);
		bool inRadius = deusEx ? (Radius == 0.0f || dist < Radius) : dist <= Radius;
		if (!levelActor->bHidden() && inRadius && Caller->FastTrace(levelActor->Location(), Location))
		{
			VisibleActors.push_back(levelActor);
		}
	}

	iterator = VisibleActors.begin();
}

bool VisibleActorsIterator::Next()
{
	if (iterator == VisibleActors.end())
	{
		*Actor = nullptr;
		return false;
	}

	*Actor = *iterator;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////

// Deus Ex's as the original's (AActor::execVisibleCollidingActors): each
// colliding actor, movers too, whose location lies within the radius -- 1000
// for none --, of the class, passed over when hidden only if bIgnoreHidden
// asks, and with the line from the spot clear as it comes, as FastTrace asks
// it: UModel::FastLineCheck, the level's BSP with the movers' polygons in it.
// The fork's asked no line at all, so HurtRadius hurt through walls, and
// passed over the hidden unless asked not to.
VisibleCollidingActorsIterator::VisibleCollidingActorsIterator(UObject* BaseClass, UObject** ReturnValue, float Radius, const vec3& Location, bool IgnoreHidden) : BaseClass(BaseClass), ReturnValue(ReturnValue), Radius(Radius), Location(Location), IgnoreHidden(IgnoreHidden)
{
	DeusEx = engine->LaunchInfo.IsDeusEx();
	if (DeusEx)
		HitActors = engine->Level->Collision.ActorRadiusCheck(Location, Radius != 0.0f ? Radius : 1000.0f);
	else
		HitActors = engine->Level->Collision.CollidingActors(Location, Radius);
}

bool VisibleCollidingActorsIterator::Next()
{
	size_t size = HitActors.size();
	while (index < size)
	{
		UActor* actor = HitActors[index++];
		if (!actor || !actor->IsA(BaseClass->Name))
			continue;
		if (DeusEx ? (IgnoreHidden && actor->bHidden()) || engine->Level->Collision.TraceAnyHit(Location, actor->Location(), nullptr, false, true, false) : !(IgnoreHidden || !actor->bHidden()))
			continue;
		*ReturnValue = actor;
		return true;
	}
	*ReturnValue = nullptr;
	return false;
}

/////////////////////////////////////////////////////////////////////////////

ZoneActorsIterator::ZoneActorsIterator(UZoneInfo* zone, UObject* BaseClass, UObject** Actor) : Zone(zone), BaseClass(BaseClass), Actor(Actor)
{
	for (UActor* levelActor : engine->Level->Actors)
	{
		if (levelActor && levelActor->Region().Zone == zone && levelActor->IsA(BaseClass->Name))
		{
			ZoneActors.push_back(levelActor);
		}
	}

	iterator = ZoneActors.begin();
}

bool ZoneActorsIterator::Next()
{
	if (iterator == ZoneActors.end())
	{
		*Actor = nullptr;
		return false;
	}

	*Actor = *iterator;
	iterator++;

	return *Actor;
}

/////////////////////////////////////////////////////////////////////////////

// The original's MultiLineCheck under the two Deus Ex trace iterators: the
// hits as Deus Ex's trace orders them (TraceTester::Trace: the level's one
// hit, the line cut to 5 units past it, the actors along what is left,
// nearest first), the level's with the LevelInfo for its actor; up to 64 in
// all. Nothing beyond the first wall.
static CollisionHitList DeusExMultiLineCheck(const vec3& start, const vec3& end, const vec3& extent, bool visibilityOnly)
{
	float radius = length(extent.xy());
	float height = std::abs(extent.z);

	CollisionHitList result;
	int count = 0;
	for (CollisionHit hit : engine->Level->Collision.Trace(start, end, height, radius, true, true, visibilityOnly))
	{
		if (count++ >= 64)
			break;
		if (!hit.Actor)
			hit.Actor = engine->LevelInfo;
		result.push_back(hit);
	}
	return result;
}

// Where a hit lies: the start along the line, as the original's hit location
// (Start + Direction * Time), which keeps a coordinate the line does not change
static vec3 HitLocationOf(const vec3& start, const vec3& end, float fraction)
{
	return start + (end - start) * fraction;
}

TraceTextureIterator::TraceTextureIterator(UObject* BaseClass, UObject** OutActor, NameString* TexName, NameString* TexGroup, int* Flags, vec3* HitLoc, vec3* HitNorm, const vec3& End, const vec3& Start, const vec3& Extent)
	: OutActor(OutActor), TexName(TexName), TexGroup(TexGroup), flags(Flags), HitLoc(HitLoc), HitNorm(HitNorm), End(End), Start(Start)
{
	m_CollList = DeusExMultiLineCheck(Start, End, Extent, false);
	m_Iterator = m_CollList.begin();
}

bool TraceTextureIterator::Next()
{
	if (m_Iterator == m_CollList.end())
	{
		*OutActor = nullptr;
		return false;
	}
	const CollisionHit& hit = *m_Iterator;
	++m_Iterator;

	// Every hit, in turn. A hit on the level gives the texture of the
	// surface hit, its group (the texture's outer) and the surface's
	// PolyFlags -- a filed mover's polygon's, where the hit is one
	// (CollisionSystem, filed movers) --; an actor, no texture and flags 0.
	*OutActor = hit.Actor;
	*HitLoc = HitLocationOf(Start, End, hit.Fraction);
	*HitNorm = hit.Normal;
	UTexture* texture = nullptr;
	*flags = 0;
	if (hit.Actor == engine->LevelInfo && hit.Node)
	{
		// The node the line meets the level at, as the original's BSP check
		// gives it -- the head of the plane's coplanar nodes -- not the
		// polygon of them it crossed.
		const BspNode* node = hit.NodeHead ? hit.NodeHead : hit.Node;
		const BspSurface& surface = engine->Level->Model->Surfaces[node->Surf];
		texture = surface.Material;
		*flags = surface.PolyFlags;
	}
	else if (hit.Actor == engine->LevelInfo && hit.FiledPoly)
	{
		// The filed polygon's surface, as the node the original files has it
		texture = hit.FiledPoly->Texture;
		*flags = hit.FiledPoly->PolyFlags;
	}
	*TexName = texture ? texture->Name : NameString();
	if (texture && texture->Outer())
		*TexGroup = texture->Outer()->Name;
	else if (texture && texture->package)
		*TexGroup = texture->package->GetPackageName(); // a texture with no group: its package, the outer the original names
	else
		*TexGroup = NameString();
	return true;
}

/////////////////////////////////////////////////////////////////////////////

TraceVisibleActorsIterator::TraceVisibleActorsIterator(UObject* BaseClass, UObject** OutActor, vec3* HitLoc, vec3* HitNorm, const vec3& End, const vec3& Start, const vec3& Extent)
	: OutActor(OutActor), HitLoc(HitLoc), HitNorm(HitNorm), End(End), Start(Start)
{
	m_CollList = DeusExMultiLineCheck(Start, End, Extent, true);
	m_Iterator = m_CollList.begin();
}

bool TraceVisibleActorsIterator::Next()
{
	if (m_Iterator == m_CollList.end())
	{
		*OutActor = nullptr;
		return false;
	}
	const CollisionHit& hit = *m_Iterator;
	++m_Iterator;

	*OutActor = hit.Actor;
	*HitLoc = mix(Start, End, hit.Fraction);
	*HitNorm = hit.Normal;
	return true;
}

void AllObjectsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	if (!(m_Iterator == m_Objects.end()))
		marker.MarkConst(*m_Iterator);
}

void AllActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
}

void BasedActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : BasedActors)
		marker.MarkConst(a);
}

void ChildActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : ChildActors)
		marker.MarkConst(a);
}

void CycleActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
}

void RadiusActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : RadiusActors)
		marker.MarkConst(a);
}

void TouchingActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : TouchingActors)
		marker.MarkConst(a);
}

void TraceActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(SelfActor);
	marker.MarkConst(BaseClass);
	for (TraceInfo& t : tracedActors)
		marker.MarkConst(t.tracedActor);
}

void VisibleActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : VisibleActors)
		marker.MarkConst(a);
}

void VisibleCollidingActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(BaseClass);
	for (UActor* a : HitActors)
		marker.MarkConst(a);
}

void ZoneActorsIterator::Mark(GCMarker& marker)
{
	marker.MarkConst(Zone);
	marker.MarkConst(BaseClass);
	for (UActor* a : ZoneActors)
		marker.MarkConst(a);
}

void TraceTextureIterator::Mark(GCMarker& marker)
{
	for (const CollisionHit& hit : m_CollList)
		marker.MarkConst(hit.Actor);
}

void TraceVisibleActorsIterator::Mark(GCMarker& marker)
{
	for (const CollisionHit& hit : m_CollList)
		marker.MarkConst(hit.Actor);
}
