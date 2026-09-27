#pragma once

#include <cstdint>

// The fractal textures' engine as Deus Ex's Fire.dll runs it -- fire
// sparks, water drops, the fire and water passes, the wet and wave textures'
// output and ice (dx-reverse-info's fire-dll.md). Plain buffers in and out,
// so that it can be checked against the original's own routines.
namespace FireEngine
{
	// A sine of 256 steps as bytes (0 to 255 about 127.5), the same raised by
	// 32 (capped at 255) and by 128 (wrapped): shared by every texture.
	extern uint8_t SinTable[256];
	extern uint8_t SinTablePlus32[256];
	extern uint8_t SinTablePlus128[256];
	void InitTables();

	// The random bytes every fractal texture draws from, in turn: a table of
	// 64 words, each draw XOR-ing the word 32 ahead into the next.
	uint8_t Random();
	void SetRandomState(const uint32_t* words64, int byteIndex);
	void GetRandomState(uint32_t* words64, int& byteIndex);

	struct Spark
	{
		uint8_t Type, Heat, X, Y, A, B, C, D;
	};

	// A fire texture's state for one tick.
	struct Fire
	{
		uint8_t* Bits = nullptr;
		int UBits = 0;
		int UMask = 0;
		int VMask = 0;
		Spark* Sparks = nullptr; // SparksLimit entries
		int SparksLimit = 0;
		int NumSparks = 0;
		int GlobalPhase = 0;
		uint8_t AuxPhase = 0;
		uint8_t FX_Frequency = 0;
		uint8_t StarStatus = 0;
	};

	// Heat sums (0 to 1020) to colours, cooled by RenderHeat.
	void BuildFireRenderTable(uint8_t* table1028, int renderHeat);
	// Draws and moves the sparks, spawning and ending the ones that come and go.
	void RedrawSparks(Fire& fire);
	// The fire pass: each pixel from its neighbours' heat, rising or in place.
	void FirePass(uint8_t* bits, const uint8_t* renderTable, int usize, int vsize, bool rising);
	// Stars light the dark spots the pass left under them.
	void PostDrawSparks(Fire& fire);

	struct Drop
	{
		uint8_t Type, Depth, X, Y, A, B, C, D;
	};

	// A water texture's state for one tick. Fields is the water at half the
	// texture's size in each direction, two heights a cell: each row holds
	// USize/2 of the first and then USize/2 of the second.
	struct Water
	{
		uint8_t* Bits = nullptr;
		uint8_t* Fields = nullptr;
		int USize = 0;
		int VSize = 0;
		int UBits = 0;
		int UMask = 0;
		int VMask = 0;
		Drop* Drops = nullptr;
		int NumDrops = 0;
		int GlobalPhase = 0;
		uint8_t Parity = 0;
	};

	void BuildWaterTable(uint8_t* table1536);
	void RedrawDrops(Water& water);
	// One step of the water, alternating which field moves, and its slopes
	// through renderTable into the pixels.
	void WaterPass(Water& water, const uint8_t* renderTable, const uint8_t* waterTable);

	// The wet texture: a slope to a sideways shift of WaveAmp's scale.
	void BuildRefractionTable(uint8_t* table1028, int waveAmp);
	// Each pixel from the source's row, shifted by the shift the pass left there.
	void ApplyWet(uint8_t* bits, const uint8_t* source, int usize, int vsize, int ubits, int umask);
	// The wave texture: a slope to a lit colour.
	void BuildWaveLight(uint8_t* table1028, int waveAmp, int bumpMapLight, int bumpMapAngle, int phongRange, int phongSize);

	// The ice texture's panning, in the original's units.
	struct Ice
	{
		uint8_t PanningStyle = 0;
		uint8_t HorizPanSpeed = 0;
		uint8_t VertPanSpeed = 0;
		uint8_t Frequency = 0;
		uint8_t Amplitude = 0;
		float MasterCount = 0.0f;
		float UDisplace = 0.0f;
		float VDisplace = 0.0f;
		float UPosition = 0.0f;
		float VPosition = 0.0f;
	};
	void MoveIce(Ice& ice, float deltaTime);
	// Each pixel from the moving texture's row, shifted by the still one's
	// value there: the glass over a moving source, or the glass moving.
	void BlitIce(uint8_t* bits, const uint8_t* glass, const uint8_t* source, int usize, int vsize, int ubits, int umask, int vmask, int upos, int vpos, bool moveIce);
}
