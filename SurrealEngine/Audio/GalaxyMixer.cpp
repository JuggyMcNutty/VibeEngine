
#include "Precomp.h"
#include "GalaxyMixer.h"
#include "AudioDevice.h"
#include <algorithm>
#include <cmath>
#include <cstring>

GalaxyMixer::GalaxyMixer(int rate, int channels) : rate(rate), voices(channels)
{
	reverb.lines.resize(3 * Reverb::Length * 2);
}

void GalaxyMixer::Play(int channel, std::shared_ptr<const GalaxySample> sample, int volume, int panning, float pitch)
{
	std::unique_lock lock(mutex);
	Voice& voice = voices[channel];
	voice.sample = std::move(sample);
	voice.playing = voice.sample && voice.sample->frames > 0;
	voice.position = 0;
	voice.volume = volume;
	voice.panning = panning;
	SetPitch(voice, pitch);
}

void GalaxyMixer::Update(int channel, int volume, int panning, float pitch)
{
	std::unique_lock lock(mutex);
	Voice& voice = voices[channel];
	voice.volume = volume;
	voice.panning = panning;
	SetPitch(voice, pitch);
}

void GalaxyMixer::Stop(int channel)
{
	std::unique_lock lock(mutex);
	voices[channel].playing = false;
	voices[channel].sample.reset();
}

bool GalaxyMixer::IsPlaying(int channel)
{
	std::unique_lock lock(mutex);
	return voices[channel].playing;
}

// The voice's rate is the sample's times the pitch, in whole hertz, as a
// 16.16 step (glxControlSample).
void GalaxyMixer::SetPitch(Voice& voice, float pitch)
{
	if (!voice.sample)
		return;
	int64_t frequency = std::max((int64_t)(voice.sample->rate * (double)pitch), (int64_t)0);
	uint64_t step = ((frequency / rate) << 16) + (((frequency % rate) << 16) / rate);
	voice.step = (uint32_t)std::min(step, (uint64_t)0xffffffff);
}

// SetVolumes hands Galaxy 127 times the louder slider; its mixer gains every
// voice by twice that squared, times the voice's own full volume of 32258,
// then through four more full-scale factors of 32767.
void GalaxyMixer::SetMasterVolume(float louderSlider)
{
	int volume = std::clamp((int)(int64_t)(louderSlider * 127.0), 0, 127);
	int gain = (2 * volume * volume * 32258) >> 15;
	for (int i = 0; i < 4; i++)
		gain = (0x7fff * gain) >> 15;
	std::unique_lock lock(mutex);
	master = gain;
}

// Galaxy's reverb (galaxy-dll.md, Reverb): three stages, each a pair of
// allpass filters -- the left of each stage one of the zone's echoes, the
// right the next -- with a one-pole lowpass at the cutoff in their feedback.
// An echo's delay is its time; its gain g makes the filter's coefficient
// 1 - g, so a strong echo is nearly a plain delay and a weak one passes the
// sound on nearly as it came. The last stage's output feeds back into the
// first, left and right swapped, at half with the sound coming in, and joins
// the mix at the master gain. Set again, it starts from silence.
void GalaxyMixer::SetReverb(const ReverbSettings* settings)
{
	float key[15] = {};
	if (settings && settings->masterGain != 0.0f && settings->cutoffHz <= rate)
	{
		key[0] = settings->masterGain;
		key[1] = settings->cutoffHz;
		for (int i = 0; i < 6; i++)
		{
			key[2 + i] = settings->delaySeconds[i];
			key[8 + i] = settings->gains[i];
		}
		key[14] = 1.0f;
	}

	std::unique_lock lock(mutex);
	if (std::memcmp(key, reverbKey, sizeof(key)) == 0)
		return;
	std::memcpy(reverbKey, key, sizeof(key));

	reverbOn = key[14] != 0.0f;
	if (!reverbOn)
		return;

	std::fill(reverb.lines.begin(), reverb.lines.end(), 0.0f);
	reverb.position = 0;
	reverb.last[0] = 0.0f;
	reverb.last[1] = 0.0f;
	reverb.volume = settings->masterGain;
	double v = 1.0 - std::cos(settings->cutoffHz * 6.2820001 / rate);
	float lowpass = (float)(std::sqrt((v + 2.0) * v) - v);
	for (int i = 0; i < 6; i++)
	{
		double time = settings->delaySeconds[i] == 0.0f ? 0.001 : settings->delaySeconds[i];
		double g = 1.0 - settings->gains[i];
		if (g == 1.0)
			g = 0.99900001;
		reverb.delay[i] = (int)(int64_t)(rate * time);
		reverb.allpass[i] = (float)g;
		reverb.negAllpass[i] = (float)-g;
		reverb.pass[i] = (float)(1.0 - g * g);
		reverb.lowpass[i] = lowpass;
		reverb.keep[i] = (float)(1.0 - lowpass);
		reverb.lowpassState[i] = 0.0f;
	}
}

void GalaxyMixer::Mix(float* output, int frames)
{
	std::unique_lock lock(mutex);

	std::fill(output, output + frames * 2, 0.0f);
	sendBuffer.assign(frames * 2, 0.0f);

	for (Voice& voice : voices)
	{
		if (voice.playing)
			MixVoice(voice, output, sendBuffer.data(), frames);
	}

	if (reverbOn)
		RunReverb(output, sendBuffer.data(), frames);

	// The samples' mix goes out saturated to 16 bits.
	for (int i = 0; i < frames * 2; i++)
		output[i] = std::clamp(output[i], -1.0f, 32767.0f / 32768.0f);
}

// A side's gain is the square root of its share of the pan: Galaxy's table of
// 32768, sqrt(i / 32767) at full scale (glxInit).
static int PanGain(int share)
{
	return (int)(std::sqrt(share * 0.00003051850947599719) * 32767.0);
}

void GalaxyMixer::MixVoice(Voice& voice, float* dry, float* send, int frames)
{
	const GalaxySample& sample = *voice.sample;

	// Each side's gain, and the reverb's at the voice's reverb level of 127
	// in 128; a sound behind with UseSurround plays centred, its right side
	// inverted.
	int pan = voice.panning & 0x7fff;
	int gain = (voice.volume * master) >> 15;
	int left = (gain * PanGain(0x7fff - pan)) >> 15;
	int right = (gain * PanGain(pan)) >> 15;
	int leftSend = (left * 127) >> 7;
	int rightSend = (right * 127) >> 7;
	if (voice.panning & 0x8000)
	{
		right = -right;
		rightSend = -rightSend;
	}
	const float scale = 1.0f / (32768.0f * 32768.0f);
	float gainL = left * scale, gainR = right * scale;
	float sendL = leftSend * scale, sendR = rightSend * scale;

	bool loop = sample.looped && sample.loopStart < sample.loopEnd && sample.loopEnd <= sample.frames;
	const int16_t* data = sample.data.data();
	int channels = sample.channels;
	for (int i = 0; i < frames; i++)
	{
		uint64_t index = voice.position >> 16;
		if (loop && index >= sample.loopEnd)
		{
			voice.position -= (sample.loopEnd - sample.loopStart) << 16;
			index = voice.position >> 16;
		}
		else if (!loop && index >= sample.frames)
		{
			voice.playing = false;
			voice.sample.reset();
			return;
		}

		// Linear between this frame and the next, as its SSE and MMX mixers
		// resample; a stereo sample's two sides are two voices at one pan.
		float t = (voice.position & 0xfffe) * (1.0f / 65536.0f);
		const int16_t* frame = data + index * channels;
		float a = frame[0], b = frame[channels];
		if (channels == 2)
		{
			a += frame[1];
			b += frame[3];
		}
		float s = a + (b - a) * t;

		dry[i * 2] += s * gainL;
		dry[i * 2 + 1] += s * gainR;
		send[i * 2] += s * sendL;
		send[i * 2 + 1] += s * sendR;
		voice.position += voice.step;
	}
}

void GalaxyMixer::RunReverb(float* dry, const float* send, int frames)
{
	const int mask = Reverb::Length - 1;
	float* lines = reverb.lines.data();
	int position = reverb.position;
	float lastL = reverb.last[0], lastR = reverb.last[1];
	for (int i = 0; i < frames; i++)
	{
		float x[2] = { (send[i * 2] + lastR) * 0.5f, (send[i * 2 + 1] + lastL) * 0.5f };
		for (int stage = 0; stage < 3; stage++)
		{
			float* line = lines + stage * Reverb::Length * 2;
			for (int side = 0; side < 2; side++)
			{
				int k = stage * 2 + side;
				float delayed = line[((position - reverb.delay[k]) & mask) * 2 + side];
				float state = delayed * reverb.lowpass[k] + reverb.lowpassState[k] * reverb.keep[k];
				reverb.lowpassState[k] = state;
				line[position * 2 + side] = x[side] + state * reverb.allpass[k];
				x[side] = x[side] * reverb.negAllpass[k] + delayed * reverb.pass[k];
			}
		}
		lastL = x[0];
		lastR = x[1];
		dry[i * 2] += x[0] * reverb.volume;
		dry[i * 2 + 1] += x[1] * reverb.volume;
		position = (position + 1) & mask;
	}
	reverb.position = position;
	reverb.last[0] = lastL;
	reverb.last[1] = lastR;
}
