
#include "Precomp.h"
#include "NPawn.h"
#include "VM/NativeFunc.h"
#include "VM/Frame.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/USound.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Engine.h"

void NPawn::RegisterFunctions()
{
	RegisterVMNativeFunc_0("Pawn", "AddPawn", &NPawn::AddPawn, 529);
	RegisterVMNativeFunc_10("Pawn", "AIPickRandomDestination", &NPawn::AIPickRandomDestination_Deus, 709);
	RegisterVMNativeFunc_2("Pawn", "CanSee", &NPawn::CanSee, 533);
	RegisterVMNativeFunc_3("Pawn", "CheckValidSkinPackage", &NPawn::CheckValidSkinPackage, 0);
	RegisterVMNativeFunc_0("Pawn", "ClearPaths", &NPawn::ClearPaths, 522);
	RegisterVMNativeFunc_5("Pawn", "ClientHearSound", &NPawn::ClientHearSound_Deus, 0);
	RegisterVMNativeFunc_1("Pawn", "EAdjustJump", &NPawn::EAdjustJump, 523);
	RegisterVMNativeFunc_3("Pawn", "FindBestInventoryPath", &NPawn::FindBestInventoryPath, 540);
	RegisterVMNativeFunc_4("Pawn", "FindPathTo", &NPawn::FindPathTo, 518);
	RegisterVMNativeFunc_4("Pawn", "FindPathToward", &NPawn::FindPathToward, 517);
	RegisterVMNativeFunc_2("Pawn", "FindRandomDest", &NPawn::FindRandomDest, 525);
	RegisterVMNativeFunc_2("Pawn", "FindStairRotation", &NPawn::FindStairRotation, 524);
	RegisterVMNativeFunc_3("Pawn", "LineOfSightTo", &NPawn::LineOfSightTo_Deus, 514);
	RegisterVMNativeFunc_2("Pawn", "MoveTo", &NPawn::MoveTo, 500);
	RegisterLatentAction(501, LatentRunState::MoveTo);
	RegisterVMNativeFunc_2("Pawn", "MoveToward", &NPawn::MoveToward, 502);
	RegisterLatentAction(503, LatentRunState::MoveToward);
	RegisterVMNativeFunc_5("Pawn", "PickAnyTarget", &NPawn::PickAnyTarget, 534);
	RegisterVMNativeFunc_5("Pawn", "PickTarget", &NPawn::PickTarget, 531);
	RegisterVMNativeFunc_1("Pawn", "PickWallAdjust", &NPawn::PickWallAdjust, 526);
	RegisterVMNativeFunc_0("Pawn", "RemovePawn", &NPawn::RemovePawn, 530);
	RegisterVMNativeFunc_0("Pawn", "StopWaiting", &NPawn::StopWaiting, 0);
	RegisterVMNativeFunc_3("Pawn", "StrafeFacing", &NPawn::StrafeFacing_Deus, 506);
	RegisterLatentAction(507, LatentRunState::StrafeFacing);
	RegisterVMNativeFunc_3("Pawn", "StrafeTo", &NPawn::StrafeTo_Deus, 504);
	RegisterLatentAction(505, LatentRunState::StrafeTo);
	RegisterVMNativeFunc_1("Pawn", "TurnTo", &NPawn::TurnTo, 508);
	RegisterLatentAction(509, LatentRunState::TurnTo);
	RegisterVMNativeFunc_1("Pawn", "TurnToward", &NPawn::TurnToward, 510);
	RegisterLatentAction(511, LatentRunState::TurnToward);
	RegisterVMNativeFunc_0("Pawn", "WaitForLanding", &NPawn::WaitForLanding, 527);
	RegisterLatentAction(528, LatentRunState::WaitForLanding);
	RegisterVMNativeFunc_2("Pawn", "actorReachable", &NPawn::actorReachable, 520);
	RegisterVMNativeFunc_2("Pawn", "pointReachable", &NPawn::pointReachable, 521);

	RegisterVMNativeFunc_4("Pawn", "AICanHear", &NPawn::AICanHear, 706);
	RegisterVMNativeFunc_7("Pawn", "AICanSee", &NPawn::AICanSee, 705);
	RegisterVMNativeFunc_3("Pawn", "AICanSmell", &NPawn::AICanSmell, 707);
	RegisterVMNativeFunc_7("Pawn", "AIDirectionReachable", &NPawn::AIDirectionReachable, 708);
	RegisterVMNativeFunc_1("Pawn", "ComputePathnodeDistances", &NPawn::ComputePathnodeDistances, 1020);
	RegisterVMNativeFunc_5("Pawn", "ReachablePathnodes", &NPawn::ReachablePathnodes, 1004);
}

void NPawn::AddPawn(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->nextPawn() = SelfPawn->Level()->PawnList();
	SelfPawn->Level()->PawnList() = SelfPawn;
}

void NPawn::CanSee(UObject* Self, UObject* Other, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->CanSee(otherActor);
}

void NPawn::CheckValidSkinPackage(const std::string& SkinPack, const std::string& MeshName, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Pawn.CheckValidSkinPackage");
	ReturnValue = false;
}

void NPawn::ClearPaths(UObject* Self)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	selfPawn->ClearPaths();
}

void NPawn::ClientHearSound(UObject* Self, UObject* Actor, int Id, UObject* S, const vec3& SoundLocation, const vec3& Parameters)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	UActor* AActor = UObject::Cast<UActor>(Actor);
	USound* Sound = UObject::Cast<USound>(S);

	SelfPawn->ClientHearSound(AActor, Id, Sound, SoundLocation, Parameters);
}

void NPawn::ClientHearSound_Deus(UObject* Self, UObject* Actor, int Id, UObject* S, const vec3& SoundLocation, const vec3& Parameters)
{
	UObject::Cast<UPawn>(Self)->DeusExClientHearSound(UObject::Cast<UActor>(Actor), Id, UObject::Cast<USound>(S), SoundLocation, Parameters);
}

void NPawn::EAdjustJump(UObject* Self, vec3& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->EAdjustJump();
}

void NPawn::FindBestInventoryPath(UObject* Self, float& MinWeight, bool bPredictRespawns, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->FindBestInventoryPath(bPredictRespawns, MinWeight);
}

void NPawn::FindPathTo(UObject* Self, const vec3& aPoint, std::optional<bool> bSinglePath, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	// The original's findPathTo, which clears the nodes itself as it
	// gathers them (engine-dll.md, the search).
	ReturnValue = selfPawn->DeusExFindPathFromScript(nullptr, aPoint, bSinglePath.value_or(false), bClearPaths.value_or(true));
}

void NPawn::FindPathToward(UObject* Self, UObject* anActor, std::optional<bool> bSinglePath, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	// The original's findPathToward; no goal, no path and nothing else.
	UActor* goal = UObject::Cast<UActor>(anActor);
	ReturnValue = goal ? selfPawn->DeusExFindPathFromScript(goal, vec3(0.0f), bSinglePath.value_or(false), bClearPaths.value_or(true)) : nullptr;
}

void NPawn::FindRandomDest(UObject* Self, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindRandomDest();
}

// Deus Ex's Look Up Stairs, the original's (APawn::execFindStairRotation,
// Engine.dll 0x103bb880; dx-reverse-info/engine-dll.md, small): with a frame
// of 0.33 s or less, a box of the pawn's radius by 1 against the level and
// the movers looks 2h ahead from the eye, h its CollisionHeight plus its
// BaseEyeHeight; past 0.8h of room, down 3h from halfway: a floor under
// 0.7h - 6 down is stairs up if the floor at the far end is 10 nearer, one
// over 0.7h + 6 stairs down if a line from the feet meets nothing 0.9 of the
// way and the far end's floor is 10 farther. The pitch -- over 0x8000 made
// negative in place first -- eases toward 5400 up, -5000 down, or level (or
// its own, when it already looks that way), at 8, or 8000 over the
// difference (whole) under 1000, times the frame, at most the whole way.
static int DeusExFindStairRotation(UPawn* pawn, float deltaTime)
{
	Rotator& view = pawn->ViewRotation();
	if (deltaTime > 0.33f)
		return view.Pitch;
	if (view.Pitch > 0x8000)
		view.Pitch = (view.Pitch & 0xffff) - 0x10000;

	vec3 dir, y, z;
	Coords::TableRotation(Rotator(0, view.Yaw, view.Roll)).GetAxes(dir, y, z);
	vec3 eye = pawn->Location() + vec3(0.0f, 0.0f, pawn->BaseEyeHeight());
	float height = pawn->CollisionHeight() + pawn->BaseEyeHeight();
	vec3 slice(pawn->CollisionRadius(), pawn->CollisionRadius(), 1.0f);
	TraceFlags flags;
	flags.movers = true;
	flags.world = true;
	auto time = [&](const vec3& start, const vec3& end, const vec3& extent) {
		CollisionHit hit = pawn->XLevel()->Collision.TraceFirstHit(start, end, pawn, extent, flags);
		return hit.Actor ? hit.Fraction : 1.0f;
	};

	int stairRot = 0;
	float dist = 2.0f * height * time(eye, eye + dir * (2.0f * height), slice);
	if (dist > 0.8f * height)
	{
		float down = 3.0f * height;
		vec3 spot = eye + dir * (0.5f * dist);
		float t = time(spot, spot - vec3(0.0f, 0.0f, down), slice);
		if (t < 1.0f)
		{
			float firstDown = down * t;
			if (firstDown < 0.7f * height - 6.0f)
			{
				spot = eye + dir * dist;
				t = time(spot, spot - vec3(0.0f, 0.0f, down), slice);
				stairRot = std::max(0, view.Pitch);
				if (down * t < firstDown - 10.0f)
					stairRot = 5400;
			}
			else if (firstDown > 0.7f * height + 6.0f)
			{
				if (time(pawn->Location(), pawn->Location() + dir * (0.9f * dist), vec3(0.0f)) == 1.0f)
				{
					spot = eye + dir * dist;
					t = time(spot, spot - vec3(0.0f, 0.0f, down), slice);
					stairRot = std::min(0, view.Pitch);
					if (down * t > firstDown + 10.0f)
						stairRot = -5000;
				}
			}
		}
	}

	// In single precision, as the original's sums come out: 3000 eased 0.4
	// of the way is 1800, not 1799
	int diff = std::abs(view.Pitch - stairRot);
	if (diff > 0)
	{
		float rate = diff < 1000 ? (float)(8000 / diff) : 8.0f;
		float portion = std::min(rate * deltaTime, 1.0f);
		stairRot = (int)((1.0f - portion) * (float)view.Pitch + (float)stairRot * portion);
	}
	return stairRot;
}

void NPawn::FindStairRotation(UObject* Self, float DeltaTime, int& ReturnValue)
{
	UPawn* pawn = UObject::Cast<UPawn>(Self);
	ReturnValue = DeusExFindStairRotation(pawn, DeltaTime);
}

void NPawn::LineOfSightTo(UObject* Self, UObject* Other, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->LineOfSightTo(otherActor, false);
}

void NPawn::MoveTo(UObject* Self, const vec3& NewDestination, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->MoveTo(NewDestination, speed ? *speed : 1.0f);
}

void NPawn::MoveToward(UObject* Self, UObject* NewTarget, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->MoveToward(UObject::Cast<UActor>(NewTarget), speed ? *speed : 1.0f);
}

void NPawn::PickAnyTarget(UObject* Self, float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart, UObject*& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickAnyTarget(bestAim, bestDist, FireDir, projStart);
}

void NPawn::AIPickRandomDestination(UObject* Self, float minDist, float maxDist, int centralYaw, float yawDistribution, int centralPitch, float pitchDistribution, int tries, float multiplier, vec3& dest)  
{  
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);  
	if (!selfPawn) { dest = {}; return; }  
  
	selfPawn->ClearPaths();  
  
	Array<UNavigationPoint*> candidates;  
	for (UNavigationPoint* nav = selfPawn->Level()->NavigationPointList(); nav; nav = nav->nextNavigationPoint())  
	{  
		if (!selfPawn->ActorReachable(nav)) continue;  
		vec3 toPoint = nav->Location() - selfPawn->Location();  
		float dist = length(toPoint);  
		if (dist < minDist || dist > maxDist * multiplier) continue;  
  
		Rotator dir = Rotator::FromVector(normalize(toPoint));  
		int yawDiff = std::abs(dir.Yaw - centralYaw);  
		int pitchDiff = std::abs(dir.Pitch - centralPitch);  
		if (yawDiff > yawDistribution * 65536.0f / 360.0f) continue;  
		if (pitchDiff > pitchDistribution * 65536.0f / 360.0f) continue;  
  
		candidates.push_back(nav);  
	}  
  
	if (candidates.empty()) { dest = {}; return; }  
  
	float r = static_cast<float>(static_cast<double>(std::rand()) / RAND_MAX);
	size_t idx = static_cast<size_t>(r * candidates.size());  
	dest = candidates[idx]->Location();  
}

void NPawn::PickTarget(UObject* Self, float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart, UObject*& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickTarget(bestAim, bestDist, FireDir, projStart);
}

void NPawn::PickWallAdjust(UObject* Self, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickWallAdjust();
}

void NPawn::RemovePawn(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);

	if (SelfPawn->Level()->PawnList() == SelfPawn)
	{
		SelfPawn->Level()->PawnList() = SelfPawn->nextPawn();
		SelfPawn->nextPawn() = nullptr;
	}
	else
	{
		UPawn* prevPawn = nullptr;
		for (UPawn* cur = SelfPawn->Level()->PawnList(); cur != nullptr; cur = cur->nextPawn())
		{
			if (cur->nextPawn() == SelfPawn)
			{
				cur->nextPawn() = SelfPawn->nextPawn();
				SelfPawn->nextPawn() = nullptr;
				break;
			}
		}
	}
}

void NPawn::StopWaiting(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->SleepTimeLeft = 0.0f;
}

void NPawn::StrafeFacing(UObject* Self, const vec3& NewDestination, UObject* NewTarget)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeFacing(NewDestination, UObject::Cast<UActor>(NewTarget));
}

void NPawn::StrafeTo(UObject* Self, const vec3& NewDestination, const vec3& NewFocus)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeTo(NewDestination, NewFocus);
}

void NPawn::TurnTo(UObject* Self, const vec3& NewFocus)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->TurnTo(NewFocus);
}

void NPawn::TurnToward(UObject* Self, UObject* NewTarget)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->TurnToward(UObject::Cast<UActor>(NewTarget));
}

void NPawn::WaitForLanding(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->WaitForLanding();
}

void NPawn::actorReachable(UObject* Self, UObject* anActor, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->ActorReachable(UObject::Cast<UActor>(anActor), true);
}

void NPawn::pointReachable(UObject* Self, const vec3& aPoint, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PointReachable(aPoint);
}

void NPawn::AICanHear(UObject* Self, UObject* Other, std::optional<float> Volume, std::optional<float> Radius, float& ReturnValue)
{
	// No script calls it; the event manager does.
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->AICanHear(UObject::Cast<UActor>(Other), Volume, Radius);
}

void NPawn::AICanSee(UObject* Self, UObject* Other, std::optional<float> Visibility, std::optional<bool> bCheckVisibility, std::optional<bool> bCheckDir, std::optional<bool> bCheckCylinder, std::optional<bool> bCheckLOS, float& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->AICanSee(UObject::Cast<UActor>(Other), Visibility, bCheckVisibility, bCheckDir, bCheckCylinder, bCheckLOS);
}

void NPawn::AICanSmell(UObject* Self, UObject* Other, std::optional<float> Smell, float& ReturnValue)
{
	// The original always returns 0: nothing smells in this build.
	ReturnValue = 0.0f;
}

void NPawn::AIDirectionReachable(UObject* Self, const vec3& Focus, int Yaw, int Pitch, float minDist, float maxDist, vec3& bestDest, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->AIDirectionReachable(Focus, Yaw, Pitch, minDist, maxDist, bestDest);
}

void NPawn::AIPickRandomDestination_Deus(UObject* Self, float minDist, float maxDist, int centralYaw, float yawDistribution, int centralPitch, float pitchDistribution, int tries, float multiplier, vec3& dest, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->AIPickRandomDestination(minDist, maxDist, centralYaw, yawDistribution, centralPitch, pitchDistribution, tries, multiplier, dest);
}

void NPawn::ComputePathnodeDistances(UObject* Self, std::optional<UObject*> startActor)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	selfPawn->ComputePathnodeDistances(startActor ? UObject::Cast<UActor>(*startActor) : nullptr);
}

void NPawn::LineOfSightTo_Deus(UObject* Self, UObject* Other, std::optional<bool> bIgnoreDistance, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->LineOfSightTo(otherActor, bIgnoreDistance ? *bIgnoreDistance : false);
}

// Up to 32 navigation points and their distances, nearest first, from the
// original's GetPathnodeList; BaseClass is read and not used.
class ReachablePathnodesIterator : public Iterator
{
public:
	ReachablePathnodesIterator(Array<std::pair<UNavigationPoint*, float>> nodes, UObject** navPoint, float* distance) : Nodes(std::move(nodes)), NavPoint(navPoint), Distance(distance) {}
	bool Next() override
	{
		if (Pos >= Nodes.size())
			return false;
		*NavPoint = Nodes[Pos].first;
		*Distance = Nodes[Pos].second;
		Pos++;
		return true;
	}

	void Mark(GCMarker& marker) override
	{
		for (auto& node : Nodes)
			marker.MarkConst(node.first);
	}

private:
	Array<std::pair<UNavigationPoint*, float>> Nodes;
	size_t Pos = 0;
	UObject** NavPoint = nullptr;
	float* Distance = nullptr;
};

void NPawn::ReachablePathnodes(UObject* Self, UObject* BaseClass, UObject*& NavPoint, UObject* FromPoint, float& distance, std::optional<bool> bUsePrunedPaths)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	Frame::CreatedIterator = std::make_unique<ReachablePathnodesIterator>(selfPawn->GetPathnodeList(UObject::Cast<UActor>(FromPoint), bUsePrunedPaths.value_or(false)), &NavPoint, &distance);
}

void NPawn::StrafeFacing_Deus(UObject* Self, const vec3& NewDestination, UObject* NewTarget, std::optional<float> speed)
{
	// Deus Ex's strafe: the speed, 1 by default (the scripts give none),
	// caps an NPC's DesiredSpeed while a player keeps MaxDesiredSpeed, and
	// bReducedSpeed clears. It needs a target, faces it and looks at where
	// it is.
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	UActor* target = UObject::Cast<UActor>(NewTarget);
	if (!target)
		return;
	SelfPawn->bReducedSpeed() = false;
	SelfPawn->DesiredSpeed() = SelfPawn->bIsPlayer() ? SelfPawn->MaxDesiredSpeed() : std::clamp(SelfPawn->MaxDesiredSpeed(), 0.0f, speed.value_or(1.0f));
	SelfPawn->Focus() = target->Location();
	SelfPawn->StrafeFacing(NewDestination, target);
}

void NPawn::StrafeTo_Deus(UObject* Self, const vec3& NewDestination, const vec3& NewFocus, std::optional<float> speed)
{
	// Deus Ex's strafe: an NPC's DesiredSpeed is its MaxDesiredSpeed held
	// to at most the speed (1 by default: all of it, not the 0.8 the base
	// strafe gives).
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeTo(NewDestination, NewFocus);
	SelfPawn->DesiredSpeed() = SelfPawn->bIsPlayer() ? SelfPawn->MaxDesiredSpeed() : std::clamp(SelfPawn->MaxDesiredSpeed(), 0.0f, speed.value_or(1.0f));
}
