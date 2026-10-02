
#include "Precomp.h"
#include "UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UWarpZoneMarker.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Collision/TopLevel/CollisionHit.h"
#include "Engine.h"

// Deus Ex's reachability tests, the original's (engine-dll.md, reaching): its
// pointReachable and actorReachable, the Reachable they ask, the walk, fly
// and swim tests under that, and the moves each steps with -- a test move
// that pawns do not stop, stepping up what blocks it and down to a floor --
// the pawn moved along and put back. AIDirectionReachable walks with the same
// moves.

namespace
{
	// UE1's MINMOVETHRESHOLD: a move shorter than this made no progress.
	const float MinMoveThreshold = 4.1f;

	bool PainFor(UPawn* pawn, UZoneInfo* zone)
	{
		return zone && zone->bPainZone() && zone->DamageType() != pawn->ReducedDamageType();
	}

	bool Water(UZoneInfo* zone)
	{
		return zone && zone->bWaterZone();
	}

	vec3 SafeNormal(const vec3& v)
	{
		float len = length(v);
		return len > 0.0f ? v / len : vec3(0.0f);
	}

	// ClassifyDistanceSq (Engine.dll 0x103c7f80): -1 nearer than the range,
	// 1 beyond it, 0 in it.
	int ClassifyDistanceSq(float minSq, float distSq, float maxSq)
	{
		if (distSq < minSq)
			return -1;
		return distSq > maxSq ? 1 : 0;
	}
}

// ULevel::MoveActor as the reach tests move a pawn (bTest and bIgnorePawns):
// along the delta until the level, or an actor that blocks it, stops it --
// held off it as every move is (TryMoveHeldOff), no events -- with its zones
// found again silently. Deus Ex's bIgnorePawns passes through pawns and
// decorations alike, unless bStatic (Engine.dll 0x103995f1).
CollisionHit UPawn::ReachTestMove(const vec3& delta)
{
	CollisionHit blockingHit;
	if (dot(delta, delta) < 0.00000001f)
		return blockingHit;

	if (bCollideWorld() || bBlockActors() || bBlockPlayers())
	{
		bool useBlockPlayers = IsPlayerOrProjectile();
		CollisionHitList hits = XLevel()->Collision.Trace(Location(), Location() + HeldOffDelta(delta), CollisionHeight(), CollisionRadius(), bCollideActors(), bCollideWorld(), false);
		for (const CollisionHit& hit : hits)
		{
			if (hit.Actor)
			{
				if (hit.Actor == this || (!hit.Actor->bStatic() && (hit.Actor->IsA("Pawn") || hit.Actor->IsA("Decoration"))))
					continue;
				bool isBlocking = (useBlockPlayers || hit.Actor->IsPlayerOrProjectile())
					? (hit.Actor->bBlockPlayers() && bBlockPlayers())
					: (hit.Actor->bBlockActors() && bBlockActors());
				if (isBlocking && !hit.Actor->IsBasedOn(this) && !IsBasedOn(hit.Actor))
				{
					blockingHit = hit;
					break;
				}
			}
			else
			{
				blockingHit = hit;
				break;
			}
		}
		blockingHit.Fraction = HeldOffFraction(delta, blockingHit.Fraction);
	}

	XLevel()->Collision.RemoveFromCollision(this);
	Location() += delta * blockingHit.Fraction;
	XLevel()->Collision.AddToCollision(this);
	Region() = FindRegion();
	FootRegion() = FindRegion({ 0.0f, 0.0f, -CollisionHeight() });
	HeadRegion() = FindRegion({ 0.0f, 0.0f, EyeHeight() });
	return blockingHit;
}

// APawn::walkMove (Engine.dll 0x103c3290): a move across, stepping up
// MaxStepHeight over what blocks it, then down to the floor within
// MaxStepHeight + 2. 5 is the goal bumped, 1 moved, 0 no progress (or a wall
// too steep to step), -1 no floor (or one too steep).
int UPawn::DeusExWalkMove(vec3 delta, CollisionHit& hit, UActor* goalActor, float threshold, bool adjust)
{
	vec3 start = Location();
	delta.z = 0.0f;
	float gravityDir = (Region().Zone && Region().Zone->ZoneGravity().z > 0.0f) ? 1.0f : -1.0f;
	vec3 down(0.0f, 0.0f, MaxStepHeight() * gravityDir);

	hit = ReachTestMove(delta);
	if (goalActor && hit.Actor == goalActor)
		return 5;
	if (hit.Fraction < 1.0f)
	{
		delta = delta * (1.0f - hit.Fraction);
		hit = ReachTestMove(-down);
		hit = ReachTestMove(delta);
		if (goalActor && hit.Actor == goalActor)
			return 5;
		hit = ReachTestMove(down);
		if (goalActor && hit.Actor == goalActor)
			return 5;
		if (hit.Fraction < 1.0f && hit.Normal.z < 0.7f)
		{
			TestMoveTo(start, true);
			return 0;
		}
	}

	vec3 dropFrom = Location();
	hit = ReachTestMove(vec3(0.0f, 0.0f, (MaxStepHeight() + 2.0f) * gravityDir));
	if (hit.Fraction == 1.0f)
	{
		TestMoveTo(adjust ? start : dropFrom, true);
		return -1;
	}
	if (hit.Normal.z < 0.7f)
	{
		TestMoveTo(start, true);
		return -1;
	}

	vec3 moved = Location() - start;
	if (dot(moved, moved) < threshold * threshold)
	{
		if (adjust)
			TestMoveTo(start, true);
		return 0;
	}
	return 1;
}

// APawn::flyMove (Engine.dll 0x103c3900): a move, stepping up MaxStepHeight
// over what blocks it.
int UPawn::DeusExFlyMove(vec3 delta, UActor* goalActor, float threshold, bool adjust)
{
	vec3 start = Location();
	CollisionHit hit = ReachTestMove(delta);
	if (goalActor && hit.Actor == goalActor)
		return 5;
	if (hit.Fraction < 1.0f)
	{
		delta = delta * (1.0f - hit.Fraction);
		ReachTestMove(vec3(0.0f, 0.0f, MaxStepHeight()));
		hit = ReachTestMove(delta);
		if (goalActor && hit.Actor == goalActor)
			return 5;
	}

	vec3 moved = Location() - start;
	if (dot(moved, moved) < threshold * threshold)
	{
		if (adjust)
			TestMoveTo(start, true);
		return 0;
	}
	return 1;
}

// APawn::swimMove (Engine.dll 0x103c3ca0): flyMove in the water; a move that
// leaves the water goes back to the water line and makes no progress.
int UPawn::DeusExSwimMove(vec3 delta, UActor* goalActor, float threshold, bool adjust)
{
	vec3 start = Location();
	CollisionHit hit = ReachTestMove(delta);
	if (goalActor && hit.Actor == goalActor)
		return 5;
	if (!Water(Region().Zone))
	{
		vec3 end = Location();
		FindWaterLine(start, end);
		if (end.x != Location().x || end.y != Location().y || end.z != Location().z)
			ReachTestMove(end - Location());
		return 0;
	}
	if (hit.Fraction < 1.0f)
	{
		delta = delta * (1.0f - hit.Fraction);
		ReachTestMove(vec3(0.0f, 0.0f, MaxStepHeight()));
		hit = ReachTestMove(delta);
		if (goalActor && hit.Actor == goalActor)
			return 5;
	}

	vec3 moved = Location() - start;
	if (dot(moved, moved) < threshold * threshold)
	{
		if (adjust)
			TestMoveTo(start, true);
		return 0;
	}
	return 1;
}

// APawn::findWaterLine (Engine.dll 0x103d3b10): the water's edge between a
// point in the water and one out of it, halved until they lie within a unit.
void UPawn::FindWaterLine(vec3 start, vec3& end)
{
	bool hereWater = Water(Region().Zone);
	for (;;)
	{
		vec3 d = end - start;
		if (dot(d, d) < 1.0f)
			return;
		vec3 mid = (start + end) * 0.5f;
		if (Water(XLevel()->Model->FindRegion(mid, Level()).Zone) == hereWater)
			end = mid;
		else
			start = mid;
	}
}

// APawn::SuggestJumpVelocity (Engine.dll 0x103c2c80): how long a jump of the
// given upward speed stays in the air to come down at the destination's
// height, and the speed across that covers the distance in that time, no more
// than GroundSpeed.
void UPawn::SuggestJumpVelocity(vec3 dest, vec3& vel)
{
	float gravityZ = Region().Zone ? Region().Zone->ZoneGravity().z : -950.0f;
	if (gravityZ >= 0.0f)
		gravityZ = -100.0f;
	float deltaZ = dest.z - Location().z;
	float jumpZ = vel.z;
	float time = 0.0f;
	float currentZ = 0.0f;
	while (currentZ > deltaZ || vel.z > 0.0f)
	{
		vel.z = vel.z + gravityZ * 0.05f;
		time += 0.05f;
		currentZ = currentZ + vel.z * 0.05f;
	}
	if (std::abs(vel.z) > 1.0f)
		time = time - (currentZ - deltaZ) / vel.z;

	vel = dest - Location();
	vel.z = 0.0f;
	float speed;
	if (time > 0.0f)
	{
		float across = (float)std::sqrt(vel.x * vel.x + vel.y * vel.y);
		if (across != 0.0f)
			vel = vel * (1.0f / across);
		speed = std::min(across / time, GroundSpeed());
	}
	else
	{
		vel = SafeNormal(vel);
		speed = GroundSpeed();
	}
	vel = vel * speed;
	vel.z = jumpZ;
}

// APawn::FindBestJump (Engine.dll 0x103c2f50): a jump toward the destination,
// landed (jumpLanding), counts where it comes down more than 8 units nearer,
// not 350 or more below where it started, out of a pain zone the pawn does not
// resist, and out of water unless the pawn can swim.
bool UPawn::FindBestJump(vec3 dest, vec3 testVel, vec3& landing, bool movePawn)
{
	vec3 start = Location();
	SuggestJumpVelocity(dest, testVel);
	landing = JumpLanding(testVel, true);

	bool found = false;
	if (!PainFor(this, FootRegion().Zone) && (bCanSwim() || !Water(Region().Zone)))
	{
		float before = length(dest - start);
		float after = length(dest - Location());
		if (start.z - Location().z < 350.0f && before - after > 8.0f)
			found = true;
	}
	if (!movePawn)
		TestMoveTo(start, true);
	return found;
}

// APawn::walkReachable (Engine.dll 0x103c1b70): walkMoves toward the
// destination, the collision radius at a time (a jumper's at least 128), at
// most 100 of them, until within the threshold across and its height -- the
// goal's when it is taller -- up or down (a slope's rise allowed); a fall
// taken as a jump by a jumper or retried in steps of MaxStepHeight, a flyer
// flying the rest, water swum. The pawn and its velocity are put back.
int UPawn::DeusExWalkReachable(vec3 dest, float threshold, int reachFlags, UActor* goalActor)
{
	vec3 start = Location();
	vec3 startVelocity = Velocity();
	int result = reachFlags | 1;
	bool success = false;
	float thresholdSq = threshold * threshold;
	float moveSize = bCanJump() ? std::max(CollisionRadius(), 128.0f) : CollisionRadius();
	float moveSizeSq = moveSize * moveSize;
	float heightCheck = goalActor ? std::max(CollisionHeight(), goalActor->CollisionHeight()) : CollisionHeight();
	CollisionHit hit;

	int ticks = 100;
	int stillMoving = 1;
	while (stillMoving == 1)
	{
		vec3 direction = dest - Location();
		float dz = direction.z;
		direction.z = 0.0f;
		float acrossSq = direction.x * direction.x + direction.y * direction.y;
		if (!(dz <= heightCheck || (dz - heightCheck) * (dz - heightCheck) * 0.8f <= acrossSq))
		{
			stillMoving = 0;
			break;
		}

		if (acrossSq <= thresholdSq)
		{
			stillMoving = 0;
			if (std::abs(dz) < heightCheck)
			{
				success = true;
			}
			else if (hit.Normal.z < 0.95f && hit.Normal.z > 0.7f)
			{
				float slope = (float)std::sqrt(1.0 / ((double)hit.Normal.z * hit.Normal.z) - 1.0);
				float goalRadius = goalActor ? goalActor->CollisionRadius() : 46.0f;
				if (dz < 0.0f && CollisionRadius() * slope + CollisionHeight() > -dz)
					success = true;
				else if (CollisionRadius() < goalRadius && (goalRadius + 15.0f - CollisionRadius()) * slope + heightCheck > dz)
					success = true;
			}
		}
		else
		{
			vec3 delta = (acrossSq >= moveSizeSq) ? SafeNormal(direction) * moveSize : direction;
			stillMoving = DeusExWalkMove(delta, hit, goalActor, MinMoveThreshold, false);
			if (stillMoving != 1)
			{
				if (stillMoving == 5)
				{
					stillMoving = 0;
					success = true;
				}
				else if (Region().ZoneNumber != 0)
				{
					if (bCanFly())
					{
						stillMoving = 0;
						result = DeusExFlyReachable(dest, threshold, result, goalActor);
						success = result != 0;
					}
					else if (bCanJump())
					{
						result |= 8;
						if (stillMoving == -1)
						{
							vec3 landing;
							stillMoving = FindBestJump(dest, SafeNormal(direction) * GroundSpeed(), landing, true) ? 1 : 0;
						}
					}
					else if (stillMoving == -1 && moveSize > MaxStepHeight())
					{
						stillMoving = 1;
						moveSize = MaxStepHeight();
					}
				}
				else
				{
					stillMoving = 0;
					success = false;
				}
			}

			if (PainFor(this, FootRegion().Zone))
			{
				stillMoving = 0;
				success = false;
			}
			UZoneInfo* zone = Region().Zone;
			if (Water(zone))
			{
				stillMoving = 0;
				if (bCanSwim() && !PainFor(this, zone))
				{
					result = DeusExSwimReachable(dest, threshold, result, goalActor);
					success = result != 0;
				}
			}
		}
		if (--ticks < 0)
			stillMoving = 0;
	}

	if (!success && goalActor)
	{
		if (UWarpZoneMarker* marker = UObject::TryCast<UWarpZoneMarker>(goalActor))
			success = Region().Zone == (UZoneInfo*)marker->markedWarpZone();
	}
	TestMoveTo(start, true);
	Velocity() = startVelocity;
	return success ? result : 0;
}

// APawn::flyReachable (Engine.dll 0x103c1190): flyMoves toward the
// destination, max(radius, 200) at a time, at most 100, until within the
// threshold and its height; water swum by a swimmer.
int UPawn::DeusExFlyReachable(vec3 dest, float threshold, int reachFlags, UActor* goalActor)
{
	vec3 start = Location();
	vec3 startVelocity = Velocity();
	int result = reachFlags | 2;
	bool success = false;
	float thresholdSq = threshold * threshold;
	float moveSize = std::max(CollisionRadius(), 200.0f);
	float moveSizeSq = moveSize * moveSize;

	int ticks = 100;
	int stillMoving = 1;
	while (stillMoving != 0)
	{
		vec3 direction = dest - Location();
		float distSq = dot(direction, direction);
		if (distSq > thresholdSq || std::abs(direction.z) > CollisionHeight())
		{
			vec3 delta = (distSq >= moveSizeSq) ? SafeNormal(direction) * moveSize : direction;
			stillMoving = DeusExFlyMove(delta, goalActor, MinMoveThreshold, false);
			if (stillMoving == 5)
			{
				stillMoving = 0;
				success = true;
			}
			else if (stillMoving != 0)
			{
				UZoneInfo* zone = Region().Zone;
				if (Water(zone))
				{
					stillMoving = 0;
					if (bCanSwim() && !PainFor(this, zone))
					{
						result = DeusExSwimReachable(dest, threshold, result, goalActor);
						success = result != 0;
					}
				}
			}
		}
		else
		{
			stillMoving = 0;
			success = true;
		}
		if (--ticks < 0)
			stillMoving = 0;
	}

	if (!success && goalActor)
	{
		if (UWarpZoneMarker* marker = UObject::TryCast<UWarpZoneMarker>(goalActor))
			success = Region().Zone == (UZoneInfo*)marker->markedWarpZone();
	}
	TestMoveTo(start, true);
	Velocity() = startVelocity;
	return success ? result : 0;
}

// APawn::swimReachable (Engine.dll 0x103c15a0): swimMoves as flyReachable
// flies; out of the water a flyer flies the rest, and a walker near enough the
// surface climbs out and goes on as a flyer would.
int UPawn::DeusExSwimReachable(vec3 dest, float threshold, int reachFlags, UActor* goalActor)
{
	vec3 start = Location();
	vec3 startVelocity = Velocity();
	int result = reachFlags | 4;
	bool success = false;
	float thresholdSq = threshold * threshold;
	float moveSize = std::max(CollisionRadius(), 200.0f);
	float moveSizeSq = moveSize * moveSize;

	int ticks = 100;
	int stillMoving = 1;
	while (stillMoving != 0)
	{
		vec3 direction = dest - Location();
		float distSq = dot(direction, direction);
		if (distSq > thresholdSq || std::abs(direction.z) > CollisionHeight())
		{
			vec3 delta = (distSq >= moveSizeSq) ? SafeNormal(direction) * moveSize : direction;
			stillMoving = DeusExSwimMove(delta, goalActor, MinMoveThreshold, false);
			if (stillMoving == 5)
			{
				success = true;
				stillMoving = 0;
			}
			UZoneInfo* zone = Region().Zone;
			if (Water(zone))
			{
				if (PainFor(this, zone))
				{
					stillMoving = 0;
					success = false;
				}
			}
			else
			{
				stillMoving = 0;
				if (bCanFly())
				{
					result = DeusExFlyReachable(dest, threshold, result, goalActor);
					success = result != 0;
				}
				else if (bCanWalk() && MaxStepHeight() + Location().z + 50.0f > dest.z)
				{
					float up = std::max(CollisionHeight() + MaxStepHeight(), dest.z - Location().z);
					if (ReachTestMove(vec3(0.0f, 0.0f, up)).Fraction == 1.0f)
					{
						success = DeusExFlyReachable(dest, threshold, result, goalActor) != 0;
						result = 1;
					}
				}
			}
		}
		else
		{
			success = true;
			stillMoving = 0;
		}
		if (--ticks < 0)
			stillMoving = 0;
	}

	if (!success && goalActor)
	{
		if (UWarpZoneMarker* marker = UObject::TryCast<UWarpZoneMarker>(goalActor))
			success = Region().Zone == (UZoneInfo*)marker->markedWarpZone();
	}
	TestMoveTo(start, true);
	Velocity() = startVelocity;
	return success ? result : 0;
}

// APawn::Reachable (Engine.dll 0x103c1000): in water swum, walking (or
// swimming) walked, flying flown; any other physics reaches nothing.
int UPawn::DeusExReachable(vec3 dest, float threshold, UActor* goalActor)
{
	if (Water(Region().Zone))
		return DeusExSwimReachable(dest, threshold, 0, goalActor);
	if (Physics() == PHYS_Walking || Physics() == PHYS_Swimming)
		return DeusExWalkReachable(dest, threshold, 0, goalActor);
	if (Physics() == PHYS_Flying)
		return DeusExFlyReachable(dest, threshold, 0, goalActor);
	return 0;
}

// APawn::pointReachable (Engine.dll 0x103c0d30): a spot within 1,000 units
// across, not in water the pawn cannot enter or a pain zone it does not
// resist, seen from its eye unless already known, fitted for the pawn's size,
// and Reachable within 15 units.
bool UPawn::DeusExPointReachable(vec3 aPoint, bool knowVisible)
{
	vec3 d = aPoint - Location();
	if (d.x * d.x + d.y * d.y > 1000000.0f)
		return false;

	UZoneInfo* pointZone = XLevel()->Model->FindRegion(aPoint, Level()).Zone;
	if (!(Water(Region().Zone) || bCanSwim()) && Water(pointZone))
		return false;
	if (!(FootRegion().Zone && FootRegion().Zone->bPainZone()) && PainFor(this, pointZone))
		return false;
	if (!knowVisible)
	{
		vec3 eye = Location();
		eye.z += BaseEyeHeight();
		if (!FastTrace(aPoint, eye))
			return false;
	}

	vec3 standing = Location();
	vec3 dest = aPoint;
	if (TestMoveTo(aPoint, false))
	{
		dest = Location();
		TestMoveTo(standing, true);
	}
	return DeusExReachable(dest, 15.0f, nullptr) != 0;
}

// APawn::actorReachable (Engine.dll 0x103c0630): a pawn within reach of a
// blow is reached; else Reachable where it stands -- a pawn to within that
// reach (or, over 800 units off, to 800 units short of it), an inventory item
// or a trigger to where the two touch, anything else within 15 units --
// after the zone checks (no pain the pawn does not resist, no water it cannot
// swim; nothing that is not a pawn over 800 units off) and a line to it from
// the eye that only it may stop, unless that is known.
bool UPawn::DeusExActorReachable(UActor* other, bool knowVisible)
{
	if (!other)
		return false;

	vec3 dir = other->Location() - Location();
	float distSq = dot(dir, dir);
	UPawn* otherPawn = UObject::TryCast<UPawn>(other);
	if (!otherPawn)
	{
		if (distSq > 640000.0f || PainFor(this, other->Region().Zone))
			return false;
	}
	else if (PainFor(this, otherPawn->FootRegion().Zone))
	{
		return false;
	}
	if (Water(other->Region().Zone) && !bCanSwim())
		return false;

	if (!knowVisible)
	{
		vec3 eye = Location();
		eye.z += BaseEyeHeight();
		TraceFlags flags;
		flags.movers = true;
		flags.world = true;
		CollisionHit hit = XLevel()->Collision.TraceFirstHit(eye, other->Location(), this, vec3(0.0f), flags);
		if (hit.Fraction != 1.0f && hit.Actor != other)
			return false;
	}

	float threshold = 15.0f;
	UActor* goal = nullptr;
	if (otherPawn)
	{
		float melee = std::min(CollisionRadius() * 1.5f, MeleeRange());
		float reach = melee + other->CollisionRadius() + CollisionRadius();
		if (distSq <= reach * reach)
			return true;
		threshold = reach;
		if (distSq > 640000.0f)
			threshold = std::max(reach, (float)std::sqrt(distSq) - 800.0f);
		goal = other;
	}
	else
	{
		if (other->IsA("Inventory") || other->IsA("Trigger"))
			threshold = other->CollisionRadius() + CollisionRadius() - 2.0f;
		if (other->bBlockActors() || other->IsA("WarpZoneMarker"))
			goal = other;
	}

	vec3 standing = Location();
	vec3 dest = other->Location();
	if (TestMoveTo(dest, false))
	{
		dest = Location();
		TestMoveTo(standing, true);
	}
	return DeusExReachable(dest, threshold, goal) != 0;
}

// APawn::AIDirectionReachable (Engine.dll 0x103c78a0): steps of the collision
// radius held between 5 and 25 units along the direction -- walkMoves along
// the yaw alone when walking, flyMoves or swimMoves along yaw and pitch --
// at most 100, a fall retried in steps of MaxStepHeight; it stops in the void,
// a pain zone the pawn does not resist, and on entering water (leaving it,
// swimming). In range it goes on, keeping the farthest spot; it stops leaving
// the range, coming into it from beyond, or crossing it in one step, which
// counts as found. The pawn and its velocity are put back.
bool UPawn::DeusExAIDirectionReachable(const vec3& focus, int yaw, int pitch, float minDist, float maxDist, vec3& bestDest)
{
	vec3 focusPoint = focus;
	vec3 start = Location();
	vec3 startVelocity = Velocity();
	float minSq = minDist * minDist;
	float maxSq = maxDist * maxDist;
	float step = std::clamp(CollisionRadius(), 5.0f, 25.0f);
	bestDest = start;
	bool found = false;

	vec3 d = start - focusPoint;
	int prevClass = ClassifyDistanceSq(minSq, dot(d, d), maxSq);

	enum class Mode { Walk, Fly, Swim };
	Mode mode;
	if (Water(Region().Zone))
		mode = Mode::Swim;
	else if (Physics() == PHYS_Walking || Physics() == PHYS_Swimming)
		mode = Mode::Walk;
	else if (Physics() == PHYS_Flying)
		mode = Mode::Fly;
	else
		return false;
	vec3 dir = Coords::Rotation(Rotator(mode == Mode::Walk ? 0 : pitch, yaw, 0)).XAxis;

	CollisionHit hit;
	int ticks = 100;
	int stillMoving = 1;
	while (stillMoving == 1)
	{
		vec3 delta = dir * step;
		if (mode == Mode::Walk)
			stillMoving = DeusExWalkMove(delta, hit, nullptr, MinMoveThreshold, false);
		else if (mode == Mode::Fly)
			stillMoving = DeusExFlyMove(delta, nullptr, MinMoveThreshold, false);
		else
			stillMoving = DeusExSwimMove(delta, nullptr, MinMoveThreshold, false);

		if (stillMoving != 1)
		{
			if (Region().ZoneNumber == 0)
				stillMoving = 0;
			else if (mode == Mode::Walk && stillMoving == -1 && step > MaxStepHeight())
			{
				stillMoving = 1;
				step = MaxStepHeight();
			}
		}
		if (PainFor(this, FootRegion().Zone))
			stillMoving = 0;
		if ((mode == Mode::Swim) != Water(Region().Zone))
			stillMoving = 0;

		if (--ticks < 0)
		{
			stillMoving = 0;
			break;
		}
		if (stillMoving != 1)
			break;

		d = Location() - focusPoint;
		int cls = ClassifyDistanceSq(minSq, dot(d, d), maxSq);
		if (cls != 0)
		{
			if (prevClass != cls)
			{
				if (prevClass != 0)
				{
					found = true;
					bestDest = Location();
				}
				stillMoving = 0;
			}
			prevClass = cls;
		}
		else
		{
			found = true;
			bestDest = Location();
			if (prevClass > 0)
				stillMoving = 0;
			prevClass = 0;
		}
	}

	TestMoveTo(start, true);
	Velocity() = startVelocity;
	return found;
}
