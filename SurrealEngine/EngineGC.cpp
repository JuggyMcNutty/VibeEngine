
#include "Precomp.h"
#include "Engine.h"
#include "GC/GC.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Engine/UClient.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/USurrealClient.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Subsystems/UGameEngine.h"
#include "Packages/Engine/Subsystems/USurrealRenderDevice.h"
#include "Packages/Engine/Subsystems/USurrealAudioDevice.h"
#include "Packages/Engine/Subsystems/USurrealNetworkDevice.h"
#include "Packages/DeusEx/UDeusExLevelInfo.h"
#include "Packages/DeusEx/UDeusExSaveInfo.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Network/NetDriver.h"
#include "Network/NetClient.h"
#include "Network/NetServer.h"
#include "VM/Frame.h"
#include "VM/NativeFunc.h"
#include "Utils/Logger.h"
#include <chrono>
#include <cstdlib>

static bool EnvSet(const char* name)
{
	const char* value = std::getenv(name);
	return value && *value && std::string(value) != "0";
}

void Engine::RequestGarbageCollection(const std::string& reason)
{
	if (GarbageRequest.empty())
		GarbageRequest = reason;
}

void Engine::MarkRoots(GCMarker& marker)
{
	// The subsystems are the engine's own: never made None.
	marker.SetRoot("subsystems");
	marker.MarkConst(gameengine);
	marker.MarkConst(renderdev);
	marker.MarkConst(audiodev);
	marker.MarkConst(netdev);
	marker.MarkConst(client);
	marker.MarkConst(viewport);
	marker.MarkConst(canvas);
	marker.MarkConst(console);
	marker.MarkConst(dxgc);

	marker.SetRoot("level");
	marker.Mark(Level);
	marker.Mark(LevelInfo);
	marker.Mark(GameInfo);
	marker.Mark(DeusExLevelInfo);
	marker.MarkConst(LevelPackage);

	marker.SetRoot("entry");
	marker.Mark(EntryLevel);
	marker.Mark(EntryLevelInfo);
	marker.Mark(EntryGameInfo);
	marker.Mark(EntryDeusExLevelInfo);
	marker.MarkConst(EntryLevelPackage);

	marker.SetRoot("engine");
	marker.Mark(dxSaveInfo);
	marker.Mark(dxRootWindow);
	marker.Mark(DefaultTexture);
	marker.MarkConst(deusExPackage);
	marker.MarkConst(floatprop);
	marker.MarkConst(objprop);
	marker.MarkConst(vecprop);
	marker.MarkConst(rotprop);

	marker.SetRoot("natives");
	for (UFunction* func : NativeFunctions::FuncByIndex)
		marker.MarkConst(func);

	if (LevelNetDriver)
		LevelNetDriver->Mark(marker);
	if (PendingLevel && PendingLevel->Driver)
		PendingLevel->Driver->Mark(marker);
	if (ServerLevel)
		ServerLevel->Mark(marker);

	packages->MarkRoots(marker);
}

namespace
{
	// Where an object lives, for the collector's reports.
	std::string PackageKind(Engine* engine, GCObject* obj)
	{
		UObject* uobj = dynamic_cast<UObject*>(obj);
		if (!uobj)
			return dynamic_cast<Package*>(obj) ? "package objects" : "other";
		Package* pkg = uobj->package;
		if (!pkg)
			return "no package";
		if (pkg == engine->LevelPackage)
			return "the level";
		if (pkg == engine->EntryLevelPackage)
			return "the Entry level";
		if (engine->packages->IsRegisteredPackage(pkg))
			return "packages";
		return "departed levels";
	}

	bool InDepartedLevel(Engine* engine, const GCObject* obj)
	{
		const UObject* uobj = dynamic_cast<const UObject*>(obj);
		return uobj && uobj->package && uobj->package != engine->LevelPackage && uobj->package != engine->EntryLevelPackage && !engine->packages->IsRegisteredPackage(uobj->package);
	}

	template<typename Map>
	void LogTop(const std::string& title, const Map& counts, size_t limit)
	{
		std::vector<std::pair<size_t, std::string>> sorted;
		for (auto& it : counts)
			sorted.push_back({ it.second, it.first });
		std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.first > b.first; });
		if (sorted.empty())
			return;
		LogMessage(title);
		for (size_t i = 0; i < sorted.size() && i < limit; i++)
			LogMessage("    " + std::to_string(sorted[i].first) + " " + sorted[i].second);
	}
}

void Engine::CollectGarbage()
{
	std::string reason = std::move(GarbageRequest);
	GarbageRequest.clear();

	bool dryRun = EnvSet("SURREAL_GC_DRYRUN");
	bool verify = EnvSet("SURREAL_GC_VERIFY");
	if (!dryRun && !verify)
		return;

	if (!Frame::Callstack.empty() || packages->HasPendingLoads())
	{
		LogMessage("GC: not collecting with script running or objects waiting to load (" + reason + ")");
		return;
	}

	LogMessage("Collecting garbage (" + reason + ")");
	auto start = std::chrono::steady_clock::now();

	std::map<std::string, size_t> dyingByKind, dyingByClass;
	size_t dyingBytes = 0;
	Array<std::string> errors;

	GCCollectOptions options;
	options.DryRun = true;
	options.Verify = verify;
	options.Watch = [this](GCObject* obj) { return InDepartedLevel(this, obj); };
	options.Dying = [&](GCObject* obj)
	{
		dyingByKind[PackageKind(this, obj)]++;
		dyingByClass[obj->GCClassName()]++;
		if (UField* field = dynamic_cast<UField*>(obj))
		{
			if (field->package)
				errors.push_back("unreachable code: " + obj->GCDescribe());
		}
		if (UActor* actor = dynamic_cast<UActor*>(obj))
		{
			ULevel* level = actor->XLevel();
			if ((level == Level || level == EntryLevel) && level && actor->Index >= 0 && (size_t)actor->Index < level->Actors.size() && level->Actors[actor->Index] == actor)
				errors.push_back("unreachable actor in its level's Actors: " + obj->GCDescribe());
		}
	};

	GCCollectResult result = GC::Collect([this](GCMarker& marker) { MarkRoots(marker); }, nullptr, options);

	auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	size_t dying = 0;
	for (auto& it : dyingByKind)
		dying += it.second;
	LogMessage("Garbage (dry run): objects: " + std::to_string(result.ObjectsBefore) + "->" + std::to_string(result.ObjectsBefore - dying) + "; refs: " + std::to_string(result.Refs) + "; " + std::to_string((int)ms) + " ms");
	LogTop("GC dry run: what would go, by where it lives:", dyingByKind, 20);
	LogTop("GC dry run: what would go, by class:", dyingByClass, 30);

	// What keeps the levels left behind: the first holder from outside them
	// of each of their objects reached.
	std::map<std::string, size_t> retainers;
	for (auto& it : result.FirstHolder)
	{
		if (!InDepartedLevel(this, it.second.Object))
			retainers[it.second.Key + " -> " + it.first->GCDescribe()]++;
	}
	LogTop("GC dry run: departed levels' objects reached from outside them (holder -> object):", retainers, 60);
	LogTop("GC: eliminated objects kept through a reference that may not be written:", result.KeptEliminated, 30);
	LogTop("GC verify: pointers to no live object, by holder:", result.Invalid, 30);
	for (const std::string& error : errors)
		LogMessage("GC error: " + error);
}
