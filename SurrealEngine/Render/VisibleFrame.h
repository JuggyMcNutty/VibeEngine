#pragma once

#include "Engine.h"
#include "VisibleNode.h"
#include "VisibleTranslucent.h"
#include "VisibleCorona.h"
#include "VisibleActor.h"
#include "VisiblePortal.h"
#include "BspClipper.h"
#include "RenderDevice/RenderDevice.h"

class VisibleFrame
{
public:
	void Process(const vec3& location, const mat4& worldToView, const Coords& viewRotation, bool mirrorFlag = false, int portalDepth = 0, const Array<PortalSpan>& portalSpans = {}, const vec4& portalPlane = vec4(0.0f, 0.0f, 0.0f, 1.0f));
	void Draw();
	void DrawCoronas();

	RenderDevice* Device = nullptr;

	SceneNode Frame;
	BspClipper Clipper;
	vec4 ViewLocation = vec4(0.0f);
	Coords ViewRotation = {};
	int ViewZone = 0;
	//uint64_t ViewZoneMask = 0;
	int FrameCounter = 0;
	bool MirrorFlag = false;
	int PortalDepth = 0;

	Array<VisibleNode> OpaqueNodes;
	Array<VisibleActor> Actors;
	Array<VisibleTranslucent> Translucents;
	Array<VisibleCorona> Coronas;
	Array<VisiblePortal> Portals;

	// Whether an actor counts as drawn, for its LastRenderTime: stamped only
	// if some piece of its proxy survives the world in front of it.
	void AddOcclusionProxy(UActor* actor);

	// The actor's rectangle on the frame, as the original's sprite setup
	// makes it (Render.dll FDynamicSprite::Setup 0x10b23910): a mesh's
	// render box through BoundVisible, a sprite's texture at its draw scale,
	// none for a location behind the viewer or another draw type. In the
	// view's x across and y up over the depth; z the location's depth.
	bool SpriteRectangle(UActor* actor, float& minX, float& minY, float& maxX, float& maxY, float& z);

private:
	// An actor's proxy for the occlusion test, as the original's sprites
	// have one: its screen rectangle set back in the world at its
	// location's depth, filtered down the BSP and split at its planes, each
	// piece tested at its own place in the front-to-back walk.
	struct OcclusionFragment
	{
		enum { MaxVerts = 12 };
		UActor* Actor = nullptr;
		int NumVerts = 0;
		vec3 Verts[MaxVerts];
	};

	void SetupSceneFrame(const mat4& worldToView);
	bool BoundRectangle(const BBox& box, float& minX, float& minY, float& maxX, float& maxY);
	void ProcessNode(BspNode* node, size_t fragFirst, size_t fragCount, bool outside);
	void SplitFragments(size_t first, size_t count, const vec4& plane, bool viewerInFront);
	void TestLeafFragments(size_t first, size_t count, bool outside, int zone);
	void TestFragment(int index);
	void MarkZoneSeen(int zone);

	Array<OcclusionFragment> Fragments;
	Array<int> FragmentStack;
	Array<int> NearFragments;
	Array<int> ZoneSeenFrame;
	mat3 ViewToWorld;
	vec3 ViewTranslation = vec3(0.0f);
	float FrustumTanX = 1.0f;       // the frame's half width and height over the depth
	float FrustumTanY = 1.0f;
	float Now = 0.0f;
	void ProcessNodeSurface(BspNode* node, bool front);
	void ProcessRenderIterators();
	void SortTranslucent();

	void DrawOpaqueNodes();
	void DrawOpaqueActors();
	void DrawTranslucent();
	void DrawPortals();

	int FindZoneAt(const vec3& location);
	int FindZoneAt(const vec4& location, BspNode* node, BspNode* nodes);

	vec3 WarpLocationToOtherSide(UWarpZoneInfo* warpZone, vec3 p);
	vec3 WarpNormalToOtherSide(UWarpZoneInfo* warpZone, vec3 n);
	Coords WarpRotationToOtherSide(UWarpZoneInfo* warpZone, Coords rotation);
};
