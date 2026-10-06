
#include "Precomp.h"
#include "Engine.h"
#include "GC/GC.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Render/RenderSubsystem.h"
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
#include "Packages/Engine/Resources/Mesh/UMesh.h"
#include "Packages/Engine/Resources/USound.h"
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
#include <cstdio>
#include <unordered_set>
#ifdef __linux__
#include <unistd.h>
#endif
#if defined(__GLIBC__)
#include <malloc.h>
#endif

static bool EnvSet(const char* name)
{
	const char* value = std::getenv(name);
	return value && *value && std::string(value) != "0";
}

void Engine::RequestGarbageCollection(const std::string& reason, bool mapChange)
{
	if (GarbageRequest.empty())
		GarbageRequest = reason;
	GarbageMapChange = GarbageMapChange || mapChange;
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
		if (pkg->IsLevel())
			return "departed levels";
		if (engine->packages->IsRegisteredPackage(pkg))
			return "packages";
		return "other packages";
	}

	bool InDepartedLevel(Engine* engine, const GCObject* obj)
	{
		const UObject* uobj = dynamic_cast<const UObject*>(obj);
		return uobj && uobj->package && uobj->package->IsLevel() && uobj->package != engine->LevelPackage && uobj->package != engine->EntryLevelPackage;
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

static size_t ResidentBytes()
{
#ifdef __linux__
	long pages = 0, resident = 0;
	if (FILE* f = fopen("/proc/self/statm", "r"))
	{
		if (fscanf(f, "%ld %ld", &pages, &resident) != 2)
			resident = 0;
		fclose(f);
	}
	return (size_t)resident * (size_t)sysconf(_SC_PAGESIZE);
#else
	return 0;
#endif
}

void Engine::CollectGarbage()
{
	std::string reason = std::move(GarbageRequest);
	GarbageRequest.clear();
	bool mapChange = GarbageMapChange;
	GarbageMapChange = false;

	bool dryRun = EnvSet("SURREAL_GC_DRYRUN");
	bool verify = EnvSet("SURREAL_GC_VERIFY");

	// Only between frames: no script frame, no object half loaded.
	if (!Frame::Callstack.empty() || packages->HasPendingLoads())
	{
		LogMessage("GC: not collecting with script running or objects waiting to load (" + reason + ")");
		return;
	}

	LogMessage("Collecting garbage (" + reason + ")");
	auto start = std::chrono::steady_clock::now();
	size_t residentBefore = ResidentBytes();

	std::map<std::string, size_t> dyingByKind, dyingByClass;
	Array<std::string> errors;

	GCCollectOptions options;
	options.DryRun = dryRun;
	options.Verify = verify;
	if (dryRun)
	{
		options.Watch = [this](GCObject* obj) { return InDepartedLevel(this, obj); };
		options.Dying = [&](GCObject* obj)
		{
			std::string kind = PackageKind(this, obj);
			dyingByKind[kind]++;
			dyingByClass[obj->GCClassName() + " (" + kind + ")"]++;
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
	}

	// What goes with every reference to it made None: each object of a
	// level left behind (the fork's own: a stale reference becomes None
	// instead of keeping that level), and at a map change the new level's
	// actors that are not in its Actors (the original's). Code is kept.
	Array<UObject*> eliminated;
	if (!dryRun)
	{
		std::unordered_set<UActor*> listed;
		if (mapChange && Level)
		{
			for (UActor* actor : Level->Actors)
			{
				if (actor)
					listed.insert(actor);
			}
		}
		for (GCObject* gcobj : GC::GetObjects())
		{
			UObject* obj = dynamic_cast<UObject*>(gcobj);
			if (!obj || !obj->package || AnyFlags(obj->Flags, ObjectFlags::EliminateObject) || dynamic_cast<UField*>(obj))
				continue;
			Package* pkg = obj->package;
			bool eliminate = false;
			if (pkg->IsLevel() && pkg != LevelPackage && pkg != EntryLevelPackage)
				eliminate = true;
			else if (mapChange && pkg == LevelPackage && Level)
			{
				if (UActor* actor = dynamic_cast<UActor*>(obj))
					eliminate = listed.find(actor) == listed.end();
			}
			if (eliminate)
			{
				obj->Flags |= ObjectFlags::EliminateObject;
				eliminated.push_back(obj);
			}
		}
	}

	// What a collection frees, by kind, for its log.
	size_t freedActors = 0, freedTextures = 0, freedSounds = 0, freedMeshes = 0, freedPackages = 0, freedOther = 0;
	if (!dryRun)
	{
		options.Dying = [&](GCObject* obj)
		{
			if (dynamic_cast<UActor*>(obj))
				freedActors++;
			else if (dynamic_cast<UTexture*>(obj))
				freedTextures++;
			else if (dynamic_cast<USound*>(obj))
				freedSounds++;
			else if (dynamic_cast<UMesh*>(obj))
				freedMeshes++;
			else if (dynamic_cast<Package*>(obj))
				freedPackages++;
			else
				freedOther++;
		};
	}

	GarbageTextureDied = false;
	size_t keptEliminated = 0;
	auto purgeWeak = [&]()
	{
		// An eliminated object something kept (a reference that may not be
		// written) stays, and is no longer eliminated.
		for (UObject* obj : eliminated)
		{
			if (!GC::IsDying(obj))
			{
				obj->Flags = (ObjectFlags)((uint32_t)obj->Flags & ~(uint32_t)ObjectFlags::EliminateObject);
				keptEliminated++;
			}
		}
		if (GC::IsDying(CameraActor))
			CameraActor = nullptr;
		if (Level && !GC::IsDying(Level))
			Level->Light.PurgeDying();
		if (EntryLevel && EntryLevel != Level && !GC::IsDying(EntryLevel))
			EntryLevel->Light.PurgeDying();
		if (render)
			render->PurgeDying();
		if (audiodev)
			audiodev->PurgeDying();
		packages->PurgeDying();
	};

	if (!dryRun)
		LogMessage("Purging garbage");
	GCCollectResult result = GC::Collect([this](GCMarker& marker) { MarkRoots(marker); }, purgeWeak, options);

	// The device caches textures by address: one freed must not be found
	// in another's place. A map load's flush came before anything was drawn.
	if (GarbageTextureDied && render && render->DrawnSinceFlush)
		render->FlushDevice();
	GarbageTextureDied = false;
	if (!dryRun)
		GarbageDestroyedBacklog = 0;

#if defined(__GLIBC__)
	if (!dryRun)
		malloc_trim(0);
#endif

	auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	size_t residentAfter = ResidentBytes();
	if (dryRun)
	{
		size_t dying = 0;
		for (auto& it : dyingByKind)
			dying += it.second;
		LogMessage("Garbage (dry run): objects: " + std::to_string(result.ObjectsBefore) + "->" + std::to_string(result.ObjectsBefore - dying) + "; refs: " + std::to_string(result.Refs) + "; " + std::to_string((int)ms) + " ms");
		LogTop("GC dry run: what would go, by where it lives:", dyingByKind, 20);
		LogTop("GC dry run: what would go, by class (and where):", dyingByClass, 40);

		// What keeps the levels left behind: the first holder from outside
		// them of each of their objects reached.
		std::map<std::string, size_t> retainers;
		for (auto& it : result.FirstHolder)
		{
			if (!InDepartedLevel(this, it.second.Object))
				retainers[it.second.Key + " -> " + it.first->GCDescribe()]++;
		}
		LogTop("GC dry run: departed levels' objects reached from outside them (holder -> object):", retainers, 60);
	}
	else
	{
		char line[256];
		std::snprintf(line, sizeof(line), "Garbage: objects: %i->%i; refs: %i; %.1f ms; %.1f MB -> %.1f MB resident; %i eliminated, %i kept",
			(int)result.ObjectsBefore, (int)result.ObjectsAfter, (int)result.Refs, ms,
			residentBefore / 1048576.0, residentAfter / 1048576.0, (int)eliminated.size(), (int)keptEliminated);
		LogMessage(line);
		LogMessage("Freed: " + std::to_string(freedActors) + " actors, " + std::to_string(freedTextures) + " textures, " + std::to_string(freedSounds) + " sounds, " +
			std::to_string(freedMeshes) + " meshes, " + std::to_string(freedPackages) + " packages, " + std::to_string(freedOther) + " other objects");
		LogTop("GC: references made None, by holder:", result.Cleared, 20);
	}
	LogTop("GC: eliminated objects kept through a reference that may not be written:", result.KeptEliminated, 30);
	LogTop("GC verify: pointers to no live object, by holder:", result.Invalid, 30);
	for (const std::string& error : errors)
		LogMessage("GC error: " + error);
}
