
#include "Precomp.h"
#include "UPawn.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Inventory/UWeapon.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Core/UClass.h"
#include "Utils/Logger.h"
#include "Engine.h"

void UPawn::Tick(float elapsed)
{
	// Deus Ex's tick starts as the original's AActor::Tick does: a pawn in
	// stasis, or waiting for its owner, does nothing more here.
	bool deusEx = engine->LaunchInfo.IsDeusEx();
	if (deusEx && !StartTick())
		return;

	MoveTimer() -= elapsed;

	// The original's pawn physics keep a running average of the tick,
	// which moveToward reads (APawn::performPhysics).
	if (deusEx)
		AvgPhysicsTime() = 0.8f * AvgPhysicsTime() + 0.2f * elapsed;

	if (StateFrame)
	{
		if (StateFrame->LatentState == LatentRunState::MoveTo)
		{
			if (deusEx)
			{
				if (DeusExPollMoveTo())
					StateFrame->LatentState = LatentRunState::Continue;
			}
			else
			{
				TickRotateTo(Focus());
				if (TickMoveTo(Destination()))
					StateFrame->LatentState = LatentRunState::Continue;
			}
		}
		else if (StateFrame->LatentState == LatentRunState::MoveToward && deusEx)
		{
			if (DeusExPollMoveToward())
				StateFrame->LatentState = LatentRunState::Continue;
		}
		else if (StateFrame->LatentState == LatentRunState::MoveToward)
		{
			if (MoveTarget())
			{
				Focus() = MoveTarget()->Location();
				TickRotateTo(Focus());
				if (TickMoveTo(MoveTarget()->Location()))
					StateFrame->LatentState = LatentRunState::Continue;
			}
			else
			{
				StateFrame->LatentState = LatentRunState::Continue;
			}
		}
		else if (StateFrame->LatentState == LatentRunState::StrafeTo)
		{
			TickRotateTo(Focus());
			if (TickMoveTo(Destination()))
				StateFrame->LatentState = LatentRunState::Continue;
		}
		else if (StateFrame->LatentState == LatentRunState::StrafeFacing)
		{
			if (engine->LaunchInfo.ue1Version > 219 && FaceTarget())
			{
				Focus() = FaceTarget()->Location();
				TickRotateTo(Focus());
				vec3 oldDest = Destination();
				if (TickMoveTo(Destination()))
					StateFrame->LatentState = LatentRunState::Continue;
				Destination() = oldDest;
			}
			else if (engine->LaunchInfo.ue1Version <= 219)
			{
				// How did StrafeFacing work in this version? We don't have FaceTarget.
				LogUnimplemented("StrafeFacing not implemented for old versions");
				StateFrame->LatentState = LatentRunState::Continue;
			}
			else
			{
				StateFrame->LatentState = LatentRunState::Continue;
			}
		}
		else if (StateFrame->LatentState == LatentRunState::TurnTo)
		{
			if (TickRotateTo(Focus()))
				StateFrame->LatentState = LatentRunState::Continue;
		}
		else if (StateFrame->LatentState == LatentRunState::TurnToward)
		{
			if (engine->LaunchInfo.ue1Version > 219 && FaceTarget())
			{
				Focus() = FaceTarget()->Location();
				if (TickRotateTo(FaceTarget()->Location()))
					StateFrame->LatentState = LatentRunState::Continue;
			}
			else if (engine->LaunchInfo.ue1Version <= 219)
			{
				// How did StrafeFacing work in this version? We don't have FaceTarget.
				LogUnimplemented("TurnToward not implemented for old versions");
				StateFrame->LatentState = LatentRunState::Continue;
			}
			else
			{
				StateFrame->LatentState = LatentRunState::Continue;
			}
		}
		else if (StateFrame->LatentState == LatentRunState::WaitForLanding)
		{
			if (Physics() != PHYS_Falling)
			{
				StateFrame->LatentState = LatentRunState::Continue;
			}
			else
			{
				// To do: need to send a LongFall event if the fall state lasts long enough
			}
		}
	}

	UActor::Tick(elapsed);

	if (bIsPlayer() && Role() >= ROLE_AutonomousProxy)
	{
		if (engine->LaunchInfo.ue1Version < 400 || bViewTarget())
			CallEvent(this, EventName::UpdateEyeHeight, { ExpressionValue::FloatValue(elapsed) });
		else
			ViewRotation() = Rotation();
	}

	if (Weapon())
	{
		Weapon()->Location() = Location();
		Weapon()->UpdateActorZone();
	}

	if (Role() == ROLE_Authority)
	{
		if (PainTime() > 0.0f)
		{
			PainTime() = std::max(PainTime() - elapsed, 0.0f);
			if (PainTime() == 0.0f)
				CallEvent(this, EventName::PainTimer);
		}
		if (SpeechTime() > 0.0f)
		{
			SpeechTime() = std::max(SpeechTime() - elapsed, 0.0f);
			if (SpeechTime() == 0.0f)
				CallEvent(this, EventName::SpeechTimer);
		}
		if (engine->LaunchInfo.ue1Version >= 436 && bAdvancedTactics())
			CallEvent(this, EventName::UpdateTactics, { ExpressionValue::FloatValue(elapsed) });


		SightCounter() -= elapsed;
		if (SightCounter() <= 0.0f)
		{
			//apply random offest to pawns sight counter so not all
			//pawns get evaluated at the same frame 
			//numbers are arbitrarily picked -> if it causes perf. problems, 
			//maybe switch to using some limit of how many pawns can be evaluated 
			//every frame
			SightCounter() += 0.4f - 0.2f * (static_cast <float> (rand()) / static_cast <float> (RAND_MAX));

			//search for target, if the pawn has an event to see them
			if (IsEventEnabled(EventName::SeePlayer))
			{
				//search every pawn in the map, if they are visible fire the related event
				for (UPawn* other = Level()->PawnList(); other; other = other->nextPawn())
				{
					//skip self, dead pawns and the currenty chosen enemy (handeld by the if(Enemy()) branch)
					if (other == this || other == Enemy() || other->Health() <= 0)
						continue;
					//if the enemy pawn is visible, notify the Unreal scrip about it
					if (CanSee(other))
						CallEvent(this, EventName::SeePlayer, { ExpressionValue::ObjectValue(other) });
				}
			}

			//if the pawn already has an enemy, update its tracking data
			if (Enemy())
			{
				if (CanSee(Enemy()))
				{
					LastSeenPos() = Enemy()->Location();
					LastSeeingPos() = Location();
					if (engine->LaunchInfo.ue1Version > 219)
						LastSeenTime() = Level()->TimeSeconds();
				}
				else if (IsEventEnabled(EventName::EnemyNotVisible))
				{
					CallEvent(this, EventName::EnemyNotVisible);
				}
			}
		}
	}
}

void UPawn::TickRotating(float elapsed)
{
	if (Physics() == PHYS_Spider)
		return;

	bRotateToDesired() = true;
	bFixedRotationDir() = false;

	if (Rotation() == DesiredRotation())
		return;

	Rotator rot = Rotation();

	if ((DesiredRotation().Yaw & 0xffff) != (rot.Yaw & 0xffff))
	{
		rot.Yaw = Rotator::TurnToShortest(rot.Yaw, DesiredRotation().Yaw, (int)std::abs(RotationRate().Yaw * elapsed));
	}

	if ((DesiredRotation().Pitch & 0xffff) != (rot.Pitch & 0xffff))
	{
		rot.Pitch = DesiredRotation().Pitch & 0xffff;
		if (rot.Pitch < 0x8000)
		{
			rot.Pitch = std::max(rot.Pitch, RotationRate().Pitch);
		}
		else if (rot.Pitch < 0x10000 - RotationRate().Pitch)
		{
			rot.Pitch = 0x10000 - RotationRate().Pitch;
		}
	}

	// To do: apply RotationRate().Roll

	Rotation() = rot;

	if (Rotation() == DesiredRotation())
	{
		CallEvent(this, EventName::EndedRotation);
	}
}

bool UPawn::TickRotateTo(const vec3& target)
{
	if (Physics() == PHYS_Spider)
		return true;

	DesiredRotation() = Rotator::FromVector(target - Location());

	if (Physics() == PHYS_Walking && (!MoveTarget() || !MoveTarget()->IsA("Pawn")))
	{
		DesiredRotation().Pitch = 0;
	}

	int doneAngle = 2000;
	return (std::abs(DesiredRotation().Yaw - (Rotation().Yaw & 0xffff)) < doneAngle) || (std::abs(DesiredRotation().Yaw - (Rotation().Yaw & 0xffff)) > 0xffff - doneAngle);
}

// The original's APawn::moveToward (dx-reverse-info/engine-dll.md, moving):
// one tick of a move toward dest -- the acceleration set toward it at the
// full rate --, and whether it is reached: within 16 units across (and the
// pawn's height, at least 48, up or down), or the move's time out, or for a
// pawn to move to within reach of it. Near the spot it slows. The fork's own
// took a spot reached within a fifth of the speed across, 20 units or more
// at a walk, outside a pawn's cylinder -- so ScriptedPawn's CheckDestLoc
// counted each leg of a patrol a miss and backed the pawn off at the fourth.
bool UPawn::DeusExMoveToward(const vec3& dest)
{
	vec3 dir = dest - Location();
	if (Physics() == PHYS_Walking)
	{
		dir.z = 0.0f;
	}
	else if (Physics() == PHYS_Falling)
	{
		// Falling, it steers only where gravity is well under its zone's
		// default, and arrives once well under the spot.
		UZoneInfo* zone = Region().Zone;
		UZoneInfo* defaultZone = zone ? zone->Class->GetDefaultObject<UZoneInfo>() : nullptr;
		if (!zone || !defaultZone || defaultZone->ZoneGravity().z * 0.9f >= zone->ZoneGravity().z)
			return false;
		dir.z = 0.0f;
		float across = length(dir);
		Acceleration() = (across > 0.0f ? dir * (1.0f / across) : vec3(0.0f)) * AccelRate();
		if (Velocity().z >= 0.0f)
			return false;
		return Location().z + 100.0f < dest.z;
	}

	// An item it has come up to it touches.
	UActor* target = MoveTarget();
	if (target && UObject::TryCast<UInventory>(target))
	{
		vec3 offset = Location() - target->Location();
		if (std::abs(offset.z) < CollisionHeight() && offset.x * offset.x + offset.y * offset.y < CollisionRadius() * CollisionRadius())
			CallEvent(target, EventName::Touch, { ExpressionValue::ObjectValue(this) });
	}

	float dist = length(dir);
	bool glider = bCanGlide() && !bCanStrafe() && (Physics() == PHYS_Flying || Physics() == PHYS_Swimming);

	if (dir.x * dir.x + dir.y * dir.y < 256.0f && std::abs(dir.z) < std::max(CollisionHeight(), 48.0f))
	{
		if (!glider)
			Acceleration() = vec3(0.0f);
		return true;
	}

	// A glider goes the way it faces; the rest straight at the spot.
	if (glider)
		dir = Coords::Rotation(Rotation()).XAxis;
	else if (dist > 0.0f)
		dir = dir * (1.0f / dist);
	Acceleration() = dir * AccelRate();

	if (MoveTimer() < 0.0f)
		return true;
	if (target && UObject::TryCast<UPawn>(target))
		return target->CollisionRadius() + CollisionRadius() + MeleeRange() * 0.8f > dist;

	// Moving fast, it steers its velocity onto the spot's line.
	float speed = length(Velocity());
	if (!glider && speed > 100.0f)
	{
		vec3 velDir = Velocity() * (1.0f / speed);
		Acceleration() -= (velDir - dir) * ((1.0f - dot(velDir, dir)) * speed * 0.2f);
	}

	// Within a tick and a half of it, it slows: to half its speed once, and
	// to 200 units a second over its speed.
	if (AvgPhysicsTime() * speed * 1.4f <= dist)
		return false;
	if (!bReducedSpeed())
	{
		DesiredSpeed() *= 0.5f;
		bReducedSpeed() = true;
	}
	if (speed > 0.0f)
		DesiredSpeed() = std::min(DesiredSpeed(), 200.0f / speed);
	return glider;
}

// The original's latent MoveTo, each tick (APawn::execPollMoveTo): a pawn
// walking around what it bumped (bAdvancedTactics) has its script's
// AlterDestination turn the destination for the tick.
bool UPawn::DeusExPollMoveTo()
{
	vec3 dest = Destination();
	if (bAdvancedTactics() && Physics() == PHYS_Walking)
		CallEvent(this, "AlterDestination");
	TickRotateTo(Destination());
	bool done = DeusExMoveToward(Destination());
	Destination() = dest;
	return done;
}

// The original's latent MoveToward, each tick (APawn::execPollMoveToward):
// toward where the target is -- a flyer to 0.7 of a pawn's height over its
// middle --, the destination turned as MoveTo's is; a pawn target leaves
// the speed as it was, and one in water is given up by a pawn that cannot
// swim.
bool UPawn::DeusExPollMoveToward()
{
	UActor* target = MoveTarget();
	if (!target)
		return true;

	Destination() = target->Location();
	if (Physics() == PHYS_Flying && UObject::TryCast<UPawn>(target))
		Destination().z += target->CollisionHeight() * 0.7f;
	else if (Physics() == PHYS_Spider)
		Destination() -= Floor() * target->CollisionRadius();
	Focus() = Destination();

	float desiredSpeed = DesiredSpeed();
	if (bAdvancedTactics() && Physics() == PHYS_Walking)
		CallEvent(this, "AlterDestination");
	TickRotateTo(Destination());
	bool done = DeusExMoveToward(Destination());

	target = MoveTarget();
	if (target)
	{
		Destination() = target->Location();
		if (UObject::TryCast<UPawn>(target))
		{
			DesiredSpeed() = desiredSpeed;
			if (!bCanSwim() && target->Region().Zone && target->Region().Zone->bWaterZone())
				MoveTimer() = -1.0f;
		}
	}
	return done;
}

bool UPawn::TickMoveTo(const vec3& target)
{
	if (engine->LaunchInfo.IsDeusEx())
		return DeusExMoveToward(target);

	if (MoveTimer() < 0.0f)
		return true;

	if (Physics() == PHYS_Walking)
	{
		vec2 delta = target.xy() - Location().xy();
		float distSqr = dot(delta, delta);
		float velocitySqr = dot(Velocity(), Velocity());
		if (distSqr < 1.0f || distSqr < velocitySqr * 0.05f)
			return true;

		Acceleration() = vec3(normalize(delta) * AccelRate(), 0.0f);
	}
	else
	{
		vec3 delta = target - Location();
		float distSqr = dot(delta, delta);
		float velocitySqr = dot(Velocity(), Velocity());
		if (distSqr < 1.0f || distSqr < velocitySqr * 0.05f)
			return true;

		Acceleration() = normalize(delta) * AccelRate();
	}

	return false;
}

void UPawn::MoveTo(const vec3& newDestination, float speed)
{
	MoveTarget() = nullptr;
	bReducedSpeed() = false;
	DesiredSpeed() = clamp(speed, 0.0f, MaxDesiredSpeed());
	Destination() = newDestination;
	Focus() = newDestination;
	SetMoveDuration(newDestination - Location());
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::MoveTo;

	// Deus Ex's takes its first step at once, as the original's exec does.
	if (engine->LaunchInfo.IsDeusEx())
	{
		TickRotateTo(Focus());
		DeusExMoveToward(Destination());
	}
}

void UPawn::MoveToward(UActor* newTarget, float speed)
{
	if (!newTarget)
		return;

	MoveTarget() = newTarget;
	Destination() = newTarget->Location();
	Focus() = newTarget->Location();
	bReducedSpeed() = false;
	DesiredSpeed() = clamp(speed, 0.0f, MaxDesiredSpeed());
	bool deusEx = engine->LaunchInfo.IsDeusEx();
	if (UObject::TryCast<UPawn>(newTarget))
		MoveTimer() = deusEx ? 1.2f : 1.0f;
	else
		SetMoveDuration(newTarget->Location() - Location());
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::MoveToward;

	// Deus Ex's takes its first step at once, as the original's exec does.
	if (deusEx)
	{
		TickRotateTo(Focus());
		DeusExMoveToward(Destination());
	}
}

void UPawn::StrafeFacing(const vec3& newDestination, UActor* newTarget)
{
	if (!newTarget)
		return;

	Destination() = newDestination;
	if (engine->LaunchInfo.ue1Version > 219)
		FaceTarget() = newTarget;
	SetMoveDuration(newDestination - Location());
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::StrafeFacing;
}

void UPawn::StrafeTo(const vec3& newDestination, const vec3& newFocus)
{
	MoveTarget() = nullptr;
	bReducedSpeed() = false;
	DesiredSpeed() = bIsPlayer() ? MaxDesiredSpeed() : 0.8f * MaxDesiredSpeed();
	Destination() = newDestination;
	Focus() = newFocus;
	SetMoveDuration(newDestination - Location());
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::StrafeTo;
}

void UPawn::TurnTo(const vec3& newFocus)
{
	MoveTarget() = nullptr;
	Focus() = newFocus;
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::TurnTo;
}

void UPawn::TurnToward(UActor* newTarget)
{
	if (!newTarget)
		return;

	if (engine->LaunchInfo.ue1Version > 219)
		FaceTarget() = newTarget;
	Focus() = newTarget->Location();
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::TurnToward;
}

void UPawn::WaitForLanding()
{
	if (StateFrame)
		StateFrame->LatentState = LatentRunState::WaitForLanding;
}

void UPawn::SetMoveDuration(const vec3& deltaMove)
{
	float scale = DesiredSpeed() * GetSpeed();
	MoveTimer() = scale > 0.0f ? 1.0f + 1.3f * length(deltaMove) / scale : 0.5f;
}

float UPawn::GetSpeed()
{
	switch (Physics())
	{
	case PHYS_Walking:
	case PHYS_Falling:
	case PHYS_Spider:
		return GroundSpeed();
	case PHYS_Flying:
		return AirSpeed();
	case PHYS_Swimming:
		return WaterSpeed();
	default:
		return 0.0f;
	}
}
