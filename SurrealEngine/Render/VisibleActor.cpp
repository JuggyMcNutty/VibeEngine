
#include "Precomp.h"
#include "VisibleActor.h"
#include "VisibleFrame.h"
#include "VisibleBrush.h"
#include "VisibleMesh.h"
#include "VisibleSprite.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"

void VisibleActor::Process(VisibleFrame* frame, UActor* actor)
{
	// Deus Ex coronas come from the viewer's leaf and are kept across
	// frames (RenderSubsystem::DrawCoronasDX), not gathered here
	if (actor->bCorona() && !engine->LaunchInfo.IsDeusEx())
	{
		frame->Coronas.push_back(actor);
	}

	if (actor->bHidden())
		return;

	UActor* viewportActor = engine->viewport->Actor();
	bool isOwnedByViewport = actor->IsOwnedBy(viewportActor);

	bool behindView = frame->PortalDepth > 0;
	if (UPawn* pawn = UObject::TryCast<UPawn>(viewportActor))
	{
		if (pawn->bBehindView())
			behindView = true;
	}

	if ((behindView || !isOwnedByViewport) && actor->bOnlyOwnerSee())
		return;

	if (engine->LaunchInfo.ue1Version > 219)
	{
		if (!behindView && isOwnedByViewport && actor->bOwnerNoSee())
			return;
	}

	if (!behindView && actor == engine->CameraActor) // Hide CameraActor as if bOwnerNoSee was set for it
		return;

	if (!frame->Clipper.IsAABBVisible(actor->BspInfo.BoundingBox))
		return;

	// Drawn for the depth buffer to hide; counted as drawn only if some of
	// it survives the world in front of it, as the original counts it
	actor->LastVisibleFrame = frame->FrameCounter;
	frame->AddOcclusionProxy(actor);

	EDrawType dt = (EDrawType)actor->DrawType();
	if (dt == DT_Mesh && actor->Mesh())
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
	else if ((dt == DT_Sprite || dt == DT_SpriteAnimOnce) && actor->Texture())
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
	else if (dt == DT_Brush && actor->Brush() && UActor::TryCast<UMover>(actor))
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
}

void VisibleActor::DrawOpaque(VisibleFrame* frame)
{
	if (Type == DT_Mesh)
	{
		VisibleMesh vismesh;
		if (vismesh.DrawMesh(frame, Actor, false, false))
		{
			vec3 v = Actor->Location() - frame->ViewLocation.xyz();
			frame->AddTranslucent(VisibleFrame::ActorTranslucentList(Actor), VisibleTranslucent(*this, dot(v, v)));
		}
	}
	else if (Type == DT_Sprite || Type == DT_SpriteAnimOnce)
	{
		// A sprite is drawn with the translucent parts, in the list its style takes
		vec3 v = Actor->Location() - frame->ViewLocation.xyz();
		frame->AddTranslucent(VisibleFrame::ActorTranslucentList(Actor), VisibleTranslucent(*this, dot(v, v)));
	}
	else if (Type == DT_Brush)
	{
		// A mover's translucent polygons go with the level's surfaces, as
		// the original files a mover's polygons into the level's BSP
		VisibleBrush visbrush;
		if (visbrush.Draw(frame, Actor, false))
		{
			vec3 v = Actor->Location() - frame->ViewLocation.xyz();
			frame->AddTranslucent(VisibleFrame::TranslucentSurfaces, VisibleTranslucent(*this, dot(v, v)));
		}
	}
}

void VisibleActor::DrawTranslucent(VisibleFrame* frame)
{
	if (Type == DT_Mesh)
	{
		VisibleMesh vismesh;
		vismesh.DrawMesh(frame, Actor, false, true);
	}
	else if (Type == DT_Sprite || Type == DT_SpriteAnimOnce)
	{
		VisibleSprite vissprite;
		vissprite.Draw(frame, Actor);
	}
	else if (Type == DT_Brush)
	{
		VisibleBrush visbrush;
		visbrush.Draw(frame, Actor, true);
	}
}
