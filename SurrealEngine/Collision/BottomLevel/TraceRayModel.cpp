
#include "Precomp.h"
#include "TraceRayModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"

// A model with no BSP (a brush never built) is hit by nothing, as the
// original's.
CollisionHitList TraceRayModel::Trace(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly)
{
	Model = model;
	CollisionHitList hits;
	if (!Model || Model->Nodes.empty())
		return hits;
	Trace(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes.front(), hits, 0.0, tmax);
	hits.SortByFraction();
	return hits;
}

bool TraceRayModel::TraceAnyHit(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly)
{
	Model = model;
	if (!Model || Model->Nodes.empty())
		return false;
	return TraceAnyHit(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes.front(), 0.0, tmax);
}

// A node is a solid boundary (the original's FBspNode::IsCsg) when it has a
// polygon and none of the flags asked: NF_NotCsg, NF_IsNew and what the caller
// passes but for the bits the walk keeps for itself (NF_NotCsg, NF_IsNew and
// 0x10). Down the tree the front of a boundary is empty space and its back
// solid, a node that is no boundary leaving the space as it was
// (ChildOutside). A segment with both ends within a thousandth of a unit of a
// plane counts as in front of it, as one with both ends past it.
namespace
{
	struct SolidWalk
	{
		UModel* Model;
		uint8_t Extra;
		bool WasOutside = false;
		double Fraction = 0.0;
		int Node = 0;

		bool Csg(const BspNode& node) const
		{
			return node.NumVertices > 0 && (node.NodeFlags & (NF_NotCsg | NF_IsNew | (Extra & 0xCE))) == 0;
		}

		// False once the line stops
		bool Walk(int iParent, int iNode, dvec3 start, dvec3 end, double t0, double t1, bool outside)
		{
			while (true)
			{
				if (iNode < 0)
				{
					if (outside)
					{
						WasOutside = true;
						return true;
					}
					if (WasOutside || (Extra & 0x10) == 0)
					{
						Fraction = t0;
						Node = iParent;
						return false;
					}
					return true;
				}
				const BspNode& node = Model->Nodes[iNode];
				dvec4 plane = { node.PlaneX, node.PlaneY, node.PlaneZ, -node.PlaneW };
				double d0 = dot(dvec4(start, 1.0), plane);
				double d1 = dot(dvec4(end, 1.0), plane);
				if (d0 > -0.001 && d1 > -0.001)
				{
					outside = outside || Csg(node);
					iNode = node.Front;
					continue;
				}
				if (d0 < 0.001 && d1 < 0.001)
				{
					outside = outside && !Csg(node);
					iNode = node.Back;
					continue;
				}
				double f = d0 / (d0 - d1);
				dvec3 mid = start + (end - start) * f;
				double tm = t0 + (t1 - t0) * f;
				bool nearFront = d0 > 0.0;
				bool csg = Csg(node);
				if (!Walk(iParent, nearFront ? node.Front : node.Back, start, mid, t0, tm, nearFront ? (outside || csg) : (outside && !csg)))
					return false;
				outside = nearFront ? (outside && !csg) : (outside || csg);
				iParent = iNode;
				iNode = nearFront ? node.Back : node.Front;
				start = mid;
				t0 = tm;
			}
		}
	};

	bool FastWalkNode(UModel* model, int iNode, dvec3 start, dvec3 end, bool outside)
	{
		while (iNode >= 0)
		{
			const BspNode& node = model->Nodes[iNode];
			dvec4 plane = { node.PlaneX, node.PlaneY, node.PlaneZ, -node.PlaneW };
			double d0 = dot(dvec4(start, 1.0), plane);
			double d1 = dot(dvec4(end, 1.0), plane);
			bool notCsg = (node.NodeFlags & NF_NotCsg) != 0;
			bool g0 = !std::signbit(d0);
			bool g1 = !std::signbit(d1);
			if (g0 != g1)
			{
				dvec3 mid = start + (end - start) * (d0 / (d0 - d1));
				if (!FastWalkNode(model, g1 ? node.Front : node.Back, mid, end, notCsg ? outside : g1))
					return false;
				end = mid;
			}
			outside = notCsg ? outside : g0;
			iNode = g0 ? node.Front : node.Back;
		}
		return outside;
	}
}

bool TraceRayModel::WalkSolid(UModel* model, const dvec3& start, const dvec3& end, uint8_t extraNodeFlags, double& fraction, BspNode*& crossed)
{
	// A model with no BSP answers every check with no hit, as the original's
	if (!model || model->Nodes.empty())
		return false;
	SolidWalk walk{ model, extraNodeFlags };
	if (walk.Walk(0, 0, start, end, 0.0, 1.0, model->RootOutside != 0))
		return false;
	fraction = walk.Fraction;
	crossed = &model->Nodes[walk.Node];
	return true;
}

bool TraceRayModel::FastWalk(UModel* model, const dvec3& start, const dvec3& end)
{
	if (!model || model->Nodes.empty())
		return true;
	return FastWalkNode(model, 0, start, end, model->RootOutside != 0);
}

BspNode* TraceRayModel::CoplanarNode(UModel* model, const dvec3& point, const dvec3& start, const dvec3& end)
{
	if (!model || model->Nodes.empty())
		return nullptr;
	int iNode = 0;
	while (iNode >= 0)
	{
		BspNode& node = model->Nodes[iNode];
		dvec4 plane = { node.PlaneX, node.PlaneY, node.PlaneZ, -node.PlaneW };
		double d = dot(dvec4(point, 1.0), plane);
		double dEnd = dot(dvec4(end, 1.0), plane);
		bool onPlane = d > -0.001 && d < 0.001;
		if (onPlane)
		{
			double dStart = dot(dvec4(start, 1.0), plane);
			bool csg = node.NumVertices > 0 && (node.NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;
			if (csg && ((dStart >= 0.001 && dEnd <= -0.001) || (dStart <= -0.001 && dEnd >= 0.001)))
				return &node;
		}
		// The side the walk takes for a segment starting here: in front when
		// both ends are, else behind
		bool front = onPlane ? dEnd > -0.001 : d > 0.0;
		iNode = front ? node.Front : node.Back;
	}
	return nullptr;
}

// The traversal handed both children of a plane the whole segment whenever it
// touched their side, and a long sight line then touched both sides of
// plane after plane further down. A child now gets only the part of the
// segment [t0, t1] in its half-space, widened by SplitMargin units so that a
// polygon whose edge lies on the plane is still reached from either side.
// The old test stays too, so the nodes visited are a subset of those it
// visited, in the same order.
static const double SplitMargin = 1.0;

static bool SegmentPart(double dA, double dB, double t0, double t1, double sign, double& p0, double& p1)
{
	// The part where sign * distance >= -SplitMargin
	double a = sign * dA + SplitMargin, b = sign * dB + SplitMargin;
	if (a < 0.0 && b < 0.0)
		return false;
	if (a >= 0.0 && b >= 0.0)
	{
		p0 = t0;
		p1 = t1;
	}
	else
	{
		double ts = t0 + (t1 - t0) * (a / (a - b));
		if (a >= 0.0)
		{
			p0 = t0;
			p1 = ts;
		}
		else
		{
			p0 = ts;
			p1 = t1;
		}
	}
	return true;
}

void TraceRayModel::Trace(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, BspNode* node, CollisionHitList& hits, double t0, double t1)
{
	BspNode* polynode = node;
	while (true)
	{
		if (!visibilityOnly || (polynode->NodeFlags & NF_NotVisBlocking) == 0)
		{
			double t = NodeRayIntersect(origin, tmin, dirNormalized, tmax, polynode, t0, t1);
			if (t >= tmin && t < tmax)
			{
				CollisionHit hit = { (float)t, vec3(node->PlaneX, node->PlaneY, node->PlaneZ), nullptr, polynode, node };
				if (dot(to_dvec3(hit.Normal), dirNormalized) > 0.0)
					hit.Normal = -hit.Normal;
				hits.push_back(hit);
			}
		}

		if (polynode->Plane < 0) break;
		polynode = &Model->Nodes[polynode->Plane];
	}

	dvec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
	double fromSide = dot(dvec4(origin, 1.0), plane);
	double toSide = dot(dvec4(origin + dirNormalized * tmax, 1.0), plane);

	double dA = dot(dvec4(origin + dirNormalized * t0, 1.0), plane);
	double dB = dot(dvec4(origin + dirNormalized * t1, 1.0), plane);
	double p0, p1;
	if (node->Front >= 0 && (fromSide >= 0.0 || toSide >= 0.0) && SegmentPart(dA, dB, t0, t1, 1.0, p0, p1))
		Trace(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes[node->Front], hits, p0, p1);
	if (node->Back >= 0 && (fromSide <= 0.0 || toSide <= 0.0) && SegmentPart(dA, dB, t0, t1, -1.0, p0, p1))
		Trace(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes[node->Back], hits, p0, p1);
}

bool TraceRayModel::TraceAnyHit(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly, BspNode* node, double t0, double t1)
{
	BspNode* polynode = node;
	while (true)
	{
		if (!visibilityOnly || (polynode->NodeFlags & NF_NotVisBlocking) == 0)
		{
			double t = NodeRayIntersect(origin, tmin, dirNormalized, tmax, polynode, t0, t1);
			if (t >= tmin && t < tmax)
				return true;
		}

		if (polynode->Plane < 0) break;
		polynode = &Model->Nodes[polynode->Plane];
	}

	dvec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
	double fromSide = dot(dvec4(origin, 1.0), plane);
	double toSide = dot(dvec4(origin + dirNormalized * tmax, 1.0), plane);

	double dA = dot(dvec4(origin + dirNormalized * t0, 1.0), plane);
	double dB = dot(dvec4(origin + dirNormalized * t1, 1.0), plane);
	double p0, p1;
	if (node->Front >= 0 && (fromSide >= 0.0 || toSide >= 0.0) && SegmentPart(dA, dB, t0, t1, 1.0, p0, p1) && TraceAnyHit(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes[node->Front], p0, p1))
		return true;
	else if (node->Back >= 0 && (fromSide <= 0.0 || toSide <= 0.0) && SegmentPart(dA, dB, t0, t1, -1.0, p0, p1) && TraceAnyHit(origin, tmin, dirNormalized, tmax, visibilityOnly, &Model->Nodes[node->Back], p0, p1))
		return true;
	else
		return false;
}

double TraceRayModel::NodeRayIntersect(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, BspNode* node, double t0, double t1)
{
	// The plane tests come first: they read only the node's plane, the part of
	// it the traversal has just read, and turn away most nodes. Its vertex
	// count lies on another cache line and its surface's flags in another
	// array, so on the handheld those two tests were most of the time spent
	// here. Every test returns tmax, so their order changes nothing else.

	// Test if plane is actually crossed.
	dvec4 plane = { node->PlaneX, node->PlaneY, node->PlaneZ, -node->PlaneW };
	double fromSide = dot(dvec4(origin, 1.0), plane);
	double toSide = dot(dvec4(origin + dirNormalized * tmax, 1.0), plane);
	if ((fromSide > 0.0 && toSide > 0.0) || (fromSide < 0.0 && toSide < 0.0))
		return tmax;

	// A hit lies in the node's own part of the segment, [t0, t1]: when that
	// stays clear of the plane (by more than SplitMargin), there is none.
	double partA = dot(dvec4(origin + dirNormalized * t0, 1.0), plane);
	double partB = dot(dvec4(origin + dirNormalized * t1, 1.0), plane);
	if ((partA > SplitMargin && partB > SplitMargin) || (partA < -SplitMargin && partB < -SplitMargin))
		return tmax;

	if (node->NumVertices < 3 || (node->Surf >= 0 && Model->Surfaces[node->Surf].PolyFlags & PF_NotSolid))
		return tmax;

	BspVert* v = &Model->Vertices[node->VertPool];
	vec3* points = Model->Points.data();

	dvec3 p[3];
	p[0] = to_dvec3(points[v[0].Vertex]);
	p[1] = to_dvec3(points[v[1].Vertex]);

	double t = tmax;
	int count = node->NumVertices;
	for (int i = 2; i < count; i++)
	{
		p[2] = to_dvec3(points[v[i].Vertex]);
		double tval = TriangleRayIntersect(origin, dirNormalized, tmax, p);
		if (tval >= tmin)
			t = std::min(tval, t);
		p[1] = p[2];
	}
	return t;
}

double TraceRayModel::TriangleRayIntersect(const dvec3& origin, const dvec3& dirNormalized, double tmax, const dvec3* p)
{
	// Moeller-Trumbore ray-triangle intersection algorithm:

	// Find vectors for two edges sharing p[0]
	dvec3 e1 = p[1] - p[0];
	dvec3 e2 = p[2] - p[0];

	// Begin calculating determinant - also used to calculate u parameter
	dvec3 P = cross(dirNormalized, e2);
	double det = dot(e1, P);

	// Backface check
	//if (det < 0.0)
	//	return tmax;

	// If determinant is near zero, ray lies in plane of triangle
	if (det > -FLT_EPSILON && det < FLT_EPSILON)
		return tmax;

	double inv_det = 1.0 / det;

	// Calculate distance from p[0] to ray origin
	dvec3 T = origin - p[0];

	// Calculate u parameter and test bound
	double u = dot(T, P) * inv_det;

	// Check if the intersection lies outside of the triangle
	if (u < 0.f || u > 1.f)
		return tmax;

	// Prepare to test v parameter
	dvec3 Q = cross(T, e1);

	// Calculate V parameter and test bound
	double v = dot(dirNormalized, Q) * inv_det;

	// The intersection lies outside of the triangle
	if (v < 0.f || u + v  > 1.f)
		return tmax;

	double t = dot(e2, Q) * inv_det;
	if (t <= FLT_EPSILON)
		return tmax;

	// Return hit location on triangle in barycentric coordinates
	// barycentricB = u;
	// barycentricC = v;

	return std::min(t, tmax);
}
