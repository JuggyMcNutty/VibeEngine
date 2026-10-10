
#include "Precomp.h"
#include "NDecal.h"
#include "VM/NativeFunc.h"
#include "Packages/Engine/Actors/UDecal.h"
#include "Engine.h"
#include "Packages/Engine/USurrealClient.h"

void NDecal::RegisterFunctions()
{
	if (engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_3("Decal", "AttachDecal", &NDecal::AttachDecal_Deus, 0);
	else
		RegisterVMNativeFunc_3("Decal", "AttachDecal", &NDecal::AttachDecal, 0);
	RegisterVMNativeFunc_0("Decal", "DetachDecal", &NDecal::DetachDecal, 0);
}

void NDecal::AttachDecal(UObject* Self, float TraceDistance, std::optional<vec3> DecalDir, UObject*& ReturnValue)
{
	ReturnValue = UObject::TryCast<UDecal>(Self)->AttachDecal(TraceDistance, DecalDir ? *DecalDir : vec3(0.0f));
}

// With the client's Decals off (the Display menu's Decals) the original's
// attaches none (Engine.dll ADecal::execAttachDecal, 0x103e7250): the game's
// decals then destroy themselves as they are made.
void NDecal::AttachDecal_Deus(UObject* Self, float TraceDistance, std::optional<vec3> DecalDir, BitfieldBool& ReturnValue)
{
	if (!engine->client || !engine->client->Decals)
	{
		ReturnValue = false;
		return;
	}
	UObject* Decal = UObject::TryCast<UDecal>(Self)->AttachDecal(TraceDistance, DecalDir ? *DecalDir : vec3(0.0f));
	ReturnValue = Decal != nullptr;
}

void NDecal::DetachDecal(UObject* Self)
{
	UObject::TryCast<UDecal>(Self)->DetachDecal();
}
