#pragma once

#include "UObject.h"

class UField : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

protected:
	// Loaded code is never collected: a native class loaded again would
	// register its natives twice. A field made at run time, in no package
	// (a call's temporary property), goes like any other object.
	bool IsGCRoot() const override { return package != nullptr; }
	void Mark(GCMarker& marker) override;

public:
	UField* BaseField = nullptr;
	UField* Next = nullptr;
};
