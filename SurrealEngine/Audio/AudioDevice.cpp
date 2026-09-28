
#include "Precomp.h"
#include "AudioDevice.h"
#include "AudioSource.h"
#include "GalaxyMixer.h"
#include "Engine.h"
#include "Native/NObject.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Resources/USound.h"
#include <mutex>
#include "Utils/Exception.h"
#include <atomic>
#include <map>
#include <cmath>
#include <queue>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#define UU_PER_METER 43

// Deus Ex's sounds are mixed as Galaxy mixes them (GalaxyMixer), the mix
// streamed through one source; other games' sounds are OpenAL's own sources,
// placed in 3D.
static bool UseGalaxyMixer()
{
	return engine && engine->LaunchInfo.IsDeusEx();
}

class ALSoundSource
{
public:
	ALSoundSource()
	{
		alGenSources(1, &id);
		if (alGetError() != AL_NO_ERROR)
			Exception::Throw("Failed to generate AL source");

		alSourcef(id, AL_ROLLOFF_FACTOR, 1.1f);
	}

	~ALSoundSource()
	{
		Stop();
		alSourcei(id, AL_BUFFER, 0);
		alDeleteSources(1, &id);
	}

	void Play()
	{
		alSourcePlay(id);
		if (alGetError() != AL_NO_ERROR)
			Exception::Throw("Failed to play AL source");
	}

	void Stop()
	{
		alSourceStop(id);
	}

	bool IsPlaying()
	{
		if (!alIsSource(id))
			return false;

		ALint state;
		alGetSourcei(id, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING;
	}

	USound* GetSound()
	{
		return sound;
	}

	void SetDopplerFactor(float newDopplerFactor)
	{
		if (dopplerFactor != newDopplerFactor)
		{
			dopplerFactor = newDopplerFactor;
			alSourcef(id, AL_DOPPLER_FACTOR, dopplerFactor);
		}
	}

	void SetPosition(vec3& newPosition)
	{
		position.x = newPosition.x;
		position.y = newPosition.y;
		position.z = -newPosition.z;
		alSourcefv(id, AL_POSITION, &position[0]);
	}

	void SetRadius(float newRadius)
	{
		if (radius != newRadius)
		{
			radius = newRadius;
			alSourcef(id, AL_MAX_DISTANCE, radius);
			alSourcef(id, AL_REFERENCE_DISTANCE, 0.1f * radius);
		}
	}

	void SetVelocity(vec3& newVelocity)
	{
		velocity.x = newVelocity.x;
		velocity.y = newVelocity.y;
		velocity.z = -newVelocity.z;
		alSourcefv(id, AL_VELOCITY, &velocity[0]);
	}

	void SetSound(USound* newSound)
	{
		if (sound != newSound)
		{
			sound = newSound;
			alSourcei(id, AL_BUFFER, 0);
			alGetError();
			alSourcei(id, AL_BUFFER, (ALint)(ptrdiff_t)sound->handle);

			if (sound->loopInfo.Looped)
				alSourcei(id, AL_LOOPING, AL_TRUE);
			else
				alSourcei(id, AL_LOOPING, AL_FALSE);
		}
	}

	void SetSpatial(bool bSpatial)
	{
		if (bIs3d != bSpatial)
		{
			bIs3d = bSpatial;
			alSourcei(id, AL_SOURCE_SPATIALIZE_SOFT, bIs3d);
		}
	}

	// Which slider gains this source: the Speech slider for the talk slot,
	// the Sound slider for the rest.
	void SetSpeech(bool newSpeech)
	{
		speech = newSpeech;
	}

	bool IsSpeech() const
	{
		return speech;
	}

	void SetVolume(float newVolume)
	{
		if (volume != newVolume)
		{
			volume = newVolume;
			ApplyGain();
		}
	}

	void SetGlobalVolume(float newGlobalVolume)
	{
		if (globalVolume != newGlobalVolume)
		{
			globalVolume = newGlobalVolume;
			ApplyGain();
		}
	}

	void SetPitch(float newPitch)
	{
		if (pitch != newPitch)
		{
			pitch = newPitch;
			alSourcef(id, AL_PITCH, pitch);
		}
	}

	void DoLoop()
	{
		if (sound->loopInfo.Looped)
		{
			ALint offset;
			alGetSourcei(id, AL_SAMPLE_OFFSET, &offset);
			if (offset >= sound->loopInfo.LoopEnd)
				alSourcei(id, AL_SAMPLE_OFFSET, (ALint)sound->loopInfo.LoopStart);
		}
	}

	ALuint id = -1;

private:
	void ApplyGain()
	{
		alSourcef(id, AL_GAIN, volume * globalVolume);
		alSourcef(id, AL_MAX_GAIN, volume * globalVolume);
	}

	UActor* actor = nullptr;
	USound* sound = nullptr;
	vec3 position = vec3(0.0f);
	vec3 velocity = vec3(0.0f);
	float radius = 0.0f;
	float volume = 0.0f;
	float globalVolume = 1.0f; // Comes from Audio Subsystem
	float pitch = 0.0f;
	float dopplerFactor = 0.0f;
	bool bIs3d = false;
	bool speech = false;
};

// TODO list:
//  Sound looping
//  Positional audio
//  Music fade in/out
//  Music crossfade
//  EFX effects
//  Support for real EAX hardware
//  (maybe) A3D style sound tracing

class OpenALAudioDevice : public AudioDevice
{
public:
	OpenALAudioDevice(int inFrequency, int numVoices, int inMusicBufferCount, int inMusicBufferSize)
	{
		frequency = inFrequency;
		musicBufferCount = inMusicBufferCount;
		musicBufferSize = inMusicBufferSize;

		musicBuffer.resize(musicBufferSize * musicBufferCount);
		musicQueue.Resize(musicBufferCount);

		for (int i = 0; i < musicBufferCount; i++)
		{
			musicQueue.Push(&musicBuffer[musicBufferSize * i]);
			musicQueue.Pop();
		}

		// Init OpenAL
		// TODO: Add device enumeration
		alDevice = alcOpenDevice(NULL);
		if (alDevice == nullptr)
			Exception::Throw("Failed to initialize OpenAL device");

		const ALCint ctxAttribs[] =
		{
			ALC_FREQUENCY, inFrequency,
			ALC_REFRESH, 60,
			ALC_SYNC, ALC_FALSE
		};

		alContext = alcCreateContext(alDevice, NULL);
		if (alContext == nullptr)
			Exception::Throw("Failed to initialize OpenAL context");

		if (alcMakeContextCurrent(alContext) == ALC_FALSE)
			Exception::Throw("Failed to make OpenAL context current");

		// init listener state
		ALfloat listenerOri[] = { 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f };
		alListener3f(AL_POSITION, 0, 0, 0.0f);
		alListener3f(AL_VELOCITY, 0, 0, 0);
		alListenerfv(AL_ORIENTATION, listenerOri);
		alListenerf(AL_METERS_PER_UNIT, 1.f / UU_PER_METER);

		alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		alSpeedOfSound(343.3f / (1.0f / UU_PER_METER));

		if (UseGalaxyMixer())
		{
			// Galaxy's 32 channel records; the mix goes out at OutputRate, as
			// Galaxy's does, so its reverb's delays are the original's samples.
			mixer = std::make_unique<GalaxyMixer>(frequency, 32);
			StartMixStream();
		}
		else
		{
			// Init sound sources
			alcGetIntegerv(alDevice, ALC_MONO_SOURCES, 1, &monoSources);
			alcGetIntegerv(alDevice, ALC_STEREO_SOURCES, 1, &stereoSources);

			// TODO: how do we prioritize mono vs stereo source count?
			sources.resize(monoSources);
		}

		// init music source/buffer
		alGenSources(1, &alMusicSource);
		alSourcei(alMusicSource, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);

		alMusicBuffers.resize(musicBufferCount);
		alGenBuffers(musicBufferCount, &alMusicBuffers[0]);

		// init playback thread
		musicThreadData.thread = std::thread([this]() { MusicThreadMain(); });
	}

	~OpenALAudioDevice()
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.exitFlag = true;
		lock.unlock();
		musicThreadData.thread.join();

		if (mixer)
			StopMixStream();

		alSourceStop(alMusicSource);
		alDeleteSources(1, &alMusicSource);

		alDeleteBuffers((ALsizei)alMusicBuffers.size(), &alMusicBuffers[0]);

		sources.clear();

		for (USound* sound : sounds)
		{
			alDeleteBuffers(1, (ALuint*)&sound->handle);
		}

		alcDestroyContext(alContext);
		alcCloseDevice(alDevice);
	}

	int GetTotalChannels() override
	{
		return mixer ? mixer->GetChannels() : (int)sources.size();
	}

	void AddSound(USound* sound) override
	{
		if (mixer)
		{
			// The mixer's own 16-bit copy, which a channel keeps while it plays,
			// two silent frames past the end for the last frame's interpolation.
			auto sample = std::make_shared<GalaxySample>();
			sample->channels = std::max(sound->channels, 1);
			sample->frames = sound->samples.size() / sample->channels;
			sample->rate = sound->frequency;
			sample->data.resize((sample->frames + 2) * sample->channels);
			for (size_t i = 0, count = sample->frames * sample->channels; i < count; i++)
				sample->data[i] = (int16_t)std::clamp((int)std::lround(sound->samples[i] * 32768.0f), -32768, 32767);
			sample->looped = sound->loopInfo.Looped;
			sample->loopStart = sound->loopInfo.LoopStart;
			sample->loopEnd = sound->loopInfo.LoopEnd;
			mixedSounds[sound] = std::move(sample);
			return;
		}

		sounds.push_back(sound);

		ALenum format = AL_FORMAT_MONO_FLOAT32;
		if (sound->channels == 2)
			format = AL_FORMAT_STEREO_FLOAT32;

		ALuint id;
		alGenBuffers(1, &id);
		alBufferData(id, format, sound->samples.data(), (ALsizei)(sound->samples.size()*sizeof(sound->samples[0])), sound->frequency);
		alError = alGetError();
		if (alError != AL_NO_ERROR)
			Exception::Throw("Failed to buffer sound data for " + sound->Name.ToString());

		sound->handle = (void*)(ptrdiff_t)id;
	}

	void RemoveSound(USound* sound) override
	{
		if (mixer)
		{
			mixedSounds.erase(sound);
			return;
		}

		for (auto it = sounds.begin(); it != sounds.end(); ++it)
		{
			if (*it == sound)
			{
				// TODO: find sources playing this sound and stop them?
				sounds.erase(it);
				alDeleteBuffers(1, reinterpret_cast<const ALuint*>(&sound->handle));
				return;
			}
		}
	}

	bool IsPlaying(int channel) override
	{
		if (mixer)
			return mixer->IsPlaying(channel);
		ALSoundSource& source = sources[channel];
		return source.IsPlaying();
	}

	void PlayMusic(std::unique_ptr<AudioSource> source) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.music = std::move(source);
		musicThreadData.musicUpdate = true;
		musicThreadData.currentOrder.store(-1);
	}

	int GetMusicOrder() override
	{
		return musicThreadData.currentOrder.load();
	}

	void SetReverb(const ReverbSettings* settings) override
	{
		if (mixer)
			mixer->SetReverb(settings);
	}

	void SetMusicOrder(int order) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.jumpOrder = order;
	}

	void PlaySound(int channel, USound* sound, vec3& location, float volume, float radius, float pitch, bool speech) override
	{
		if (!std::isfinite(volume) || volume < 0.0f || !std::isfinite(pitch) || channel >= sources.size())
			Exception::Throw("Invalid PlaySound arguments");

		ALSoundSource& source = sources[channel];
		if (source.IsPlaying())
		{
			LogMessage("Attempted to play sound on active channel " + std::to_string(channel));
			return;
		}

		source.SetSound(sound);
		source.SetPosition(location);
		source.SetSpeech(speech);
		source.SetVolume(volume);
		source.SetGlobalVolume(speech ? globalSpeechVolume : globalSoundVolume);
		source.SetRadius(radius);
		source.SetPitch(pitch);
		source.SetSpatial(true);
		source.Play();
	}

	void UpdateSound(int channel, USound* sound, vec3& location, float volume, float radius, float pitch) override
	{
		if (!std::isfinite(volume) || volume < 0.0f || !std::isfinite(pitch) || channel >= sources.size())
			Exception::Throw("Invalid PlaySound arguments");

		ALSoundSource& source = sources[channel];

		source.SetPosition(location);
		source.SetVolume(volume);
		source.SetGlobalVolume(source.IsSpeech() ? globalSpeechVolume : globalSoundVolume);
		source.SetRadius(radius);
		source.SetPitch(pitch);

		source.DoLoop();
	}

	void PlayMixedSound(int channel, USound* sound, int volume, int panning, float pitch) override
	{
		if (!mixer || channel >= mixer->GetChannels() || !std::isfinite(pitch))
			Exception::Throw("Invalid PlayMixedSound arguments");

		auto it = mixedSounds.find(sound);
		mixer->Play(channel, it != mixedSounds.end() ? it->second : nullptr, volume, panning, pitch);
	}

	void UpdateMixedSound(int channel, int volume, int panning, float pitch) override
	{
		if (!mixer || channel >= mixer->GetChannels() || !std::isfinite(pitch))
			Exception::Throw("Invalid UpdateMixedSound arguments");

		mixer->Update(channel, volume, panning, pitch);
	}

	void StopSound(int channel) override
	{
		if (mixer)
		{
			if (channel >= mixer->GetChannels())
				Exception::Throw("Invalid StopSound arguments");
			mixer->Stop(channel);
			return;
		}

		if (channel >= sources.size())
			Exception::Throw("Invalid StopSound arguments");

		sources[channel].Stop();
	}

	std::string getALErrorString()
	{
		switch (alGetError())
		{
		case AL_NO_ERROR:
			return "AL_NO_ERROR";
		case AL_INVALID_NAME:
			return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:
			return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:
			return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:
			return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:
			return "AL_OUT_OF_MEMORY";
		default:
			return "AL_UNKNOWN_ERROR";
		}
	}

	void PlayMusicBuffer(AudioSource* music)
	{
		int format = (music->GetChannels() == 1) ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
		int freq = music->GetFrequency();

		ALenum error;
		for (int i = 0; i < musicBufferCount; i++)
		{
			alBufferData(alMusicBuffers[i], format, musicQueue.Pop(), musicBufferSize*4, freq);
			if ((error = alGetError()) != AL_NO_ERROR)
				Exception::Throw("alBufferData failed in PlayMusicBuffer: " + getALErrorString());

			alSourceQueueBuffers(alMusicSource, 1, &alMusicBuffers[i]);
			if (alGetError() != AL_NO_ERROR)
				Exception::Throw("alSourceQueueBuffers failed in PlayMusicBuffer: " + getALErrorString());
		}

		alSourcePlay(alMusicSource);
		if (alGetError() != AL_NO_ERROR)
			Exception::Throw("alSourcePlay failed in PlayMusicBuffer: " + getALErrorString());
	}

	void UpdateMusicBuffer(AudioSource* music)
	{
		ALint status;
		alGetSourcei(alMusicSource, AL_BUFFERS_PROCESSED, &status);

		while (status)
		{
			ALuint buffer;
			alSourceUnqueueBuffers(alMusicSource, 1, &buffer);

			int format = (music->GetChannels() == 1) ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
			int freq = music->GetFrequency();

			alBufferData(buffer, format, musicQueue.Pop(), musicBufferSize*4, freq);
			if (alGetError() != AL_NO_ERROR)
				Exception::Throw("alBufferData failed in UpdateMusicBuffer: " + getALErrorString());

			alSourceQueueBuffers(alMusicSource, 1, &buffer);
			if (alGetError() != AL_NO_ERROR)
				Exception::Throw("alSourceQueueBuffers failed in UpdateMusicBuffer: " + getALErrorString());

			status--;
		}

		alGetSourcei(alMusicSource, AL_SOURCE_STATE, &status);
		if (status == AL_STOPPED)
		{
			alSourcePlay(alMusicSource);
			if (alGetError() != AL_NO_ERROR)
				Exception::Throw("alSourcePlay failed in PlayMusicBuffer: " + getALErrorString());
		}
	}

	void SetMusicVolume(float volume) override
	{
		alSourcef(alMusicSource, AL_GAIN, volume);
	}

	void SetSoundVolume(float volume) override
	{
		globalSoundVolume = volume;
		if (mixer)
			mixer->SetMasterVolume(std::max(globalSoundVolume, globalSpeechVolume));

		for (auto& soundSource : sources)
		{
			if (!soundSource.IsSpeech())
				soundSource.SetGlobalVolume(globalSoundVolume);
		}
	}

	void SetSpeechVolume(float volume) override
	{
		globalSpeechVolume = volume;
		if (mixer)
			mixer->SetMasterVolume(std::max(globalSoundVolume, globalSpeechVolume));

		for (auto& soundSource : sources)
		{
			if (soundSource.IsSpeech())
				soundSource.SetGlobalVolume(globalSpeechVolume);
		}
	}

	void Update() override
	{
		UActor* listener = engine->CameraActor;
		if (listener)
		{
			// Update listener properties
			vec3& location = listener->Location();
			vec3& velocity = listener->Velocity();

			vec3 at, left, up;
			Coords::Rotation(listener->Rotation()).GetAxes(at, left, up);

			ALfloat listenerOri[6] = { up.x, up.y, -up.z, -at.x, -at.y, at.z };
			alListener3f(AL_POSITION, location.x, location.y, -location.z);
			alListener3f(AL_VELOCITY, velocity.x, velocity.y, -velocity.z);
			alListenerfv(AL_ORIENTATION, listenerOri);
		}
	}

	void MusicThreadMain()
	{
		std::unique_ptr<AudioSource> currentMusic;
		bool musicPlaying = false;

		while (true)
		{
			// Lock the mutex. Grab the data we need from the main thread. Then unlock.
			std::unique_lock lock(musicThreadData.mutex);
			if (musicThreadData.exitFlag)
				break;
			if (musicThreadData.musicUpdate)
			{
				if (musicPlaying)
				{
					alSourceStop(alMusicSource);
					alSourceUnqueueBuffers(alMusicSource, (ALsizei)alMusicBuffers.size(), &alMusicBuffers[0]);
					musicPlaying = false;
				}
				currentMusic = std::move(musicThreadData.music);
				musicThreadData.musicUpdate = false;
			}
			int jumpOrder = musicThreadData.jumpOrder;
			musicThreadData.jumpOrder = -1;
			lock.unlock();
			// Never touch anything from musicThreadData after this point
			// (currentOrder is atomic and written below).

			if (jumpOrder >= 0 && currentMusic)
				currentMusic->SetOrder(jumpOrder);

			if (currentMusic)
			{
				musicThreadData.currentOrder.store(currentMusic->GetOrder());

				while (musicQueue.Size() < musicBufferCount)
				{
					// render a chunk of music
					currentMusic->ReadSamples(musicQueue.GetNextFree(), musicBufferSize);
					musicQueue.Push(musicQueue.GetNextFree());
				}

				if (!musicPlaying)
				{
					PlayMusicBuffer(currentMusic.get());
					musicPlaying = true;
				}
				else if (musicQueue.Size() > 0)
				{
					UpdateMusicBuffer(currentMusic.get());
				}

				// TODO: music fade in/out
			}
			else
			{
				musicPlaying = false;
			}
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5ms);
		}
	}

	// The mixer's output: one stereo source, straight to the speakers, its
	// queue 10 ms buffers deep to Galaxy's default Latency of 40 ms.
	void StartMixStream()
	{
		mixBufferFrames = std::max(frequency / 100, 64);
		alGenSources(1, &alMixSource);
		alSourcei(alMixSource, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
		alSourcei(alMixSource, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(alMixSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSourcef(alMixSource, AL_ROLLOFF_FACTOR, 0.0f);
		if (alIsExtensionPresent("AL_SOFT_direct_channels"))
			alSourcei(alMixSource, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
		alMixBuffers.resize(4);
		alGenBuffers((ALsizei)alMixBuffers.size(), &alMixBuffers[0]);
		alGetError();
		mixThread = std::thread([this]() { MixThreadMain(); });
	}

	void StopMixStream()
	{
		mixExit = true;
		mixThread.join();
		alSourceStop(alMixSource);
		alSourcei(alMixSource, AL_BUFFER, 0);
		alDeleteSources(1, &alMixSource);
		alDeleteBuffers((ALsizei)alMixBuffers.size(), &alMixBuffers[0]);
	}

	void MixThreadMain()
	{
		std::vector<float> block(mixBufferFrames * 2);
		auto queue = [&](ALuint buffer) {
			mixer->Mix(block.data(), mixBufferFrames);
			alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, block.data(), (ALsizei)(block.size() * sizeof(float)), frequency);
			alSourceQueueBuffers(alMixSource, 1, &buffer);
		};

		for (ALuint buffer : alMixBuffers)
			queue(buffer);
		alSourcePlay(alMixSource);

		while (!mixExit)
		{
			ALint processed = 0;
			alGetSourcei(alMixSource, AL_BUFFERS_PROCESSED, &processed);
			while (processed-- > 0)
			{
				ALuint buffer = 0;
				alSourceUnqueueBuffers(alMixSource, 1, &buffer);
				if (buffer == 0)
					break;
				queue(buffer);
			}

			// Played dry before a buffer came: on again.
			ALint state = AL_PLAYING;
			alGetSourcei(alMixSource, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING)
				alSourcePlay(alMixSource);

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(2ms);
		}
	}

	ALCdevice* alDevice = nullptr;
	ALCcontext* alContext = nullptr;
	ALenum alError = 0;
	ALuint alMusicSource = 0;
	Array<ALuint> alMusicBuffers;
	Array<ALSoundSource> sources;
	ALint monoSources = 0;
	ALint stereoSources = 0;

	// Deus Ex's mixer, the sounds it holds, and its output
	std::unique_ptr<GalaxyMixer> mixer;
	std::unordered_map<USound*, std::shared_ptr<const GalaxySample>> mixedSounds;
	ALuint alMixSource = 0;
	Array<ALuint> alMixBuffers;
	int mixBufferFrames = 0;
	std::thread mixThread;
	std::atomic<bool> mixExit{ false };

	template<class T> class RingQueue
	{
	public:
		RingQueue()
		{
			Data = nullptr;
			Len = 0;
			Num = 0;
			Current = 0;
		}

		RingQueue(size_t n)
		{
			Data = static_cast<T*>(malloc(sizeof(T) * n));
			Len = n;
			Num = 0;
			Current = 0;
		}

		RingQueue(size_t n, const T& Value)
		{
			Data = static_cast<T*>(malloc(sizeof(T) * n));
			for (int i = 0; i < n; i++)
				Data[i] = Value;

			Len = n;
			Num = 0;
			Current = 0;
		}

		~RingQueue()
		{
			free(Data);
		}

		bool Empty()
		{
			return (Num == 0);
		}

		size_t Size()
		{
			return Num;
		}

		T& Front()
		{
			return Data[Current];
		}

		T& GetNextFree()
		{
			size_t Index = Current + Num;
			if (Index >= Len)
				Index -= Len;

			return Data[Index];
		}

		bool Push(const T& Val)
		{
			if (Num == Len)
				return false;

			GetNextFree() = Val;
			Num++;

			return true;
		}

		bool Push(T& Val)
		{
			if (Num == Len)
				return false;

			GetNextFree() = Val;
			Num++;

			return true;
		}

		T& Pop()
		{
			T& Out = Data[Current];
			if (Num > 0)
			{
				Current++;
				if (Current >= Len)
					Current = 0;

				Num--;
			}
			return Out;
		}

		bool Resize(size_t NewSize)
		{
			T* NewData = static_cast<T*>(realloc(Data, sizeof(T) * NewSize));
			if (NewData == NULL)
				return false;

			Data = NewData;
			Len = NewSize;
			return true;
		}

		void Clear()
		{
			Current = 0;
			Num = 0;
		}

	private:
		T* Data = nullptr;
		size_t Len = 0;
		size_t Current = 0;
		size_t Num = 0;
	};

	int frequency = 48000;
	Array<USound*> sounds;

	// Note: variables changed in musicThreadData *must* be done within a mutex lock to be thread safe
	struct
	{
		std::mutex mutex;
		std::thread thread;
		bool exitFlag = false;
		std::unique_ptr<AudioSource> music;
		bool musicUpdate = false;
		int jumpOrder = -1;                     // an order to jump the playing song to
		std::atomic<int> currentOrder{ -1 };    // the order playing, written by the music thread alone
	} musicThreadData;

	RingQueue<float*> musicQueue;
	int musicBufferCount = 0;
	int musicBufferSize = 0;
	Array<float> musicBuffer;
	float currentMusicVolume = 0.0f;
	float targetMusicVolume = 0.0f;
	float fadeRate = 0.0f;
	float globalSoundVolume = 1.0f;
	float globalSpeechVolume = 1.0f;
};

std::unique_ptr<AudioDevice> AudioDevice::Create(int frequency, int numVoices, int musicBufferCount, int musicBufferSize)
{
	return std::make_unique<OpenALAudioDevice>(frequency, numVoices, musicBufferCount, musicBufferSize);
}
