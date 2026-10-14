
#include "Precomp.h"
#include "UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Actors/NavigationPoint/UInventorySpot.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Utils/Logger.h"
#include "Engine.h"

bool UPawn::ActorReachable(UActor* anActor, bool checkNavpoint)
{
	// Deus Ex's is the original's actorReachable (UPawn_ReachDeusEx.cpp).
	return DeusExActorReachable(anActor, false);
}

bool UPawn::PointReachable(vec3 aPoint, bool knowVisible)
{
	// Deus Ex's is the original's pointReachable (UPawn_ReachDeusEx.cpp).
	return DeusExPointReachable(aPoint, knowVisible);
}
