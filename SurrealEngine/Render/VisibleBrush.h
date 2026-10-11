#pragma once

class VisibleFrame;
class UActor;
class UModel;
class UMover;
struct Poly;

class VisibleBrush
{
public:
	bool Draw(VisibleFrame* frame, UActor* actor, bool translucentPass);

private:
	bool DrawBrushPoly(VisibleFrame* frame, UModel* model, const Poly& poly, UMover* mover, bool translucentPass);
};
