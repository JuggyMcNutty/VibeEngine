
#include "Precomp.h"
#include "UPawn.h"
#include "Engine.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "VM/ScriptCall.h"

void UPawn::InitActorZone()
{
	UActor::InitActorZone();

	FootRegion() = FindRegion({ 0.0f, 0.0f, -CollisionHeight() });
	HeadRegion() = FindRegion({ 0.0f, 0.0f, EyeHeight() });

	if (PlayerReplicationInfo())
		PlayerReplicationInfo()->PlayerZone() = Region().Zone;
}

void UPawn::UpdateActorZone()
{
	if (bDeleteMe())
		return;

	UActor::UpdateActorZone();

	// Deus Ex's Pawn.FootZoneChange and HeadZoneChange set PainTime
	// themselves; its engine does not.

	PointRegion oldfootregion = FootRegion();
	PointRegion newfootregion = FindRegion({ 0.0f, 0.0f, -CollisionHeight() });
	if (oldfootregion.Zone && oldfootregion.Zone != newfootregion.Zone)
	{
		CallEvent(this, EventName::FootZoneChange, { ExpressionValue::ObjectValue(newfootregion.Zone) });
	}

	FootRegion() = newfootregion;

	PointRegion oldheadregion = HeadRegion();
	PointRegion newheadregion = FindRegion({ 0.0f, 0.0f, EyeHeight() });
	if (oldheadregion.Zone && oldheadregion.Zone != newheadregion.Zone)
	{
		CallEvent(this, EventName::HeadZoneChange, { ExpressionValue::ObjectValue(newheadregion.Zone) });
	}

	HeadRegion() = newheadregion;

	if (PlayerReplicationInfo() && Level()->NetMode() != NM_Client)
		PlayerReplicationInfo()->PlayerZone() = Region().Zone;
}
