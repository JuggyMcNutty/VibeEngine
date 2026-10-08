
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

void UActor::TickSwimming(float elapsed)
{
	// Deus Ex's pawns swim as the original's do.
	if (engine->LaunchInfo.IsDeusEx())
	{
		if (UPawn* deusExPawn = UObject::TryCast<UPawn>(this))
			deusExPawn->DeusExPhysSwimming(elapsed);
		return;
	}

	// Only pawns can swim!
	UPawn* pawn = PreparePawnMovementTick();
	if (!pawn)
		return;

	// Update the actor velocity based on the acceleration and zone

	UZoneInfo* zone = Region().Zone;
	// UDecoration* decor = UObject::TryCast<UDecoration>(this);
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(this);

	float maxSpeed = player ? player->WaterSpeed() : pawn->WaterSpeed() * pawn->DesiredSpeed();
	ApplyMovementAcceleration(elapsed, pawn->AccelRate() * 0.3f, zone->ZoneFluidFriction(), maxSpeed);

	//float gravityDirection = zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f;

	float timeLeft = elapsed;
	vec3 vel = Velocity() + zone->ZoneVelocity() * elapsed * 25.0f;

	// The mass floored at 1, as the original's falling in water has it: a
	// massless pawn (Deus Ex's GeneratorScout) would make this 0/0.
	vel.z += zone->ZoneGravity().z * (1.0f - Buoyancy() / std::max(Mass(), 1.0f)) * elapsed;

	//required for "stepping out of water"
	float gravityDirection = zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f;
	vec3 stepUpDelta(0.0f, 0.0f, -gravityDirection * pawn->MaxStepHeight());

	//same as tick walking, having any velosity at all should probably enable this branch
	if (length(vel))
	{
		for (int iteration = 0; timeLeft > 0.0f && iteration < 5; iteration++)
		{
			if (ShouldAbortMovementTick(PHYS_Swimming))
				break;

			vec3 moveDelta = vel * timeLeft;

			CollisionHit hit = TryMove(moveDelta);
			timeLeft -= timeLeft * hit.Fraction;
			moveDelta = vel * timeLeft;

			if (hit.Fraction < 1.0f)
			{
				if (player && UObject::IsType<UDecoration>(hit.Actor) && UObject::Cast<UDecoration>(hit.Actor)->bPushable() && dot(hit.Normal, moveDelta) < -0.9f)
				{
					// We hit a pushable decoration that is facing our movement direction

					//same question as with tick walk -> why set teleport flag on decoration?
					bJustTeleported() = true;
					Velocity() = Velocity() * Mass() / (Mass() + hit.Actor->Mass());
					FireHitWall(hit);
					timeLeft = 0.0f;
				}
				else
				{
					// We hit a wall
					FireHitWall(hit);

					TryMove(stepUpDelta);
					if (!Region().Zone->bWaterZone())
					{
						TryMove(moveDelta);
						bJustTeleported() = true;
						if (Physics() == PHYS_Swimming)
							SetPhysics(PHYS_Falling);
						break;
					}
					TryMove(-stepUpDelta);

					//removed the second scaling, that caused the "exiting the water is difficult" bug
					//it appears to not fix it completely, but it helps
					vec3 alignedDelta = moveDelta - hit.Normal * dot(moveDelta, hit.Normal);

					if (dot(moveDelta, alignedDelta) >= 0.0f) // Don't end up going backwards
					{
						hit = TryMove(alignedDelta);
						timeLeft -= timeLeft * hit.Fraction;
						if (hit.Fraction < 1.0f)
						{
							FireHitWall(hit);
						}
					}
					else
					{
						timeLeft = 0.0f;
					}
				}
			}
		}
	}

	RecomputeVelocityFromDisplacement(elapsed);

	if (!Region().Zone->bWaterZone())
	{
		if (Physics() == PHYS_Swimming)
			SetPhysics(PHYS_Falling);
	}
}

// Deus Ex's swimming, the original's (Engine.dll; dx-reverse-info/
// engine-dll.md, moving: into the water, swimming). The original swims a
// whole tick in one move, as the fork's sub-step of the tick is swum here. A
// player on a ladder climbs it instead (DeusExLadder).

namespace
{
	vec3 SafeNormal(const vec3& v)
	{
		float len = length(v);
		return len > 0.0f ? v / len : vec3(0.0f);
	}

	bool InWater(UZoneInfo* zone)
	{
		return zone && zone->bWaterZone();
	}
}

// APawn::physSwimming (0x103d3cd0).
void UPawn::DeusExPhysSwimming(float deltaTime)
{
	if (DeusExLadder(deltaTime))
		return;
	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;

	// Its head out of the water, a pawn rising over 100 slows.
	if (!InWater(HeadRegion().Zone) && Velocity().z > 100.0f)
		Velocity().z *= 1.0f - deltaTime;

	OldLocation() = Location();
	bJustTeleported() = false;
	DeusExCalcVelocity(SafeNormal(Acceleration()), deltaTime, WaterSpeed(), zone->ZoneFluidFriction(), true, false, true);
	float velZ = Velocity().z;

	// The zone's velocity for a player, and for another pawn in a zone moving
	// over 300; the move, by Swim, gives back the part of the tick it spent
	// out of the water.
	vec3 zoneVel(0.0f);
	if (UObject::TryCast<UPlayerPawn>(this) || dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 90000.0f)
		zoneVel = zone->ZoneVelocity() * 25.0f * deltaTime;
	vec3 adjusted = (Velocity() + zoneVel) * deltaTime;
	CollisionHit hit;
	float remaining = deltaTime * DeusExSwim(adjusted, hit);

	if (hit.Fraction < 1.0f)
	{
		vec3 gravDir(0.0f, 0.0f, zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f);
		vec3 desiredDir = SafeNormal(adjusted);
		float upDown = dot(gravDir, SafeNormal(Velocity()));
		if (std::abs(hit.Normal.z) < 0.2f && upDown < 0.5f && upDown > -0.2f)
		{
			// A wall met swimming level is stepped up, the step's rise kept
			// out of the velocity.
			float stepZ = Location().z;
			DeusExStepUp(gravDir, desiredDir, adjusted * (1.0f - hit.Fraction), hit);
			OldLocation().z = Location().z + (OldLocation().z - stepZ);
		}
		else
		{
			// Any other hit slides along it, a second wall through
			// TwoWallAdjust; each slide's Swim makes the time left over.
			ProcessHitWall(hit.Normal, hit.Actor);
			vec3 oldHitNormal = hit.Normal;
			vec3 delta = (adjusted - hit.Normal * dot(adjusted, hit.Normal)) * (1.0f - hit.Fraction);
			if (dot(delta, adjusted) >= 0.0f)
			{
				float given = DeusExSwim(delta, hit);
				remaining = remaining * (1.0f - hit.Fraction) * given;
				if (hit.Fraction < 1.0f)
				{
					ProcessHitWall(hit.Normal, hit.Actor);
					TwoWallAdjust(desiredDir, delta, hit.Normal, oldHitNormal, hit.Fraction);
					given = DeusExSwim(delta, hit);
					remaining = remaining * (1.0f - hit.Fraction) * given;
				}
			}
		}
	}

	// The velocity is the distance over the time the move took, but for a Z
	// velocity something changed during it (Deus Ex's player stops when its
	// head goes under).
	if (!bJustTeleported() && remaining < deltaTime)
	{
		bool changedZ = velZ != Velocity().z;
		velZ = Velocity().z;
		Velocity() = (Location() - OldLocation()) / (deltaTime - remaining);
		if (changedZ)
			Velocity().z = velZ;
	}

	// Out of the water it falls, and one swimming up out of it hops.
	if (!InWater(Region().Zone))
	{
		if (Physics() == PHYS_Swimming)
			SetPhysics(PHYS_Falling, nullptr);
		if (Velocity().z < 160.0f && Velocity().z > 0.0f)
			Velocity().z = length(Velocity().xy()) * 0.4f + 40.0f;
	}

	if (remaining > 0.01f)
	{
		if (Physics() == PHYS_Falling)
			TickFalling(remaining);
		else if (Physics() == PHYS_Flying)
			TickFlying(remaining);
	}
}

// APawn::startSwimming (0x103d2b70): a pawn whose move -- falling or walking
// -- took it into water, which its script made swim (ZoneChange), goes back
// to the water line between where the step began and where it is, the
// step's time for the way back given to the time left. Its velocity becomes
// the move's own doubled less the old (at most 4,000); one sinking slower
// than 160 sinks at 80 + 0.7 of its speed across, then it swims what is left.
void UPawn::DeusExStartSwimming(const vec3& oldVelocity, float timeTick, float remaining)
{
	vec3 end = Location();
	FindWaterLine(OldLocation(), end);
	float waterTime = 0.0f;
	if (end != Location())
	{
		waterTime = timeTick * length(end - Location()) / length(Location() - OldLocation());
		remaining += waterTime;
		TryMoveHeldOff(end - Location());
	}
	// The move's time is all given back only for a pawn moved after its move
	// (the original would divide by 0); its velocity is kept then.
	if (!bBounce() && !bJustTeleported() && timeTick - waterTime > 0.0f)
	{
		vec3 velocity = (Location() - OldLocation()) / (timeTick - waterTime) * 2.0f - oldVelocity;
		if (dot(velocity, velocity) > 16000000.0f)
			velocity = SafeNormal(velocity) * 4000.0f;
		Velocity() = velocity;
	}
	if (Velocity().z > -160.0f && Velocity().z < 0.0f)
		Velocity().z = -80.0f - length(Velocity().xy()) * 0.7f;
	if (remaining > 0.01f)
		DeusExPhysSwimming(remaining);
}

// APawn::Swim (0x103d3850): a move through the water; one that leaves it is
// brought back down to the water line, and the part of the move given back
// is returned.
float UPawn::DeusExSwim(const vec3& delta, CollisionHit& hit)
{
	vec3 start = Location();
	float given = 0.0f;
	hit = TryMoveHeldOff(delta);
	if (!InWater(Region().Zone))
	{
		vec3 end = Location();
		FindWaterLine(start, end);
		if (end != Location())
		{
			given = length(end - Location()) / length(delta);
			hit = TryMoveHeldOff(end - Location());
		}
	}
	return given;
}

// APawn::stepUp (0x103ce4a0): up MaxStepHeight against gravity, the move,
// and down again. A player meeting a pushable decoration head on is slowed by
// its mass; a wall met far enough along is stepped up again; anything else is
// slid along, a second wall through TwoWallAdjust. Coming down on a floor
// steeper than 0.5 slides the rest of the way.
void UPawn::DeusExStepUp(const vec3& gravDir, const vec3& desiredDir, vec3 delta, CollisionHit& hit)
{
	vec3 down = gravDir * MaxStepHeight();
	hit = TryMoveHeldOff(-down);
	hit = TryMoveHeldOff(delta);
	if (hit.Fraction < 1.0f)
	{
		UDecoration* decoration = UObject::TryCast<UDecoration>(hit.Actor);
		if (UObject::TryCast<UPlayerPawn>(this) && decoration && decoration->bPushable() && dot(desiredDir, hit.Normal) < -0.9f)
		{
			bJustTeleported() = true;
			Velocity() = Velocity() * (Mass() / (decoration->Mass() + Mass()));
			ProcessHitWall(hit.Normal, hit.Actor);
			if (Physics() == PHYS_Falling)
				return;
		}
		else if (std::abs(hit.Normal.z) < 0.2f && dot(delta, delta) * hit.Fraction > 144.0f)
		{
			DeusExStepUp(gravDir, desiredDir, delta * (1.0f - hit.Fraction), hit);
			if (Physics() == PHYS_Falling)
				return;
		}
		else
		{
			ProcessHitWall(hit.Normal, hit.Actor);
			vec3 oldHitNormal = hit.Normal;
			vec3 slide = (delta - hit.Normal * dot(delta, hit.Normal)) * (1.0f - hit.Fraction);
			if (dot(slide, delta) >= 0.0f)
			{
				hit = TryMoveHeldOff(slide);
				if (hit.Fraction < 1.0f)
				{
					ProcessHitWall(hit.Normal, hit.Actor);
					if (Physics() == PHYS_Falling)
						return;
					TwoWallAdjust(desiredDir, slide, hit.Normal, oldHitNormal, hit.Fraction);
					hit = TryMoveHeldOff(slide);
				}
			}
		}
	}
	hit = TryMoveHeldOff(down);
	if (hit.Fraction < 1.0f && hit.Normal.z < 0.5f)
	{
		vec3 slide = (down - hit.Normal * dot(down, hit.Normal)) * (1.0f - hit.Fraction);
		if (dot(slide, down) >= 0.0f)
			hit = TryMoveHeldOff(slide);
	}
}
