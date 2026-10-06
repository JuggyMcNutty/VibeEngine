
#include "Precomp.h"
#include "NameString.h"

Array<std::string> NameString::Names;
std::unordered_map<std::string, int> NameString::CompareStringToIndex;
std::unordered_map<std::string, std::pair<int, int>> NameString::SpellStringToIndex;
std::vector<uint8_t> NameString::Marks;
std::vector<int> NameString::Available;
std::vector<std::pair<int, int>> NameString::CollectableSpellings;

NameString NameString::Collectable(const std::string& value)
{
	NameString name;
	if (!value.empty())
		name.GetIndex(value, true);
	return name;
}

void NameString::BeginMark()
{
	for (uint8_t& mark : Marks)
		mark &= ~MarkedFlag;
}

std::vector<int> NameString::Sweep()
{
	// A spelling goes first, then its compare entry once no spelling of it
	// is left: a spelling kept keeps its compare entry.
	for (auto& spelling : CollectableSpellings)
	{
		if (Marks[spelling.first] != CollectableFlag)
			Marks[spelling.second] |= MarkedFlag;
	}

	std::vector<int> freed;
	std::vector<std::pair<int, int>> kept;
	for (auto& spelling : CollectableSpellings)
	{
		int spelled = spelling.first, compare = spelling.second;
		if (Marks[spelled] == CollectableFlag)
		{
			SpellStringToIndex.erase(Names[spelled]);
			Names[spelled] = std::string();
			Marks[spelled] = 0;
			Available.push_back(spelled);
			if (Marks[compare] == CollectableFlag)
			{
				CompareStringToIndex.erase(Names[compare]);
				Names[compare] = std::string();
				Marks[compare] = 0;
				Available.push_back(compare);
				freed.push_back(compare);
			}
		}
		else if (Marks[spelled] & CollectableFlag)
		{
			kept.push_back(spelling);
		}
	}
	CollectableSpellings = std::move(kept);
	return freed;
}
