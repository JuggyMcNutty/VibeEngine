#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

struct ReverbSettings;

// A sound as Galaxy's mixer holds it: 16-bit frames, zeros past the end.
struct GalaxySample
{
	std::vector<int16_t> data;      // interleaved when stereo
	int channels = 1;
	uint64_t frames = 0;
	int rate = 0;
	bool looped = false;
	uint64_t loopStart = 0;
	uint64_t loopEnd = 0;           // the first frame not in the loop
};

// Galaxy.dll's sample mixer and zone reverb, as Deus Ex's sounds play
// (galaxy-dll.md, Each frame and Reverb): a channel's volume and pan come
// worked out by the subsystem, and the mixer gains the two sides by the
// square roots of the pan, resamples linearly and sends everything to the
// reverb, whose output joins the mix at the zone's master gain.
class GalaxyMixer
{
public:
	GalaxyMixer(int rate, int channels);

	int GetChannels() const { return (int)voices.size(); }

	// volume is Galaxy's voice volume, 0-32767; panning 0-32767 from left to
	// right, 49152 a sound behind with UseSurround.
	void Play(int channel, std::shared_ptr<const GalaxySample> sample, int volume, int panning, float pitch);
	void Update(int channel, int volume, int panning, float pitch);
	void Stop(int channel);
	bool IsPlaying(int channel);

	// Galaxy's sample volume: the louder of the Sound and Speech sliders,
	// 0 to 1.
	void SetMasterVolume(float louderSlider);

	// nullptr, or a master gain of 0, turns the reverb off.
	void SetReverb(const ReverbSettings* settings);

	// Interleaved stereo, overwritten.
	void Mix(float* output, int frames);

private:
	struct Voice
	{
		std::shared_ptr<const GalaxySample> sample;
		bool playing = false;
		uint64_t position = 0;      // 16.16 frames
		uint32_t step = 0;          // 16.16 frames per output frame
		int volume = 0;
		int panning = 16383;
	};

	struct Reverb
	{
		static constexpr int Length = 16384;
		int position = 0;
		int delay[6] = {};
		float allpass[6] = {};      // g, -g and 1 - g^2 of each echo's filter
		float negAllpass[6] = {};
		float pass[6] = {};
		float lowpass[6] = {};      // a and 1 - a of its lowpass
		float keep[6] = {};
		float lowpassState[6] = {};
		float last[2] = {};
		float volume = 0.0f;
		std::vector<float> lines;   // three stages of Length stereo frames
	};

	void SetPitch(Voice& voice, float pitch);
	void MixVoice(Voice& voice, float* dry, float* send, int frames);
	void RunReverb(float* dry, const float* send, int frames);

	std::mutex mutex;
	int rate = 44100;
	std::vector<Voice> voices;
	int master = 0;                 // the voices' common gain, 0-32767
	bool reverbOn = false;
	float reverbKey[15] = {};
	Reverb reverb;
	std::vector<float> sendBuffer;
};
