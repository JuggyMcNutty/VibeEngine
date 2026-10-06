
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
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_9("Pawn", "AIPickRandomDestination", &NPawn::AIPickRandomDestination, 709);
	else
		RegisterVMNativeFunc_10("Pawn", "AIPickRandomDestination", &NPawn::AIPickRandomDestination_Deus, 709);
	RegisterVMNativeFunc_2("Pawn", "CanSee", &NPawn::CanSee, 533);
	RegisterVMNativeFunc_3("Pawn", "CheckValidSkinPackage", &NPawn::CheckValidSkinPackage, 0);
	RegisterVMNativeFunc_0("Pawn", "ClearPaths", &NPawn::ClearPaths, 522);
	RegisterVMNativeFunc_5("Pawn", "ClientHearSound", &NPawn::ClientHearSound, 0);
	RegisterVMNativeFunc_1("Pawn", "EAdjustJump", &NPawn::EAdjustJump, 523);
	RegisterVMNativeFunc_3("Pawn", "FindBestInventoryPath", &NPawn::FindBestInventoryPath, 540);
	RegisterVMNativeFunc_4("Pawn", "FindPathTo", &NPawn::FindPathTo, 518);
	RegisterVMNativeFunc_4("Pawn", "FindPathToward", &NPawn::FindPathToward, 517);
	RegisterVMNativeFunc_2("Pawn", "FindRandomDest", &NPawn::FindRandomDest, 525);
	RegisterVMNativeFunc_2("Pawn", "FindStairRotation", &NPawn::FindStairRotation, 524);
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "LineOfSightTo", &NPawn::LineOfSightTo, 514);
	else
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
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "StrafeFacing", &NPawn::StrafeFacing, 506);
	else
		RegisterVMNativeFunc_3("Pawn", "StrafeFacing", &NPawn::StrafeFacing_Deus, 506);
	RegisterLatentAction(507, LatentRunState::StrafeFacing);
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "StrafeTo", &NPawn::StrafeTo, 504);
	else
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

	if (engine->LaunchInfo.IsDeusEx())
	{
		RegisterVMNativeFunc_4("Pawn", "AICanHear", &NPawn::AICanHear, 706);
		RegisterVMNativeFunc_7("Pawn", "AICanSee", &NPawn::AICanSee, 705);
		RegisterVMNativeFunc_3("Pawn", "AICanSmell", &NPawn::AICanSmell, 707);
		RegisterVMNativeFunc_7("Pawn", "AIDirectionReachable", &NPawn::AIDirectionReachable, 708);
		RegisterVMNativeFunc_1("Pawn", "ComputePathnodeDistances", &NPawn::ComputePathnodeDistances, 1020);
		RegisterVMNativeFunc_5("Pawn", "ReachablePathnodes", &NPawn::ReachablePathnodes, 1004);
	}
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
	if (engine->LaunchInfo.IsDeusEx())
	{
		// The original's findPathTo, which clears the nodes itself as it
		// gathers them (engine-dll.md, the search).
		ReturnValue = selfPawn->DeusExFindPathFromScript(nullptr, aPoint, bSinglePath.value_or(false), bClearPaths.value_or(true));
		return;
	}
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindPathTo(aPoint, bSinglePath ? *bSinglePath : false);
}

void NPawn::FindPathToward(UObject* Self, UObject* anActor, std::optional<bool> bSinglePath, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (engine->LaunchInfo.IsDeusEx())
	{
		// The original's findPathToward; no goal, no path and nothing else.
		UActor* goal = UObject::Cast<UActor>(anActor);
		ReturnValue = goal ? selfPawn->DeusExFindPathFromScript(goal, vec3(0.0f), bSinglePath.value_or(false), bClearPaths.value_or(true)) : nullptr;
		return;
	}
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindPathToward(anActor, bSinglePath ? *bSinglePath : false);
}

void NPawn::FindRandomDest(UObject* Self, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindRandomDest();
}

// Deus Ex's Look Up Stairs: the original (UE1's own, engine-dll.md, Small)
// probes the floor ahead at eye height with a frame of 0.33 s or less and
// eases the view pitch toward looking down (-5000) or up (5400) a flight of
// stairs, or back to level. The probe distances and the easing rate are the
// fork's reading of that; the targets and the frame gate are the original's.
void NPawn::FindStairRotation(UObject* Self, float DeltaTime, int& ReturnValue)
{
	UPawn* pawn = UObject::Cast<UPawn>(Self);

	int pitch = pawn->ViewRotation().Pitch & 0xffff;
	if (pitch > 0x8000)
		pitch -= 0x10000;
	ReturnValue = pitch;

	if (DeltaTime > 0.33f)
		return;
	ULevel* level = pawn->XLevel();
	if (!level || !level->Model)
		return;

	auto blocked = [&](const vec3& from, const vec3& to) -> bool
	{
		dvec3 origin = to_dvec3(from);
		dvec3 delta = to_dvec3(to) - origin;
		double len = length(delta);
		if (len < 0.01)
			return false;
		TraceRayModel tracer;
		return tracer.TraceAnyHit(level->Model, origin, 0.01, delta * (1.0 / len), len, false);
	};

	vec3 at, left, up;
	Coords::Rotation(pawn->Rotation()).GetAxes(at, left, up);
	at.z = 0.0f;
	float len = length(at);
	if (len < 0.01f)
		return;
	at *= 1.0f / len;

	vec3 eyes = pawn->Location();
	eyes.z += pawn->EyeHeight();
	float footZ = pawn->Location().z - pawn->CollisionHeight();
	const float step = 25.0f;
	vec3 probe = eyes + at * (4.0f * pawn->CollisionRadius() + 32.0f);

	int target = 0;
	if (!blocked(eyes, probe))
	{
		// The floor under the probe point: above a step up means stairs up,
		// none until a step down means stairs down, else level. A wall at
		// eye height ahead keeps the view level.
		if (blocked(probe, vec3(probe.x, probe.y, footZ + step)))
			target = 5400;
		else if (!blocked(vec3(probe.x, probe.y, footZ + step), vec3(probe.x, probe.y, footZ - step)))
			target = -5000;
	}

	ReturnValue = pitch + (int)((target - pitch) * std::min(5.0f * DeltaTime, 1.0f));
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
