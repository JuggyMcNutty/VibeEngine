#pragma once

#include "GC/GC.h"
#include "Package.h"
#include "IniFile.h"
#include "GameFolder.h"
#include <list>
#include <filesystem>

namespace fs = std::filesystem;

class PackageStream;
class UObject;
class UClass;

struct IntObject
{
	NameString Name;
	NameString Class;
	NameString MetaClass;
	std::string Description;
};

struct NativeClass
{
	NameString Package;
	NameString Name;
	NameString Base;
	std::function<UObject* (const NameString& name, UClass* cls, ObjectFlags flags)> CreateFunc;
};

class PackageManager
{
public:
	PackageManager(const GameLaunchInfo& launchInfo);

	bool IsUnreal1() const { return launchInfo.IsUnreal1(); }
	bool IsUnreal1_226() const { return launchInfo.IsUnreal1_226(); }
	bool IsUnreal1_227() const { return launchInfo.IsUnreal1_227(); }
	bool IsUnreal1_227k() const { return launchInfo.IsUnreal1_227k(); }
	bool IsUnrealTournament() const { return launchInfo.IsUnrealTournament(); }
	bool IsUnrealTournament_469() const { return launchInfo.IsUnrealTournament_469(); }
	bool IsDeusEx() const { return launchInfo.IsDeusEx(); }
	bool IsCliveBarkersUndying() const { return launchInfo.IsCliveBarkersUndying(); }
	bool IsKlingonHonorGuard() const  { return launchInfo.IsKlingonHonorGuard(); }
	bool IsRune() const { return launchInfo.IsRune(); }

	fs::path GetRootFolderPath() const { return gameRootFolderPath; }
	fs::path GetSystemFolderPath() const { return gameSystemFolderPath; }
	fs::path GetSaveFolderPath() const { return gameSaveFolderPath; }
	fs::path GetCacheFolderPath() const { return gameCacheFolderPath; }

	Package* GetPackage(const NameString& name);
	Array<NameString> GetPackageNames() const;
	// Whether a package of that name is on the paths, and whether it is a map.
	bool HasPackage(const NameString& name) const { return packageFilenames.find(name) != packageFilenames.end(); }
	bool IsMapPackage(const NameString& name) const;
	void ScanSaveInfos();
	Package* GetSaveInfoPackage(const NameString& saveFolderName);
	void RemoveSaveInfoPackage(const NameString& saveFolderName);
	std::map<NameString, Package*> GetSaveInfoPackages() const { return saveInfos; };

	Package* LoadMap(const std::string& path);
	// A map from a file of any name, as the package of the name given: a
	// download kept in the cache.
	Package* LoadMapFile(const NameString& name, const std::string& path);
	void UnloadPackage(Package* package);

	// A package's file for a net game, as Core's appFindPackageFile finds
	// one: the search paths' by name, else a download kept in the cache by
	// its GUID, whose date is then brought up to now; "" for none.
	std::string FindPackageFile(const NameString& name, const std::string& guid);
	// Where a download of that GUID is kept: the cache, the GUID, CacheExt.
	std::string GetCachedPackagePath(const std::string& guid) const;
	// A package file's GUID from its header alone, as Package gives it.
	static std::string ReadPackageGuid(const std::string& path);
	bool IsPackageLoaded(const NameString& name) const;
	// A package of a net game found under its name in this file -- a download
	// in the cache --, until RestorePackageFiles.
	void UsePackageFile(const NameString& name, const std::string& path);
	// Each name's own file again, but a package loaded from its download,
	// which stays until ReleaseNetPackages lets it go.
	void RestorePackageFiles();
	// The packages loaded from downloads let go, but those named in `keep`,
	// and their names' own files back, as the original's map load collects
	// what the new level does not use: another server's package of such a
	// name then loads in their place.
	void ReleaseNetPackages(const Array<NameString>& keep);
	// The cache's downloads left unfinished, and those unused for more than
	// PurgeCacheDays (Core's appCleanFileCache).
	void CleanFileCache();

	void CloseStreams();

	Package* LoadSaveFile(const std::string& path);
	Package* LoadSaveSlot(const uint32_t slotNum);

	std::shared_ptr<PackageStream> GetStream(Package* package);

	UClass* FindClass(const NameString& name);
	// A class by its package and name, or by its name alone in any package
	// loaded, as the original's console finds one (StaticFindObject with
	// ANY_PACKAGE): the game's menus name DeusExMPGame, not DeusEx.DeusExMPGame.
	UClass* FindClassAnyPackage(const NameString& name);

	std::string GetMapExtension() const { return mapExtension; }
	std::string GetSaveExtension() const { return saveExtension; }

	std::unique_ptr<IniFile> GetIniFile(NameString iniName);
	std::unique_ptr<IniFile> GetUserIniFile();
	std::unique_ptr<IniFile> GetSystemIniFile();
	Array<NameString> GetIniKeysFromSection(NameString iniName, const NameString& sectionName);
	std::string GetIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, std::string default_value = "", const int index = 0);
	bool FindIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, std::string& value);
	Array<std::string> GetIniValues(NameString iniName, const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	std::string GetDefaultIniValue(const NameString& sectionName, const NameString& keyName, std::string default_value = "", const int index = 0);
	Array<std::string> GetDefaultIniValues(const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	std::string GetDefUserIniValue(const NameString& sectionName, const NameString& keyName, std::string default_value = "", const int index = 0);
	Array<std::string> GetDefUserIniValues(const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	void SetIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, const std::string& newValue, const int index = 0);
	void SetIniValues(NameString iniName, const NameString& sectionName, const NameString& keyName, const Array<std::string>& newValues);
	void SaveAllIniFiles();

	std::string GetVideoFilename(const std::string& name);

	std::string Localize(NameString packageName, const NameString& sectionName, const NameString& keyName, const int index = 0);

	Array<IntObject>& GetIntObjects(const NameString& metaclass);
	const Array<std::string>& GetMaps() const { return maps; }

	bool MissingSESystemIni() const { return missing_se_system_ini; }

	Package* GetTransientPackage() { return GetPackage("Transient"); }
	Package* CreateEmptyPackage(const NameString& name);

private:
	void CreateTransientPackage();

	std::unique_ptr<IniFile>& LoadIniFile(NameString iniName);
	std::unique_ptr<IniFile>& LoadUserIniFile();
	std::unique_ptr<IniFile>& LoadSystemIniFile();
	void LoadEngineIniFiles();
	void LoadFileExtensions();
	void LoadIntFiles();
	void LoadPackageRemaps();
	std::map<NameString, std::string> ParseIntPublicValue(const std::string& value);

	void ScanForMaps();

	void ScanFolder(const std::string& packagedir, const std::string& search);
	void ScanPaths();

	void DelayLoadNow();
	void RegisterFunctions();

	void RegisterNativeClasses();

	template<typename T>
	void RegisterNativeClass(const NameString& packageName, const NameString& className, const NameString& baseClass = {});

	std::map<NameString, NativeClass> NativeClasses;

	Array<UObject*> delayLoads;
	int delayLoadActive = 0;

	std::map<NameString, std::string> packageFilenames;
	// What UsePackageFile replaced: a name's file before, "" for none.
	std::map<NameString, std::string> replacedPackageFilenames;
	std::map<NameString, GCRoot<Package>> packages;
	std::map<NameString, std::unique_ptr<IniFile>> iniFiles;
	std::map<NameString, std::unique_ptr<IniFile>> intFiles;
	std::map<std::string, std::string> packageRemaps;
	std::map<NameString, Package*> saveInfos;

	std::map<NameString, std::string> aviFilenames;

	std::map<NameString, Array<IntObject>> IntObjects;

	std::unique_ptr<IniFile> defaultIniFile;  // Holds Default.ini
	std::unique_ptr<IniFile> defaultUserFile; // Holds DefUser.ini

	Array<std::string> mapFolders;
	Array<std::string> maps;

	// So that we don't have to build fs::path()s all over the place
	fs::path gameRootFolderPath;
	fs::path gameSystemFolderPath;
	fs::path gameSaveFolderPath;
	fs::path gameCacheFolderPath;

	std::string mapExtension;
	std::string saveExtension;
	std::string languageExtension;

	bool missing_se_system_ini = false;

	struct OpenStream
	{
		Package* Pkg = nullptr;
		std::shared_ptr<PackageStream> Stream;
	};

	std::list<OpenStream> openStreams;

	GameLaunchInfo launchInfo;

	friend class Package;
	friend struct SetDelayLoadActive;
};

struct SetDelayLoadActive
{
	SetDelayLoadActive(PackageManager* p) : p(p) { p->delayLoadActive++; }
	~SetDelayLoadActive() { p->delayLoadActive--; }
	PackageManager* p;
};
