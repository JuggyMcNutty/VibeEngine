#pragma once

#include <vector>
#include <list>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>

struct GCAllocation;
class GCObjectList;
class GCMarker;

// An object the collector owns: reachable from a root it stays, the rest is
// destroyed, then freed (vibe/docs/ENGINE.md, objects and memory).
class GCObject
{
protected:
	virtual ~GCObject() = default;

	// Every reference this object holds that keeps another alive.
	virtual void Mark(GCMarker& marker) = 0;

	// The sweep's first phase, everything it frees still allocated: what the
	// object must undo while the objects it points at are there to be told.
	virtual void OnGCDestroy() {}

	// Kept whatever reaches it (code: the fork never unloads a class).
	virtual bool IsGCRoot() const { return false; }

	// A reference to an eliminated object is made None where the marker
	// finds it, and the object is not followed.
	virtual bool IsGCEliminated() const { return false; }

public:
	// For the collector's log: the object's class, and the class and name.
	virtual std::string GCClassName() const { return "?"; }
	virtual std::string GCDescribe() const { return GCClassName(); }

private:
	GCAllocation* Allocation();
	friend class GC;
	friend class GCMarker;
};

struct GCAllocation
{
	GCObject* object() { return reinterpret_cast<GCObject*>(this + 1); }
	uint64_t unreferencedFlag : 1;
	uint64_t memsize : 63;
	GCAllocation* allocklistNext;
};

struct GCStats
{
	size_t numObjects = 0;
	size_t memoryUsage = 0;
};

class GCRootNode
{
public:
	GCRootNode();
	~GCRootNode();

	void set(GCObject* value) { obj = value; }
	GCObject* get() const { return obj; }

private:
	GCObject* obj = nullptr;
	GCRootNode* prev = nullptr;
	GCRootNode* next = nullptr;

	GCRootNode(const GCRootNode&) = delete;
	GCRootNode& operator=(const GCRootNode&) = delete;
	friend class GC;
};

template<typename T>
class GCRoot
{
public:
	GCRoot() : node(std::make_unique<GCRootNode>()) {}
	GCRoot(T* value) : GCRoot() { set(value); }

	void set(T* value) { node->set(value); }
	T* get() const { return static_cast<T*>(node->get()); }

	explicit operator bool() const { return get(); }
	T* operator->() { return get(); }
	const T* operator->() const { return get(); }

private:
	std::unique_ptr<GCRootNode> node;
};

// What a collection is asked to do.
struct GCCollectOptions
{
	// Mark only: nothing made None, nothing freed (SURREAL_GC_DRYRUN).
	bool DryRun = false;
	// Every pointer the marker is handed must be a live allocation
	// (SURREAL_GC_VERIFY).
	bool Verify = false;
	// The objects the dry run names the first holder of.
	std::function<bool(GCObject*)> Watch;
	// The dry run: each object that would go, before anything is unmarked.
	std::function<void(GCObject*)> Dying;
};

struct GCCollectResult
{
	size_t ObjectsBefore = 0;
	size_t ObjectsAfter = 0;
	size_t BytesBefore = 0;
	size_t BytesAfter = 0;
	size_t Refs = 0;
	// References made None, by the holder's class and field.
	std::map<std::string, size_t> Cleared;
	// Eliminated objects reached through a reference that may not be
	// written: they stay, by the holder's class and field.
	std::map<std::string, size_t> KeptEliminated;
	// SURREAL_GC_VERIFY: pointers that were no live allocation, by holder.
	std::map<std::string, size_t> Invalid;
	// The dry run: each watched object reached, what reached it first (none
	// for a root) and that holder's class and field.
	struct Holder
	{
		const GCObject* Object = nullptr;
		std::string Key;
	};
	std::unordered_map<GCObject*, Holder> FirstHolder;
};

// The marking pass's worklist. A holder's Mark hands each reference over:
// Mark for one the collector may make None, MarkConst for one it may not.
class GCMarker
{
public:
	template<typename T>
	void Mark(T*& ref)
	{
		if (ref && Visit(static_cast<GCObject*>(ref), true))
			ref = nullptr;
	}

	// A reference that must not be written, such as a subsystem the engine
	// holds: an eliminated object reached this way stays, and is logged.
	void MarkConst(const GCObject* obj)
	{
		if (obj)
			Visit(const_cast<GCObject*>(obj), false);
	}

	// A list: entries of eliminated objects are dropped.
	template<typename ArrayT>
	void MarkArray(ArrayT& list)
	{
		size_t out = 0;
		for (size_t i = 0, count = list.size(); i < count; i++)
		{
			auto obj = list[i];
			if (obj && Visit(static_cast<GCObject*>(obj), true))
				continue;
			list[out++] = obj;
		}
		if (out != list.size())
			list.resize(out);
	}

	// The field the next references are held in, for the log ("Actors", a
	// property): a holder's Mark is entered with none.
	void SetField(const char* field) { FieldName = field; FieldObject = nullptr; }
	void SetField(const GCObject* field) { FieldObject = field; FieldName = nullptr; }

	// A root's name, for the log, while a root marker hands its references.
	void SetRoot(const char* root) { RootName = root; FieldName = nullptr; FieldObject = nullptr; }

	// Whether references to an eliminated object are being made None.
	bool Eliminates() const { return !Options->DryRun; }

private:
	GCMarker(const GCCollectOptions& options, GCCollectResult& result, const std::unordered_set<GCAllocation*>* live) : Options(&options), Result(&result), Live(live) {}

	// Whether the reference is to be made None.
	bool Visit(GCObject* obj, bool writable);
	std::string HolderKey() const;
	void Drain();

	const GCCollectOptions* Options = nullptr;
	GCCollectResult* Result = nullptr;
	const std::unordered_set<GCAllocation*>* Live = nullptr;
	std::vector<GCObject*> Worklist;
	const GCObject* Holder = nullptr;
	const char* FieldName = nullptr;
	const GCObject* FieldObject = nullptr;
	const char* RootName = nullptr;

	friend class GC;
};

class GC
{
public:
	template<typename T, typename... Args>
	static T* Alloc(Args&&... args)
	{
		GCAllocation* alloc = AllocMemory(sizeof(T));
		try
		{
			return new((unsigned char*)alloc->object()) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			FreeMemory(alloc);
			throw;
		}
	}

	// Marks from the roots -- the GCRoot nodes, every IsGCRoot object, and
	// what markRoots hands the marker --, lets purgeWeak drop what the weak
	// holders keep of the dying (IsDying), then destroys and frees them.
	static GCCollectResult Collect(const std::function<void(GCMarker&)>& markRoots, const std::function<void()>& purgeWeak, const GCCollectOptions& options = {});

	// During a collection's purge and sweep: whether the object goes.
	static bool IsDying(const GCObject* obj);
	static bool IsCollecting() { return collecting; }

	static GCStats GetStats();
	static GCObjectList GetObjects();

private:
	static GCAllocation* GetAllocations();
	static GCAllocation* AllocMemory(size_t size);
	static void FreeMemory(GCAllocation* allocation);

	static bool collecting;

	friend class GCObjectList;
	friend class GCMarker;
};

class GCObjectList
{
public:
	class iterator
	{
	public:
		iterator(GCAllocation* item) : item(item) {}

		GCObject* operator*() const { return item->object(); }
		iterator& operator++() { item = item->allocklistNext; return *this; }

		bool operator==(const iterator& other) const { return item == other.item; }

	private:
		GCAllocation* item;
	};

	iterator begin() { return GC::GetAllocations(); }
	iterator end() { return nullptr; }
};

inline GCObjectList GC::GetObjects() { return {}; }

inline GCAllocation* GCObject::Allocation() { return reinterpret_cast<GCAllocation*>(this) - 1; }

inline bool GC::IsDying(const GCObject* obj)
{
	return collecting && obj && const_cast<GCObject*>(obj)->Allocation()->unreferencedFlag;
}
