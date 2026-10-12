
#include "Precomp.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "CollisionSystem.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Math/floating.h"
#include "Math/coords.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Collision/TopLevel/OverlapTest.h"
#include "Collision/TopLevel/TraceTest.h"

bool CollisionSystem::IsOverlapping(UActor* actor1, UActor* actor2)
{
	OverlapTester overlap(this);
	return overlap.IsOverlapping(actor1, actor2);
}

CollisionHitList CollisionSystem::OverlapTest(UActor* actor)
{
	return OverlapTest(actor->Location(), actor->CollisionHeight(), actor->CollisionRadius(), true, false, false);
}

CollisionHitList CollisionSystem::OverlapTest(const vec3& location, float height, float radius, bool testActors, bool testWorld, bool visibilityOnly)
{
	OverlapTester overlap(this);
	return overlap.TestOverlap(location, height, radius, testActors, testWorld, visibilityOnly);
}

bool CollisionSystem::TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	TraceTester trace(this);
	return trace.TraceAnyHit(from, to, tracingActor, traceActors, traceWorld, visibilityOnly);
}

CollisionHitList CollisionSystem::Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	TraceTester trace(this);
	return trace.Trace(from, to, height, radius, traceActors, traceWorld, visibilityOnly);
}

CollisionHitList CollisionSystem::TraceDecal(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly)
{
	TraceRayModel trace;
	return trace.Trace(Level->Model, origin, tmin, dirNormalized, tmax, visibilityOnly);
}

bool CollisionSystem::SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight)
{
	TraceTester trace(this);
	return trace.SightBlocked(from, to, blocksSight);
}

Array<UActor*> CollisionSystem::ActorRadiusCheck(const vec3& origin, float radius)
{
	OverlapTester overlap(this);
	return overlap.ActorRadiusCheck(origin, radius);
}

CollisionHit CollisionSystem::TraceFirstHit(const vec3& from, const vec3& to, UActor* tracingActor, const vec3& extents, const TraceFlags& flags)
{
	for (const CollisionHit& hit : Trace(from, to, extents.z, extents.x, flags.traceActors(), flags.traceWorld(), false))
	{
		if (hit.Actor && (!tracingActor || !tracingActor->IsOwnedBy(hit.Actor)))
		{
			if (hit.Actor->IsA("Pawn"))
			{
				if (flags.pawns)
					return hit;
			}
			else if (hit.Actor->IsA("Mover"))
			{
				if (flags.movers)
					return hit;
			}
			else if (hit.Actor->IsA("ZoneInfo"))
			{
				if (flags.zoneChanges)
					return hit;
			}
			else if (flags.others)
			{
				if (!flags.onlyProjectiles || hit.Actor->bProjTarget() || (hit.Actor->bBlockActors() && hit.Actor->bBlockPlayers()))
					return hit;
			}
		}
		else if (flags.world && !hit.Actor)
		{
			CollisionHit worldHit = hit;
			if (tracingActor)
				worldHit.Actor = tracingActor->Level();
			return worldHit;
		}
	}
	return {};
}

Array<UActor*> CollisionSystem::CollidingActors(const vec3& origin, float radius)
{
	OverlapTester overlap(this);
	return overlap.CollidingActors(origin, radius);
}

Array<UActor*> CollisionSystem::CollidingActors(const vec3& origin, float height, float radius)
{
	OverlapTester overlap(this);
	return overlap.CollidingActors(origin, height, radius);
}

Array<UActor*> CollisionSystem::EncroachingActors(UActor* actor)
{
	OverlapTester overlap(this);
	return overlap.EncroachingActors(actor);
}

Array<UActor*> CollisionSystem::EncroachedActors(const vec3& location, float height, float radius)
{
	OverlapTester overlap(this);
	return overlap.EncroachedActors(location, height, radius);
}

void CollisionSystem::SetLevel(ULevel* level)
{
	Level = level;
}

void CollisionSystem::AddToCollision(UActor* actor)
{
	if (actor->bCollideActors())
	{
		vec3 location = actor->Location();
		float height = actor->CollisionHeight();
		float radius = actor->CollisionRadius();
		vec3 extents = { radius, radius, height };

		if (UMover* mover = UObject::TryCast<UMover>(actor))
		{
			UModel* brush = mover->Brush();
			if (brush)
			{
				mat4 objectToWorld = mat4::translate(actor->Location()) * Coords::Rotation(actor->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-actor->PrePivot());
				BBox bbox = brush->BoundingBox.transform(objectToWorld);
				location = bbox.center();
				extents = bbox.extents() + 0.1f; // 0.1 for numerical stability
			}
		}

		actor->Collision.Inserted = true;
		actor->Collision.Location = location;
		actor->Collision.Extents = extents;

		ivec3 start = GetStartExtents(location, extents);
		ivec3 end = GetEndExtents(location, extents);
		for (int z = start.z; z < end.z; z++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int x = start.x; x < end.x; x++)
				{
					ActorCells.Get(GetBucketId(x, y, z)).push_back(actor);
				}
			}
		}
	}
}

void CollisionSystem::RemoveFromCollision(UActor* actor)
{
	if (actor->Collision.Inserted)
	{
		vec3 location = actor->Collision.Location;
		vec3 extents = actor->Collision.Extents;

		ivec3 start = GetStartExtents(location, extents);
		ivec3 end = GetEndExtents(location, extents);
		for (int z = start.z; z < end.z; z++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int x = start.x; x < end.x; x++)
				{
					ActorCells.Remove(GetBucketId(x, y, z), actor);
				}
			}
		}

		actor->Collision.Inserted = false;
	}
}

// Deus Ex's movers in the BSP (dx-reverse-info engine-dll.md, traces: the
// movers in the BSP). The original's tracker splits each polygon down the
// tree and drops what lands in solid; a line in open space meets the same
// polygons whole. Not carried: the refiling of the movers filed under one
// that is filed again, which the original does wherever they are.
void CollisionSystem::FileMovers()
{
	Filed.clear();
	FiledCells.Clear();
	if (!Level)
		return;
	for (UActor* actor : Level->Actors)
	{
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (!mover || mover->bStatic() || mover->bDeleteMe() || !mover->Brush())
			continue;
		auto filed = std::make_unique<FiledMover>();
		File(mover, *filed);
		AddFiledToCells(filed.get());
		Filed.push_back(std::move(filed));
	}
}

// Each frame drawn, every mover not hidden, in view or not, that has moved
// or turned since it was filed is filed again where it is
// (Render.dll's SetupDynamics).
void CollisionSystem::RefileMovers()
{
	for (auto& filed : Filed)
	{
		UMover* mover = filed->Mover;
		if (!mover || mover->bHidden() || (mover->Location() == filed->Location && mover->Rotation() == filed->Rotation))
			continue;
		RemoveFiledFromCells(filed.get());
		File(mover, *filed);
		AddFiledToCells(filed.get());
	}
}

// The original's DestroyActor does not tell the tracker: the mover's
// polygons stay where it was last filed until the tracker is made again.
void CollisionSystem::MoverDestroyed(UActor* actor)
{
	for (auto& filed : Filed)
	{
		if (filed->Mover == actor)
			filed->Mover = nullptr;
	}
}

// The brush's polygons where the mover is, as the renderer places them; the
// filing recorded in its SavedPos and SavedRot, as the original's is
void CollisionSystem::File(UMover* mover, FiledMover& filed)
{
	filed.Mover = mover;
	filed.Location = mover->Location();
	filed.Rotation = mover->Rotation();
	mover->SavedPos() = filed.Location;
	mover->SavedRot() = filed.Rotation;
	filed.Polys.clear();

	mat4 objectToWorld = mat4::translate(mover->Location()) * Coords::Rotation(mover->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-mover->PrePivot());
	mat4 tableRotation = Coords::TableRotation(mover->Rotation()).ToMatrix();
	vec3 boundsMin(1e30f), boundsMax(-1e30f);
	UModel* brush = mover->Brush();
	if (brush && brush->Polys)
	{
		for (const Poly& poly : brush->Polys->Polys)
		{
			if (poly.Vertices.size() < 3)
				continue;
			FiledMoverPoly filedPoly;
			for (const vec3& v : poly.Vertices)
			{
				vec3 world = (objectToWorld * vec4(v, 1.0f)).xyz();
				filedPoly.Vertices.push_back(world);
				boundsMin = vec3(std::min(boundsMin.x, world.x), std::min(boundsMin.y, world.y), std::min(boundsMin.z, world.z));
				boundsMax = vec3(std::max(boundsMax.x, world.x), std::max(boundsMax.y, world.y), std::max(boundsMax.z, world.z));
			}
			// The plane from the corners (Newell's), facing as the polygon does
			vec3 n(0.0f);
			const Array<vec3>& w = filedPoly.Vertices;
			for (size_t i = 0; i < w.size(); i++)
			{
				const vec3& a = w[i];
				const vec3& b = w[(i + 1) % w.size()];
				n.x += (a.y - b.y) * (a.z + b.z);
				n.y += (a.z - b.z) * (a.x + b.x);
				n.z += (a.x - b.x) * (a.y + b.y);
			}
			float len = length(n);
			if (len <= 0.0f)
				continue;
			n = n / len;
			vec3 facing = (objectToWorld * vec4(poly.Normal, 0.0f)).xyz();
			if (dot(n, facing) < 0.0f)
				n = -n;
			filedPoly.Normal = n;
			filedPoly.PlaneW = dot(n, w[0]);
			filedPoly.NormalOriginal = (tableRotation * vec4(poly.Normal, 0.0f)).xyz();
			filedPoly.PolyFlags = poly.PolyFlags;
			filedPoly.Texture = poly.Texture;
			filed.Polys.push_back(std::move(filedPoly));
		}
	}
	if (filed.Polys.empty())
	{
		boundsMin = boundsMax = mover->Location();
	}
	filed.Center = (boundsMin + boundsMax) * 0.5f;
	filed.Extents = (boundsMax - boundsMin) * 0.5f + 0.1f;
}

void CollisionSystem::AddFiledToCells(FiledMover* filed)
{
	ivec3 start = GetStartExtents(filed->Center, filed->Extents);
	ivec3 end = GetEndExtents(filed->Center, filed->Extents);
	for (int z = start.z; z < end.z; z++)
		for (int y = start.y; y < end.y; y++)
			for (int x = start.x; x < end.x; x++)
				FiledCells.Get(GetBucketId(x, y, z)).push_back(filed);
}

void CollisionSystem::RemoveFiledFromCells(FiledMover* filed)
{
	ivec3 start = GetStartExtents(filed->Center, filed->Extents);
	ivec3 end = GetEndExtents(filed->Center, filed->Extents);
	for (int z = start.z; z < end.z; z++)
		for (int y = start.y; y < end.y; y++)
			for (int x = start.x; x < end.x; x++)
				FiledCells.Remove(GetBucketId(x, y, z), filed);
}

// Calls visit once for each filed mover in the cells the segment's box
// reaches, until it returns true; then so does this.
template<typename Visit>
bool CollisionSystem::VisitFiledOnRay(const vec3& from, const vec3& to, Visit&& visit)
{
	if (Filed.empty())
		return false;
	int checkCounter = ++FiledCheckCounter;
	ivec3 start = GetRayStartExtents(from, to);
	ivec3 end = GetRayEndExtents(from, to);
	if (end.x - start.x >= 100 || end.y - start.y >= 100 || end.z - start.z >= 100)
	{
		// A long line: every filed mover, there being few
		for (auto& filed : Filed)
			if (visit(filed.get()))
				return true;
		return false;
	}
	for (int z = start.z; z < end.z; z++)
	{
		for (int y = start.y; y < end.y; y++)
		{
			for (int x = start.x; x < end.x; x++)
			{
				for (FiledMover* filed : FiledCells.Find(GetBucketId(x, y, z)))
				{
					if (filed->CheckCounter != checkCounter)
					{
						filed->CheckCounter = checkCounter;
						if (visit(filed))
							return true;
					}
				}
			}
		}
	}
	return false;
}

// Where the segment from + t * delta, t in 0..1, crosses the polygon, from
// either side
bool CollisionSystem::FiledPolyHit(const FiledMoverPoly& poly, const dvec3& from, const dvec3& delta, double& t)
{
	dvec3 n = to_dvec3(poly.Normal);
	double d0 = dot(n, from) - poly.PlaneW;
	double d1 = d0 + dot(n, delta);
	if ((d0 > 0.0 && d1 > 0.0) || (d0 < 0.0 && d1 < 0.0) || d0 == d1)
		return false;
	t = d0 / (d0 - d1);
	dvec3 p = from + delta * t;
	// Inside a convex polygon: on one side of every edge
	int sign = 0;
	const Array<vec3>& v = poly.Vertices;
	for (size_t i = 0; i < v.size(); i++)
	{
		dvec3 a = to_dvec3(v[i]);
		dvec3 b = to_dvec3(v[(i + 1) % v.size()]);
		double side = dot(cross(b - a, p - a), n);
		if (side > 1e-6)
		{
			if (sign < 0)
				return false;
			sign = 1;
		}
		else if (side < -1e-6)
		{
			if (sign > 0)
				return false;
			sign = -1;
		}
	}
	return true;
}

static bool FiledPolyPasses(const FiledMoverPoly& poly, bool visibilityOnly)
{
	if (poly.PolyFlags & PF_NotSolid)
		return true;
	return visibilityOnly && (poly.PolyFlags & (PF_Invisible | PF_Portal));
}

bool CollisionSystem::FiledMoversBlock(const vec3& from, const vec3& to, bool visibilityOnly)
{
	dvec3 origin = to_dvec3(from);
	dvec3 delta = to_dvec3(to) - origin;
	return VisitFiledOnRay(from, to, [&](FiledMover* filed) {
		for (const FiledMoverPoly& poly : filed->Polys)
		{
			double t;
			if (!FiledPolyPasses(poly, visibilityOnly) && FiledPolyHit(poly, origin, delta, t))
				return true;
		}
		return false;
	});
}

void CollisionSystem::TraceFiledMovers(const dvec3& origin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, CollisionHitList& hits)
{
	dvec3 delta = dirNormalized * tmax;
	vec3 from = vec3(origin);
	vec3 to = vec3(origin + delta);
	VisitFiledOnRay(from, to, [&](FiledMover* filed) {
		// The nearest polygon crossed; of two in one plane, the one filed
		// first, which the original's tracker files as the node the walk
		// crosses, a later coplanar one as a node beyond it
		double best = 2.0;
		const FiledMoverPoly* bestPoly = nullptr;
		for (const FiledMoverPoly& poly : filed->Polys)
		{
			double t;
			if (!FiledPolyPasses(poly, visibilityOnly) && FiledPolyHit(poly, origin, delta, t) && (best - t) * tmax > 0.001)
			{
				best = t;
				bestPoly = &poly;
			}
		}
		if (best <= 1.0)
		{
			// The surface is the first polygon filed in that plane, whose node
			// the original's walk crosses whatever the polygon's own extent
			for (const FiledMoverPoly& poly : filed->Polys)
			{
				if (&poly == bestPoly)
					break;
				if (dot(poly.Normal, bestPoly->Normal) > 0.9999f && std::abs(poly.PlaneW - bestPoly->PlaneW) < 0.001f && !FiledPolyPasses(poly, visibilityOnly))
				{
					bestPoly = &poly;
					break;
				}
			}
			// The node's plane, turned to face the line's origin as the
			// original gives the level's hits
			vec3 normal = dot(to_dvec3(bestPoly->NormalOriginal), dirNormalized) > 0.0 ? -bestPoly->NormalOriginal : bestPoly->NormalOriginal;
			CollisionHit hit((float)(best * tmax), normal, nullptr, nullptr, nullptr);
			hit.Filed = true;
			hit.FiledPoly = bestPoly;
			// A polygon filed in the plane of a node of the level's own that
			// the line crosses there is met as that node: the original's walk
			// crosses the node's plane and finds the filed polygon's, within a
			// thousandth, behind it, so the hit is the node's, its normal and
			// surface (DXMP_Silo's DeusExMover19 and 25 against the wall round
			// their doorways: VisibleConsole)
			if (BspNode* node = TraceRayModel::CoplanarNode(Level->Model, origin + delta * best, origin, origin + delta))
			{
				hit.Node = hit.NodeHead = node;
				hit.FiledPoly = nullptr;
				hit.Normal = vec3(node->PlaneX, node->PlaneY, node->PlaneZ);
				if (dot(to_dvec3(hit.Normal), dirNormalized) > 0.0)
					hit.Normal = -hit.Normal;
			}
			hits.push_back(hit);
		}
		return false;
	});
}
