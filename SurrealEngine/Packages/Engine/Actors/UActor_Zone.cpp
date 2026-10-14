
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

	// Destroying what enters a destructive zone, and inventory left in a
	// no-inventory zone, are Deus Ex's scripts' own (ZoneInfo.ActorEntered
	// gives stray inventory 1.5 seconds; decorations and fragments check
	// bDestructive): its engine does neither.
}
