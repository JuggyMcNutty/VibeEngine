
#include "Precomp.h"
#include "NGameInfo.h"
#include "VM/NativeFunc.h"
#include "Engine.h"
#include "Network/NetDriver.h"
#include "Package/PackageManager.h"

#include <sstream>

void NGameInfo::RegisterFunctions()
{
	RegisterVMNativeFunc_1("GameInfo", "GetNetworkNumber", &NGameInfo::GetNetworkNumber, 0);
	RegisterVMNativeFunc_5("GameInfo", "ParseKillMessage", &NGameInfo::ParseKillMessage, 0);
}

// The original's (Engine.dll 0x103e69d0): the address the level's net
// driver is bound to, 0.0.0.0; "" with none.
void NGameInfo::GetNetworkNumber(UObject* Self, std::string& ReturnValue)
{
	ReturnValue = engine->LevelNetDriver ? NetAddressString(engine->LevelNetDriver->LocalAddr) : "";
}

void NGameInfo::ParseKillMessage(const std::string& KillerName, const std::string& VictimName, const std::string& WeaponName, const std::string& DeathMessage, std::string& ReturnValue)
{
	size_t len = DeathMessage.size();
	std::string result;
	result.reserve(DeathMessage.size() * 2);
	for (size_t i = 0; i < len; i++)
	{
		if (DeathMessage[i] == '%' && i + 1 < len)
		{
			if (DeathMessage[i + 1] == 'k')
			{
				result += KillerName;
				i++;
			}
			else if (DeathMessage[i + 1] == 'o')
			{
				result += VictimName;
				i++;
			}
			else if (DeathMessage[i + 1] == 'w')
			{
				result += WeaponName;
				i++;
			}
		}
		else
		{
			result.push_back(DeathMessage[i]);
		}
	}

	ReturnValue = result;
}
