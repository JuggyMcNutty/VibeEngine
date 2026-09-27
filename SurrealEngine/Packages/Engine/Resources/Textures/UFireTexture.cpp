
#include "Precomp.h"
#include "UFireTexture.h"

void UFireTexture::Load(ObjectStream* stream)
{
	UFractalTexture::Load(stream);

	int size = stream->ReadIndex();
	Sparks.resize(size);
	for (int i = 0; i < size; i++)
	{
		FireEngine::Spark& spark = Sparks[i];
		spark.Type = stream->ReadUInt8();
		spark.Heat = stream->ReadUInt8();
		spark.X = stream->ReadUInt8();
		spark.Y = stream->ReadUInt8();
		spark.A = stream->ReadUInt8();
		spark.B = stream->ReadUInt8();
		spark.C = stream->ReadUInt8();
		spark.D = stream->ReadUInt8();
	}
}

void UFireTexture::Save(PackageStreamWriter* stream)
{
	UFractalTexture::Save(stream);

	// The sparks placed in the editor, not the ones they let go (31 on).
	int count = 0;
	for (int i = 0; i < std::min(NumSparks(), (int)Sparks.size()); i++)
	{
		if (Sparks[i].Type < 31)
			count++;
	}
	stream->WriteIndex(count);
	for (int i = 0; i < std::min(NumSparks(), (int)Sparks.size()); i++)
	{
		const FireEngine::Spark& spark = Sparks[i];
		if (spark.Type >= 31)
			continue;
		stream->WriteUInt8(spark.Type);
		stream->WriteUInt8(spark.Heat);
		stream->WriteUInt8(spark.X);
		stream->WriteUInt8(spark.Y);
		stream->WriteUInt8(spark.A);
		stream->WriteUInt8(spark.B);
		stream->WriteUInt8(spark.C);
		stream->WriteUInt8(spark.D);
	}
}

// The original's constructor and PostLoad: masks, never masked itself, the
// colours for RenderHeat, and room for SparksLimit sparks.
void UFireTexture::Prepare()
{
	FireEngine::InitTables();
	UMask() = USize() - 1;
	VMask() = VSize() - 1;
	bMasked() = false;
	StarStatus() = 1;

	if (OldRenderHeat() != RenderHeat())
	{
		FireEngine::BuildFireRenderTable(RenderTable().data(), RenderHeat());
		OldRenderHeat() = RenderHeat();
	}

	SparksLimit() = clamp(SparksLimit(), 4, 0x2000);
	int limit = SparksLimit();
	NumSparks() = std::min(NumSparks(), (int)Sparks.size());
	if (NumSparks() > limit)
	{
		// Over the limit, what the sparks let go goes first.
		for (int i = NumSparks() - 1; i >= 0 && NumSparks() > limit; i--)
		{
			if (Sparks[i].Type >= 31)
				Sparks[i] = Sparks[--NumSparks()];
		}
		NumSparks() = std::min(NumSparks(), limit);
	}
	size_t oldSize = Sparks.size();
	Sparks.resize(limit);
	for (size_t i = oldSize; i < Sparks.size(); i++)
		Sparks[i] = {};
}

// The original's ConstantTimeTick: the sparks, the pass, the stars.
void UFireTexture::UpdateFrame()
{
	UnrealMipmap& mipmap = UsedMipmaps.front();
	if (mipmap.Width < 8 || mipmap.Height < 8 || Sparks.empty())
		return;

	FireEngine::Fire fire;
	fire.Bits = mipmap.Data.data();
	fire.UBits = mipmap.UBits;
	fire.UMask = mipmap.Width - 1;
	fire.VMask = mipmap.Height - 1;
	fire.Sparks = Sparks.data();
	fire.SparksLimit = (int)Sparks.size();
	fire.NumSparks = NumSparks();
	fire.GlobalPhase = GlobalPhase();
	fire.AuxPhase = AuxPhase();
	fire.FX_Frequency = FX_Frequency();
	fire.StarStatus = StarStatus();

	FireEngine::RedrawSparks(fire);
	FireEngine::FirePass(fire.Bits, RenderTable().data(), mipmap.Width, mipmap.Height, bRising());
	FireEngine::PostDrawSparks(fire);

	NumSparks() = fire.NumSparks;
	GlobalPhase() = fire.GlobalPhase;
	AuxPhase() = fire.AuxPhase;
	StarStatus() = fire.StarStatus;
	TextureModified = true;
}
