
#include "Precomp.h"
#include "UEventManager.h"
#include "Package/PackageManager.h"
#include "Package/ObjectStream.h"
#include "Package/PackageStream.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Engine.h"
#include <algorithm>
#include <cctype>
#include <chrono>

// The fork's own layout of the saved manager, which its saves before
// 2026-09-27 carry; the original's is read and written since.
static const int32_t EventManagerMagic = 0x4D454941; // 'AIEM'

static PropertyDataOffset EventManagerPropOffset(ULevelInfo* info)
{
	static UClass* cachedClass = nullptr;
	static PropertyDataOffset cachedOffset;
	if (info->Class != cachedClass)
	{
		cachedClass = info->Class;
		cachedOffset = info->GetPropertyDataOffset("EventManager");
	}
	return cachedOffset;
}

UEventManager* UEventManager::Get()
{
	ULevelInfo* info = engine->LevelInfo;
	if (!info)
		return nullptr;
	PropertyDataOffset offset = EventManagerPropOffset(info);
	if (offset.DataOffset == ~(size_t)0)
		return nullptr;
	return UObject::TryCast<UEventManager>(info->Value<UObject*>(offset));
}

void UEventManager::InitEventManager(ULevelInfo* info)
{
	if (!info)
		return;
	PropertyDataOffset offset = EventManagerPropOffset(info);
	if (offset.DataOffset == ~(size_t)0)
	{
		LogMessage("LevelInfo has no EventManager property; the AI event system is off");
		return;
	}
	if (UObject::TryCast<UEventManager>(info->Value<UObject*>(offset)))
		return;
	UClass* cls = engine->packages->GetPackage("Engine")->GetClass("EventManager");
	info->Value<UObject*>(offset) = info->package->NewObject("EventManager0", cls, ObjectFlags::NoFlags);
}

UEventManager::EventType& UEventManager::GetEventType(const NameString& name)
{
	EventType& type = Events[name];
	if (type.Name.IsNone())
		type.Name = name;
	return type;
}

UEventManager::Sender* UEventManager::FindSender(EventType& type, UActor* actor, bool create)
{
	for (auto& sender : type.Senders)
	{
		if (sender->Actor == actor && !sender->Delete)
			return sender.get();
	}
	if (!create)
		return nullptr;
	type.Senders.push_back(std::make_unique<Sender>());
	type.Senders.back()->Actor = actor;
	return type.Senders.back().get();
}

UEventManager::Receiver* UEventManager::FindReceiver(EventType& type, UActor* actor)
{
	for (auto& receiver : type.Receivers)
	{
		if (receiver->Actor == actor && !receiver->Delete)
			return receiver.get();
	}
	return nullptr;
}

void UEventManager::SetEventCallback(UActor* actor, const NameString& eventName, const NameString& callback, const NameString& scoreCallback, bool bCheckVisibility, bool bCheckDir, bool bCheckCylinder, bool bCheckLOS)
{
	// The actor's receiver for the event, made at the ring's end or found.
	EventType& type = GetEventType(eventName);
	Receiver* receiver = FindReceiver(type, actor);
	if (!receiver)
	{
		type.Receivers.push_back(std::make_unique<Receiver>());
		receiver = type.Receivers.back().get();
		receiver->Actor = actor;
		Ring.push_back({ &type, receiver });
		receiver->LastTurnFrame = FrameCounter;
	}
	receiver->Callback = callback;
	receiver->ScoreCallback = scoreCallback;
	receiver->bCheckVisibility = bCheckVisibility;
	receiver->bCheckDir = bCheckDir;
	receiver->bCheckCylinder = bCheckCylinder;
	receiver->bCheckLOS = bCheckLOS;
}

void UEventManager::ClearEventCallback(UActor* actor, const NameString& eventName)
{
	auto it = Events.find(eventName);
	if (it == Events.end())
		return;
	if (Receiver* receiver = FindReceiver(it->second, actor))
		receiver->Delete = true;
}

void UEventManager::SendEvent(UActor* actor, const NameString& eventName, uint8_t sense, float value, float radius, bool start)
{
	// A pulse in this frame's slot: the sense's number becomes at least
	// Value, and for audio the radius at least Radius. AIStartEvent also
	// makes it the current level: what the sender goes on giving off.
	Sender* sender = FindSender(GetEventType(eventName), actor, true);
	SenseLevels& slot = sender->Slots[SlotIndex];
	switch (sense)
	{
	case SenseVisual:
		slot.Visual = std::max(slot.Visual, value);
		if (start)
			sender->Current.Visual = value;
		break;
	case SenseAudio:
		slot.Audio = std::max(slot.Audio, value);
		slot.AudioRadius = std::max(slot.AudioRadius, radius);
		if (start)
		{
			sender->Current.Audio = value;
			sender->Current.AudioRadius = radius;
		}
		break;
	case SenseSmell:
		slot.Smell = std::max(slot.Smell, value);
		if (start)
			sender->Current.Smell = value;
		break;
	default:
		break;
	}
}

void UEventManager::EndEvent(UActor* actor, const NameString& eventName, uint8_t sense)
{
	auto it = Events.find(eventName);
	if (it == Events.end())
		return;
	Sender* sender = FindSender(it->second, actor, false);
	if (!sender)
		return;
	switch (sense)
	{
	case SenseVisual: sender->Current.Visual = 0.0f; break;
	case SenseAudio: sender->Current.Audio = 0.0f; sender->Current.AudioRadius = 0.0f; break;
	case SenseSmell: sender->Current.Smell = 0.0f; break;
	default: break;
	}
}

void UEventManager::ClearEvent(UActor* actor, const NameString& eventName)
{
	auto it = Events.find(eventName);
	if (it == Events.end())
		return;
	if (Sender* sender = FindSender(it->second, actor, false))
		sender->Current = SenseLevels();
}

void UEventManager::ActorDestroyed(UActor* actor)
{
	// The actor's events are marked for deletion, it raises 0 from here,
	// and it stops being any listener's best sender.
	for (auto& [name, type] : Events)
	{
		for (auto& sender : type.Senders)
		{
			if (sender->Actor == actor)
			{
				sender->Current = SenseLevels();
				sender->Delete = true;
			}
		}
		for (auto& receiver : type.Receivers)
		{
			if (receiver->Actor == actor)
				receiver->Delete = true;
			if (receiver->BestActor == actor)
				receiver->BestActor = nullptr;
		}
	}
}

UEventManager::SenseLevels UEventManager::PeakLevels(const Sender& sender, int slotsBack) const
{
	// Each sense's highest number over the slots since the receiver's last
	// turn.
	SenseLevels peak;
	for (int i = 0; i < slotsBack; i++)
	{
		const SenseLevels& slot = sender.Slots[(SlotIndex - i + NumSlots) % NumSlots];
		peak.Visual = std::max(peak.Visual, slot.Visual);
		peak.Audio = std::max(peak.Audio, slot.Audio);
		peak.AudioRadius = std::max(peak.AudioRadius, slot.AudioRadius);
		peak.Smell = std::max(peak.Smell, slot.Smell);
	}
	return peak;
}

void UEventManager::ComputeSenseDetection(Receiver& receiver, UActor* senderActor, const SenseLevels& peak, float& visibility, float& volume, float& smell)
{
	visibility = 0.0f;
	volume = 0.0f;
	smell = 0.0f;

	UActor* actor = receiver.Actor;

	// An inventory item with an owner counts as its owner for a pawn, and
	// its visibility is scaled by the square root of its collision size
	// (height times radius) over the owner's.
	float visualScale = 1.0f;
	if (UInventory* item = UObject::TryCast<UInventory>(senderActor))
	{
		if (UActor* owner = item->Owner())
		{
			float itemSize = item->CollisionHeight() * item->CollisionRadius();
			float ownerSize = owner->CollisionHeight() * owner->CollisionRadius();
			if (itemSize > 0.0f && ownerSize > 0.0f)
				visualScale = std::sqrt(itemSize / ownerSize);
			senderActor = owner;
		}
	}

	UPawn* pawn = UObject::TryCast<UPawn>(actor);
	if (peak.Visual > 0.0f)
	{
		if (pawn)
		{
			visibility = pawn->AICanSee(senderActor, peak.Visual, receiver.bCheckVisibility, receiver.bCheckDir, receiver.bCheckCylinder, receiver.bCheckLOS) * visualScale;
		}
		else
		{
			// The visual number, if a trace to the sender meets no wall or
			// mover.
			auto blocks = [](UActor* hit) { return UObject::TryCast<UMover>(hit) != nullptr; };
			if (!actor->XLevel()->Collision.SightBlocked(actor->Location(), senderActor->Location(), blocks))
				visibility = peak.Visual * visualScale;
		}
	}
	if (peak.Audio > 0.0f)
	{
		if (pawn)
		{
			volume = pawn->AICanHear(senderActor, peak.Audio, peak.AudioRadius);
		}
		else
		{
			vec3 delta = senderActor->Location() - actor->Location();
			float radius = peak.AudioRadius > 0.0f ? peak.AudioRadius : 800.0f;
			if (length(delta) < radius)
				volume = peak.Audio;
		}
	}
	// AICanSmell returns 0 in the original too.
}

void UEventManager::CallListener(EventType& type, Receiver& receiver, uint8_t state)
{
	// The callback function (AIEvent when none) with the event's name, the
	// state and the receiver's XAIParams.
	NameString callback = receiver.Callback.IsNone() ? NameString("AIEvent") : receiver.Callback;
	UFunction* func = FindEventFunction(receiver.Actor, callback);
	if (!func)
		return;

	UStructProperty* paramsProp = nullptr;
	for (UProperty* prop : func->Properties)
	{
		if (AnyFlags(prop->PropFlags, PropertyFlags::Parm) && !AnyFlags(prop->PropFlags, PropertyFlags::ReturnParm))
		{
			if (auto structProp = UObject::TryCast<UStructProperty>(prop))
				paramsProp = structProp;
		}
	}
	if (!paramsProp || !paramsProp->Struct)
		return;

	struct RawStruct { uint8_t Byte; };
	ExpressionValue params = ExpressionValue::PropertyValue(paramsProp);
	void* data = &params.ToType<RawStruct&>();
	for (UProperty* prop : paramsProp->Struct->Properties)
	{
		void* field = static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset;
		if (prop->Name == "bestActor")
			*static_cast<UActor**>(field) = receiver.BestActor;
		else if (prop->Name == "Score")
			*static_cast<float*>(field) = receiver.BestScore;
		else if (prop->Name == "Visibility")
			*static_cast<float*>(field) = receiver.BestVisibility;
		else if (prop->Name == "Volume")
			*static_cast<float*>(field) = receiver.BestVolume;
		else if (prop->Name == "Smell")
			*static_cast<float*>(field) = receiver.BestSmell;
	}

	CallEvent(receiver.Actor, callback, {
		ExpressionValue::NameValue(type.Name),
		ExpressionValue::ByteValue(state),
		std::move(params) });
}

void UEventManager::QueueCall(EventType& type, Receiver& receiver, uint8_t state)
{
	if (!receiver.CallDue)
		DueCalls.push_back({ &type, &receiver });
	receiver.CallDue = true;
	receiver.CallState = state;
}

bool UEventManager::ProcessReceiver(EventType& type, Receiver& receiver)
{
	UActor* actor = receiver.Actor;

	// Which senders count: a receiver drawn in the last 5 s or within
	// 1,200 units of the player, and not in stasis, weighs every sender;
	// any other only those within 400 units.
	float now = actor->Level()->TimeSeconds();
	bool near = (now - actor->LastRenderTime() < 5.0f || actor->DistanceFromPlayer() <= 1200.0f) && !actor->InStasis();

	struct Candidate
	{
		UEventManager::Sender* From;
		float Score;
	};
	std::vector<Candidate> candidates;
	for (auto& sender : type.Senders)
	{
		if (sender->Delete || !sender->Actor || sender->Actor == actor)
			continue;
		vec3 delta = sender->Actor->Location() - actor->Location();
		if (!near && dot(delta, delta) > 400.0f * 400.0f)
			continue;

		// A sender's score is its distance squared plus 1, or what the
		// receiver's score callback returns; 0 or less drops the sender.
		float score = dot(delta, delta) + 1.0f;
		if (!receiver.ScoreCallback.IsNone())
			score = CallEvent(actor, receiver.ScoreCallback, {
				ExpressionValue::ObjectValue(actor),
				ExpressionValue::ObjectValue(sender->Actor),
				ExpressionValue::FloatValue(score) }).ToFloat();
		if (score <= 0.0f)
			continue;
		if (candidates.size() >= 256)
		{
			LogMessage("Event manager: more than 256 senders for " + type.Name.ToString() + ", the rest are ignored");
			break;
		}
		candidates.push_back(Candidate{ sender.get(), score });
	}

	bool weighed = !candidates.empty();

	int slotsBack = FrameCounter - receiver.LastTurnFrame;
	if (slotsBack > NumSlots)
	{
		LogMessage("Event manager not cycling quickly enough");
		slotsBack = NumSlots;
	}
	if (slotsBack < 1)
		slotsBack = 1;
	receiver.LastTurnFrame = FrameCounter;

	// The best sender: in order of score, lowest first -- the nearest, by
	// default -- the first the receiver senses.
	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.Score < b.Score; });
	Sender* best = nullptr;
	float visibility = 0.0f, volume = 0.0f, smell = 0.0f;
	float bestScore = 0.0f;
	for (Candidate& candidate : candidates)
	{
		SenseLevels peak = PeakLevels(*candidate.From, slotsBack);
		if (!peak.Any())
			continue;
		ComputeSenseDetection(receiver, candidate.From->Actor, peak, visibility, volume, smell);
		if (visibility > 0.0f || volume > 0.0f || smell > 0.0f)
		{
			best = candidate.From;
			bestScore = candidate.Score;
			break;
		}
	}

	// The state the receiver is called for. An event already on:
	// ChangeBest for a new best sender, End for none. An event off: Begin
	// for a best sender with a current level in some sense, turning the
	// event on; Pulse for one with pulses only, leaving it off.
	if (receiver.EventOn)
	{
		if (!best)
		{
			receiver.EventOn = false;
			receiver.BestActor = nullptr;
			receiver.BestScore = 0.0f;
			receiver.BestVisibility = 0.0f;
			receiver.BestVolume = 0.0f;
			receiver.BestSmell = 0.0f;
			QueueCall(type, receiver, StateEnd);
		}
		else if (best->Actor != receiver.BestActor)
		{
			receiver.BestActor = best->Actor;
			receiver.BestScore = bestScore;
			receiver.BestVisibility = visibility;
			receiver.BestVolume = volume;
			receiver.BestSmell = smell;
			QueueCall(type, receiver, StateChangeBest);
		}
	}
	else if (best)
	{
		receiver.BestActor = best->Actor;
		receiver.BestScore = bestScore;
		receiver.BestVisibility = visibility;
		receiver.BestVolume = volume;
		receiver.BestSmell = smell;
		if (best->Current.Any())
		{
			receiver.EventOn = true;
			QueueCall(type, receiver, StateBegin);
		}
		else
		{
			QueueCall(type, receiver, StatePulse);
		}
	}
	return weighed;
}

void UEventManager::Tick()
{
	using clock = std::chrono::steady_clock;
	auto start = clock::now();
	FrameCounter++;

	// The receivers in ring order, from where the last frame stopped, until
	// each has had a turn or 2 ms have passed. The clock is read only after
	// a receiver that had a sender to weigh, so one such receiver has its
	// turn each frame.
	size_t turns = Ring.size();
	for (size_t i = 0; i < turns && !Ring.empty(); i++)
	{
		if (RingPos >= Ring.size())
			RingPos = 0;
		auto entry = Ring[RingPos];
		RingPos++;
		Receiver& receiver = *entry.second;
		if (receiver.Delete || !receiver.Actor || receiver.Actor->bDeleteMe())
			continue;
		bool weighed = ProcessReceiver(*entry.first, receiver);
		if (weighed && clock::now() - start > std::chrono::milliseconds(2))
			break;
	}

	// The ring moves on: each sender's next slot starts at its current
	// levels, and a sender with nothing left in its 16 slots is deleted.
	SlotIndex = (SlotIndex + 1) % NumSlots;
	for (auto& [name, type] : Events)
	{
		for (auto& sender : type.Senders)
		{
			sender->Slots[SlotIndex] = sender->Current;
			bool anything = false;
			for (const SenseLevels& slot : sender->Slots)
			{
				if (slot.Any())
				{
					anything = true;
					break;
				}
			}
			if (!anything)
				sender->Delete = true;
		}
	}

	// The calls, after the pass and with the senders' slots moved on, as the
	// original's: a pulse a call raises lands in the next frame's slot, for
	// every listener to weigh, not in this frame's after some had their turn.
	std::vector<std::pair<EventType*, Receiver*>> calls;
	calls.swap(DueCalls);
	for (auto& [type, receiver] : calls)
	{
		receiver->CallDue = false;
		if (receiver->Delete || !receiver->Actor || receiver->Actor->bDeleteMe())
			continue;
		CallListener(*type, *receiver, receiver->CallState);
	}

	CleanupEvents();
}

void UEventManager::CleanupEvents()
{
	// Deleting is deferred to here, outside AIProcess. Event types stay for
	// the level's life.
	for (auto& [name, type] : Events)
	{
		type.Senders.erase(std::remove_if(type.Senders.begin(), type.Senders.end(),
			[](const std::unique_ptr<Sender>& s) { return s->Delete; }), type.Senders.end());

		bool anyReceiver = false;
		for (auto& receiver : type.Receivers)
			anyReceiver = anyReceiver || receiver->Delete;
		if (anyReceiver)
		{
			size_t pos = 0;
			for (size_t i = 0; i < Ring.size(); i++)
			{
				if (!Ring[i].second->Delete)
				{
					Ring[pos] = Ring[i];
					if (RingPos == i)
						RingPos = pos;
					pos++;
				}
				else if (RingPos > i)
				{
					RingPos--;
				}
			}
			Ring.resize(pos);
			type.Receivers.erase(std::remove_if(type.Receivers.begin(), type.Receivers.end(),
				[](const std::unique_ptr<Receiver>& r) { return r->Delete; }), type.Receivers.end());
		}
	}
}

void UEventManager::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	// A save the fork made before 2026-09-27 has the fork's own layout.
	uint32_t start = stream->Tell();
	if (stream->Remaining() >= 8 && stream->ReadInt32() == EventManagerMagic)
	{
		LoadForkLayout(stream);
		return;
	}
	stream->Seek(start);

	// The original's layout (UEventManager::Serialize, Engine.dll
	// 0x103825f0): its level, three counters, the receiver the process ring
	// starts at, and 256 hash buckets of event types.
	stream->ReadObject<UObject>(); // level
	stream->ReadInt32(); // refProcessing
	stream->ReadInt32(); // deleteCount
	int32_t currentSlot = stream->ReadInt32();
	UAIReceiverEvent* firstProcess = stream->ReadObject<UAIReceiverEvent>();
	UAIEventType* buckets[256];
	for (int i = 0; i < 256; i++)
		buckets[i] = stream->ReadObject<UAIEventType>();
	stream->ThrowIfNotEnd();

	SlotIndex = currentSlot & (NumSlots - 1);

	// Each type's senders and receivers into the manager's lists, the ring
	// then in the original's process order.
	std::map<UAIReceiverEvent*, std::pair<EventType*, Receiver*>> made;
	for (UAIEventType* savedType : buckets)
	{
		for (int guard = 0; savedType && guard < 4096; guard++)
		{
			savedType->LoadNow();
			EventType& type = GetEventType(savedType->EventName);
			for (UAISenderEvent* saved = UObject::TryCast<UAISenderEvent>(savedType->Senders); saved; saved = UObject::TryCast<UAISenderEvent>(saved->NextEvent))
			{
				saved->LoadNow();
				if (saved->bBeingDestroyed || !saved->EventActor)
					continue;
				Sender* sender = FindSender(type, saved->EventActor, true);
				for (int i = 0; i < NumSlots; i++)
					sender->Slots[i] = saved->Settings[i];
				sender->Current = saved->CurrentSettings;
			}
			for (UAIReceiverEvent* saved = UObject::TryCast<UAIReceiverEvent>(savedType->Receivers); saved; saved = UObject::TryCast<UAIReceiverEvent>(saved->NextEvent))
			{
				saved->LoadNow();
				if (saved->bBeingDestroyed || !saved->EventActor)
					continue;
				SetEventCallback(saved->EventActor, type.Name, saved->Callback, saved->ScoreCallback, saved->bCheckVisibility, saved->bCheckDir, saved->bCheckCylinder, saved->bCheckLOS);
				Receiver* receiver = FindReceiver(type, saved->EventActor);
				receiver->EventOn = saved->bEventOn != 0;
				receiver->BestActor = saved->BestSender;
				receiver->BestScore = saved->BestScore;
				// Its next turn weighs the slots from its nextSlot on.
				receiver->LastTurnFrame = FrameCounter - ((SlotIndex - saved->NextSlot) & (NumSlots - 1));
				made[saved] = { &type, receiver };
			}
			savedType = UObject::TryCast<UAIEventType>(savedType->NextEventType);
		}
	}

	if (firstProcess && made.find(firstProcess) != made.end())
	{
		std::vector<std::pair<EventType*, Receiver*>> ring;
		UAIReceiverEvent* cur = firstProcess;
		for (size_t guard = 0; cur && guard <= made.size(); guard++)
		{
			auto it = made.find(cur);
			if (it == made.end())
				break;
			ring.push_back(it->second);
			cur = UObject::TryCast<UAIReceiverEvent>(cur->NextProcess);
			if (cur == firstProcess)
				break;
		}
		if (ring.size() == Ring.size())
		{
			Ring = std::move(ring);
			RingPos = 0;
		}
	}
}

void UEventManager::LoadForkLayout(ObjectStream* stream)
{
	stream->ReadInt32(); // version
	int typeCount = stream->ReadIndex();
	for (int i = 0; i < typeCount; i++)
	{
		NameString name = stream->ReadName();
		EventType& type = GetEventType(name);

		int senderCount = stream->ReadIndex();
		for (int j = 0; j < senderCount; j++)
		{
			UActor* actor = stream->ReadObject<UActor>();
			SenseLevels current;
			current.Visual = stream->ReadFloat();
			current.Audio = stream->ReadFloat();
			current.AudioRadius = stream->ReadFloat();
			current.Smell = stream->ReadFloat();
			if (actor)
			{
				Sender* sender = FindSender(type, actor, true);
				sender->Current = current;
				sender->Slots[SlotIndex] = current;
			}
		}

		int receiverCount = stream->ReadIndex();
		for (int j = 0; j < receiverCount; j++)
		{
			UActor* actor = stream->ReadObject<UActor>();
			NameString callback = stream->ReadName();
			NameString scoreCallback = stream->ReadName();
			uint8_t flags = stream->ReadUInt8();
			UActor* bestActor = stream->ReadObject<UActor>();
			float score = stream->ReadFloat();
			float vis = stream->ReadFloat();
			float vol = stream->ReadFloat();
			float sm = stream->ReadFloat();
			if (actor)
			{
				SetEventCallback(actor, name, callback, scoreCallback, flags & 1, flags & 2, flags & 4, flags & 8);
				Receiver* receiver = FindReceiver(type, actor);
				receiver->EventOn = flags & 16;
				receiver->BestActor = bestActor;
				receiver->BestScore = score;
				receiver->BestVisibility = vis;
				receiver->BestVolume = vol;
				receiver->BestSmell = sm;
			}
		}
	}
	stream->ThrowIfNotEnd();
}

void UEventManager::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	// Saved in the original's layout, so either engine loads the other's
	// save: the marked events go first, as the original's Serialize cleans
	// them up; each type, sender and receiver becomes an object of its own,
	// owned by the manager.
	CleanupEvents();

	Package* pkg = package;
	UClass* typeClass = engine->packages->GetPackage("Engine")->GetClass("AIEventType");
	UClass* senderClass = engine->packages->GetPackage("Engine")->GetClass("AISenderEvent");
	UClass* receiverClass = engine->packages->GetPackage("Engine")->GetClass("AIReceiverEvent");
	// Named as the original names them, by class and a count of each.
	static int typeCount = 0, senderCount = 0, receiverCount = 0;
	auto make = [&](UClass* cls, int& count) -> UObject*
	{
		UObject* obj = pkg->NewObject(NameString(cls->Name.ToString() + std::to_string(count++)), cls, ObjectFlags::LoadContextFlags);
		obj->Outer() = this;
		return obj;
	};

	// A type in its bucket by the original's hash, the bucket's types in the
	// order of their names, case aside as its _wcsicmp does, in lowercase
	// (FindEvent, 0x103834b0); each list's events in the order they came.
	std::map<Receiver*, UAIReceiverEvent*> savedReceivers;
	std::vector<std::pair<std::string, UAIEventType*>> bucketTypes[256];
	for (auto& [name, type] : Events)
	{
		UAIEventType* savedType = UObject::Cast<UAIEventType>(make(typeClass, typeCount));
		savedType->EventName = type.Name;
		uint32_t hash = StrTools::ue1_strihash(type.Name.ToString());
		savedType->EventHash = (int32_t)hash;

		UAIEvent* last = nullptr;
		for (auto& sender : type.Senders)
		{
			if (sender->Delete || !sender->Actor)
				continue;
			UAISenderEvent* saved = UObject::Cast<UAISenderEvent>(make(senderClass, senderCount));
			saved->EventType = savedType;
			saved->EventActor = sender->Actor;
			for (int i = 0; i < NumSlots; i++)
				saved->Settings[i] = sender->Slots[i];
			saved->CurrentSettings = sender->Current;
			if (last)
				last->NextEvent = saved;
			else
				savedType->Senders = saved;
			last = saved;
		}

		last = nullptr;
		for (auto& receiver : type.Receivers)
		{
			if (receiver->Delete || !receiver->Actor)
				continue;
			UAIReceiverEvent* saved = UObject::Cast<UAIReceiverEvent>(make(receiverClass, receiverCount));
			saved->EventType = savedType;
			saved->EventActor = receiver->Actor;
			saved->Callback = receiver->Callback;
			saved->ScoreCallback = receiver->ScoreCallback;
			saved->bCheckVisibility = receiver->bCheckVisibility;
			saved->bCheckDir = receiver->bCheckDir;
			saved->bCheckCylinder = receiver->bCheckCylinder;
			saved->bCheckLOS = receiver->bCheckLOS;
			saved->bEventOn = receiver->EventOn;
			saved->BestScore = receiver->BestScore;
			saved->BestSender = receiver->BestActor;
			// The first slot its next turn weighs: the current one for a
			// receiver that had its turn in the last frame.
			int behind = std::clamp(FrameCounter - receiver->LastTurnFrame, 0, NumSlots - 1);
			saved->NextSlot = (SlotIndex - behind) & (NumSlots - 1);
			savedReceivers[receiver.get()] = saved;
			if (last)
				last->NextEvent = saved;
			else
				savedType->Receivers = saved;
			last = saved;
		}

		bucketTypes[hash & 255].push_back({ type.Name.ToString(), savedType });
	}

	UAIEventType* buckets[256] = {};
	for (int i = 0; i < 256; i++)
	{
		auto& types = bucketTypes[i];
		std::sort(types.begin(), types.end(), [](const auto& a, const auto& b)
		{
			const std::string& x = a.first;
			const std::string& y = b.first;
			for (size_t k = 0; k < x.size() && k < y.size(); k++)
			{
				int cx = std::tolower((unsigned char)x[k]);
				int cy = std::tolower((unsigned char)y[k]);
				if (cx != cy)
					return cx < cy;
			}
			return x.size() < y.size();
		});
		for (size_t k = 0; k < types.size(); k++)
			types[k].second->NextEventType = k + 1 < types.size() ? types[k + 1].second : nullptr;
		buckets[i] = types.empty() ? nullptr : types.front().second;
	}

	// The process ring, from where the manager resumes.
	UAIReceiverEvent* firstProcess = nullptr;
	std::vector<UAIReceiverEvent*> ring;
	for (size_t k = 0; k < Ring.size(); k++)
	{
		auto it = savedReceivers.find(Ring[(RingPos + k) % Ring.size()].second);
		if (it != savedReceivers.end())
			ring.push_back(it->second);
	}
	for (size_t k = 0; k < ring.size(); k++)
	{
		ring[k]->NextProcess = ring[(k + 1) % ring.size()];
		ring[k]->PrevProcess = ring[(k + ring.size() - 1) % ring.size()];
	}
	if (!ring.empty())
		firstProcess = ring.front();

	// Its level: the original's AIProcess does nothing without one.
	ULevel* level = engine->Level && engine->Level->package == package ? engine->Level : nullptr;
	if (!level)
		LogMessage("An event manager was saved without its level; the original's NPCs would not sense its events");
	stream->WriteObject(level);
	stream->WriteInt32(0); // refProcessing
	stream->WriteInt32(0); // deleteCount
	stream->WriteInt32(SlotIndex);
	stream->WriteObject(firstProcess);
	for (int i = 0; i < 256; i++)
		stream->WriteObject(buckets[i]);
}

/////////////////////////////////////////////////////////////////////////////

void UAIEventType::Load(ObjectStream* stream)
{
	UObject::Load(stream);
	EventName = stream->ReadName();
	EventHash = stream->ReadInt32();
	Senders = stream->ReadObject<UObject>();
	Receivers = stream->ReadObject<UObject>();
	NextEventType = stream->ReadObject<UObject>();
}

void UAIEventType::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);
	stream->WriteName(EventName);
	stream->WriteInt32(EventHash);
	stream->WriteObject(Senders);
	stream->WriteObject(Receivers);
	stream->WriteObject(NextEventType);
}

void UAIEvent::Load(ObjectStream* stream)
{
	UObject::Load(stream);
	EventType = stream->ReadObject<UObject>();
	EventActor = stream->ReadObject<UActor>();
	bBeingDestroyed = stream->ReadInt32();
	NextEvent = stream->ReadObject<UObject>();
}

void UAIEvent::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);
	stream->WriteObject(EventType);
	stream->WriteObject(EventActor);
	stream->WriteInt32(bBeingDestroyed);
	stream->WriteObject(NextEvent);
}

void UAISenderEvent::Load(ObjectStream* stream)
{
	UAIEvent::Load(stream);
	auto read = [&](UEventManager::SenseLevels& levels)
	{
		levels.Visual = stream->ReadFloat();
		levels.Audio = stream->ReadFloat();
		levels.AudioRadius = stream->ReadFloat();
		levels.Smell = stream->ReadFloat();
	};
	for (auto& settings : Settings)
		read(settings);
	read(CurrentSettings);
}

void UAISenderEvent::Save(PackageStreamWriter* stream)
{
	UAIEvent::Save(stream);
	auto write = [&](const UEventManager::SenseLevels& levels)
	{
		stream->WriteFloat(levels.Visual);
		stream->WriteFloat(levels.Audio);
		stream->WriteFloat(levels.AudioRadius);
		stream->WriteFloat(levels.Smell);
	};
	for (const auto& settings : Settings)
		write(settings);
	write(CurrentSettings);
}

void UAIReceiverEvent::Load(ObjectStream* stream)
{
	UAIEvent::Load(stream);
	Callback = stream->ReadName();
	ScoreCallback = stream->ReadName();
	bInvokeCallback = stream->ReadInt32();
	EventState = stream->ReadUInt8();
	bCheckVisibility = stream->ReadInt32();
	bCheckDir = stream->ReadInt32();
	bCheckCylinder = stream->ReadInt32();
	bCheckLOS = stream->ReadInt32();
	bEventOn = stream->ReadInt32();
	BestScore = stream->ReadFloat();
	BestSender = stream->ReadObject<UActor>();
	NextSlot = stream->ReadInt32();
	NextProcess = stream->ReadObject<UObject>();
	PrevProcess = stream->ReadObject<UObject>();
}

void UAIReceiverEvent::Save(PackageStreamWriter* stream)
{
	UAIEvent::Save(stream);
	stream->WriteName(Callback);
	stream->WriteName(ScoreCallback);
	stream->WriteInt32(bInvokeCallback);
	stream->WriteUInt8(EventState);
	stream->WriteInt32(bCheckVisibility);
	stream->WriteInt32(bCheckDir);
	stream->WriteInt32(bCheckCylinder);
	stream->WriteInt32(bCheckLOS);
	stream->WriteInt32(bEventOn);
	stream->WriteFloat(BestScore);
	stream->WriteObject(BestSender);
	stream->WriteInt32(NextSlot);
	stream->WriteObject(NextProcess);
	stream->WriteObject(PrevProcess);
}

void UEventManager::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	// A sender or receiver whose actor is eliminated is dropped, as
	// DestroyActor drops a destroyed one.
	bool cleared = false;
	marker.SetField("Events");
	for (auto& [name, type] : Events)
	{
		for (auto& sender : type.Senders)
		{
			if (!sender->Actor)
				continue;
			marker.Mark(sender->Actor);
			if (!sender->Actor)
			{
				sender->Delete = true;
				cleared = true;
			}
		}
		for (auto& receiver : type.Receivers)
		{
			marker.Mark(receiver->BestActor);
			if (!receiver->Actor)
				continue;
			marker.Mark(receiver->Actor);
			if (!receiver->Actor)
			{
				receiver->Delete = true;
				cleared = true;
			}
		}
	}
	if (cleared)
	{
		DueCalls.clear();
		CleanupEvents();
	}
}

void UAIEventType::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	marker.SetField("chain");
	marker.Mark(Senders);
	marker.Mark(Receivers);
	marker.Mark(NextEventType);
}

void UAIEvent::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	marker.SetField("chain");
	marker.Mark(EventType);
	marker.Mark(EventActor);
	marker.Mark(NextEvent);
}

void UAIReceiverEvent::Mark(GCMarker& marker)
{
	UAIEvent::Mark(marker);
	marker.Mark(BestSender);
	marker.Mark(NextProcess);
	marker.Mark(PrevProcess);
}
