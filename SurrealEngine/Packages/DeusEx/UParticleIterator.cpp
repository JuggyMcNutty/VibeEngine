
#include "Precomp.h"
#include "UParticleIterator.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Utils/Random.h"

// The original's behaviour: dx-reverse-info/deusex-dll.md, particles and lasers.

void UParticleIterator::UpdateParticles(float deltaTime)
{
	auto particles = Particles();
	float fullLife = OwnerLifeSpan();
	bool anyAlive = false;

	for (int i = 0; i < 64; i++)
	{
		DXParticle& p = particles[i];
		if (!p.bActive)
			continue;

		// Ages while its life is above 0 -- one whose life is 0 lives on --
		// and one whose life runs out is deleted
		if (p.LifeSpan > 0.0f)
		{
			p.LifeSpan -= deltaTime;
			if (p.LifeSpan <= 0.0f)
				p.LifeSpan = -1.0f;
		}
		if (p.LifeSpan < 0.0f)
		{
			DeleteParticle(i);
			continue;
		}
		anyAlive = true;
		float age = fullLife - p.LifeSpan;

		// Drifts: the initial horizontal velocity plus a new random offset
		// each update, from -3 to +2
		p.Velocity.x = p.InitVel.x + FRandRange(-3.0f, 2.0f);
		p.Velocity.y = p.InitVel.y + FRandRange(-3.0f, 2.0f);

		// Rises or falls: with the generator's gravity a fifth of the
		// zone's; else the initial Z velocity plus the age at the rise rate,
		// give or take a tenth
		if (bOwnerUsesGravity())
			p.Velocity.z += deltaTime * OwnerZoneGravity() * 0.2f;
		else
			p.Velocity.z = p.InitVel.z + FRandRange(0.9f, 1.1f) * (age * OwnerRiseRate());

		// Grows with age, (age + 1) times the generator's draw scale held to
		// 0.01 to 3, when asked
		p.DrawScale = bOwnerScales() ? clamp((age + 1.0f) * OwnerDrawScale(), 0.01f, 3.0f) : OwnerDrawScale();

		// Fades with its remaining life, when asked
		p.ScaleGlow = bOwnerFades() && fullLife != 0.0f ? p.LifeSpan / fullLife : 1.0f;

		// Moves by its velocity
		p.Location += p.Velocity * deltaTime;
	}

	// The proxy is hidden while no particle lives
	if (UActor* proxy = Proxy())
		proxy->bHidden() = !anyAlive;
}

UActor* UParticleIterator::CurrentItem()
{
	UActor* proxy = Proxy();
	int i = Index();
	if (!proxy || i < 0 || i >= 64)
		return nullptr;

	auto particles = Particles();
	DXParticle& p = particles[i];
	if (!p.bActive)
		return nullptr;

	vec3 location = p.Location;
	if (!proxy->SetLocation(location))
	{
		DeleteParticle(i);
		return nullptr;
	}
	proxy->DrawScale() = p.DrawScale;
	proxy->ScaleGlow() = p.ScaleGlow;
	return proxy;
}

void UParticleIterator::DeleteParticle(int i)
{
	auto particles = Particles();
	if (particles[i].bActive)
	{
		particles[i].bActive = 0;
		NextFreeParticle() = i;
	}
}
