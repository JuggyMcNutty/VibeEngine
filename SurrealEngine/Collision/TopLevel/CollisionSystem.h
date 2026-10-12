#pragma once

#include "Math/vec.h"
#include "Math/rotator.h"
#include <functional>
#include <memory>

class ULevel;
class UActor;
class UMover;
class CollisionHitList;
class CollisionHit;

// What reaches each 256-unit cell, in the order it was added. An
// open-addressed table keyed by bucket id (linear probing, a power-of-two
// size, at most half full): finding a cell is a multiply and, for an empty
// one, usually one cache line of ids, where a std::unordered_map of
// std::lists took a division and a chain of pointers, and a list node
// allocated for every cell of every move. A cell keeps its slot once used,
// so the table only ever grows.
template<typename T>
class CellTable
{
public:
	const Array<T>& Find(uint32_t id) const
	{
		size_t slot = FindSlot(id);
		return slot != NoSlot ? Items[slot] : None;
	}

	Array<T>& Get(uint32_t id) // made if new
	{
		if ((Count + 1) * 2 > Ids.size())
			Grow();
		size_t mask = Ids.size() - 1;
		size_t slot = Hash(id) & mask;
		while (Ids[slot] != id && Ids[slot] != NoCell)
			slot = (slot + 1) & mask;
		if (Ids[slot] == NoCell)
		{
			Ids[slot] = id;
			Count++;
		}
		return Items[slot];
	}

	// Every entry, keeping the others in order, as std::list::remove did
	void Remove(uint32_t id, const T& item)
	{
		size_t slot = FindSlot(id);
		if (slot != NoSlot)
		{
			Array<T>& cell = Items[slot];
			cell.erase(std::remove(cell.begin(), cell.end(), item), cell.end());
		}
	}

	void Clear()
	{
		Ids.clear();
		Items.clear();
		Count = 0;
	}

private:
	size_t FindSlot(uint32_t id) const
	{
		if (Ids.empty())
			return NoSlot;
		size_t mask = Ids.size() - 1;
		for (size_t slot = Hash(id) & mask; ; slot = (slot + 1) & mask)
		{
			if (Ids[slot] == id)
				return slot;
			if (Ids[slot] == NoCell)
				return NoSlot;
		}
	}

	void Grow()
	{
		size_t capacity = std::max<size_t>(4096, Ids.size() * 2);
		Array<uint32_t> ids(capacity, NoCell);
		Array<Array<T>> items(capacity);
		size_t mask = capacity - 1;
		for (size_t i = 0; i < Ids.size(); i++)
		{
			if (Ids[i] == NoCell)
				continue;
			size_t slot = Hash(Ids[i]) & mask;
			while (ids[slot] != NoCell)
				slot = (slot + 1) & mask;
			ids[slot] = Ids[i];
			items[slot] = std::move(Items[i]);
		}
		Ids = std::move(ids);
		Items = std::move(items);
	}

	static uint32_t Hash(uint32_t id) { uint32_t h = id * 0x9E3779B1u; return h ^ (h >> 16); }
	static const uint32_t NoCell = 0xffffffff; // never a bucket id: those have 30 bits
	static const size_t NoSlot = ~(size_t)0;

	Array<uint32_t> Ids; // NoCell where a slot is free
	Array<Array<T>> Items;
	size_t Count = 0;
	inline static const Array<T> None;
};

// Deus Ex: a mover where the level's BSP last filed it (dx-reverse-info
// engine-dll.md, traces: the movers in the BSP): its brush's polygons in the
// world as they were then. A line test meets them there, whatever the mover
// collides with, hidden or not, and on after the mover is destroyed.
class UTexture;

struct FiledMoverPoly
{
	Array<vec3> Vertices;
	vec3 Normal;
	float PlaneW; // dot(Normal, a vertex)
	// The polygon's normal turned as the original's tracker turns it, by its
	// sine table: the normal of the node it files, which a hit reports
	// (TraceTester: a mover's hit against the level's)
	vec3 NormalOriginal;
	uint32_t PolyFlags;
	// The polygon's texture, which with its PolyFlags a TraceTexture hit on
	// it gives, as the original's filed node has them for its surface
	UTexture* Texture = nullptr;
};

struct FiledMover
{
	UMover* Mover = nullptr; // none once destroyed: it stays filed
	vec3 Location;
	Rotator Rotation;
	vec3 Center, Extents; // the cells it is in
	Array<FiledMoverPoly> Polys;
	int CheckCounter = 0;
};

struct TraceFlags
{
	bool pawns = false;
	bool movers = false;
	bool others = false;
	bool world = false;
	bool zoneChanges = false;
	bool onlyProjectiles = false;

	bool traceActors() const { return pawns || movers || others || zoneChanges || onlyProjectiles; }
	bool traceWorld() const { return world; }
};

class CollisionSystem
{
public:
	bool IsOverlapping(UActor* actor1, UActor* actor2);
	CollisionHitList OverlapTest(UActor* actor);
	CollisionHitList OverlapTest(const vec3& location, float height, float radius, bool testActors, bool testWorld, bool visibilityOnly);

	bool TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly);
	CollisionHitList Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly);
	CollisionHitList TraceDecal(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly);
	CollisionHit TraceFirstHit(const vec3& from, const vec3& to, UActor* tracingActor, const vec3& extents, const TraceFlags& flags);

	// Whether sight along a segment is blocked: by world and mover surfaces
	// that block visibility, or by an actor on it that blocksSight says blocks
	// (Deus Ex's AI line of sight).
	bool SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight);

	// As the original's FCollisionHash::ActorRadiusCheck: every colliding
	// actor, movers too, whose location lies within radius of origin.
	Array<UActor*> ActorRadiusCheck(const vec3& origin, float radius);

	Array<UActor*> CollidingActors(const vec3& origin, float radius);
	Array<UActor*> CollidingActors(const vec3& origin, float height, float radius);
	Array<UActor*> EncroachingActors(UActor* actor);
	Array<UActor*> EncroachedActors(const vec3& location, float height, float radius);

	void SetLevel(ULevel* level);
	void AddToCollision(UActor* actor);
	void RemoveFromCollision(UActor* actor);

	// Deus Ex's movers in the BSP: filed afresh at a map's load and after a
	// save (all, where they are), each drawn frame again where a shown one
	// has moved or turned since; a destroyed one stays filed.
	void FileMovers();
	void RefileMovers();
	void MoverDestroyed(UActor* actor);
	// Whether a filed mover's polygon crosses the segment: any but a
	// PF_NotSolid one, and with visibilityOnly not a PF_Invisible or
	// PF_Portal one.
	bool FiledMoversBlock(const vec3& from, const vec3& to, bool visibilityOnly);
	// Each filed mover's nearest polygon on the line, as a hit (its
	// Fraction in units along the line, Filed set)
	void TraceFiledMovers(const dvec3& origin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, CollisionHitList& hits);

private:
	static ivec3 GetStartExtents(const vec3& location, const vec3& extents)
	{
		int xx = (int)std::floor((location.x - extents.x) * (1.0f / 256.0f));
		int yy = (int)std::floor((location.y - extents.y) * (1.0f / 256.0f));
		int zz = (int)std::floor((location.z - extents.z) * (1.0f / 256.0f));
		return { xx, yy, zz };
	}

	static ivec3 GetRayStartExtents(const vec3& from, const vec3& to)
	{
		float x = std::min(from.x, to.x);
		float y = std::min(from.y, to.y);
		float z = std::min(from.z, to.z);
		int xx = (int)std::floor(x * (1.0f / 256.0f));
		int yy = (int)std::floor(y * (1.0f / 256.0f));
		int zz = (int)std::floor(z * (1.0f / 256.0f));
		return { xx, yy, zz };
	}

	static ivec3 GetSweepStartExtents(const vec3& from, const vec3& to, const vec3& extents)
	{
		float x = std::min(from.x, to.x) - extents.x;
		float y = std::min(from.y, to.y) - extents.y;
		float z = std::min(from.z, to.z) - extents.z;
		int xx = (int)std::floor(x * (1.0f / 256.0f));
		int yy = (int)std::floor(y * (1.0f / 256.0f));
		int zz = (int)std::floor(z * (1.0f / 256.0f));
		return { xx, yy, zz };
	}

	static ivec3 GetEndExtents(const vec3& location, const vec3& extents)
	{
		int xx = (int)std::floor((location.x + extents.x) * (1.0f / 256.0f)) + 1;
		int yy = (int)std::floor((location.y + extents.y) * (1.0f / 256.0f)) + 1;
		int zz = (int)std::floor((location.z + extents.z) * (1.0f / 256.0f)) + 1;
		return { xx, yy, zz };
	}

	static ivec3 GetRayEndExtents(const vec3& from, const vec3& to)
	{
		float x = std::max(from.x, to.x);
		float y = std::max(from.y, to.y);
		float z = std::max(from.z, to.z);
		int xx = (int)std::floor(x * (1.0f / 256.0f)) + 1;
		int yy = (int)std::floor(y * (1.0f / 256.0f)) + 1;
		int zz = (int)std::floor(z * (1.0f / 256.0f)) + 1;
		return { xx, yy, zz };
	}

	static ivec3 GetSweepEndExtents(const vec3& from, const vec3& to, const vec3& extents)
	{
		float x = std::max(from.x, to.x) + extents.x;
		float y = std::max(from.y, to.y) + extents.y;
		float z = std::max(from.z, to.z) + extents.z;
		int xx = (int)std::floor(x * (1.0f / 256.0f)) + 1;
		int yy = (int)std::floor(y * (1.0f / 256.0f)) + 1;
		int zz = (int)std::floor(z * (1.0f / 256.0f)) + 1;
		return { xx, yy, zz };
	}

	static uint32_t GetBucketId(int x, int y, int z)
	{
		return ((x & 0x3ff) << 20) | ((y & 0x3ff) << 10) | (z & 0x3ff);
	}

	// The actors whose collision boxes reach each cell
	const Array<UActor*>& FindCell(uint32_t id) const { return ActorCells.Find(id); }

	void File(UMover* mover, FiledMover& filed);
	void AddFiledToCells(FiledMover* filed);
	void RemoveFiledFromCells(FiledMover* filed);
	template<typename Visit> bool VisitFiledOnRay(const vec3& from, const vec3& to, Visit&& visit);
	static bool FiledPolyHit(const FiledMoverPoly& poly, const dvec3& from, const dvec3& delta, double& t);

	ULevel* Level = nullptr;
	CellTable<UActor*> ActorCells;
	Array<std::unique_ptr<FiledMover>> Filed;
	CellTable<FiledMover*> FiledCells;
	int FiledCheckCounter = 0;

	friend class CollisionTester;
};
