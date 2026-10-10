
#include "Precomp.h"
#include "UFont.h"
#include "Textures/UTexture.h"

void UFont::Load(ObjectStream* stream)
{
	// Note: 63 package and older inherited from UTexture. Newer versions inherit from UObject
	// We must keep the old relationship intact to load the older games.
	// If the game is newer then this is not a valid texture and any properties on UTexture do not exist.

	if (stream->GetVersion() <= 63)
	{
		UTexture::Load(stream);

		pages.resize(1);
		FontPage& page = pages.front();
		page.Texture = this;
		page.Characters.resize(stream->ReadIndex());
		for (FontCharacter& character : page.Characters)
		{
			character.StartU = stream->ReadInt32();
			character.StartV = stream->ReadInt32();
			character.USize = stream->ReadInt32();
			character.VSize = stream->ReadInt32();
		}
		charactersPerPage = (int)page.Characters.size();
	}
	else
	{
		UObject::Load(stream);

		pages.resize(stream->ReadIndex());
		for (FontPage& page : pages)
		{
			page.Texture = stream->ReadObject<UTexture>();
			page.Characters.resize(stream->ReadIndex());
			for (FontCharacter& character : page.Characters)
			{
				character.StartU = stream->ReadInt32();
				character.StartV = stream->ReadInt32();
				character.USize = stream->ReadInt32();
				character.VSize = stream->ReadInt32();
			}
		}

		charactersPerPage = stream->ReadUInt32();
	}
}

void UFont::Save(PackageStreamWriter* stream)
{
	if (stream->GetVersion() <= 63)
	{
		UTexture::Save(stream);

		FontPage& page = pages.front();
		stream->WriteIndex((int)page.Characters.size());
		for (FontCharacter& character : page.Characters)
		{
			stream->WriteInt32(character.StartU);
			stream->WriteInt32(character.StartV);
			stream->WriteInt32(character.USize);
			stream->WriteInt32(character.VSize);
		}
	}
	else
	{
		UObject::Save(stream);

		stream->WriteIndex((int)pages.size());
		for (FontPage& page : pages)
		{
			stream->WriteObject(page.Texture);
			stream->WriteIndex((int)page.Characters.size());
			for (FontCharacter& character : page.Characters)
			{
				stream->WriteInt32(character.StartU);
				stream->WriteInt32(character.StartV);
				stream->WriteInt32(character.USize);
				stream->WriteInt32(character.VSize);
			}
		}

		stream->WriteUInt32(charactersPerPage);
	}
}

FontGlyph UFont::GetGlyph(char c) const
{
	FontGlyph glyph = FindGlyph(c);
	if (glyph.USize == 0)
	{
		if (c >= 'a' && c <= 'z')
		{
			c += 'A' - 'a';
			glyph = FindGlyph(c);
		}

		if (glyph.USize == 0)
			glyph = FindGlyph(32);
	}
	return glyph;
}

// A character as Extension's GC finds it (XGC's glyph lookup, Extension.dll
// 0x10026880): on page c / CharactersPerPage at c % CharactersPerPage, and
// one not there has no size -- it draws nothing and takes no room.
FontGlyph UFont::GetPageGlyph(uint8_t c) const
{
	if (charactersPerPage <= 0)
		return {};
	size_t page = c / charactersPerPage;
	size_t index = c % charactersPerPage;
	if (page >= pages.size() || index >= pages[page].Characters.size())
		return {};
	return { pages[page].Texture, pages[page].Characters[index] };
}

FontGlyph UFont::FindGlyph(char c) const
{
	size_t index = c;
	for (auto& page : pages)
	{
		if (index < page.Characters.size())
		{
			return { page.Texture, page.Characters[index] };
		}
		index -= page.Characters.size();
	}
	return {};
}

void UFont::Mark(GCMarker& marker)
{
	UTexture::Mark(marker);
	marker.SetField("pages");
	for (FontPage& page : pages)
		marker.Mark(page.Texture);
}
