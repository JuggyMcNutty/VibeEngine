
#include "Precomp.h"
#include "UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Actors/NavigationPoint/UInventorySpot.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "VM/ScriptCall.h"
#include "Utils/Logger.h"
#include "Engine.h"

UNavigationPoint* UPawn::SetRouteCache(const Array<UNavigationPoint*>& points)
{
	if (engine->LaunchInfo.ue1Version > 219)
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

std::pair<Array<UNavigationPoint*>, int32_t> UPawn::FindPathToEndPoint(UNavigationPoint* start, int maxNodes)
{
	// The original's search (APawn::breadthPathFrom, Engine.dll 0x103dcd60):
	// a best-first walk over the level's reach specs. What a node costs to
	// reach is the spec's distance plus the node's own penalty (cost, from the
	// SpecialCost event or ExtraCost, which ClearPaths sets) plus what was
	// spent to reach the node expanded so far, and the nodes not yet expanded
	// are kept in an open list sorted by that cost. The fork's was a Dijkstra
	// by spec distance alone, which chose different routes where the two
	// disagreed (UNATCOTroop1 pressing against geometry on a route the original
	// never took, MoveConsole).
	//
	// The open list and the route live in the nodes' own fields, as they do in
	// the original: visitedWeight what the node cost to reach, nextOrdered and
	// prevOrdered the open list, previousPath the way back.
	//
	// maxNodes is the script's node cap, which none of Deus Ex's eleven
	// FindPathToward calls passes: given, the original gives the search up after
	// four nodes; left at 0, after 1000, and it says so in the log.
	if ((start->bPlayerOnly() && !bIsPlayer()))
		return { {}, 0 };

	const Array<LevelReachSpec>& reachSpecs = XLevel()->ReachSpecs;

	int radius = (int)CollisionRadius();
	int height = (int)CollisionHeight();

	int iterations = 0;
	int pathCount = 0;
	int listIndex = 1;
	UNavigationPoint* frontNode = start;
	UNavigationPoint* node = start;

	while (node)
	{
		// An end point is where the pawn can walk to from where it stands, so
		// the search is over: the route reads back from it, each node's
		// previousPath the one before it.
		if (node->bEndPoint())
		{
			start->previousPath() = nullptr;

			Array<UNavigationPoint*> path;
			for (UNavigationPoint* at = node; at; at = at->previousPath())
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
			path.push_back(start);

			return { path, node->visitedWeight() };
		}

		// A node only the player may use is not expanded for a pawn that is not
		// the player; the start node is expanded whatever it is.
		if (!(node->bPlayerOnly() && !bIsPlayer() && node != start))
		{
			for (int specIndex : node->upstreamPaths())
			{
				if (specIndex < 0 || (size_t)specIndex >= reachSpecs.size())
					break;
				const LevelReachSpec& reachSpec = reachSpecs[specIndex];

				// Note: startActor instead of endActor because upstreamPaths is the reverse travel direction
				UNavigationPoint* endActor = reachSpec.startActor;
				if (!endActor)
					continue;

				if (reachSpec.collisionRadius < radius || reachSpec.collisionHeight < height)
					continue; // Skip nav node links that we can't pass through

				// To do: the original also skips a link whose reachFlags the pawn's
				// own move flags do not cover (calcMoveFlags, 0x10326d10)

				// What has the node cost to reach by way of this link? An end
				// point counts its bestPathWeight too, which is the reach spec
				// distance that reached it.
				int32_t newCost = reachSpec.distance + endActor->cost() + node->visitedWeight()
				                + endActor->bestPathWeight() * (endActor->bEndPoint() ? 1 : 0);

				// Only a cheaper way to the node is worth taking.
				if (endActor->visitedWeight() <= newCost)
					continue;

				// Take the node out of the open list if it is in it, then put it
				// back where its cost belongs, keeping the list sorted.
				UNavigationPoint* prev = endActor->prevOrdered();
				if (prev != nullptr)
				{
					prev->nextOrdered() = endActor->nextOrdered();
					if (endActor->nextOrdered())
						endActor->nextOrdered()->prevOrdered() = prev;

					if (frontNode == endActor)
					{
						if (prev->visitedWeight() > newCost)
							frontNode = prev;
					}
					else if (endActor->visitedWeight() > frontNode->visitedWeight() && newCost < frontNode->visitedWeight())
					{
						listIndex--;
					}
				}
				else
				{
					// Not in the list: it sorts before the front node or after it,
					// and the front of the list will not have to walk as far.
					if (newCost <= frontNode->visitedWeight())
						listIndex--;
					else
						listIndex++;
				}

				endActor->previousPath() = node;
				endActor->visitedWeight() = newCost;

				// Walk the list from the cheaper of the front node and the node
				// just expanded for the place the cost belongs at, giving up if
				// the list has broken into a loop.
				UNavigationPoint* at = frontNode->visitedWeight() < newCost ? node : frontNode;
				for (int walked = 0; at->nextOrdered() && at->nextOrdered()->visitedWeight() < newCost;)
				{
					if (++walked > 500)
					{
						LogMessage("Breadth path list overflow from " + start->GetPathName());
						return { {}, 0 };
					}
					at = at->nextOrdered();
				}

				if (at->nextOrdered() != endActor)
				{
					UNavigationPoint* after = at->nextOrdered();
					if (after)
						after->prevOrdered() = endActor;
					endActor->nextOrdered() = after;
					at->nextOrdered() = endActor;
					endActor->prevOrdered() = at;
				}
			}
		}

		// The front of the open list moves on as far as the list has grown: half
		// as far again as the index it has reached.
		while (pathCount < (int)(listIndex * 0.5f))
		{
			if (!frontNode->nextOrdered())
				break;
			frontNode = frontNode->nextOrdered();
			pathCount++;
		}

		iterations++;
		if (maxNodes != 0 && iterations > 4)
			return { {}, 0 };
		if (iterations > 1000)
		{
			LogMessage("1000 Navigation nodes searched from " + start->GetPathName());
			return { {}, 0 };
		}

		node = node->nextNavigationPoint();
	}

	return { {}, 0 };
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
		// Engine.dll 0x103da060).
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
	// The original first asks whether the target itself can be walked to
	// straight (APawn::findPathToward, Engine.dll 0x103db3f0: CanMoveTo for
	// a navigation point, pointReachable for a plain spot, and a pass over
	// the candidate nodes for one already reachable) and returns the target
	// as the route when so -- no search, no node detours. The fork always
	// searched, so a bot whose next patrol point was in plain sight walked
	// off through path nodes to reach it.
	if (auto aNavPoint = UObject::TryCast<UNavigationPoint>(anActor))
	{
		if (ActorReachable(aNavPoint, true))
			return SetRouteCache({ aNavPoint });
		if (!MarkReachableNavEndPoints())
			return SetRouteCache({});
		// The search spends the start node's visitedWeight as what the route has
		// cost so far, which the original seeds with the node's own weight from
		// the goal -- zero for the node the pawn stands on (findPathToward,
		// Engine.dll 0x103db875).
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
