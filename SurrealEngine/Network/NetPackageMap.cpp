
#include "Precomp.h"
#include "NetPackageMap.h"
#include "NetConnection.h"
#include "NetChannel.h"
#include "Package/Package.h"
#include "Packages/Core/UClass.h"
#include "GC/GC.h"
#include "Packages/Core/UObject.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Engine/Actors/UActor.h"
#include <algorithm>

void NetPackageMap::Compute()
{
	MaxObjectIndex = 0;
	MaxNameIndex = 0;
	PackageIndex.clear();
	NameIndex.clear();
	ClassCaches.clear();

	for (int i = 0; i < (int)List.size(); i++)
	{
		PackageInfo& info = List[i];
		const auto& generations = info.Pkg->GetGenerations();
		info.ObjectBase = MaxObjectIndex;
		info.NameBase = MaxNameIndex;
		info.ObjectCount = info.Pkg->GetExportCount();
		info.NameCount = info.Pkg->GetNameCount();
		info.LocalGeneration = (int)generations.size();
		if (info.RemoteGeneration == 0)
			info.RemoteGeneration = info.LocalGeneration;
		// An older generation on the other side counts only what it had.
		if (info.RemoteGeneration < info.LocalGeneration && info.RemoteGeneration > 0)
		{
			const auto& generation = generations[info.RemoteGeneration - 1];
			info.ObjectCount = std::min(info.ObjectCount, generation.ExportCount);
			info.NameCount = std::min(info.NameCount, generation.NameCount);
		}
		MaxObjectIndex += info.ObjectCount;
		MaxNameIndex += info.NameCount;

		for (int j = 0; j < info.NameCount; j++)
			NameIndex.emplace(info.Pkg->GetName(j), info.NameBase + j);

		PackageIndex[info.Pkg] = i;
	}
}

UObject* NetPackageMap::IndexToObject(uint32_t index)
{
	for (PackageInfo& info : List)
	{
		if (index >= (uint32_t)info.ObjectBase && index < (uint32_t)(info.ObjectBase + info.ObjectCount))
			return info.Pkg->GetUObject((int)(index - info.ObjectBase) + 1);
	}
	return nullptr;
}

int NetPackageMap::ObjectToIndex(UObject* obj)
{
	if (!obj || !obj->package)
		return -1;
	auto it = PackageIndex.find(obj->package);
	if (it == PackageIndex.end())
		return -1;
	const PackageInfo& info = List[it->second];
	int exportIndex = (int)obj->exportIndex;
	if (exportIndex >= info.ObjectCount || !info.Pkg->IsExportObject(obj, exportIndex))
		return -1;
	return info.ObjectBase + exportIndex;
}

UObject* NetPackageMap::ReadObject(NetBitReader& reader)
{
	if (reader.ReadBit())
	{
		// A dynamic actor, by the channel it has on this connection; 0 is None.
		uint32_t index = reader.ReadInt(NetConnection::MaxChannels);
		if (index == 0 || reader.IsError() || !Connection)
			return nullptr;
		NetChannel* channel = Connection->Channels[index];
		if (channel && channel->ChType == ChannelType::Actor && !channel->Closing)
			return static_cast<NetActorChannel*>(channel)->Actor;
		return nullptr;
	}
	uint32_t index = reader.ReadInt(MaxObjectIndex);
	if (reader.IsError())
		return nullptr;
	return IndexToObject(index);
}

bool NetPackageMap::WriteObject(NetBitWriter& writer, UObject* obj)
{
	UActor* actor = UObject::TryCast<UActor>(obj);
	if (actor && !actor->bStatic() && !actor->bNoDelete())
	{
		writer.WriteBit(true);
		NetActorChannel* channel = Connection ? Connection->FindActorChannel(actor) : nullptr;
		writer.WriteInt(channel ? channel->ChIndex : 0, NetConnection::MaxChannels);
		return channel && channel->OpenAcked;
	}
	int index = obj ? ObjectToIndex(obj) : -1;
	if (index != -1)
	{
		writer.WriteBit(false);
		writer.WriteInt(index, MaxObjectIndex);
		return true;
	}
	writer.WriteBit(true);
	writer.WriteInt(0, NetConnection::MaxChannels);
	return obj == nullptr;
}

bool NetPackageMap::CanSerializeObject(UObject* obj)
{
	UActor* actor = UObject::TryCast<UActor>(obj);
	if (!actor || actor->bStatic() || actor->bNoDelete())
		return true;
	return Connection && Connection->FindActorChannel(actor);
}

NameString NetPackageMap::ReadName(NetBitReader& reader)
{
	uint32_t index = reader.ReadInt(MaxNameIndex + 1);
	if (reader.IsError() || index >= MaxNameIndex)
		return NameString("None");
	for (PackageInfo& info : List)
	{
		if (index < (uint32_t)info.NameCount)
			return info.Pkg->GetName((int)index);
		index -= info.NameCount;
	}
	return NameString("None");
}

void NetPackageMap::WriteName(NetBitWriter& writer, const NameString& name)
{
	auto it = NameIndex.find(name);
	writer.WriteInt(it != NameIndex.end() ? it->second : MaxNameIndex, MaxNameIndex + 1);
}

NetPackageMap::FieldNetCache* NetPackageMap::ClassNetCache::GetFromIndex(int index)
{
	for (ClassNetCache* c = this; c; c = c->Super)
	{
		if (index >= c->FieldsBase && index < c->FieldsBase + (int)c->Fields.size())
			return &c->Fields[index - c->FieldsBase];
	}
	return nullptr;
}

NetPackageMap::FieldNetCache* NetPackageMap::ClassNetCache::GetFromField(UField* field)
{
	for (ClassNetCache* c = this; c; c = c->Super)
	{
		for (FieldNetCache& f : c->Fields)
		{
			if (f.Field == field)
				return &f;
		}
	}
	return nullptr;
}

NetPackageMap::ClassNetCache* NetPackageMap::GetClassNetCache(UClass* cls)
{
	if (!cls)
		return nullptr;
	auto it = ClassCaches.find(cls);
	if (it != ClassCaches.end())
		return it->second.get();
	if (ObjectToIndex(cls) == -1)
		return nullptr;

	auto cache = std::make_unique<ClassNetCache>();
	ClassNetCache* result = cache.get();
	ClassCaches[cls] = std::move(cache);
	result->Class = cls;

	if (UClass* super = UObject::TryCast<UClass>(cls->BaseStruct))
	{
		result->Super = GetClassNetCache(super);
		if (result->Super)
		{
			result->FieldsBase = result->Super->GetMaxIndex();
			result->RepProperties = result->Super->RepProperties;
			result->RepElements = result->Super->RepElements;
		}
	}

	// The class's own replicated fields -- its properties marked for the
	// net, and its net functions that override none -- in the order of the
	// package's exports, as both sides sort them.
	Array<UField*> netFields;
	for (UField* field = cls->Children; field; field = field->Next)
	{
		if (UProperty* prop = UObject::TryCast<UProperty>(field))
		{
			if (AllFlags(prop->PropFlags, PropertyFlags::Net))
				netFields.push_back(field);
		}
		else if (UFunction* func = UObject::TryCast<UFunction>(field))
		{
			if (AllFlags(func->FuncFlags, FunctionFlags::Net) && !func->BaseField)
				netFields.push_back(field);
		}
	}
	std::stable_sort(netFields.begin(), netFields.end(), [](UField* a, UField* b) { return a->exportIndex < b->exportIndex; });

	for (UField* field : netFields)
	{
		if (ObjectToIndex(field) != -1)
		{
			FieldNetCache f;
			f.Field = field;
			f.FieldNetIndex = result->GetMaxIndex();
			result->Fields.push_back(f);
		}
	}

	for (FieldNetCache& f : result->Fields)
	{
		if (UProperty* prop = UObject::TryCast<UProperty>(f.Field))
		{
			f.RepIndex = (int)result->RepElements.size();
			for (int i = 0; i < prop->ArrayDimension; i++)
				result->RepElements.push_back({ &f, i });
			result->RepProperties.push_back(&f);
		}
	}
	return result;
}

void NetPackageMap::Mark(GCMarker& marker)
{
	for (PackageInfo& info : List)
		marker.MarkConst(info.Pkg);
	for (auto& it : ClassCaches)
		marker.MarkConst(it.first);
}
