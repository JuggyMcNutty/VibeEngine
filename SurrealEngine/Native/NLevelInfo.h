#pragma once

#include "Packages/Core/UObject.h"

struct PointRegion;

class NLevelInfo
{
public:
	static void RegisterFunctions();

	static void GetAddressURL(UObject* Self, std::string& ReturnValue);
	static void GetLocalURL(UObject* Self, std::string& ReturnValue);
	static void InitEventManager(UObject* Self);
};
