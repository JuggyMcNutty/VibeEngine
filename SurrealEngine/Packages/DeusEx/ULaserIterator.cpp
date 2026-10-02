
#include "Precomp.h"
#include "ULaserIterator.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Math/coords.h"
#include "Utils/Random.h"

// The original's CurrentItem (DeusEx.dll 0x1001a1a0; dx-reverse-info/
// deusex-dll.md, particles and lasers). The script's AddBeam gives a beam
// its length / 16 + 1 segments (/ 15 + 1 for a random beam), and Init counts
// MaxItems as the active beams' segments plus one, and starts PrevLoc and
// SavedLoc at the emitter.
UActor* ULaserIterator::CurrentItem()
{
	UActor* proxy = Proxy();
	if (!proxy)
		return URenderIterator::CurrentItem();

	// The current item's beam: the first whose segments, counted on over the
	// active beams, pass the index -- past them all (the one extra item),
	// the first, at its end
	auto beams = Beams();
	int b = 0;
	int counted = 0;
	for (int i = 0; i < 8; i++)
	{
		if (beams[i].bActive)
		{
			counted += beams[i].NumSegments;
			if (Index() < counted)
			{
				b = i;
				break;
			}
		}
	}
	const DXBeam& beam = beams[b];

	// The k-th of a beam's N segments at k/N of its length
	float t = beam.NumSegments > 0 ? 1.0f - (float)(counted - Index()) / (float)beam.NumSegments : 1.0f;
	t = clamp(t, 0.0f, 1.0f);
	vec3 location = beam.Location + Coords::Rotation(beam.Rotation).XAxis * (beam.Length * t);
	Rotator rotation = beam.Rotation;

	if (bRandomBeam())
	{
		// The spot jittered by a random unit vector and the one before, the
		// segment aimed back at where the last one's would end, and that end
		// moved on as far again
		vec3 jitter;
		float len2;
		do
		{
			jitter = vec3(FRand() * 2.0f - 1.0f, FRand() * 2.0f - 1.0f, FRand() * 2.0f - 1.0f);
			len2 = dot(jitter, jitter);
		} while (len2 > 1.0f || len2 == 0.0f);
		jitter = jitter / std::sqrt(len2);
		location += jitter + PrevRand();
		vec3 next = location * 2.0f - PrevLoc();
		rotation = Rotator::FromVector(PrevLoc() - next);
		PrevLoc() = next;
		PrevRand() = jitter;
	}

	proxy->SetLocation(location);
	proxy->Rotation() = rotation;

	// One segment is drawn again at the end: while none is kept, this one
	// is, with the items so far over all of them for its chance
	UActor* emitter = UObject::TryCast<UActor>(Outer());
	if (emitter && SavedLoc().x == emitter->Location().x && SavedLoc().y == emitter->Location().y && SavedLoc().z == emitter->Location().z &&
		(float)NextItem() / (float)std::max(MaxItems(), 1) > FRand())
	{
		SavedLoc() = location;
		SavedRot() = rotation;
	}
	if (++NextItem() == MaxItems())
	{
		proxy->SetLocation(SavedLoc());
		proxy->Rotation() = SavedRot();
	}
	return proxy;
}
