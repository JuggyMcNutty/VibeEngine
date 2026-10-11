
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
	// Deus Ex's pawns walk as the original's do.
	if (engine->LaunchInfo.IsDeusEx())
	{
		if (UPawn* deusExPawn = UObject::TryCast<UPawn>(this))
			deusExPawn->DeusExPhysWalking(elapsed);
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

	float accelRate = pawn->AccelRate() * (isCrouching ? 0.3f : 1.0f);
	float maxSpeed = (player ? player->GroundSpeed() : pawn->GroundSpeed() * pawn->DesiredSpeed()) * (isCrouching ? 0.3f : 1.0f);
	ApplyMovementAcceleration(elapsed, accelRate, zone->ZoneGroundFriction(), maxSpeed);

	Velocity().z = 0.0f;

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
			timeLeft -= timeLeft * hit.Fraction;
			moveDelta = vel * timeLeft;

			//check for fall and backtrack
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta))
				return;

			// if hit, step up and try again - maybe there was a ledge to get over
			if (hit.Fraction < 1.0f)
			{
				TryMove(stepUpDelta);
				oldPosition = Location();
				hit = TryMove(moveDelta);
				timeLeft -= timeLeft * hit.Fraction;

				//check for fall and backtrack
				if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta * 2.0f))
				{
					TryMove(-stepUpDelta);
					return;
				}

				// move back down to original vertical position
				TryMove(-stepUpDelta);
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
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta))
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
	}

	// Still walking, the pawn stands where the step down left it, on its
	// floor. The original's physWalking floats a pawn 2.1 and a tenth of its
	// floor trace over the floor, 4.8 for MaxStepHeight 25 (Deus Ex's walk,
	// below); other games' traces stop a unit short of the floor, 3.8 under
	// where the float leaves them for MaxStepHeight 25.
	if (Physics() == PHYS_Walking)
	{
		float floatHeight = 0.1f * (pawn->MaxStepHeight() + 2.0f) + 2.1f;
		const float traceMargin = 1.0f;
		if (floatHeight > traceMargin)
			TryMove(vec3(0.0f, 0.0f, -gravityDirection * (floatHeight - traceMargin)));
	}

	RecomputeVelocityFromDisplacement(elapsed);
	Velocity().z = 0.0f;
}

namespace
{
	vec3 SafeNormal(const vec3& v)
	{
		float len = length(v);
		return len > 0.0f ? v / len : vec3(0.0f);
	}

	// A wall too steep to walk up: hit, its normal's Z over 0.01 and under
	// 0.7.
	bool SlopedWall(const CollisionHit& hit)
	{
		return hit.Fraction < 1.0f && hit.Normal.z > 0.01f && hit.Normal.z < 0.7f;
	}
}

// APawn::physWalking (Engine.dll 0x103ca540; dx-reverse-info/engine-dll.md,
// moving: walking), Deus Ex's, a whole frame at once (TickPhysics). After
// the ladder check, the speed, level (calcVelocity, braking), and the move,
// with the zone's velocity for a player and for another pawn in a zone moving
// over 300; then steps of it -- a player's, and another pawn's whose move
// over the time left is over 20 units, of at most 0.05 s, half the time left
// -- each moved held off what it meets and stepped up over what blocks it,
// and the floor under it measured. A floor is stood on (floated 2.1 over
// where the trace stops), stepped down onto, or, sloped where the zone's
// ground friction x its normal's Z is under 3.3, slid down by gravity x the
// frame's time squared / (2 x that friction, at least 0.5). With no floor,
// or one steeper than 0.7, a pawn that cannot jump -- or whose MayFall says
// so, asked once if its state probes it -- stops where the frame began, its
// move given up (MoveTimer -1); any other gets Falling and falls the rest of
// the frame, unless its script made it do something else: a bird that flies
// moves on along its move. A step that took it into water swims the rest.
void UPawn::DeusExPhysWalking(float deltaTime)
{
	if (DeusExLadder(deltaTime))
		return;
	if (HasLeftWorld())
		return;

	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;
	bool player = UObject::TryCast<UPlayerPawn>(this) != nullptr;

	Velocity().z = 0.0f;
	Acceleration().z = 0.0f;
	DeusExCalcVelocity(SafeNormal(Acceleration()), deltaTime, GroundSpeed(), zone->ZoneGroundFriction(), false, true, false);

	vec3 desiredMove = Velocity();
	if (player || dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 90000.0f)
		desiredMove += zone->ZoneVelocity() * (25.0f * deltaTime);
	desiredMove.z = 0.0f;

	vec3 gravDir(0.0f, 0.0f, zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f);
	float reach = MaxStepHeight() + 2.0f;
	vec3 down = gravDir * reach;
	OldLocation() = Location();
	bJustTeleported() = false;

	TraceFlags levelFlags; // the original's flags 6
	levelFlags.movers = true;
	levelFlags.world = true;
	vec3 extent(CollisionRadius(), CollisionRadius(), CollisionHeight());

	float remaining = deltaTime;
	int iterations = 0;
	bool mayFallAsked = false;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		float timeTick = remaining;
		if (remaining > 0.05f && (player || dot(desiredMove, desiredMove) * remaining * remaining > 400.0f))
			timeTick = std::min(remaining * 0.5f, 0.05f);
		remaining -= timeTick;
		vec3 delta = desiredMove * timeTick;
		vec3 stepStart = Location();
		vec3 stepMove = delta;

		CollisionHit floor;
		bool measure = true;
		if (std::abs(delta.x) < 0.0001f && std::abs(delta.y) < 0.0001f && std::abs(delta.z) < 0.0001f)
		{
			// Still: a pawn on the same base, a line 20 down from its bottom
			// meeting the floor 4.1 to 4.6 away, is left as it is.
			remaining = 0.0f;
			bHitSlopedWall() = false;
			vec3 bottom = Location() - vec3(0.0f, 0.0f, CollisionHeight());
			CollisionHit under = XLevel()->Collision.TraceFirstHit(bottom, bottom - vec3(0.0f, 0.0f, 20.0f), this, vec3(0.0f), levelFlags);
			float underDist = under.Fraction * 20.0f;
			if (under.Actor && under.Actor == ActorBase() && underDist >= 4.1f && underDist <= 4.6f)
				measure = false;
		}
		else
		{
			// Last step met a wall too steep to walk up: a box along the move
			// (its length + 4, at least 30) meeting one again turns the move
			// along it; a player's three lines from a step's height over its
			// feet, ahead and 45 degrees to each side, 100 long, look for one.
			CollisionHit hit;
			if (bHitSlopedWall())
			{
				float size = length(delta);
				vec3 dir = delta / size;
				CollisionHit ahead = XLevel()->Collision.TraceFirstHit(Location(), Location() + dir * std::max(size + 4.0f, 30.0f), this, extent, levelFlags);
				bHitSlopedWall() = SlopedWall(ahead);
				if (bHitSlopedWall())
				{
					vec3 normal = SafeNormal(vec3(ahead.Normal.x, ahead.Normal.y, 0.0f));
					delta -= normal * dot(delta, normal);
				}
				else if (player)
				{
					vec3 eye(Location().x, Location().y, Location().z - CollisionHeight() + MaxStepHeight() + 4.0f);
					vec3 dirs[3] = { dir, SafeNormal(vec3(dir.y + dir.x, dir.y - dir.x, dir.z)), SafeNormal(vec3(dir.x - dir.y, dir.x + dir.y, dir.z)) };
					for (const vec3& d : dirs)
					{
						bHitSlopedWall() = SlopedWall(XLevel()->Collision.TraceFirstHit(eye, eye + d * 100.0f, this, vec3(0.0f), levelFlags));
						if (bHitSlopedWall())
							break;
					}
				}
				hit = TryMoveHeldOff(delta);
			}
			else
			{
				hit = TryMoveHeldOff(delta);
				bHitSlopedWall() = SlopedWall(hit);
			}
			if (bDeleteMe())
				return;

			if (hit.Fraction < 1.0f)
			{
				DeusExStepUp(gravDir, SafeNormal(delta), delta * (1.0f - hit.Fraction), hit);
				if (bDeleteMe())
					return;
				if (Physics() == PHYS_Falling)
				{
					// The step up left it falling: what the step did not move
					// across goes back to the time left; Falling, then a fall,
					// or a flight straight up at AirSpeed.
					float moved = length(vec2(Location().x - stepStart.x, Location().y - stepStart.y));
					remaining += (1.0f - std::min(moved / length(stepMove), 1.0f)) * timeTick;
					CallEvent(this, EventName::Falling);
					if (Physics() == PHYS_Falling)
					{
						if (remaining > 0.01f)
							TickFalling(remaining, iterations);
					}
					else if (Physics() == PHYS_Flying)
					{
						Velocity() = vec3(0.0f, 0.0f, AirSpeed());
						Acceleration() = vec3(0.0f, 0.0f, AccelRate());
						if (remaining > 0.01f)
							TickFlying(remaining);
					}
					return;
				}
			}
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining);
				return;
			}
		}
		if (!measure)
			continue;

		// The floor: a box of the pawn's size down MaxStepHeight + 2. None, or
		// the same base within 2.4, is stood on, lifted to 2.1 from nearer
		// than 1.9; any other the pawn steps down onto, its base then.
		floor = TryMove(down, true);
		UActor* floorActor = floor.Fraction < 1.0f ? (floor.Actor ? floor.Actor : Level()) : nullptr;
		float floorDist = reach * floor.Fraction;
		if (floor.Fraction >= 1.0f || (floorActor == ActorBase() && floorDist <= 2.4f))
		{
			if (floorDist < 1.9f)
			{
				vec3 normal = floor.Normal;
				TryMoveHeldOff(vec3(0.0f, 0.0f, 2.1f - floorDist));
				floor.Fraction = 0.0f;
				floor.Normal = normal;
			}
		}
		else
		{
			floor = TryMoveHeldOff(down);
			if (bDeleteMe())
				return;
			floorActor = floor.Fraction < 1.0f ? (floor.Actor ? floor.Actor : Level()) : nullptr;
			if (floorActor != ActorBase())
				SetSupportBase(floorActor);
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining);
				return;
			}
		}

		if (floor.Fraction >= 1.0f || floor.Normal.z < 0.7f)
		{
			// No floor, or one too steep.
			if (bCanJump() && !mayFallAsked && IsEventEnabled(EventName::MayFall))
			{
				mayFallAsked = true;
				CallEvent(this, EventName::MayFall);
			}
			if (!bCanJump())
			{
				Velocity() = vec3(0.0f);
				Acceleration() = vec3(0.0f);
				SetLocation(OldLocation());
				MoveTimer() = -1.0f;
				return;
			}
			if (floor.Fraction < 1.0f)
				bHitSlopedWall() = true;
			float stepSize = length(stepMove);
			if (stepSize == 0.0f)
			{
				remaining = 0.0f;
			}
			else
			{
				float moved = length(vec2(Location().x - stepStart.x, Location().y - stepStart.y));
				remaining += (1.0f - std::min(moved / stepSize, 1.0f)) * timeTick;
			}
			Velocity().z = 0.0f;
			CallEvent(this, EventName::Falling);
			if (Physics() == PHYS_Walking)
				SetPhysics(PHYS_Falling, nullptr);
			if (Physics() == PHYS_Falling)
			{
				float velZ = Velocity().z;
				if (!bJustTeleported() && deltaTime > remaining)
					Velocity() = (Location() - OldLocation()) / (deltaTime - remaining);
				Velocity().z = velZ;
				if (remaining > 0.01f)
					TickFalling(remaining, iterations);
				return;
			}
			TryMoveHeldOff(desiredMove * remaining);
			remaining = 0.0f;
		}
		else if (floor.Normal.z < 1.0f && floor.Normal.z * zone->ZoneGroundFriction() < 3.3f)
		{
			float friction = std::max(zone->ZoneGroundFriction(), 0.5f);
			vec3 slideMove = zone->ZoneGravity() * (deltaTime / (2.0f * friction) * deltaTime);
			vec3 slide = slideMove - floor.Normal * dot(slideMove, floor.Normal);
			if (dot(slide, slideMove) >= 0.0f)
				TryMoveHeldOff(slide);
			if (bDeleteMe())
				return;
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining);
				return;
			}
		}
	}

	if (!bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
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
