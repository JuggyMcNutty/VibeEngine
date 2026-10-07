
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

void UActor::TickFalling(float elapsed)
{
	if (HasLeftWorld())
		return;

	UZoneInfo* zone = Region().Zone;
	UDecoration* decor = UObject::TryCast<UDecoration>(this);
	UPawn* pawn = UObject::TryCast<UPawn>(this);

	// UnrealScript property references
	vec3& acceleration = Acceleration();
	vec3& velocity = Velocity();
	float groundSpeed = 0.0f;

	if (pawn)
	{
		groundSpeed = pawn->GroundSpeed();
		float maxAccel = engine->LaunchInfo.ue1Version > 219 ? pawn->AirControl() * pawn->AccelRate() : 0.0f;
		float accel = length(acceleration);
		if (accel > maxAccel)
			acceleration = normalize(acceleration) * maxAccel;
	}

	float gravityScale = 2.0f;
	float fluidFriction = 0.0f;

	if (decor && decor->bBobbing())
	{
		gravityScale = 1.0f;
	}
	else if (pawn && pawn->FootRegion().Zone->bWaterZone() && velocity.z < 0.0f)
	{
		fluidFriction = pawn->FootRegion().Zone->ZoneFluidFriction();
	}

	OldLocation() = Location();
	bJustTeleported() = false;

	float fluidFactor = 1.0f - fluidFriction * elapsed;
	vec3 accelVector = acceleration * 1.5f;
	vec3 gravityVector = gravityScale * zone->ZoneGravity();

	vec3 oldVelocity = velocity;
	vec3 newVelocity = oldVelocity * fluidFactor + (accelVector + gravityVector) * 0.5f * elapsed;

	// Limit air control to controlling which direction we are moving in the XY plane, but not increase the speed beyond the ground speed
	vec2 velocity2d = velocity.xy();
	vec2 newVelocity2d = newVelocity.xy();
	float curSpeedSquared = dot(velocity2d, velocity2d);
	if (pawn && curSpeedSquared >= (groundSpeed * groundSpeed) && dot(newVelocity2d, newVelocity2d) > curSpeedSquared)
	{
		float xySpeed = length(velocity2d);
		newVelocity = vec3(normalize(newVelocity2d) * xySpeed, newVelocity.z);
	}
	velocity = newVelocity;

	// In water Deus Ex's falls take the original's step (Engine.dll
	// AActor::physFalling 0x103d0a50): gravity less the buoyancy against the
	// mass, the zone's fluid friction twice over; the move takes the step's
	// mean velocity, and the velocity after it is twice that less the old
	// when it gained downward or was rising, the mean otherwise. A crate with
	// more buoyancy than mass floats (FloatConsole).
	if (engine->LaunchInfo.IsDeusEx() && zone->bWaterZone())
	{
		float netGravity = 1.0f - Buoyancy() / std::max(Mass(), 1.0f);
		float friction = 1.0f - 2.0f * elapsed * zone->ZoneFluidFriction();
		newVelocity = oldVelocity * friction + (zone->ZoneGravity() * netGravity + acceleration) * 0.5f * elapsed;
		bool doubled = newVelocity.z < oldVelocity.z || oldVelocity.z >= 0.0f;
		velocity = doubled ? newVelocity * 2.0f - oldVelocity : newVelocity;
	}

	// Deus Ex's falls move as the original's physFalling does, by
	// ULevel::MoveActor, so an actor comes to rest held off its floor: 2
	// units, and the box trace's backoff, a tenth of the last move + 2.
	bool heldOff = engine->LaunchInfo.IsDeusEx();
	auto move = [&](const vec3& delta) { return heldOff ? TryMoveHeldOff(delta) : TryMove(delta); };

	// Deus Ex's floor is a normal over 0.7 (Engine.dll physFalling
	// 0x103d0a50); other games keep 0.7071.
	bool deusEx = engine->LaunchInfo.IsDeusEx();
	auto isFloor = [&](const vec3& normal) { return deusEx ? normal.z > 0.7f : normal.z > 0.7071f; };

	float timeLeft = elapsed;
	for (int iteration = 0; timeLeft > 0.0f && iteration < 5; iteration++)
	{
		if (ShouldAbortMovementTick(PHYS_Falling))
			break;

		float zoneTerminalVelocity = zone->ZoneTerminalVelocity();
		if (dot(velocity, velocity) > zoneTerminalVelocity * zoneTerminalVelocity)
		{
			velocity = normalize(velocity) * zoneTerminalVelocity;
			newVelocity = velocity;
		}

		vec3 moveDelta = (newVelocity + zone->ZoneVelocity() * elapsed * 25.0f) * timeLeft;
		vec3 dirNormal = normalize(newVelocity);

		// The step's start and length, for the velocity a landing takes.
		vec3 stepStart = Location();
		float stepTime = timeLeft;

		CollisionHit hit = move(moveDelta);
		timeLeft -= timeLeft * hit.Fraction;

		if (hit.Fraction < 1.0f)
		{
			// Deus Ex: a decoration that hits the player loses a landing
			// (not under 0).
			if (deusEx && decor && UObject::TryCast<UPlayerPawn>(hit.Actor))
				decor->numLandings() = std::max(decor->numLandings() - 1, 0);

			if (hit.Actor && hit.Actor->IsA("Pawn"))
			{
				// So projectiles don't think they hit a wall.
			}
			else
			{
				FireHitWall(hit);
			}

			// Hit the level
			if (bBounce())
			{
				vec3 reflectedDelta = reflect(moveDelta, hit.Normal);
				hit = move(reflectedDelta);
			}
			else
			{
				if (!isFloor(hit.Normal))
				{
					// We hit a slope. Try to follow it.
					vec3 alignedDelta = (moveDelta - hit.Normal * dot(moveDelta, hit.Normal)) * (1.0f - hit.Fraction);
					if (dot(moveDelta, alignedDelta) >= 0.0f) // Don't end up going backwards
					{
						hit = move(alignedDelta);
						if (hit.Fraction < 1.0f && isFloor(hit.Normal))
						{
							// A floor the slide meets lands with the velocity as
							// it is, the step's time spent.
							PhysLanded(hit.Actor, hit.Normal, 0.0f);
							return;
						}
					}

					// adjust velocity along the slope
					if (!bBounce())
						RecomputeVelocityFromDisplacement(elapsed);

					timeLeft = 0.0f;
				}
				else
				{
					// Deus Ex: the velocity becomes the move's own, the
					// distance gone over the time it took, and the time the
					// move did not take is left to the landing.
					if (deusEx && !bJustTeleported() && hit.Fraction > 0.1f && hit.Fraction * stepTime > 0.003f)
						velocity = (Location() - stepStart) / (hit.Fraction * stepTime);
					PhysLanded(hit.Actor, hit.Normal, timeLeft);
					timeLeft = 0.0f;
				}
			}
		}
	}
}
