#pragma once

#include "Packages/Core/UObject.h"

class UActor;
class ULevelInfo;

// How NPCs learn of shots, footsteps, noises, alarms, bodies and distress:
// actors raise named events, NPCs listen for them, and each frame this
// manager works out who senses what and calls the listeners' script. The
// original is Engine.dll's UEventManager, a C++ class with no script; the
// game's dx-reverse-info/engine-dll.md has the read. One lives in each level's
// package (LevelInfo.EventManager), so a save keeps every listener and
// event, in the original's layout: its event types, senders and receivers
// objects of their own (UAIEventType and the rest, below), which a save
// makes from the manager's lists and a load turns back into them.
class UEventManager : public UObject
{
public:
	void Mark(GCMarker& marker) override;
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	// The one in the level's LevelInfo.EventManager, or none: the natives
	// do nothing without one, and only InitEventManager creates it.
	static UEventManager* Get();
	static void InitEventManager(ULevelInfo* info);

	void SetEventCallback(UActor* actor, const NameString& eventName, const NameString& callback, const NameString& scoreCallback, bool bCheckVisibility, bool bCheckDir, bool bCheckCylinder, bool bCheckLOS);
	void ClearEventCallback(UActor* actor, const NameString& eventName);
	// A pulse (AISendEvent), or with start also the current level
	// (AIStartEvent): what the sender goes on giving off.
	void SendEvent(UActor* actor, const NameString& eventName, uint8_t sense, float value, float radius, bool start);
	void EndEvent(UActor* actor, const NameString& eventName, uint8_t sense);
	void ClearEvent(UActor* actor, const NameString& eventName);
	void ActorDestroyed(UActor* actor);
	void Tick();

	// EAIEventType: visual, audio or olfactory.
	enum { SenseVisual = 0, SenseAudio = 1, SenseSmell = 2 };
	// EAIEventState.
	enum { StateBegin = 0, StateEnd = 1, StatePulse = 2, StateChangeBest = 3 };

	struct SenseLevels
	{
		float Visual = 0.0f;
		float Audio = 0.0f;
		float AudioRadius = 0.0f;
		float Smell = 0.0f;
		bool Any() const { return Visual > 0.0f || Audio > 0.0f || Smell > 0.0f; }
	};

	static const int NumSlots = 16;

	struct Sender
	{
		UActor* Actor = nullptr;
		// Four numbers for each of the last 16 frames, in a ring, and the
		// current level of each.
		SenseLevels Slots[NumSlots];
		SenseLevels Current;
		bool Delete = false;
	};

	struct Receiver
	{
		UActor* Actor = nullptr;
		NameString Callback;      // called as AIEvent when none
		NameString ScoreCallback; // none: the score is distance squared plus 1
		bool bCheckVisibility = true;
		bool bCheckDir = true;
		bool bCheckCylinder = false;
		bool bCheckLOS = true;
		bool EventOn = false;
		// The receiver's XAIParams: the best sender and what was sensed.
		UActor* BestActor = nullptr;
		float BestScore = 0.0f;
		float BestVisibility = 0.0f;
		float BestVolume = 0.0f;
		float BestSmell = 0.0f;
		int LastTurnFrame = 0;
		bool Delete = false;
		// A call due after the pass, and the state it reports
		bool CallDue = false;
		uint8_t CallState = 0;
	};

	struct EventType
	{
		NameString Name;
		std::vector<std::unique_ptr<Sender>> Senders;
		std::vector<std::unique_ptr<Receiver>> Receivers;
	};

	std::map<NameString, EventType> Events;

	// Every receiver is in one ring the manager walks, resuming where the
	// last frame stopped; a new receiver goes at the ring's end.
	std::vector<std::pair<EventType*, Receiver*>> Ring;
	size_t RingPos = 0;
	int FrameCounter = 0;
	int SlotIndex = 0;

private:
	// The fork's own saved layout, before the original's (2026-09-27).
	void LoadForkLayout(ObjectStream* stream);
	EventType& GetEventType(const NameString& name);
	Sender* FindSender(EventType& type, UActor* actor, bool create);
	Receiver* FindReceiver(EventType& type, UActor* actor);
	bool ProcessReceiver(EventType& type, Receiver& receiver);
	SenseLevels PeakLevels(const Sender& sender, int slotsBack) const;
	void ComputeSenseDetection(Receiver& receiver, UActor* senderActor, const SenseLevels& peak, float& visibility, float& volume, float& smell);
	void CallListener(EventType& type, Receiver& receiver, uint8_t state);
	void QueueCall(EventType& type, Receiver& receiver, uint8_t state);
	void CleanupEvents();

	// The receivers whose calls are due, in the order they had their turns
	std::vector<std::pair<EventType*, Receiver*>> DueCalls;
};

// The original's saved event manager keeps its parts as objects of their
// own in the level's package, its C++ classes (Engine's UnEventManager.h,
// XAIEventType and the rest), in the layouts below. Only a save's form: the
// manager works on its own lists.
class UAIEventType : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	NameString EventName;
	int32_t EventHash = 0;
	UObject* Senders = nullptr;
	UObject* Receivers = nullptr;
	UObject* NextEventType = nullptr;
};

class UAIEvent : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	UObject* EventType = nullptr;
	UActor* EventActor = nullptr;
	int32_t bBeingDestroyed = 0;
	UObject* NextEvent = nullptr;
};

class UAISenderEvent : public UAIEvent
{
public:
	using UAIEvent::UAIEvent;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	UEventManager::SenseLevels Settings[UEventManager::NumSlots];
	UEventManager::SenseLevels CurrentSettings;
};

class UAIReceiverEvent : public UAIEvent
{
public:
	using UAIEvent::UAIEvent;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	NameString Callback;
	NameString ScoreCallback;
	int32_t bInvokeCallback = 0;
	uint8_t EventState = 0;
	int32_t bCheckVisibility = 0;
	int32_t bCheckDir = 0;
	int32_t bCheckCylinder = 0;
	int32_t bCheckLOS = 0;
	int32_t bEventOn = 0;
	float BestScore = 0.0f;
	UActor* BestSender = nullptr;
	int32_t NextSlot = 0;
	UObject* NextProcess = nullptr;
	UObject* PrevProcess = nullptr;
};
