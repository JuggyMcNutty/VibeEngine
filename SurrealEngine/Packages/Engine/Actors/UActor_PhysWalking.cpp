
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

bool UActor::ShouldAbortJumping(UPawn* pawn, vec3 oldPosition, vec3 stepDownDelta)
{
	//check if we can still walk, if so, nothing changes
	if (TryStepToGround(stepDownDelta))
	{
		return false;
	}

	if (pawn->bCanJump())
	{
		CallEvent(this, EventName::MayFall);

		//the pawn decides to not jump over the ledge,
		//stop imediately and backtrack
		if (!pawn->bCanJump())
		{
			Velocity() = vec3(0.0f);
			Acceleration() = vec3(0.0f);
			TryMove(oldPosition - Location());
			return true;
		}
	}

	SetPhysics(PHYS_Falling);
	SetBase(nullptr, true);
	return false;
}

void UActor::TickWalking(float elapsed)
{
	// Deus Ex: a player on a ladder climbs it instead.
	if (engine->LaunchInfo.IsDeusEx())
	{
		UPawn* climber = UObject::TryCast<UPawn>(this);
		if (climber && climber->DeusExLadder(elapsed))
			return;
	}

	// Only pawns can walk!
	UPawn* pawn = PreparePawnMovementTick();
	if (!pawn)
		return;

	// Update the actor velocity based on the acceleration and zone

	UZoneInfo* zone = Region().Zone;
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(this);
	bool isCrouching = player && player->bIsWalking();

	Velocity().z = 0.0f;

	if (engine->LaunchInfo.IsDeusEx())
	{
		// Deus Ex's: the original's speed, level, braking with no
		// acceleration (APawn::physWalking 0x103ca540).
		Acceleration().z = 0.0f;
		float accelSize = length(Acceleration());
		vec3 accelDir = accelSize > 0.0f ? Acceleration() / accelSize : vec3(0.0f);
		pawn->DeusExCalcVelocity(accelDir, elapsed, pawn->GroundSpeed(), zone->ZoneGroundFriction(), false, true, false);
	}
	else
	{
		float accelRate = pawn->AccelRate() * (isCrouching ? 0.3f : 1.0f);
		float maxSpeed = (player ? player->GroundSpeed() : pawn->GroundSpeed() * pawn->DesiredSpeed()) * (isCrouching ? 0.3f : 1.0f);
		ApplyMovementAcceleration(elapsed, accelRate, zone->ZoneGroundFriction(), maxSpeed);
	}

	Velocity().z = 0.0f;

	// Deus Ex: a pawn its walk took into water, which its script made swim
	// (ZoneChange), swims the rest of the tick with its velocity as the old,
	// as the original's physWalking hands over to startSwimming after its
	// moves.
	auto intoWater = [&]() {
		if (!engine->LaunchInfo.IsDeusEx() || Physics() != PHYS_Swimming)
			return false;
		pawn->DeusExStartSwimming(Velocity(), elapsed, 0.0f);
		return true;
	};

	// The classic step up, move and step down algorithm:

	float gravityDirection = zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f;
	vec3 stepUpDelta(0.0f, 0.0f, -gravityDirection * pawn->MaxStepHeight());
	vec3 stepDownDelta(0.0f, 0.0f, gravityDirection * pawn->MaxStepHeight() * stepDownDeltaFactor);

	// "Step up and move" as long as we have time left and only hitting surfaces with low enough slope that it could be walked
	float timeLeft = elapsed;
	vec3 vel = Velocity() + zone->ZoneVelocity() * elapsed * 25.0f;

	//old position for tracking
	vec3 oldPosition = Location();

	//included Z in check - if its not 0 due to Zone properties, no action would have been taken previously
	//could lead to latend bugs
	if (length(vel) > 0)
	{
		for (int iteration = 0; timeLeft > 0.0f && iteration < 5; iteration++)
		{
			if (ShouldAbortMovementTick(PHYS_Walking))
				break;

			vec3 moveDelta = vel * timeLeft;

			//movement logic was inverted, causing overhaed buttons to be to easy to push
			// -> kevlar suit button in VortexRikers activates by moving under it
			//alternative approach: first move without stepUp -> only step up on collision
			//also yields a simpler code path since it avoids the need for "headbump" checks

			// try move forward
			CollisionHit hit = TryMove(moveDelta);
			if (intoWater())
				return;
			timeLeft -= timeLeft * hit.Fraction;
			moveDelta = vel * timeLeft;

			//check for fall and backtrack
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta) || intoWater())
				return;

			// if hit, step up and try again - maybe there was a ledge to get over
			if (hit.Fraction < 1.0f)
			{
				TryMove(stepUpDelta);
				oldPosition = Location();
				hit = TryMove(moveDelta);
				if (intoWater())
					return;
				timeLeft -= timeLeft * hit.Fraction;

				//check for fall and backtrack
				if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta * 2.0f))
				{
					TryMove(-stepUpDelta);
					return;
				}
				if (intoWater())
					return;

				// move back down to original vertical position
				TryMove(-stepUpDelta);
				if (intoWater())
					return;
			}

			oldPosition = Location();

			if (hit.Fraction < 1.0f)
			{
				if (player && hit.Actor)
				{
					if (UObject::IsType<UDecoration>(hit.Actor) && UObject::Cast<UDecoration>(hit.Actor)->bPushable() && dot(hit.Normal, moveDelta) < -0.9f)
					{
						// We hit a pushable decoration that is facing our movement direction

						//why does hitting a pushable decoration set the teleport flag?
						bJustTeleported() = true;
						vel = Velocity() = Velocity() * Mass() / (Mass() + hit.Actor->Mass());
						FireHitWall(hit);
						timeLeft = 0.0f;
					}
					else if (hit.Actor->bCollideActors() && hit.Actor->CollisionHeight() > 0.0f && hit.Actor->CollisionRadius() > 0.0f)
					{
						// TODO: We hit a non-movable actor

					}
				}
				else if (hit.Normal.z < 0.2f && hit.Normal.z > -0.2f)
				{
					// We hit a wall
					FireHitWall(hit);

					vec3 alignedDelta = (moveDelta - hit.Normal * dot(moveDelta, hit.Normal)) * (1.0f - hit.Fraction);
					if (dot(moveDelta, alignedDelta) >= 0.0f) // Don't end up going backwards
					{
						hit = TryMove(alignedDelta);
						if (intoWater())
							return;
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

			//check for fall and backtrack
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta) || intoWater())
				return;
		}
	}
	else
	{
		// Can we reach the ground from here?
		if (!TryStepToGround(stepDownDelta))
		{
			SetPhysics(PHYS_Falling);
			SetBase(nullptr, true);
		}
		else if (intoWater())
		{
			return;
		}
	}

	// Still walking, the pawn stands where the step down left it, on its
	// floor. The original's physWalking floats a pawn over its floor: its
	// trace down, MaxStepHeight + 2 long, stops a tenth of its length short
	// (UModel::LineCheck's backoff for a box), and a pawn that trace finds
	// nearer than 1.9 goes up to 2.1 -- so it stands 2.1 and a tenth of the
	// trace over the floor, 4.8 for MaxStepHeight 25 (dx-reverse-info/
	// engine-dll.md, walking). Deus Ex's traces back off as the original's,
	// so its pawns take the same measure; other games' stop a unit short of
	// the floor, 3.8 under where the float leaves them for MaxStepHeight 25.
	if (Physics() == PHYS_Walking)
	{
		if (engine->LaunchInfo.IsDeusEx())
		{
			float reach = pawn->MaxStepHeight() + 2.0f;
			CollisionHit floor = TryMove(vec3(0.0f, 0.0f, gravityDirection * reach), true);
			float floorDist = reach * floor.Fraction;
			if (floor.Fraction < 1.0f && floorDist < 1.9f)
				TryMove(vec3(0.0f, 0.0f, -gravityDirection * (2.1f - floorDist)));
		}
		else
		{
			float floatHeight = 0.1f * (pawn->MaxStepHeight() + 2.0f) + 2.1f;
			const float traceMargin = 1.0f;
			if (floatHeight > traceMargin)
				TryMove(vec3(0.0f, 0.0f, -gravityDirection * (floatHeight - traceMargin)));
		}
	}

	RecomputeVelocityFromDisplacement(elapsed);
	Velocity().z = 0.0f;
}

// APawn::calcVelocity (Engine.dll 0x103cd7a0; dx-reverse-info/engine-dll.md,
// moving: the speed), as Deus Ex's walking (GroundSpeed, the zone's ground
// friction, braking) and swimming (WaterSpeed, its fluid friction, as a
// fluid, buoyant) ask it. It turns and brakes with the larger of the
// friction and the fluid's 1. With no acceleration a braking pawn slows in
// slices of 0.03 s, to the slices' velocities that still point the old way
// weighted by their time, and stops under 10 or turned about. Otherwise the
// acceleration is cut to AccelRate (0.3 of it for a walking player -- Deus
// Ex's player walks whenever it swims) and the velocity turns toward it.
// Then the fluid's friction, the acceleration and the buoyancy; then the
// speed held to maxSpeed (x DesiredSpeed but for a player), a walking
// player's to 0.3 of it (0.6 in a net game), brought down to that no faster
// than the friction allows.
void UPawn::DeusExCalcVelocity(const vec3& accelDir, float deltaTime, float maxSpeed, float friction, bool fluid, bool brake, bool buoyant)
{
	float turning = std::max(fluid ? 1.0f : 0.0f, friction);
	bool player = UObject::TryCast<UPlayerPawn>(this) != nullptr;
	bool walkingPlayer = player && bIsWalking();

	if (brake && Acceleration() == vec3(0.0f))
	{
		vec3 oldVelocity = Velocity();
		vec3 sumVelocity(0.0f);
		float remainingTime = deltaTime;
		while (remainingTime > 0.03f)
		{
			Velocity() = Velocity() - Velocity() * (2.0f * 0.03f * turning);
			if (dot(Velocity(), oldVelocity) > 0.0f)
				sumVelocity += Velocity() * (0.03f / deltaTime);
			remainingTime -= 0.03f;
		}
		Velocity() = Velocity() - Velocity() * (2.0f * remainingTime * turning);
		if (dot(Velocity(), oldVelocity) > 0.0f)
			sumVelocity += Velocity() * (remainingTime / deltaTime);
		Velocity() = sumVelocity;
		if (dot(oldVelocity, Velocity()) < 0.0f || dot(Velocity(), Velocity()) < 100.0f)
			Velocity() = vec3(0.0f);
	}
	else
	{
		float speed = length(Velocity());
		float accelRate = walkingPlayer ? AccelRate() * 0.3f : AccelRate();
		if (dot(Acceleration(), Acceleration()) > accelRate * accelRate)
			Acceleration() = accelDir * accelRate;
		Velocity() = Velocity() - (Velocity() - accelDir * speed) * (deltaTime * turning);
	}
	Velocity() = Velocity() * (1.0f - (fluid ? friction : 0.0f) * deltaTime) + Acceleration() * deltaTime;

	if (!player)
		maxSpeed *= DesiredSpeed();

	if (buoyant)
	{
		// The mass as it is; a massless pawn would make this 0/0, and takes 1.
		float mass = Mass() != 0.0f ? Mass() : 1.0f;
		Velocity() = Velocity() + Region().Zone->ZoneGravity() * (deltaTime * (1.0f - Buoyancy() / mass));
	}

	float squared = dot(Velocity(), Velocity());
	if (walkingPlayer)
	{
		float walkSpeed = maxSpeed * (Level()->NetMode() != NM_Standalone ? 0.6f : 0.3f);
		if (squared > walkSpeed * walkSpeed)
		{
			float size = std::sqrt(squared);
			Velocity() = Velocity() / size * std::max(walkSpeed, (1.0f - 2.0f * turning * deltaTime) * size);
			return;
		}
	}
	if (squared > maxSpeed * maxSpeed)
		Velocity() = Velocity() * (maxSpeed / std::sqrt(squared));
}
