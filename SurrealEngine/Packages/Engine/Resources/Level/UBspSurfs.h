#pragma once

#include "UModel.h"

class UBspSurfs : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	Array<BspSurface> Surfaces;
};
