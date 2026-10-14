
#include "Precomp.h"
#include "UActor.h"
#include "Engine.h"

void UActor::TickTrailer(float elapsed)
{
	if (!Owner())
		return;

	vec3 newLocation = Owner()->Location();

	if (bTrailerPrePivot())
	{
		newLocation += PrePivot();
	}

	SetLocation(newLocation, true);

	if (bTrailerSameRotation() && DrawType() != DT_Sprite)
	{
		SetRotation(Owner()->Rotation());
	}
}
