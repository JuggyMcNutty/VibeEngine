
#include "Precomp.h"
#include "NetRemote.h"
#include "NetChannel.h"
#include "NetDriver.h"
#include "NetSerialize.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Utils/Logger.h"
#include "VM/Bytecode.h"
#include "VM/ExpressionEvaluator.h"
#include "VM/Frame.h"
#include "Engine.h"

bool NetReplicationCondition(UFunction* rootFunction, UActor* actor)
{
	UClass* cls = UObject::TryCast<UClass>(rootFunction->Outer());
	if (!cls || !cls->Code)
		return false;
	int index = cls->Code->FindStatementIndex(rootFunction->ReplicationOffset);
	if (index < 0)
		return false;
	ExpressionEvalResult result = ExpressionEvaluator::Eval(cls->Code->Statements[index], actor, actor, nullptr);
	return result.Value.GetType() != ExpressionValueType::Nothing && result.Value.ToBool();
}

bool NetProcessRemoteFunction(UFunction* function, UObject* instance, CallArguments& args)
{
	UActor* actor = UObject::TryCast<UActor>(instance);
	if (!actor || AnyFlags(function->FuncFlags, FunctionFlags::Static) || actor->bDeleteMe())
		return false;

	// A simulated proxy runs only the functions marked simulated.
	bool absorb = actor->Role() <= ROLE_SimulatedProxy && !AnyFlags(function->FuncFlags, FunctionFlags::Simulated);

	if (actor->Level()->NetMode() == NM_Standalone)
		return false;
	if (!AnyFlags(function->FuncFlags, FunctionFlags::Net))
		return absorb;

	// A server's call would go to the client whose player owns the actor;
	// the fork is no server yet, and a client's own actor runs its call.
	if (actor->Role() == ROLE_Authority)
		return absorb;

	// The condition is the first declaration's.
	UFunction* root = function;
	while (UFunction* super = UObject::TryCast<UFunction>(root->BaseField))
		root = super;
	if (!NetReplicationCondition(root, actor))
		return absorb;

	NetConnection* connection = engine->LevelNetDriver ? engine->LevelNetDriver->ServerConnection.get() : nullptr;
	if (!connection)
		return absorb;

	// An unreliable call is dropped when the connection has no room.
	bool reliable = AnyFlags(function->FuncFlags, FunctionFlags::NetReliable);
	if (!reliable && !connection->IsNetReady(false))
		return true;

	NetPackageMap& map = connection->PackageMap;
	NetPackageMap::ClassNetCache* cache = map.GetClassNetCache(actor->Class);
	NetPackageMap::FieldNetCache* field = cache ? cache->GetFromField(root) : nullptr;
	NetActorChannel* channel = connection->FindActorChannel(actor);
	if (!field || !channel || channel->Closing)
		return true;

	NetOutBunch bunch(channel, false);
	bunch.Data.WriteInt(field->FieldNetIndex, cache->GetMaxIndex());

	// The call's parameters in a frame of the called function's; each goes as
	// the declaration's parameter: a bool its bit, any other a bit for
	// whether it is not zero, then its value.
	Array<UProperty*> callParms;
	for (UProperty* prop : Frame::CallParms(function))
	{
		if (prop != function->ReturnParm)
			callParms.push_back(prop);
	}
	std::unique_ptr<uint64_t[]> frame(new uint64_t[((size_t)function->StructSize + 7) / 8 + 1]());
	uint8_t* data = reinterpret_cast<uint8_t*>(frame.get());
	for (UProperty* prop : callParms)
		prop->ConstructArray(data + prop->DataOffset.DataOffset);
	for (size_t i = 0; i < callParms.size() && i < args.size(); i++)
	{
		if (args[i].GetType() != ExpressionValueType::Nothing)
			ExpressionValue::Variable(data, callParms[i]).Store(args[i]);
	}

	size_t index = 0;
	for (UField* f = root->Children; f; f = f->Next)
	{
		UProperty* prop = UObject::TryCast<UProperty>(f);
		if (!prop)
			continue;
		if ((prop->PropFlags & (PropertyFlags::Parm | PropertyFlags::ReturnParm)) != PropertyFlags::Parm)
			break;
		size_t parmIndex = index++;
		if (parmIndex >= callParms.size() || map.ObjectToIndex(prop) == -1)
			continue;

		const void* value = data + callParms[parmIndex]->DataOffset.DataOffset;
		if (UObject::TryCast<UBoolProperty>(prop))
		{
			NetWriteItem(prop, bunch.Data, map, value);
		}
		else
		{
			bool send = !NetIsZero(prop, value);
			bunch.Data.WriteBit(send);
			if (send)
				NetWriteItem(prop, bunch.Data, map, value);
		}
	}

	for (UProperty* prop : callParms)
		prop->DestructArray(data + prop->DataOffset.DataOffset);

	bunch.bReliable = reliable;
	if (!bunch.Data.IsError())
		channel->SendBunch(bunch, true);
	else
		LogMessage("Net: remote function bunch overflowed");
	return true;
}
