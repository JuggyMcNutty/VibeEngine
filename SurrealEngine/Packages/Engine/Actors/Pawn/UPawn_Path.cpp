
#include "Precomp.h"
#include "UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Actors/NavigationPoint/UInventorySpot.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Collision/TopLevel/CollisionHit.h"
#include "VM/ScriptCall.h"
#include "Utils/Logger.h"
#include "Engine.h"

UNavigationPoint* UPawn::SetRouteCache(const Array<UNavigationPoint*>& points)
{
	// Deus Ex's engine never fills RouteCache: its SetRouteCache only logs "Hey,
	// who called SetRouteCache?", and nothing calls it (engine-dll.md, the
	// search) -- so Teleporter.SpecialHandling, which looks at RouteCache[1],
	// never finds a teleporter there.
	if (engine->LaunchInfo.ue1Version > 219 && !engine->LaunchInfo.IsDeusEx())
	{
		auto cache = RouteCache();
		for (size_t i = 0; i < cache.size(); i++)
			cache[i] = (i < points.size()) ? points[i] : nullptr;
	}
	return !points.empty() ? points.front() : nullptr;
}

UActor* UPawn::PathSpecialHandling(const Array<UNavigationPoint*>& bestPath)
{
#if 0
	return SetRouteCache(bestPath);
#else
	IsInPathSpecialHandling = true;
	UActor* oldBestPoint = SetRouteCache(bestPath);
	if (!oldBestPoint)
	{
		IsInPathSpecialHandling = false;
		return nullptr;
	}

	UActor* bestPoint = oldBestPoint;

	if (oldBestPoint->IsEventEnabled(EventName::SpecialHandling))
	{
		bestPoint = UObject::Cast<UActor>(CallEvent(oldBestPoint, EventName::SpecialHandling, { ExpressionValue::ObjectValue(this) }).ToObject());
		if (!bCanDoSpecial())
			bestPoint = nullptr;
		SpecialGoal() = bestPoint;

		if (bestPoint && bestPoint != oldBestPoint)
		{
			if (!ActorReachable(bestPoint))
			{
				bestPoint = UObject::Cast<UActor>(FindPathToward(bestPoint, false));
			}
		}
	}
	else
	{
		if (SpecialGoal() == oldBestPoint)
			SpecialGoal() = nullptr;
	}

	IsInPathSpecialHandling = false;
	return bestPoint;
#endif
}

// The original's search (APawn::breadthPathFrom, Engine.dll 0x103dcd60;
// engine-dll.md, the search): best first from the start, over the reach specs
// the other way round, each node it expands the next in its own open list --
// kept sorted by what a node cost to reach -- until it expands an end point.
// What a node costs is the spec's distance plus the node's own penalty (cost,
// from SpecialCost or ExtraCost) plus what reaching the node expanded cost,
// and an end point's bestPathWeight on top. Everything lives in the nodes'
// own fields, as it does in the original: visitedWeight the cost,
// nextOrdered and prevOrdered the open list, previousPath the way back -- so
// the route is the end node's previousPath chain back to the start.
bool UPawn::BreadthPathFrom(UNavigationPoint* start, UNavigationPoint*& endNode, bool singlePath, int moveFlags)
{
	const Array<LevelReachSpec>& reachSpecs = XLevel()->ReachSpecs;

	int radius = (int)CollisionRadius();
	int height = (int)CollisionHeight();

	// Where a node is put back into the open list is found by walking it from
	// a front node, which starts at the start and moves on as the list grows:
	// each node expanded adds 1 to the index, a new node costing more than the
	// front 1 and one costing no more 1 less, a node moved in front of it 1
	// less; and the front steps on until it has stepped half the index.
	int iterations = 0;
	int pathCount = 0;
	int listIndex = 1;
	UNavigationPoint* frontNode = start;

	for (UNavigationPoint* node = start; node; node = node->nextOrdered())
	{
		if (node->bEndPoint())
		{
			start->previousPath() = nullptr;
			endNode = node;
			return true;
		}

		// A node only the player may use is not expanded for a pawn that is not
		// the player; the start is expanded whatever it is.
		if (!(node->bPlayerOnly() && !bIsPlayer() && node != start))
		{
			for (int specIndex : node->upstreamPaths())
			{
				if (specIndex < 0 || (size_t)specIndex >= reachSpecs.size())
					break;
				const LevelReachSpec& reachSpec = reachSpecs[specIndex];

				// upstreamPaths is the reverse travel direction: the node reached
				// is the spec's start.
				UNavigationPoint* reached = reachSpec.startActor;
				if (!reached)
					continue;

				// A link the pawn does not fit, or whose flags are not all the
				// pawn's own (calcMoveFlags), is not taken.
				if (reachSpec.collisionRadius < radius || reachSpec.collisionHeight < height || (moveFlags & reachSpec.reachFlags) != reachSpec.reachFlags)
					continue;

				int32_t newCost = reachSpec.distance + reached->cost() + node->visitedWeight()
				                + (reached->bEndPoint() ? reached->bestPathWeight() : 0);
				if (reached->visitedWeight() <= newCost)
					continue;

				// Take the node out of the open list if it is in it.
				UNavigationPoint* prev = reached->prevOrdered();
				if (prev)
				{
					prev->nextOrdered() = reached->nextOrdered();
					if (reached->nextOrdered())
						reached->nextOrdered()->prevOrdered() = prev;

					if (frontNode == reached)
					{
						if (prev->visitedWeight() > newCost)
							frontNode = prev;
					}
					else if (reached->visitedWeight() > frontNode->visitedWeight() && newCost < frontNode->visitedWeight())
					{
						listIndex--;
					}
				}
				else if (newCost <= frontNode->visitedWeight())
				{
					listIndex--;
				}
				else
				{
					listIndex++;
				}

				reached->previousPath() = node;
				reached->visitedWeight() = newCost;

				// Put it back where its cost belongs: walking from the front when
				// the front costs less, else from the node expanded.
				UNavigationPoint* at = frontNode->visitedWeight() < newCost ? frontNode : node;
				for (int walked = 0; at->nextOrdered() && at->nextOrdered()->visitedWeight() < newCost; at = at->nextOrdered())
				{
					if (++walked > 500)
					{
						LogMessage("Breadth path list overflow from " + start->Name.ToString());
						return false;
					}
				}
				if (at->nextOrdered() != reached)
				{
					UNavigationPoint* after = at->nextOrdered();
					if (after)
						after->prevOrdered() = reached;
					reached->nextOrdered() = after;
					at->nextOrdered() = reached;
					reached->prevOrdered() = at;
				}
			}

			listIndex++;
			while (pathCount < (int)(listIndex * 0.5))
			{
				pathCount++;
				if (frontNode->nextOrdered())
					frontNode = frontNode->nextOrdered();
			}
		}

		// The caps: four nodes for a single-path search, silently; otherwise 1000.
		iterations++;
		if (singlePath && iterations > 4)
			return false;
		if (iterations > 1000)
		{
			LogMessage("1000 navigation nodes searched from " + start->Name.ToString());
			return false;
		}
	}

	return false;
}

std::pair<Array<UNavigationPoint*>, int32_t> UPawn::FindPathToEndPoint(UNavigationPoint* start, int maxNodes)
{
	// Other games' route from the original's search: the end point's way back
	// to the start -- the start the chain's last node -- with the parts the
	// pawn already touches skipped.
	if ((start->bPlayerOnly() && !bIsPlayer()))
		return { {}, 0 };

	UNavigationPoint* endNode = nullptr;
	if (!BreadthPathFrom(start, endNode, maxNodes != 0, CalcMoveFlags()))
		return { {}, 0 };

	int radius = (int)CollisionRadius();
	int height = (int)CollisionHeight();
	Array<UNavigationPoint*> path;
	for (UNavigationPoint* at = endNode; at; at = at->previousPath())
	{
		// Skip path parts we already are touching: within the pawn's
		// cylinder, its radius across and its height up or down. The height
		// was once counted in the distance across too, so a pawn standing on
		// a node a step below its middle kept being sent to it.
		vec3 d = at->Location() - Location();
		if (d.x * d.x + d.y * d.y < (float)radius * (float)radius && std::abs(d.z) < (float)height)
		{
			path.clear();
		}
		else
		{
			path.push_back(at);
		}
	}

	return { path, endNode->visitedWeight() };
}

void UPawn::ClearPaths()
{
	for (UNavigationPoint* cur = Level()->NavigationPointList(); cur; cur = cur->nextNavigationPoint())
	{
		cur->bEndPoint() = false;
		// The search keeps its open list and the route in the nodes' own
		// fields (visitedWeight, nextOrdered, prevOrdered, previousPath), and
		// reads visitedWeight as what a node cost to reach, so they are reset
		// here: every node unreachable and a million more than any route, which
		// is what the original's clearPaths does (APawn::clearPaths,
		// Engine.dll 0x103da050).
		cur->visitedWeight() = 10000000;
		cur->nextOrdered() = nullptr;
		cur->prevOrdered() = nullptr;
		if (!engine->LaunchInfo.IsKlingonHonorGuard())
		{
			if (cur->bSpecialCost())
				cur->cost() = CallEvent(cur, "SpecialCost", { ExpressionValue::ObjectValue(this) }).ToInt();
			else
				cur->cost() = cur->ExtraCost();
		}
	}
}

// ---------------------------------------------------------------------------
// Deus Ex's path search, the original's (Engine.dll findPathToward 0x103db3f0
// and findPathTo 0x103dc1d0; engine-dll.md, the search).

namespace
{
	// Unreal Tournament's FSortedPathList as Deus Ex's Engine.dll has it
	// (addPath 0x103dd170, removePath 0x103dd2a0): up to 32 navigation points,
	// nearest first, each with its distance -- squared as gathered, the first
	// one's root once findEndPoint has taken it.
	struct SortedPathList
	{
		static const int MaxSorted = 32;
		UNavigationPoint* Path[MaxSorted] = {};
		int Dist[MaxSorted] = {};
		int NumPoints = 0;

		void AddPath(UNavigationPoint* node, int dist)
		{
			int n = 0;
			if (NumPoints > 8)
			{
				if (dist > Dist[NumPoints / 2])
				{
					n = NumPoints / 2;
					if (NumPoints > 16 && dist > Dist[n + NumPoints / 4])
						n += NumPoints / 4;
				}
				else if (NumPoints > 16 && dist > Dist[NumPoints / 4])
				{
					n = NumPoints / 4;
				}
			}
			while (n < NumPoints && dist > Dist[n])
				n++;

			if (n < MaxSorted)
			{
				UNavigationPoint* nextPath = Path[n];
				int nextDist = Dist[n];
				Path[n] = node;
				Dist[n] = dist;
				if (NumPoints < MaxSorted)
					NumPoints++;
				for (n++; n < NumPoints; n++)
				{
					std::swap(Path[n], nextPath);
					std::swap(Dist[n], nextDist);
				}
			}
		}

		void RemovePath(int p)
		{
			for (int i = p; i < NumPoints - 1; i++)
			{
				Path[i] = Path[i + 1];
				Dist[i] = Dist[i + 1];
			}
			NumPoints--;
		}
	};

	int SquaredDistance(const vec3& a, const vec3& b)
	{
		vec3 d = a - b;
		return (int)(int64_t)(d.x * (double)d.x + d.y * (double)d.y + d.z * (double)d.z);
	}

	// Whether a reach spec the pawn fits is open to it (definePathsFor's and
	// CanMoveTo's test, Engine.dll 0x103daaa0 and 0x103dad20): the line between
	// its two nodes traced against the level and the movers, and only a mover
	// counting against it -- one the pawn cannot open (bCanOpenDoors, and
	// bIsPlayer or the mover not bPlayerOnly).
	bool SpecOpen(UPawn* pawn, const LevelReachSpec& spec, int moveFlags, int radius, int height)
	{
		if (spec.collisionRadius < radius || spec.collisionHeight < height || (moveFlags & spec.reachFlags) != spec.reachFlags)
			return false;
		if (!spec.startActor || !spec.endActor)
			return false;

		TraceFlags flags;
		flags.movers = true;
		flags.world = true;
		CollisionHit hit = pawn->XLevel()->Collision.TraceFirstHit(spec.startActor->Location(), spec.endActor->Location(), pawn, vec3(0.0f), flags);
		UMover* mover = hit.Actor ? UObject::TryCast<UMover>(hit.Actor) : nullptr;
		if (!mover)
			return true;
		return pawn->bCanOpenDoors() && (pawn->bIsPlayer() || !mover->bPlayerOnly());
	}

	// CanMoveTo (0x103dad20): the goal is the end of one of the anchor's own
	// reach specs (its Paths, then its PrunedPaths), and that spec is open.
	bool CanMoveTo(UPawn* pawn, UNavigationPoint* anchor, UActor* goal)
	{
		const Array<LevelReachSpec>& specs = pawn->XLevel()->ReachSpecs;
		int moveFlags = pawn->CalcMoveFlags();
		int radius = (int)pawn->CollisionRadius();
		int height = (int)pawn->CollisionHeight();
		for (int pruned = 0; pruned < 2; pruned++)
		{
			for (int index : (pruned ? anchor->PrunedPaths() : anchor->Paths()))
			{
				if (index < 0 || (size_t)index >= specs.size())
					break;
				const LevelReachSpec& spec = specs[index];
				if (spec.endActor == goal && SpecOpen(pawn, spec, moveFlags, radius, height))
					return true;
			}
		}
		return false;
	}

	// definePathsFor (0x103daaa0): the end points the search stops at are the
	// anchor's forward neighbours over the specs open to the pawn -- its Paths,
	// and once those end at a -1 its PrunedPaths -- each given the spec's
	// distance as its bestPathWeight; the anchor itself costs 1,000,000, so no
	// route runs back through where the pawn stands.
	void DefinePathsFor(UPawn* pawn, SortedPathList& list)
	{
		UNavigationPoint* anchor = list.Path[0];
		if (!anchor)
			return;
		anchor->cost() = 1000000;

		const Array<LevelReachSpec>& specs = pawn->XLevel()->ReachSpecs;
		int moveFlags = pawn->CalcMoveFlags();
		int radius = (int)pawn->CollisionRadius();
		int height = (int)pawn->CollisionHeight();
		bool pruned = false;
		for (int i = 0; i < 16;)
		{
			int index = pruned ? anchor->PrunedPaths()[i] : anchor->Paths()[i];
			if (index == -1)
			{
				if (pruned)
					return;
				pruned = true;
				i = 0;
				continue;
			}
			if (index >= 0 && (size_t)index < specs.size())
			{
				const LevelReachSpec& spec = specs[index];
				if (SpecOpen(pawn, spec, moveFlags, radius, height))
				{
					spec.endActor->bEndPoint() = true;
					spec.endActor->bestPathWeight() = spec.distance;
				}
			}
			i++;
		}
	}

	// The two lists (0x103da210): the navigation points within 800 units of
	// the pawn and of the goal, nearest first, in one pass over the level's
	// list that clears each node first when bClearPaths is set, as clearPaths
	// does -- so every search starts clean, end points included. A pawn
	// standing at its MoveTarget node is anchored there instead of gathering
	// its own list; the goal's list is left alone once the goal is a node.
	void GatherPathLists(UPawn* pawn, const vec3& goal, SortedPathList& pawnList, SortedPathList& goalList, bool clearPaths, bool& anchored, bool goalListDone)
	{
		UNavigationPoint* moveTarget = UObject::TryCast<UNavigationPoint>(pawn->MoveTarget());
		if (moveTarget && std::abs(moveTarget->Location().z - pawn->Location().z) < moveTarget->CollisionHeight() + pawn->CollisionHeight())
		{
			float dx = moveTarget->Location().x - pawn->Location().x;
			float dy = moveTarget->Location().y - pawn->Location().y;
			float limit = pawn->CollisionRadius() * pawn->CollisionRadius();
			if (pawn->bIsPlayer() && UObject::TryCast<UInventorySpot>(moveTarget))
				limit += limit;
			if (dx * dx + dy * dy < limit)
			{
				anchored = true;
				pawnList.Path[0] = moveTarget;
				pawnList.Dist[0] = 0;
				pawnList.NumPoints = 1;
			}
		}

		for (UNavigationPoint* node = pawn->Level()->NavigationPointList(); node; node = node->nextNavigationPoint())
		{
			if (clearPaths)
			{
				node->nextOrdered() = nullptr;
				node->prevOrdered() = nullptr;
				node->bEndPoint() = false;
				node->visitedWeight() = 10000000;
				if (node->bSpecialCost())
					node->cost() = CallEvent(node, "SpecialCost", { ExpressionValue::ObjectValue(pawn) }).ToInt();
				else
					node->cost() = node->ExtraCost();
			}
			if (!anchored)
			{
				int dist = SquaredDistance(pawn->Location(), node->Location());
				if (dist < 640000)
					pawnList.AddPath(node, dist);
			}
			if (!goalListDone)
			{
				int dist = SquaredDistance(goal, node->Location());
				if (dist < 640000)
					goalList.AddPath(node, dist);
			}
		}
	}

	// findEndPoint (0x103da610, Unreal Tournament's): the pawn's nodes are
	// dropped from the nearest until one is seen from its eye and
	// pointReachable. Standing on it -- within max(radius, 48) across and its
	// height up or down -- the pawn is anchored there; otherwise it is the end
	// point, its bestPathWeight its distance.
	bool FindEndPoint(UPawn* pawn, SortedPathList& list, bool& anchored)
	{
		vec3 eye = pawn->Location();
		eye.z += pawn->BaseEyeHeight();
		while (list.NumPoints > 0)
		{
			UNavigationPoint* node = list.Path[0];
			if (pawn->FastTrace(node->Location(), eye) && pawn->PointReachable(node->Location(), true))
				break;
			list.RemovePath(0);
		}
		if (list.NumPoints <= 0)
			return false;

		list.Dist[0] = (int)std::sqrt((double)list.Dist[0]);
		UNavigationPoint* node = list.Path[0];
		if (list.Dist[0] < std::max((int)pawn->CollisionRadius(), 48) && std::abs(node->Location().z - pawn->Location().z) < pawn->CollisionHeight())
		{
			anchored = true;
		}
		else
		{
			node->bEndPoint() = true;
			node->bestPathWeight() = list.Dist[0];
		}
		return true;
	}

	// 0x103da880: the goal within 800 units of the anchor, in its line of
	// sight, and pointReachable with the pawn moved onto the anchor -- where it
	// is left, the caller putting it back. Otherwise the pawn's list is cut to
	// its first node.
	bool ReachableFromAnchor(UPawn* pawn, SortedPathList& list, const vec3& goal)
	{
		UNavigationPoint* anchor = list.Path[0];
		vec3 standing = pawn->Location();
		if (SquaredDistance(goal, anchor->Location()) < 640000 && pawn->FastTrace(goal, anchor->Location()))
		{
			if (pawn->TestMoveTo(anchor->Location(), false))
			{
				if (pawn->PointReachable(goal, false))
					return true;
				pawn->TestMoveTo(standing, true);
			}
		}
		list.NumPoints = 1;
		return false;
	}

	// 0x103db0e0, after a search the pawn is not anchored for: its other nodes
	// are weighed against the end point, each its distance plus what the
	// search found it cost. One cheaper than the end point's own sum, within 120
	// units of the pawn's height, and either on the far side of the pawn from
	// the route's first node or cheaper by 15% (or 150, whichever cuts deeper)
	// is a candidate, and the cheapest the pawn's eye sees and pointReachable
	// finds takes the first node's place.
	void BetterStart(UPawn* pawn, SortedPathList& list, UActor*& bestPath)
	{
		int endTotal = list.Dist[0] + list.Path[0]->visitedWeight();
		SortedPathList candidates;
		for (int i = 1; i < list.NumPoints; i++)
		{
			int dist = (int)std::sqrt((double)list.Dist[i]);
			UNavigationPoint* node = list.Path[i];
			int total = dist + node->visitedWeight();
			if (total >= endTotal || std::abs(node->Location().z - pawn->Location().z) >= 120.0f)
				continue;
			bool farSide = dot(bestPath->Location() - pawn->Location(), node->Location() - pawn->Location()) < 0.0f;
			if (farSide || total < std::max((int)(endTotal * 0.85), endTotal - 150))
				candidates.AddPath(node, total);
		}

		vec3 eye = pawn->Location();
		eye.z += pawn->BaseEyeHeight();
		for (int i = 0; i < candidates.NumPoints; i++)
		{
			UNavigationPoint* node = candidates.Path[i];
			bool visible = pawn->FastTrace(node->Location(), eye);
			bool reachable = pawn->PointReachable(node->Location(), true);
			if (visible && reachable)
			{
				bestPath = node;
				return;
			}
		}
	}

	// ReverseRouteFor (0x103dd810): the previousPath chain turned around, so
	// that it runs from what was its last node.
	void ReverseRouteFor(UNavigationPoint* node)
	{
		if (!node)
			return;
		UNavigationPoint* reversed = nullptr;
		while (node->previousPath())
		{
			UNavigationPoint* next = node->previousPath();
			node->previousPath() = reversed;
			reversed = node;
			node = next;
		}
		node->previousPath() = reversed;
	}

	// TwoWallAdjust (UE1's AActor::TwoWallAdjust): a move stopped by a second
	// wall slides along the corner where the two meet, or along the second.
	void TwoWallAdjust(const vec3& desiredDir, vec3& delta, const vec3& hitNormal, const vec3& oldHitNormal, float hitTime)
	{
		if (dot(oldHitNormal, hitNormal) <= 0.0f)
		{
			vec3 newDir = cross(hitNormal, oldHitNormal);
			float len = length(newDir);
			newDir = len > 0.0f ? newDir / len : vec3(0.0f);
			delta = newDir * (dot(delta, newDir) * (1.0f - hitTime));
			if (dot(desiredDir, delta) < 0.0f)
				delta = -delta;
		}
		else
		{
			delta = (delta - hitNormal * dot(delta, hitNormal)) * (1.0f - hitTime);
			if (dot(delta, desiredDir) <= 0.0f)
				delta = vec3(0.0f);
		}
	}
}

// The original's ULevel::FarMoveActor as a test (engine-dll.md, teleporting an
// actor): fitted in near the spot unless noCheck, then moved there with its
// zone found again silently -- no encroachment, no touches, nothing unbased,
// not bJustTeleported. The search moves the pawn onto nodes this way to ask
// what it can reach from them, and puts it back the same way.
bool UPawn::TestMoveTo(const vec3& spot, bool noCheck)
{
	if (bStatic() || !bMovable())
		return false;

	vec3 location = spot;
	if (!noCheck && (bCollideWorld() || (bCollideWhenPlacing() && Level()->NetMode() != NM_Client)))
	{
		auto result = FindSpot(spot, CollisionRadius(), CollisionHeight(), false);
		if (!result.first)
			return false;
		location = result.second;
	}

	XLevel()->Collision.RemoveFromCollision(this);
	Location() = location;
	OldLocation() = location;
	XLevel()->Collision.AddToCollision(this);
	Region() = FindRegion();
	FootRegion() = FindRegion({ 0.0f, 0.0f, -CollisionHeight() });
	HeadRegion() = FindRegion({ 0.0f, 0.0f, EyeHeight() });
	return true;
}

// The original's APawn::jumpLanding (Engine.dll 0x103c2530; engine-dll.md, the
// search): where a falling pawn will come down. Its fall is stepped through
// in tenths of a second -- the zone's gravity, fluid friction and velocity --
// each step a test move that slides along a wall (a second wall through
// TwoWallAdjust) and stops on a floor, in water, outside the world, after 35
// steps or past 1,581 units a second, the last three putting it back where it
// started. The pawn is put back unless movePawn.
vec3 UPawn::JumpLanding(vec3 velocity, bool movePawn)
{
	const float timeStep = 0.1f;
	vec3 start = Location();

	auto moveBy = [&](const vec3& delta)
		{
			CollisionHit hit = TryMove(delta, true);
			Location() += delta * hit.Fraction;
			Region() = FindRegion();
			return hit;
		};

	bool landed = false;
	int steps = 0;
	while (!landed)
	{
		UZoneInfo* zone = Region().Zone;
		if (!zone)
			break;
		velocity = velocity * (1.0f - timeStep * zone->ZoneFluidFriction()) + zone->ZoneGravity() * timeStep;
		vec3 delta = (velocity + zone->ZoneVelocity()) * timeStep;

		CollisionHit hit = moveBy(delta);
		if (Region().Zone && Region().Zone->bWaterZone())
		{
			landed = true;
		}
		else if (hit.Fraction < 1.0f)
		{
			if (hit.Normal.z > 0.7f)
			{
				landed = true;
			}
			else
			{
				vec3 oldHitNormal = hit.Normal;
				vec3 slide = (delta - hit.Normal * dot(delta, hit.Normal)) * (1.0f - hit.Fraction);
				if (dot(delta, slide) >= 0.0f)
				{
					hit = moveBy(slide);
					if (hit.Fraction < 1.0f)
					{
						if (hit.Normal.z > 0.7f)
							landed = true;
						float len = length(delta);
						vec3 desiredDir = len > 0.0f ? delta / len : vec3(0.0f);
						TwoWallAdjust(desiredDir, slide, hit.Normal, oldHitNormal, hit.Fraction);
						hit = moveBy(slide);
						if (hit.Normal.z > 0.7f)
							landed = true;
					}
				}
			}
		}

		steps++;
		if (Region().ZoneNumber == 0 || steps > 35 || dot(velocity, velocity) > 2500000.0f)
		{
			TestMoveTo(start, true);
			landed = true;
		}
	}

	vec3 landing = Location();
	if (!movePawn)
		TestMoveTo(start, true);
	return landing;
}

// HandleSpecial (Engine.dll 0x103c5de0): the route's first node answers its
// SpecialHandling. The same node: the route stands. None: no route. Another
// actor: for a pawn that can do specials it becomes SpecialGoal, and the route
// is that actor where it can be walked to -- asked again when it too has a
// SpecialHandling, its own answer taken where reachable -- or else the way to
// it, unless that is the first node; anything else, no route.
void UPawn::HandleSpecial(UActor*& bestPath)
{
	UActor* first = bestPath;
	UActor* special = UObject::Cast<UActor>(CallEvent(first, EventName::SpecialHandling, { ExpressionValue::ObjectValue(this) }).ToObject());
	bestPath = special;
	if (!special || special == first)
		return;

	if (bCanDoSpecial())
	{
		SpecialGoal() = special;
		if (ActorReachable(special))
		{
			if (!special->IsEventEnabled(EventName::SpecialHandling))
				return;
			UActor* again = UObject::Cast<UActor>(CallEvent(special, EventName::SpecialHandling, { ExpressionValue::ObjectValue(this) }).ToObject());
			if (again)
			{
				if (again == special)
					return;
				if (ActorReachable(again))
				{
					bestPath = again;
					return;
				}
			}
		}
		else
		{
			UActor* route = DeusExFindPath(special, special->Location(), false, true);
			if (route && route != first)
			{
				SpecialGoal() = special;
				bestPath = route;
				return;
			}
		}
	}
	bestPath = nullptr;
}

// What the script's FindPathToward and FindPathTo do around the search (their
// execs, Engine.dll 0x103bc9c0 and 0x103bc800): bShootSpecial and
// SpecialPause cleared, a found node with a SpecialHandling handed to
// HandleSpecial, and SpecialGoal dropped when the route is it.
UActor* UPawn::DeusExFindPathFromScript(UActor* goalActor, const vec3& goalPoint, bool singlePath, bool clearPaths)
{
	UActor* route = DeusExFindPath(goalActor, goalPoint, singlePath, clearPaths);
	bShootSpecial() = false;
	SpecialPause() = 0.0f;
	if (route && route->IsEventEnabled(EventName::SpecialHandling))
		HandleSpecial(route);
	if (route == SpecialGoal())
		SpecialGoal() = nullptr;
	return route;
}

// The search itself: findPathToward for an actor goal, findPathTo for a point
// (goalActor none). The route's first point comes back, or none. The pawn is
// moved onto nodes to ask what it can reach from them, and put back where it
// stood on every way out.
UActor* UPawn::DeusExFindPath(UActor* goalActor, vec3 goalPoint, bool singlePath, bool clearPaths)
{
	if (!Level()->NavigationPointList() || XLevel()->ReachSpecs.empty())
		return nullptr;

	// A falling pawn is searched toward by where it will come down, as a point.
	if (goalActor && Physics() != PHYS_Flying)
	{
		UPawn* goalPawn = UObject::TryCast<UPawn>(goalActor);
		if (goalPawn && goalPawn->Physics() == PHYS_Falling)
			return DeusExFindPath(nullptr, goalPawn->JumpLanding(goalPawn->Velocity(), false), singlePath, clearPaths);
	}

	UNavigationPoint* goalNav = UObject::TryCast<UNavigationPoint>(goalActor);
	vec3 goal = goalActor ? goalActor->Location() : goalPoint;
	vec3 standing = Location();
	auto putBack = [&]() { TestMoveTo(standing, true); };

	SortedPathList pawnList, goalList;
	bool anchored = false;
	bool goalNode = false;
	if (goalNav)
	{
		goalList.Path[0] = goalNav;
		goalList.Dist[0] = 0;
		goalList.NumPoints = 1;
		goalNode = true;
	}
	GatherPathLists(this, goal, pawnList, goalList, clearPaths, anchored, goalNode);
	if (pawnList.NumPoints == 0 || goalList.NumPoints == 0)
		return nullptr;

	if (!anchored && !FindEndPoint(this, pawnList, anchored))
	{
		putBack();
		return nullptr;
	}

	// Anchored, the goal may be a step away: a navigation point among the
	// anchor's own open specs is the route; another goal reachable from the
	// anchor makes the anchor the route. Otherwise the anchor's neighbours
	// are the end points.
	if (anchored)
	{
		if ((goalNav && CanMoveTo(this, pawnList.Path[0], goalNav)) || ReachableFromAnchor(this, pawnList, goal))
		{
			UActor* route = goalNav ? (UActor*)goalNav : (UActor*)pawnList.Path[0];
			putBack();
			return route;
		}
		DefinePathsFor(this, pawnList);
	}

	// Where the search starts: a navigation-point goal itself; else the
	// nearest of the goal's nodes the goal is in sight of and reachable from,
	// the pawn moved onto each to ask.
	if (!goalNode)
	{
		for (int i = 0; i < goalList.NumPoints; i++)
		{
			UNavigationPoint* node = goalList.Path[i];
			if (!FastTrace(goal, node->Location()))
				continue;
			if (TestMoveTo(node->Location(), false) && (goalActor ? DeusExActorReachable(goalActor, true) : PointReachable(goal, true)))
			{
				goalNode = true;
				goalList.Path[0] = node;
				goalList.Dist[0] = (int)std::sqrt((double)goalList.Dist[i]);
				break;
			}
		}

		if (!goalNode)
		{
			// A point has no other way. A pawn hunting takes the goal's nearest
			// node anyway; any other tries the second way, below.
			if (!goalActor)
			{
				putBack();
				return nullptr;
			}
			if (!bHunting())
			{
				// The lists again -- the pawn's around wherever the tries above
				// last moved it -- and the search from the pawn's own first node.
				GatherPathLists(this, goal, pawnList, goalList, clearPaths, anchored, false);
				DefinePathsFor(this, pawnList);
				bool endAnchored = false;
				if (!FindEndPoint(this, pawnList, endAnchored))
				{
					putBack();
					return nullptr;
				}
				UNavigationPoint* first = pawnList.Path[0];
				first->visitedWeight() = pawnList.Dist[0];
				UNavigationPoint* endNode = nullptr;
				if (!BreadthPathFrom(first, endNode, singlePath, CalcMoveFlags()))
				{
					putBack();
					return nullptr;
				}
				putBack();

				// Taken only if its end lies no farther from the goal than the
				// pawn does, then turned to run from the pawn's node; the next
				// node starts it instead where the pawn can walk there.
				vec3 toEnd = goal - endNode->Location();
				vec3 toPawn = goal - Location();
				if (dot(toEnd, toEnd) > dot(toPawn, toPawn))
					return nullptr;
				UNavigationPoint* last = endNode;
				while (last->previousPath())
					last = last->previousPath();
				ReverseRouteFor(endNode);
				UNavigationPoint* next = last->previousPath();
				if (next)
				{
					if ((anchored && pawnList.Path[0] == last) || std::abs(next->Location().z - Location().z) < 120.0f)
					{
						vec3 eye = Location();
						eye.z += BaseEyeHeight();
						bool visible = FastTrace(next->Location(), eye);
						bool reachable = PointReachable(next->Location(), true);
						UActor* route = (visible && reachable) ? (UActor*)next : (UActor*)last;
						putBack();
						return route;
					}
					return last;
				}
				if (!anchored || pawnList.Path[0] != last)
					return last;
				return nullptr;
			}
			goalNode = true;
		}
	}

	UNavigationPoint* start = goalList.Path[0];
	int moveFlags = CalcMoveFlags();
	start->visitedWeight() = goalList.Dist[0];
	UNavigationPoint* endNode = nullptr;
	if (!BreadthPathFrom(start, endNode, singlePath, moveFlags))
	{
		putBack();
		return nullptr;
	}

	UActor* route = endNode;
	putBack();
	if (!anchored && !singlePath)
		BetterStart(this, pawnList, route);
	return route;
}

// ---------------------------------------------------------------------------
// Other games' path search, the fork's own around the original's breadthPathFrom.

UObject* UPawn::FindRandomDest()
{
	// Find initial navpoints reachable from our location
	int maxActorReachableCalls = 8; // upper bound for how expensive this can get
	vec3 eyePos = Location();
	eyePos.z += BaseEyeHeight();
	std::vector<UNavigationPoint*> reachablePoints;
	for (UNavigationPoint* navPoint = Level()->NavigationPointList(); navPoint && reachablePoints.size() < maxActorReachableCalls; navPoint = navPoint->nextNavigationPoint())
	{
		if (navPoint->bPlayerOnly() && !bIsPlayer())
			continue; // Skip nav nodes only for the player if we aren't one

		float maxDist = 1000.0;
		vec3 d = navPoint->Location() - Location();
		if (dot(d, d) > maxDist * maxDist)
			continue; // Ignore things too far away

		if (!ActorReachable(navPoint))
			continue;

		navPoint->bEndPoint() = true;
		reachablePoints.push_back(navPoint);
	}

	if (reachablePoints.empty())
		return nullptr;

	// Add all navpoints reachable via reachspecs from what we can already reached
	const Array<LevelReachSpec>& reachSpecs = XLevel()->ReachSpecs;
	int radius = (int)CollisionRadius();
	int height = (int)CollisionHeight();
	for (size_t i = 0; i < reachablePoints.size(); i++)
	{
		UNavigationPoint* navPoint = reachablePoints[i];

		for (int specIndex : navPoint->Paths())
		{
			if (specIndex < 0 || (size_t)specIndex >= reachSpecs.size())
				break;
			const LevelReachSpec& reachSpec = reachSpecs[specIndex];

			if (reachSpec.endActor->bEndPoint())
				continue; // Already processed

			if (reachSpec.collisionRadius < radius || reachSpec.collisionHeight < height || reachSpec.bPruned)
				continue; // Skip nav node links that we can't pass through

			if (reachSpec.endActor->bPlayerOnly() && !bIsPlayer())
				continue; // Skip nav nodes only for the player if we aren't one

			// To do: check reachFlags

			reachSpec.endActor->bEndPoint() = true;
			reachablePoints.push_back(reachSpec.endActor);
		}
	}

	// Pick a random point from our candidates
	float randomValue = rand() / (float)RAND_MAX;
	int index = (int)std::round(randomValue * (float)(reachablePoints.size() - 1));
	return reachablePoints[index];
}

UObject* UPawn::FindPathTo(const vec3& aPoint, bool bSinglePath)
{
	return FindPathToward(FindClosestNavPoint(aPoint), bSinglePath);
}

bool UPawn::MarkReachableNavEndPoints()
{
	int maxActorReachableCalls = 8; // upper bound for how expensive this can get
	int endPointsFound = 0;
	for (UNavigationPoint* navPoint = Level()->NavigationPointList(); navPoint; navPoint = navPoint->nextNavigationPoint())
	{
		navPoint->bEndPoint() = false;

		if (endPointsFound < maxActorReachableCalls)
		{
			if (navPoint->bPlayerOnly() && !bIsPlayer())
				continue; // Skip nav nodes only for the player if we aren't one

			float maxDist = 1000.0;
			vec3 d = navPoint->Location() - Location();
			if (dot(d, d) > maxDist * maxDist)
				continue; // Ignore things too far away

			if (!ActorReachable(navPoint))
				continue;

			navPoint->bEndPoint() = true;
			endPointsFound++;
		}
	}

	return endPointsFound > 0;
}

UObject* UPawn::FindPathToward(UObject* anActor, bool singlePath)
{
	// A target that can be walked to straight is the route itself -- no
	// search, no node detours (Deus Ex's own search is DeusExFindPath).
	if (auto aNavPoint = UObject::TryCast<UNavigationPoint>(anActor))
	{
		if (ActorReachable(aNavPoint, true))
			return SetRouteCache({ aNavPoint });
		if (!MarkReachableNavEndPoints())
			return SetRouteCache({});
		// The search spends the start node's visitedWeight as what the route has
		// cost so far: nothing yet at the goal's own node.
		aNavPoint->visitedWeight() = 0;
		if (!IsInPathSpecialHandling)
			return PathSpecialHandling(FindPathToEndPoint(aNavPoint, 0).first);
		return SetRouteCache({});
	}
	else if (auto actor = UObject::TryCast<UActor>(anActor))
	{
		return FindPathToward(FindClosestNavPoint(actor->Location()), singlePath);
	}
	else
	{
		return SetRouteCache({});
	}
}

UNavigationPoint* UPawn::FindClosestNavPoint(vec3 location)
{
	// Order nav points by distance
	std::vector<std::pair<UNavigationPoint*, float>> navPoints;
	for (UNavigationPoint* navPoint = Level()->NavigationPointList(); navPoint; navPoint = navPoint->nextNavigationPoint())
	{
		if (navPoint->bPlayerOnly() && !bIsPlayer())
			continue; // Skip nav nodes only for the player if we aren't one

		float maxDist = 500;
		vec3 d = navPoint->Location() - location;
		float distsqr = dot(d, d);
		if (distsqr > maxDist * maxDist)
			continue; // Ignore things too far away

		navPoints.push_back({ navPoint, distsqr });
	}

	std::sort(navPoints.begin(), navPoints.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

	size_t maxTraces = 4; // upper bound for how expensive this can get
	navPoints.resize(std::min(navPoints.size(), maxTraces));

	// Find the first reachable nav point
	for (auto& p : navPoints)
	{
		vec3 eyePos = p.first->Location();
		eyePos.z += BaseEyeHeight();
		if (FastTrace(location, eyePos))
			return p.first;
	}
	return nullptr;
}

UObject* UPawn::FindBestInventoryPath(bool predictRespawns, float& outBestWeight)
{
	if (!MarkReachableNavEndPoints())
	{
		outBestWeight = 0.0f;
		return SetRouteCache({});
	}

	float bestWeight = 0.0f;
	UInventorySpot* bestSpot = nullptr;
	Array<UNavigationPoint*> bestPath;

	for (UNavigationPoint* navPoint = Level()->NavigationPointList(); navPoint; navPoint = navPoint->nextNavigationPoint())
	{
		auto invSpot = UObject::TryCast<UInventorySpot>(navPoint);
		if (!invSpot)
			continue;
		auto inv = invSpot->markedItem();
		if (!inv)
			continue;

		if (inv->GetStateName() != "PickUp")
			continue;

		float desire = CallEvent(inv, "BotDesireability", { ExpressionValue::ObjectValue(this) }).ToFloat();
		if (desire > 0.0f)
		{
			// Each search spends the nodes' own fields, so each starts them
			// afresh, the end points kept, from nothing spent at the spot.
			for (UNavigationPoint* cur = Level()->NavigationPointList(); cur; cur = cur->nextNavigationPoint())
			{
				cur->visitedWeight() = 10000000;
				cur->nextOrdered() = nullptr;
				cur->prevOrdered() = nullptr;
			}
			invSpot->visitedWeight() = 0;

			auto [path, pathDist] = FindPathToEndPoint(invSpot, 0);

			// To do: how to take path costs into account?
			//int cost = 0;
			//for (auto nav : path)
			//	cost += nav->cost();

			float distance = std::max((float)pathDist, 1.0f);
			float weight = desire / distance;

			if (!bestSpot || weight > bestWeight)
			{
				bestSpot = invSpot;
				bestWeight = weight;
				bestPath = std::move(path);
			}
		}
	}

	if (bestSpot)
	{
		outBestWeight = bestWeight;
		return PathSpecialHandling(bestPath);
	}
	else
	{
		outBestWeight = 0.0f;
		return SetRouteCache({});
	}
}
