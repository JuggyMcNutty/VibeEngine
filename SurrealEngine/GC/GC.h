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

	// Code whose references change after it is loaded (a class: scripts
	// write its defaults): walked every collection, where the rest of the
	// code is walked only when code was loaded (GC::Collect).
	virtual bool IsGCMutableCode() const { return false; }

	// A reference to an eliminated object is made None where the marker
	// finds it, and the object is not followed.
	virtual bool IsGCEliminated() const { return false; }

	// Every name it holds, for the name collection after a sweep
	// (NameString::Mark).
	virtual void GCMarkNames() {}

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
	// Code: kept for good, on a list of its own, which the marker never
	// visits as a target and walks only for what it refers to (GC::Collect)
	uint64_t permanent : 1;
	// Looked at once for being code, by the collection after it was made
	uint64_t scanned : 1;
	uint64_t memsize : 61;
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
	// Each object that goes (in a dry run, would go), before any is told.
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
	// SURREAL_GC_VERIFY: code's references to code (GCMarker::MarkCode) that
	// reached an object not permanent, by holder.
	std::map<std::string, size_t> CodeToCollectable;
	// SURREAL_GC_VERIFY: what the walk of the code reached that the list
	// kept from the last walk did not hold, by holder ("names" for a name).
	std::map<std::string, size_t> CodeKeptMissed;
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
		if (ref && !IsPermanent(static_cast<GCObject*>(ref)) && Visit(static_cast<GCObject*>(ref), true))
			ref = nullptr;
	}

	// A reference that must not be written, such as a subsystem the engine
	// holds: an eliminated object reached this way stays, and is logged.
	void MarkConst(const GCObject* obj)
	{
		if (obj && !IsPermanent(obj))
			Visit(const_cast<GCObject*>(obj), false);
	}

	// A reference code holds to code: its class, its chain of fields, the
	// struct, class or enum a property names. Loaded code refers to loaded
	// code, permanent, so from a permanent holder it is not handed over,
	// which keeps the walk of the permanent list short; from any other
	// holder it is handed as MarkConst. SURREAL_GC_VERIFY hands it from code
	// too and reports a target that is not permanent (CodeToCollectable).
	void MarkCode(const GCObject* obj)
	{
		if (!CodeHolder)
		{
			MarkConst(obj);
		}
		else if (Options->Verify)
		{
			if (obj && !IsPermanent(obj))
				Result->CodeToCollectable[HolderKey()]++;
			MarkConst(obj);
		}
	}

	// A list: entries of eliminated objects are dropped.
	template<typename ArrayT>
	void MarkArray(ArrayT& list)
	{
		size_t out = 0;
		for (size_t i = 0, count = list.size(); i < count; i++)
		{
			auto obj = list[i];
			if (obj && !IsPermanent(static_cast<GCObject*>(obj)) && Visit(static_cast<GCObject*>(obj), true))
				continue;
			list[out++] = obj;
		}
		if (out != list.size())
			list.resize(out);
	}

	// Code is never a target: it stays whatever reaches it
	static bool IsPermanent(const GCObject* obj);

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
	// The holder whose Mark runs is permanent (GC::Collect's walk of the list)
	bool CodeHolder = false;
	// While set, every object a reference reaches is listed once, with its
	// first holder (GC::Collect's walk of the code)
	std::vector<std::pair<const GCObject*, GCObject*>>* Recording = nullptr;
	std::unordered_set<GCObject*> Recorded;

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

	// Marks from the roots -- the GCRoot nodes, the code (every IsGCRoot
	// object, moved to the permanent list when first found and walked from
	// there for what it refers to) and what markRoots hands the marker --,
	// each object's names with it (GCMarkNames), lets purgeWeak drop what
	// the weak holders keep of the dying (IsDying), then destroys and frees
	// them.
	static GCCollectResult Collect(const std::function<void(GCMarker&)>& markRoots, const std::function<void()>& purgeWeak, const GCCollectOptions& options = {});

	// During a collection's purge and sweep: whether the object goes.
	static bool IsDying(const GCObject* obj);
	static bool IsCollecting() { return collecting; }

	static GCStats GetStats();
	// Every object, the code first; without it, only what a collection can free.
	static GCObjectList GetObjects(bool withCode = true);

	// Calls GCMarkNames on every object: the collection's own marking covers
	// every object it reaches, this the objects made while it ran.
	static void MarkNames();
	// How many objects were made during the last collection.
	static size_t BornDuringCollection() { return bornDuringCollection; }

	// Code was loaded or made: the next collection walks all of it.
	static void CodeChanged() { codeChanged = true; }

private:
	static bool codeChanged;
	static GCAllocation* GetAllocations();
	static GCAllocation* GetPermanents();
	static size_t bornDuringCollection;
	static GCAllocation* AllocMemory(size_t size);
	static void FreeMemory(GCAllocation* allocation);

	static bool collecting;

	friend class GCObjectList;
	friend class GCMarker;
};

class GCObjectList
{
public:
	// The first list, then the second
	class iterator
	{
	public:
		iterator(GCAllocation* item, GCAllocation* then) : item(item), then(then) {}

		GCObject* operator*() const { return item->object(); }
		iterator& operator++()
		{
			item = item->allocklistNext;
			if (!item && then)
			{
				item = then;
				then = nullptr;
			}
			return *this;
		}

		bool operator==(const iterator& other) const { return item == other.item; }

	private:
		GCAllocation* item;
		GCAllocation* then;
	};

	GCObjectList(bool withCode) : withCode(withCode) {}

	iterator begin()
	{
		GCAllocation* code = withCode ? GC::GetPermanents() : nullptr;
		return code ? iterator(code, GC::GetAllocations()) : iterator(GC::GetAllocations(), nullptr);
	}
	iterator end() { return iterator(nullptr, nullptr); }

private:
	bool withCode;
};

inline GCObjectList GC::GetObjects(bool withCode) { return GCObjectList(withCode); }

inline GCAllocation* GCObject::Allocation() { return reinterpret_cast<GCAllocation*>(this) - 1; }

inline bool GCMarker::IsPermanent(const GCObject* obj) { return const_cast<GCObject*>(obj)->Allocation()->permanent; }

inline bool GC::IsDying(const GCObject* obj)
{
	return collecting && obj && const_cast<GCObject*>(obj)->Allocation()->unreferencedFlag;
}
