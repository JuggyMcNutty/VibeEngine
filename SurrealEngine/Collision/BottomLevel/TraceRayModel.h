#pragma once

#include "Packages/Engine/Resources/Level/ULevel.h"

class TraceRayModel
{
public:
	CollisionHitList Trace(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly);
	bool TraceAnyHit(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly);

	// Deus Ex: the original's line walk down a model's BSP (UModel::LineCheck's;
	// dx-reverse-info engine-dll.md, traces). The segment from start to end is
	// classified plane by plane, its near part first, down to a leaf; a leaf in
	// solid space stops the line where that part began -- at the start, when
	// the start lies in solid -- and credits the node whose plane the part
	// crossed to get there, the root when none was. Gives whether it stopped,
	// how far along the segment (0 to 1) and the node. extraNodeFlags as the
	// original's caller passes them (NF_NotVisBlocking for a sight line); bit
	// 0x10 lets a start in solid pass until the line has been in empty space.
	static bool WalkSolid(UModel* model, const dvec3& start, const dvec3& end, uint8_t extraNodeFlags, double& fraction, BspNode*& crossed);

	// Deus Ex: FastLineCheck's walk: whether the segment stays in empty space
	// down the BSP, only NF_NotCsg asked, a node's side by the sign bit of the
	// distance.
	static bool FastWalk(UModel* model, const dvec3& start, const dvec3& end);

	// Deus Ex: the shallowest solid-boundary node on the way down to a point
	// of the line from start to end whose plane holds the point, within a
	// thousandth, and parts the line's ends: the node the original's walk
	// crosses there (a filed mover's polygon in that plane is met as it).
	static BspNode* CoplanarNode(UModel* model, const dvec3& point, const dvec3& start, const dvec3& end);

private:
	void Trace(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, BspNode* node, CollisionHitList& hits, double t0, double t1);
	bool TraceAnyHit(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, BspNode* node, double t0, double t1);

	double NodeRayIntersect(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, BspNode* node, double t0, double t1);
	double TriangleRayIntersect(const dvec3& origin, const dvec3& dirNormalized, double tmax, const dvec3* points);

	UModel* Model = nullptr;
};
