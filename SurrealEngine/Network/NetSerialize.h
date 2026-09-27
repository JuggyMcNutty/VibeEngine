#pragma once

#include "NetBits.h"

class UProperty;
class NetPackageMap;

// One replicated value in or out (each property type's NetSerializeItem,
// docs/re/network.md, numbering): a bool its bit, a byte its enum's bits, a
// vector, rotator and plane packed, a struct member by member. Writing says
// whether every reference in it could be resolved on the other side yet.
void NetReadItem(UProperty* prop, NetBitReader& reader, NetPackageMap& map, void* data);
bool NetWriteItem(UProperty* prop, NetBitWriter& writer, NetPackageMap& map, const void* data);

// Whether a value is its type's zero, which a call's parameter leaves unsent.
bool NetIsZero(UProperty* prop, const void* data);
