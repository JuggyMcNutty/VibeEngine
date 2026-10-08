
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
	// Deus Ex's actors fall as the original's do.
	if (engine->LaunchInfo.IsDeusEx())
	{
		DeusExTickFalling(elapsed);
		return;
	}

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

		CollisionHit hit = TryMove(moveDelta);
		timeLeft -= timeLeft * hit.Fraction;

		if (hit.Fraction < 1.0f)
		{
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
				hit = TryMove(reflectedDelta);
			}
			else
			{
				if (hit.Normal.z <= 0.7071f)
				{
					// We hit a slope. Try to follow it.
					vec3 alignedDelta = (moveDelta - hit.Normal * dot(moveDelta, hit.Normal)) * (1.0f - hit.Fraction);
					if (dot(moveDelta, alignedDelta) >= 0.0f) // Don't end up going backwards
					{
						hit = TryMove(alignedDelta);
						if (hit.Fraction < 1.0f && hit.Normal.z > 0.7071f)
						{
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
					PhysLanded(hit.Actor, hit.Normal, timeLeft);
					timeLeft = 0.0f;
				}
			}
		}
	}
}

// AActor::physFalling (Engine.dll 0x103d0a50; dx-reverse-info/engine-dll.md,
// moving: falling), Deus Ex's. A pawn's air control first, then steps of the
// tick: each moves by the step's mean velocity, its move held off what it
// meets as ULevel::MoveActor holds it; a pawn the move takes into water swims
// on, a bounce gives HitWall, a floor (normal over 0.7) lands, a wall is slid
// along; the velocity after is the move's own, doubled less the old when it
// gained downward or the old was not falling, held to the zone's terminal
// velocity. Not here: the original's ladder check first, a ladder the fork
// does not climb; and its split of a step at the top of a rise, which needs a
// step over 0.03 s and the fork's are 0.02 at most (TickPhysics).
void UActor::DeusExTickFalling(float deltaTime)
{
	if (HasLeftWorld())
		return;

	UPawn* pawn = UObject::TryCast<UPawn>(this);
	UDecoration* decor = UObject::TryCast<UDecoration>(this);
	bool player = UObject::TryCast<UPlayerPawn>(this) != nullptr;

	auto safeNormal = [](const vec3& v) {
		float len = length(v);
		return len > 0.0f ? v / len : vec3(0.0f);
	};

	// A pawn's air control: AirControl, 0.05 when over 0.15 and a box its
	// size meets the world along its move this tick. The acceleration, made
	// level, is held to it x AccelRate -- more at a standstill, 1 at
	// GroundSpeed with no more air control than 0.05 -- or else the speed
	// across held where it is; it is put back after the tick.
	vec3 savedAcceleration = Acceleration();
	float maxAirSpeed = 0.0f;
	if (pawn)
	{
		float airControl = pawn->AirControl();
		if (airControl > 0.15f)
		{
			vec3 testWalk = (safeNormal(Acceleration()) * (airControl * pawn->AccelRate()) + Velocity()) * deltaTime;
			testWalk.z = 0.0f;
			TraceFlags world;
			world.movers = true;
			world.world = true;
			CollisionHit hit = XLevel()->Collision.TraceFirstHit(Location(), Location() + testWalk, this, vec3(CollisionRadius(), CollisionRadius(), CollisionHeight()), world);
			if (hit.Actor)
				airControl = 0.05f;
		}
		float maxAccel = airControl * pawn->AccelRate();
		Acceleration().z = 0.0f;
		float speed2d = length(Velocity().xy());
		if (speed2d < 10.0f)
			maxAccel += (10.0f - speed2d) / deltaTime;
		else if (speed2d >= pawn->GroundSpeed())
		{
			if (airControl <= 0.05f)
				maxAccel = 1.0f;
			else
				maxAirSpeed = speed2d;
		}
		if (dot(Acceleration(), Acceleration()) > maxAccel * maxAccel)
			Acceleration() = safeNormal(Acceleration()) * maxAccel;
	}

	float remaining = deltaTime;
	int iterations = 0;
	int bounces = 0;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		float tick = remaining > 0.1f ? std::min(0.1f, remaining * 0.5f) : remaining;
		remaining -= tick;
		OldLocation() = Location();
		bJustTeleported() = false;
		vec3 oldVelocity = Velocity();
		UZoneInfo* zone = Region().Zone;

		// The step's mean velocity: in water, buoyant and slowed by the
		// zone's fluid friction twice over; a bobbing decoration at half
		// gravity; a player falling with its feet in water slowed by their
		// zone's fluid friction; else gravity and the acceleration.
		if (zone->bWaterZone())
		{
			float netGravity = 1.0f - Buoyancy() / std::max(Mass(), 1.0f);
			Velocity() = oldVelocity * (1.0f - 2.0f * tick * zone->ZoneFluidFriction()) + (zone->ZoneGravity() * netGravity + Acceleration()) * (0.5f * tick);
		}
		else if (decor && decor->bBobbing())
		{
			Velocity() = oldVelocity + (zone->ZoneGravity() * 0.5f + Acceleration()) * (0.5f * tick);
		}
		else if (player && pawn->FootRegion().Zone && pawn->FootRegion().Zone->bWaterZone() && oldVelocity.z < 0.0f)
		{
			Velocity() = oldVelocity * (1.0f - tick * pawn->FootRegion().Zone->ZoneFluidFriction()) + (zone->ZoneGravity() + Acceleration()) * (0.5f * tick);
		}
		else
		{
			Velocity() = oldVelocity + (zone->ZoneGravity() + Acceleration()) * (0.5f * tick);
		}

		if (maxAirSpeed > 0.0f && dot(Velocity().xy(), Velocity().xy()) > maxAirSpeed * maxAirSpeed)
		{
			vec2 across = normalize(Velocity().xy()) * maxAirSpeed;
			Velocity().x = across.x;
			Velocity().y = across.y;
		}

		// The zone's velocity for any but a pawn, for a player, and for a
		// pawn in a zone moving over 200.
		vec3 zoneVel(0.0f);
		if (!pawn || player || dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 40000.0f)
			zoneVel = zone->ZoneVelocity();
		vec3 adjusted = (Velocity() + zoneVel) * tick;
		CollisionHit hit = TryMoveHeldOff(adjusted);
		if (bDeleteMe())
			return;

		// A pawn the move took into water, which its script made swim
		// (ZoneChange), swims the rest.
		if (pawn && Physics() == PHYS_Swimming)
		{
			remaining += (1.0f - hit.Fraction) * tick;
			pawn->DeusExStartSwimming(oldVelocity, tick, remaining);
			return;
		}

		bool hitSomething = hit.Fraction < 1.0f;
		if (hitSomething)
		{
			// A decoration that hits the player loses a landing (not under 0).
			if (decor && UObject::TryCast<UPlayerPawn>(hit.Actor))
				decor->numLandings() = std::max(decor->numLandings() - 1, 0);

			if (bBounce())
			{
				// A bounce is HitWall's; the first two in the tick give the
				// rest of the step back.
				CallEvent(this, EventName::HitWall, { ExpressionValue::VectorValue(hit.Normal), ExpressionValue::ObjectValue(hit.Actor ? hit.Actor : Level()) });
				if (Physics() == PHYS_None)
					return;
				if (bounces < 2)
					remaining += (1.0f - hit.Fraction) * tick;
				bounces++;
			}
			else if (hit.Normal.z > 0.7f)
			{
				// A floor: the velocity becomes the move's own, the distance
				// gone over the time it took, and the time the move did not
				// take is left to the landing.
				remaining += (1.0f - hit.Fraction) * tick;
				if (!bJustTeleported() && hit.Fraction > 0.1f && hit.Fraction * tick > 0.003f)
					Velocity() = (Location() - OldLocation()) / (hit.Fraction * tick);
				PhysLanded(hit.Actor, hit.Normal, remaining);
				return;
			}
			else
			{
				// A wall: slid along, a second wall through TwoWallAdjust; a
				// floor the slide meets lands with the velocity as it is, the
				// step's time spent, and so does a ditch between two walls.
				ProcessHitWall(hit.Normal, hit.Actor);
				vec3 oldHitNormal = hit.Normal;
				vec3 delta = (adjusted - hit.Normal * dot(adjusted, hit.Normal)) * (1.0f - hit.Fraction);
				if (dot(delta, adjusted) >= 0.0f)
				{
					hit = TryMoveHeldOff(delta);
					if (hit.Fraction < 1.0f)
					{
						if (hit.Normal.z > 0.7f)
						{
							PhysLanded(hit.Actor, hit.Normal, remaining);
							return;
						}
						ProcessHitWall(hit.Normal, hit.Actor);
						TwoWallAdjust(safeNormal(adjusted), delta, hit.Normal, oldHitNormal, hit.Fraction);
						bool ditch = oldHitNormal.z > 0.0f && hit.Normal.z > 0.0f && delta.z == 0.0f && dot(oldHitNormal, hit.Normal) < 0.0f;
						hit = TryMoveHeldOff(delta);
						if (ditch || hit.Normal.z > 0.7f)
						{
							PhysLanded(hit.Actor, hit.Normal, remaining);
							return;
						}
					}
				}
				oldVelocity.x = (Location().x - OldLocation().x) / tick;
				oldVelocity.y = (Location().y - OldLocation().y) / tick;
			}
		}

		if ((!bBounce() || !hitSomething) && !bJustTeleported())
		{
			Velocity() = (Location() - OldLocation()) / tick - zoneVel;
			if (Velocity().z < oldVelocity.z || oldVelocity.z >= 0.0f)
				Velocity() = Velocity() * 2.0f - oldVelocity;
			float terminal = Region().Zone->ZoneTerminalVelocity();
			if (dot(Velocity(), Velocity()) > terminal * terminal)
				Velocity() = safeNormal(Velocity()) * terminal;
		}
	}
	Acceleration() = savedAcceleration;
}
