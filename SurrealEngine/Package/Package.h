#pragma once

#include "GC/GC.h"
#include "PackageFlags.h"
#include "PackageTables.h"
#include "ObjectFlags.h"
#include "NameString.h"
#include <functional>

class PackageManager;
class PackageStream;
class ObjectStream;
class UObject;
class UClass;

class Package : public GCObject
{
public:
	Package(PackageManager* packageManager, const NameString& name, const std::string& filepath);
	~Package();

	UObject* NewObject(const NameString& objname, UClass* objclass, ObjectFlags flags, bool initProperties = true);

	// Makes a runtime-made class findable in this package, so a save's
	// import of it resolves on load (the AI event manager's has no script).
	void AddRuntimeExport(UObject* obj);

	UObject* GetUObject(int objref);
	UObject* GetUObject(const NameString& className, const NameString& objectName) { return GetUObject(className, objectName, {}, true); }
	UObject* GetUObject(const NameString& className, const NameString& objectName, const NameString& group, bool ignoreGroup = false);

	UClass* GetClass(const NameString& className);

	void LoadAll();
	void Save(UObject* object = nullptr, const std::string& filename = {});

	const NameString& GetName(int index) const;
	int GetVersion() const { return Version; }
	NameString GetPackageName() const { return Name; }
	std::string GetPackageFileName() const { return FileName; }
	std::string GetPackageFilePath() const { return FilePath; }
	std::string GetPackageFileExtension() const { return FileExtension; }

	PackageManager* GetPackageManager() { return Packages; }

	ExportTableEntry* GetExportEntry(int objref);
	ImportTableEntry* GetImportEntry(int objref);
	int FindObjectReference(const NameString& className, const NameString& objectName, const NameString& group, bool ignoreGroup = false);

	std::string GetExportName(int objref);

	// What the network's package map needs: the file's GUID ("%08X" of its
	// four little-endian dwords, as USES lines write it), its generations'
	// counts, and whether an object is a given export of it.
	struct Generation { int ExportCount = 0; int NameCount = 0; };
	std::string GetGuidString() const;
	const Array<Generation>& GetGenerations() const { return Generations; }
	// The file's own counts: a runtime export (AddRuntimeExport) is not the
	// other side's.
	int GetExportCount() const { return FileExportCount; }
	int GetNameCount() const { return FileNameCount; }
	bool IsExportObject(const UObject* obj, int index) const { return index >= 0 && (size_t)index < ExportObjects.size() && ExportObjects[index] == obj; }
	PackageFlags GetFlags() const { return Flags; }
	// The packages this one imports from, in its import table's order.
	Array<NameString> GetImportedPackages() const;

	template<class T> Array<T*> GetAllObjects();

	std::string GCClassName() const override { return "Package"; }
	std::string GCDescribe() const override { return "Package " + Name.ToString(); }

protected:
	// Nothing: a package reached through its objects keeps its file and
	// tables, not its other objects. What keeps those is the package
	// manager's roots (PackageManager::MarkRoots).
	void Mark(GCMarker& marker) override {}

private:

	// A savegame, by its extension: its exports are made whatever their
	// flags.
	bool IsSaveFile() const;

	void ReadTables();
	std::unique_ptr<ObjectStream> OpenObjectStream(int index, const NameString& name, UClass* base);
	void LoadExportObject(int index);

	PackageManager* Packages = nullptr;
	NameString Name;
	std::string FilePath;
	std::string FileName;
	std::string FileExtension;

	int Version = 0;
	int LicenseeMode = 0;
	PackageFlags Flags = PackageFlags::NoFlags;
	Array<NameTableEntry> NameTable;
	Array<ExportTableEntry> ExportTable;
	Array<ImportTableEntry> ImportTable;
	uint8_t Guid[16] = {};
	Array<Generation> Generations;
	int FileExportCount = 0;
	int FileNameCount = 0;

	std::map<NameString, int> NameHash;

	Array<UObject*> ExportObjects;

	Package(const Package&) = delete;
	Package& operator=(const Package&) = delete;

	friend class PackageManager;
	friend class UObject;
	friend class PackageWriter;
};
