#pragma once

#include "NetBits.h"
#include "Utils/Array.h"
#include "Package/NameString.h"
#include <map>
#include <memory>
#include <string>

class Package;
class UObject;
class UClass;
class UField;
class UProperty;
class NetConnection;

// Objects and names by number, between two machines that load the same
// packages in the same order (docs/re/network.md): the packages a server's
// USES lines list, each its objects (exports) and names numbered after the
// last's; and per class, its replicated fields numbered after its parent's
// (Core's UPackageMap, Engine's UPackageMapLevel).
class NetPackageMap
{
public:
	struct PackageInfo
	{
		Package* Pkg = nullptr;
		NameString Name;
		std::string Guid;
		uint32_t Flags = 0;
		int FileSize = 0;
		int RemoteGeneration = 0;
		int LocalGeneration = 0;
		int ObjectBase = 0;
		int ObjectCount = 0;
		int NameBase = 0;
		int NameCount = 0;
	};

	struct FieldNetCache
	{
		UField* Field = nullptr;
		int FieldNetIndex = 0;
	};

	struct ClassNetCache
	{
		ClassNetCache* Super = nullptr;
		UClass* Class = nullptr;
		int FieldsBase = 0;
		Array<FieldNetCache> Fields;

		int GetMaxIndex() const { return FieldsBase + (int)Fields.size(); }
		FieldNetCache* GetFromIndex(int index);
		FieldNetCache* GetFromField(UField* field);
	};

	Array<PackageInfo> List;

	void Compute();

	UObject* IndexToObject(uint32_t index);
	int ObjectToIndex(UObject* obj);
	uint32_t GetMaxObjectIndex() const { return MaxObjectIndex; }

	// Loading and saving an object reference: a dynamic actor by its
	// channel on the connection, anything else by its number.
	UObject* ReadObject(NetBitReader& reader);
	bool WriteObject(NetBitWriter& writer, UObject* obj);

	NameString ReadName(NetBitReader& reader);
	void WriteName(NetBitWriter& writer, const NameString& name);

	ClassNetCache* GetClassNetCache(UClass* cls);

	NetConnection* Connection = nullptr;

private:
	uint32_t MaxObjectIndex = 0;
	uint32_t MaxNameIndex = 0;
	std::map<Package*, int> PackageIndex;
	std::map<NameString, int> NameIndex;
	std::map<UClass*, std::unique_ptr<ClassNetCache>> ClassCaches;
};
