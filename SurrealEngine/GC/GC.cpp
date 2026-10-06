
#include "Precomp.h"
#include "GC.h"

static GCRootNode* roots;
static GCAllocation* allocations;
static GCStats stats;
bool GC::collecting = false;

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

GCAllocation* GC::AllocMemory(size_t size)
{
	size_t memsize = sizeof(GCAllocation) + size;
	GCAllocation* allocation = (GCAllocation*)calloc(1, memsize);
	if (allocation == nullptr)
		throw std::bad_alloc();
	allocation->allocklistNext = allocations;
	allocation->memsize = memsize;
	// Made during a collection, it is not one of those the sweep may free.
	allocation->unreferencedFlag = !collecting;
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
	GCMarker marker(options, result, live.get());

	marker.SetRoot("GCRoot");
	for (GCRootNode* root = roots; root != nullptr; root = root->next)
		marker.MarkConst(root->obj);
	marker.SetRoot("code");
	for (GCAllocation* allocation = allocations; allocation != nullptr; allocation = allocation->allocklistNext)
	{
		if (allocation->object()->IsGCRoot())
			marker.MarkConst(allocation->object());
	}
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
	for (GCAllocation* alloc = GetAllocations(); alloc; alloc = alloc->allocklistNext)
		alloc->object()->GCMarkNames();
}
