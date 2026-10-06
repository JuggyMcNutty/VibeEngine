#pragma once

#include "UModel.h"

class UBspNodes : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	Array<BspNode> Nodes;
	Array<ZoneProperties> Zones;
};
