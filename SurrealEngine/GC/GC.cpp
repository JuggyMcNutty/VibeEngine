
#include "Precomp.h"
#include "GC.h"
#include "Package/NameString.h"

static GCRootNode* roots;
static GCAllocation* allocations;
static GCAllocation* permanents;
static GCStats stats;
bool GC::collecting = false;
size_t GC::bornDuringCollection = 0;
bool GC::codeChanged = true;
// What the code other than classes keeps: each collectable object it
// reached at the last walk of all of it, with its holder, and each
// collectable name (GC::Collect)
static std::vector<std::pair<const GCObject*, GCObject*>> codeKeeps;
static std::vector<std::pair<int, int>> codeKeepsNames;
// The permanent objects walked every collection (IsGCMutableCode), so the
// others are not touched between walks of all of them
static std::vector<GCAllocation*> mutablePermanents;

GCRootNode::GCRootNode()
{
	next = roots;
	prev = nullptr;
	if (roots)
		roots->prev = this;
	roots = this;
}

GCRootNode::~GCRootNode()
{
	if (prev)
	{
		prev->next = next;
	}
	else
	{
		roots = next;
	}

	if (next)
	{
		next->prev = prev;
	}
}

GCAllocation* GC::GetAllocations()
{
	return allocations;
}

GCAllocation* GC::GetPermanents()
{
	return permanents;
}

GCAllocation* GC::AllocMemory(size_t size)
{
	size_t memsize = sizeof(GCAllocation) + size;
	GCAllocation* allocation = (GCAllocation*)calloc(1, memsize);
	if (allocation == nullptr)
		throw std::bad_alloc();
	allocation->allocklistNext = allocations;
	allocation->memsize = memsize;
	allocation->permanent = 0;
	allocation->scanned = 0;
	// Made during a collection, it is not one of those the sweep may free.
	allocation->unreferencedFlag = !collecting;
	if (collecting)
		bornDuringCollection++;
	allocations = allocation;
	stats.numObjects++;
	stats.memoryUsage += memsize;
	return allocation;
}

// A constructor that threw: the allocation leaves the list first. It is
// the list's head unless the constructor allocated others.
void GC::FreeMemory(GCAllocation* allocation)
{
	GCAllocation** link = &allocations;
	while (*link && *link != allocation)
		link = &(*link)->allocklistNext;
	if (*link)
	{
		*link = allocation->allocklistNext;
		stats.numObjects--;
		stats.memoryUsage -= allocation->memsize;
	}
	free(allocation);
}

GCStats GC::GetStats()
{
	return stats;
}

bool GCMarker::Visit(GCObject* obj, bool writable)
{
	Result->Refs++;
	GCAllocation* allocation = obj->Allocation();
	if (Live && Live->find(allocation) == Live->end())
	{
		Result->Invalid[HolderKey()]++;
		return false;
	}
	if (obj->IsGCEliminated())
	{
		if (writable && Eliminates())
		{
			Result->Cleared[HolderKey()]++;
			return true;
		}
		if (!writable)
			Result->KeptEliminated[HolderKey()]++;
	}
	if (Recording && Recorded.insert(obj).second)
		Recording->push_back({ Holder, obj });
	if (allocation->unreferencedFlag)
	{
		allocation->unreferencedFlag = false;
		if (Options->Watch && Options->Watch(obj))
			Result->FirstHolder[obj] = { Holder, HolderKey() };
		Worklist.push_back(obj);
	}
	return false;
}

std::string GCMarker::HolderKey() const
{
	std::string key;
	if (Holder)
		key = Holder->GCClassName();
	else
		key = std::string("root ") + (RootName ? RootName : "?");
	if (FieldName)
		key += std::string(".") + FieldName;
	else if (FieldObject)
		key += "." + FieldObject->GCDescribe();
	return key;
}

void GCMarker::Drain()
{
	while (!Worklist.empty())
	{
		GCObject* obj = Worklist.back();
		Worklist.pop_back();
		Holder = obj;
		FieldName = nullptr;
		FieldObject = nullptr;
		obj->Mark(*this);
		obj->GCMarkNames();
	}
	Holder = nullptr;
	FieldName = nullptr;
	FieldObject = nullptr;
}

GCCollectResult GC::Collect(const std::function<void(GCMarker&)>& markRoots, const std::function<void()>& purgeWeak, const GCCollectOptions& options)
{
	GCCollectResult result;
	result.ObjectsBefore = stats.numObjects;
	result.BytesBefore = stats.memoryUsage;

	std::unique_ptr<std::unordered_set<GCAllocation*>> live;
	if (options.Verify)
	{
		live = std::make_unique<std::unordered_set<GCAllocation*>>();
		live->reserve(stats.numObjects);
		for (GCAllocation* allocation = allocations; allocation != nullptr; allocation = allocation->allocklistNext)
			live->insert(allocation);
	}

	// Every flag is set between collections: an object is unreferenced
	// until the marker reaches it.
	collecting = true;
	bornDuringCollection = 0;
	GCMarker marker(options, result, live.get());

	marker.SetRoot("GCRoot");
	for (GCRootNode* root = roots; root != nullptr; root = root->next)
		marker.MarkConst(root->obj);
	// Code found since the last collection joins the permanent list, where
	// it is never visited as a target and never freed. Only the objects
	// made since the last collection are looked at: the list is pushed at
	// its head, so they end at the first one scanned before, and an
	// object's package is set as it is made, so none turns into code later.
	marker.SetRoot("code");
	{
		GCAllocation* prev = nullptr;
		GCAllocation* cur = allocations;
		while (cur && !cur->scanned)
		{
			GCAllocation* next = cur->allocklistNext;
			cur->scanned = 1;
			if (cur->object()->IsGCRoot())
			{
				if (prev)
					prev->allocklistNext = next;
				else
					allocations = next;
				cur->permanent = 1;
				cur->unreferencedFlag = 0;
				cur->allocklistNext = permanents;
				permanents = cur;
				if (cur->object()->IsGCMutableCode())
					mutablePermanents.push_back(cur);
				codeChanged = true;
			}
			else
			{
				prev = cur;
			}
			cur = next;
		}
	}
	// What the permanent list keeps. A class is walked every collection,
	// as scripts write its defaults (IsGCMutableCode). The rest of the code
	// -- functions, properties, structs, states, enums, constants -- refers
	// to what it referred to when it was loaded, so it is walked when code
	// was loaded or found since the last such walk (GC::CodeChanged), and
	// the collectable objects and names it reached are kept from a list
	// until then. SURREAL_GC_VERIFY walks it every collection and reports
	// what the list would have missed (CodeKeptMissed).
	marker.CodeHolder = true;
	bool walkAll = codeChanged || options.Verify;
	bool fromList = !codeChanged;
	if (fromList)
	{
		for (auto& kept : codeKeeps)
		{
			marker.Holder = kept.first;
			marker.FieldName = nullptr;
			marker.FieldObject = nullptr;
			marker.MarkConst(kept.second);
		}
		for (auto& name : codeKeepsNames)
			NameString::MarkIndexes(name.first, name.second);
	}
	std::vector<std::pair<const GCObject*, GCObject*>> keeps;
	std::vector<std::pair<int, int>> keepsNames;
	auto walkCode = [&](GCObject* obj, bool record)
	{
		marker.Holder = obj;
		marker.FieldName = nullptr;
		marker.FieldObject = nullptr;
		if (record)
		{
			marker.Recording = &keeps;
			NameString::Recording = &keepsNames;
		}
		obj->Mark(marker);
		obj->GCMarkNames();
		marker.Recording = nullptr;
		NameString::Recording = nullptr;
	};
	if (walkAll)
	{
		for (GCAllocation* allocation = permanents; allocation != nullptr; allocation = allocation->allocklistNext)
			walkCode(allocation->object(), !allocation->object()->IsGCMutableCode());
	}
	else
	{
		for (GCAllocation* allocation : mutablePermanents)
			walkCode(allocation->object(), false);
	}
	if (walkAll)
	{
		if (fromList)
		{
			std::unordered_set<const GCObject*> held;
			for (auto& kept : codeKeeps)
				held.insert(kept.second);
			for (auto& kept : keeps)
			{
				if (!held.count(kept.second))
					result.CodeKeptMissed[kept.first->GCDescribe()]++;
			}
			std::unordered_set<int64_t> heldNames;
			for (auto& name : codeKeepsNames)
				heldNames.insert(((int64_t)name.first << 32) | (uint32_t)name.second);
			for (auto& name : keepsNames)
			{
				if (!heldNames.count(((int64_t)name.first << 32) | (uint32_t)name.second))
					result.CodeKeptMissed["names"]++;
			}
		}
		codeKeeps = std::move(keeps);
		codeKeepsNames = std::move(keepsNames);
		codeChanged = false;
	}
	marker.CodeHolder = false;
	marker.Holder = nullptr;
	marker.Drain();

	marker.SetRoot(nullptr);
	markRoots(marker);
	marker.Drain();

	if (options.DryRun)
	{
		for (GCAllocation* allocation = allocations; allocation != nullptr; allocation = allocation->allocklistNext)
		{
			if (allocation->unreferencedFlag)
			{
				if (options.Dying)
					options.Dying(allocation->object());
			}
			else
			{
				allocation->unreferencedFlag = true;
			}
		}
		collecting = false;
		result.ObjectsAfter = stats.numObjects;
		result.BytesAfter = stats.memoryUsage;
		return result;
	}

	// The weak holders let go of the dying.
	if (purgeWeak)
		purgeWeak();

	if (options.Dying)
	{
		for (GCAllocation* allocation = allocations; allocation != nullptr; allocation = allocation->allocklistNext)
		{
			if (allocation->unreferencedFlag)
				options.Dying(allocation->object());
		}
	}

	// Phase 1: every dying object is told, all of them still allocated.
	for (GCAllocation* allocation = allocations; allocation != nullptr; allocation = allocation->allocklistNext)
	{
		if (allocation->unreferencedFlag)
			allocation->object()->OnGCDestroy();
	}

	// Phase 2: destroyed and freed; the survivors unreferenced again for the
	// next collection.
	GCAllocation* prev = nullptr;
	GCAllocation* cur = allocations;
	while (cur)
	{
		if (cur->unreferencedFlag)
		{
			GCAllocation* unreferenced = cur;

			cur = cur->allocklistNext;
			if (prev)
				prev->allocklistNext = cur;
			else
				allocations = cur;

			stats.memoryUsage -= unreferenced->memsize;
			stats.numObjects--;

			GCObject* obj = unreferenced->object();
			obj->~GCObject();
			free(unreferenced);
		}
		else
		{
			cur->unreferencedFlag = true;
			prev = cur;
			cur = cur->allocklistNext;
		}
	}

	collecting = false;
	result.ObjectsAfter = stats.numObjects;
	result.BytesAfter = stats.memoryUsage;
	return result;
}

void GC::MarkNames()
{
	for (GCAllocation* alloc = permanents; alloc; alloc = alloc->allocklistNext)
		alloc->object()->GCMarkNames();
	for (GCAllocation* alloc = allocations; alloc; alloc = alloc->allocklistNext)
		alloc->object()->GCMarkNames();
}
