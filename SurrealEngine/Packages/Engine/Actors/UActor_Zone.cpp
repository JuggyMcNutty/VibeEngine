
#include "Precomp.h"
#include "UActor.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "VM/ScriptCall.h"
#include "Engine.h"

PointRegion UActor::FindRegion(const vec3& offset)
{
	return XLevel()->Model->FindRegion(Location() + offset, Level());
}

void UActor::InitActorZone()
{
	Region() = FindRegion();
	if (Region().Zone->bWaterZone() && !this->IsA("Projectile"))
	{
		SetPhysics(PHYS_Swimming);
		SetBase(nullptr, true);
	}
}

void UActor::UpdateActorZone()
{
	if (bDeleteMe())
		return;

	PointRegion oldregion = Region();
	PointRegion newregion = FindRegion();

	if (oldregion.Zone != newregion.Zone)
	{
		// ZoneChange still sees the zone being left in Region: scripts
		// compare the two (a decoration splashes only coming out of dry).
		if (oldregion.Zone)
			CallEvent(oldregion.Zone, EventName::ActorLeaving, { ExpressionValue::ObjectValue(this) });
		if (newregion.Zone)
			CallEvent(this, EventName::ZoneChange, { ExpressionValue::ObjectValue(newregion.Zone) });
		Region() = newregion;
		if (newregion.Zone)
			CallEvent(newregion.Zone, EventName::ActorEntered, { ExpressionValue::ObjectValue(this) });
	}
	else
	{
		Region() = newregion;
	}

	// Deus Ex's scripts do these themselves (ZoneInfo.ActorEntered gives
	// stray inventory 1.5 seconds; decorations and fragments check
	// bDestructive), and its engine does neither.
	if (!engine->LaunchInfo.IsDeusEx() && Region().Zone)
	{
		if (Region().Zone->bDestructive() && IsA("Carcass"))
		{
			// If the actor is a Carcass and the zone is marked as bDestructive, destroy it.
			Destroy();
		}
		else if (engine->LaunchInfo.ue1Version > 219 && Owner() == nullptr && Region().Zone->bNoInventory() && IsA("Inventory"))
		{
			// If the new zone is bNoInventory, destroy Inventory that's not owned by anyone (i.e. in pickup state).
			Destroy();
		}
	}
}
