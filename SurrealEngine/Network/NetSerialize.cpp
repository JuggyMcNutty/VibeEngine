
#include "Precomp.h"
#include "NetSerialize.h"
#include "NetPackageMap.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UEnum.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UByteProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Core/Properties/UIntProperty.h"
#include "Packages/Core/Properties/UNameProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStrProperty.h"
#include "Packages/Core/Properties/UStringProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <cmath>

namespace
{
	bool ObjectIsA(UObject* obj, UClass* cls)
	{
		for (UStruct* c = obj ? obj->Class : nullptr; c; c = c->BaseStruct)
		{
			if (c == cls)
				return true;
		}
		return false;
	}

	int CeilLogTwo(uint32_t value)
	{
		int bits = 0;
		while (bits < 32 && (1u << bits) < value)
			bits++;
		return bits;
	}

	int Round(float value)
	{
		return (int)std::lrint(value);
	}
}

void NetReadItem(UProperty* prop, NetBitReader& reader, NetPackageMap& map, void* data)
{
	if (auto boolProp = UObject::TryCast<UBoolProperty>(prop))
	{
		boolProp->SetBool(data, reader.ReadBit());
	}
	else if (auto byteProp = UObject::TryCast<UByteProperty>(prop))
	{
		uint8_t value = 0;
		if (byteProp->EnumType)
			reader.ReadBits(&value, CeilLogTwo((uint32_t)byteProp->EnumType->ElementNames.size()));
		else
			value = reader.ReadByte();
		*static_cast<uint8_t*>(data) = value;
	}
	else if (UObject::TryCast<UIntProperty>(prop))
	{
		*static_cast<int32_t*>(data) = reader.ReadInt32();
	}
	else if (UObject::TryCast<UFloatProperty>(prop))
	{
		*static_cast<float*>(data) = reader.ReadFloat();
	}
	else if (auto objProp = UObject::TryCast<UObjectProperty>(prop))
	{
		// One not of the property's class is dropped, as the original's
		// "forged object".
		UObject* obj = map.ReadObject(reader);
		if (obj && objProp->ObjectClass && !ObjectIsA(obj, objProp->ObjectClass))
			obj = nullptr;
		*static_cast<UObject**>(data) = obj;
	}
	else if (UObject::TryCast<UNameProperty>(prop))
	{
		*static_cast<NameString*>(data) = map.ReadName(reader);
	}
	else if (UObject::TryCast<UStrProperty>(prop) || UObject::TryCast<UStringProperty>(prop))
	{
		*static_cast<std::string*>(data) = reader.ReadString();
	}
	else if (auto structProp = UObject::TryCast<UStructProperty>(prop))
	{
		UStruct* s = structProp->Struct;
		if (s->Name == "Vector")
		{
			uint32_t bits = reader.ReadInt(16);
			int bias = 1 << (bits + 1);
			uint32_t max = 1u << (bits + 2);
			int x = (int)reader.ReadInt(max) - bias;
			int y = (int)reader.ReadInt(max) - bias;
			int z = (int)reader.ReadInt(max) - bias;
			*static_cast<vec3*>(data) = vec3((float)x, (float)y, (float)z);
		}
		else if (s->Name == "Rotator")
		{
			int values[3];
			for (int& value : values)
			{
				uint8_t b = reader.ReadBit() ? reader.ReadByte() : 0;
				value = b << 8;
			}
			*static_cast<Rotator*>(data) = Rotator(values[0], values[1], values[2]);
		}
		else if (s->Name == "Plane")
		{
			float* plane = static_cast<float*>(data);
			for (int i = 0; i < 4; i++)
			{
				uint8_t bytes[2];
				reader.ReadBytes(bytes, 2);
				plane[i] = (float)(int16_t)(bytes[0] | (bytes[1] << 8));
			}
		}
		else
		{
			for (UField* field = s->Children; field; field = field->Next)
			{
				UProperty* member = UObject::TryCast<UProperty>(field);
				if (!member || map.ObjectToIndex(member) == -1)
					continue;
				for (int i = 0; i < member->ArrayDimension; i++)
					NetReadItem(member, reader, map, member->GetElement(static_cast<uint8_t*>(data) + member->DataOffset.DataOffset, i));
			}
		}
	}
	else
	{
		LogMessage("Net: cannot receive a " + prop->Class->Name.ToString() + " (" + prop->Name.ToString() + ")");
		reader.SetError();
	}
}

void NetWriteItem(UProperty* prop, NetBitWriter& writer, NetPackageMap& map, const void* data)
{
	if (auto boolProp = UObject::TryCast<UBoolProperty>(prop))
	{
		writer.WriteBit(boolProp->GetBool(data));
	}
	else if (auto byteProp = UObject::TryCast<UByteProperty>(prop))
	{
		uint8_t value = *static_cast<const uint8_t*>(data);
		if (byteProp->EnumType)
			writer.WriteBits(&value, CeilLogTwo((uint32_t)byteProp->EnumType->ElementNames.size()));
		else
			writer.WriteByte(value);
	}
	else if (UObject::TryCast<UIntProperty>(prop))
	{
		writer.WriteInt32(*static_cast<const int32_t*>(data));
	}
	else if (UObject::TryCast<UFloatProperty>(prop))
	{
		writer.WriteFloat(*static_cast<const float*>(data));
	}
	else if (UObject::TryCast<UObjectProperty>(prop))
	{
		map.WriteObject(writer, *static_cast<UObject* const*>(data));
	}
	else if (UObject::TryCast<UNameProperty>(prop))
	{
		map.WriteName(writer, *static_cast<const NameString*>(data));
	}
	else if (UObject::TryCast<UStrProperty>(prop) || UObject::TryCast<UStringProperty>(prop))
	{
		writer.WriteString(*static_cast<const std::string*>(data));
	}
	else if (auto structProp = UObject::TryCast<UStructProperty>(prop))
	{
		UStruct* s = structProp->Struct;
		if (s->Name == "Vector")
		{
			const vec3& v = *static_cast<const vec3*>(data);
			int x = Round(v.x), y = Round(v.y), z = Round(v.z);
			uint32_t largest = (uint32_t)std::max(std::max(std::abs(x), std::abs(y)), std::abs(z));
			uint32_t bits = (uint32_t)std::clamp(CeilLogTwo(1 + largest), 1, 16) - 1;
			writer.WriteInt(bits, 16);
			int bias = 1 << (bits + 1);
			uint32_t max = 1u << (bits + 2);
			writer.WriteInt((uint32_t)(x + bias), max);
			writer.WriteInt((uint32_t)(y + bias), max);
			writer.WriteInt((uint32_t)(z + bias), max);
		}
		else if (s->Name == "Rotator")
		{
			const Rotator& r = *static_cast<const Rotator*>(data);
			for (int value : { r.Pitch, r.Yaw, r.Roll })
			{
				uint8_t b = (uint8_t)(value >> 8);
				writer.WriteBit(b != 0);
				if (b != 0)
					writer.WriteByte(b);
			}
		}
		else if (s->Name == "Plane")
		{
			const float* plane = static_cast<const float*>(data);
			for (int i = 0; i < 4; i++)
			{
				int16_t value = (int16_t)Round(plane[i]);
				uint8_t bytes[2] = { (uint8_t)value, (uint8_t)(value >> 8) };
				writer.WriteBytes(bytes, 2);
			}
		}
		else
		{
			for (UField* field = s->Children; field; field = field->Next)
			{
				UProperty* member = UObject::TryCast<UProperty>(field);
				if (!member || map.ObjectToIndex(member) == -1)
					continue;
				for (int i = 0; i < member->ArrayDimension; i++)
					NetWriteItem(member, writer, map, member->GetElement(static_cast<const uint8_t*>(data) + member->DataOffset.DataOffset, i));
			}
		}
	}
	else
	{
		LogMessage("Net: cannot send a " + prop->Class->Name.ToString() + " (" + prop->Name.ToString() + ")");
	}
}

bool NetIsZero(UProperty* prop, const void* data)
{
	if (UObject::TryCast<UNameProperty>(prop))
		return static_cast<const NameString*>(data)->IsNone();

	std::unique_ptr<uint64_t[]> zero(new uint64_t[(prop->ElementSize() + 7) / 8 + 1]());
	prop->ConstructElement(zero.get());
	bool result = prop->CompareElement(data, zero.get());
	prop->DestructElement(zero.get());
	return result;
}
