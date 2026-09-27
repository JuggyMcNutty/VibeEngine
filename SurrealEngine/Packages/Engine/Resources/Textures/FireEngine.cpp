
#include "Precomp.h"
#include "FireEngine.h"
#include <cmath>
#include <cstdlib>

namespace FireEngine
{
	uint8_t SinTable[256];
	uint8_t SinTablePlus32[256];
	uint8_t SinTablePlus128[256];

	static uint32_t RandomWords[64];
	static int RandomIndex = 0;
	static bool TablesReady = false;

	void InitTables()
	{
		if (TablesReady)
			return;
		for (int i = 0; i < 256; i++)
		{
			// As the original rounds it: its constants floats but the sum's,
			// through a float, truncated.
			float value = (float)(std::sin(i * 0.00390625 * (double)6.2831855f) * 127.5 + 127.44999694824219);
			SinTable[i] = (uint8_t)(int)value;
		}
		for (int i = 0; i < 256; i++)
		{
			SinTablePlus32[i] = (uint8_t)std::min(SinTable[i] + 32, 255);
			SinTablePlus128[i] = (uint8_t)(SinTable[i] + 128);
		}
		for (int i = 0; i < 64; i++)
		{
			uint32_t word = 0;
			for (int j = 0; j < 4; j++)
				word |= (uint32_t)(std::rand() & 0xff) << (j * 8);
			RandomWords[i] = word;
		}
		RandomIndex = 0;
		TablesReady = true;
	}

	uint8_t Random()
	{
		uint32_t value = RandomWords[((RandomIndex + 0x80) & 0xfc) >> 2];
		RandomIndex = (RandomIndex + 4) & 0xfc;
		RandomWords[RandomIndex >> 2] ^= value;
		return (uint8_t)value;
	}

	void SetRandomState(const uint32_t* words64, int byteIndex)
	{
		InitTables();
		for (int i = 0; i < 64; i++)
			RandomWords[i] = words64[i];
		RandomIndex = byteIndex & 0xfc;
	}

	void GetRandomState(uint32_t* words64, int& byteIndex)
	{
		for (int i = 0; i < 64; i++)
			words64[i] = RandomWords[i];
		byteIndex = RandomIndex;
	}

	/////////////////////////////////////////////////////////////////////////
	// Fire

	void BuildFireRenderTable(uint8_t* table, int renderHeat)
	{
		for (int i = 0; i < 1024; i++)
		{
			double value = i * 0.25 + 1.0 - (255 - renderHeat) * 0.0625;
			table[i] = (uint8_t)(int64_t)std::max(std::min(value, 255.0), 0.0);
		}
	}

	namespace
	{
		struct SparkPass
		{
			Fire& F;

			uint8_t& Pixel(int x, int y) { return F.Bits[x + (y << F.UBits)]; }

			bool CanSpawn() const { return F.NumSparks < F.SparksLimit; }

			// A new spark at the end of the list. The original writes only
			// what each kind sets: the rest of the slot keeps what it held.
			Spark& Spawn(uint8_t type)
			{
				Spark& s = F.Sparks[F.NumSparks++];
				s.Type = type;
				return s;
			}

			// A step of one pixel with a chance of |speed|/128 (0 to 127 of
			// a random byte below |speed|), in the speed's direction.
			void DriftX(Spark& s, int8_t speed)
			{
				if (speed >= 0)
				{
					if ((Random() & 0x7f) < speed)
						s.X = (uint8_t)(F.UMask & (s.X + 1));
				}
				else
				{
					if ((Random() & 0x7f) < -speed)
						s.X = (uint8_t)(F.UMask & (s.X - 1));
				}
			}

			void DriftY(Spark& s, int8_t speed)
			{
				if (speed >= 0)
				{
					if ((Random() & 0x7f) < speed)
						s.Y = (uint8_t)((s.Y + 1) & F.VMask);
				}
				else
				{
					if ((Random() & 0x7f) < -speed)
						s.Y = (uint8_t)((s.Y - 1) & F.VMask);
				}
			}

			// Up to 7 pixels either way on each axis, each axis on a coin toss.
			void Wander(Spark& s)
			{
				if (Random() & 1)
					s.X = (uint8_t)(F.UMask & (s.X + (Random() & 0xf) - 7));
				if (Random() & 1)
					s.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0xf) - 7));
			}

			// Up to 7 pixels either way, as the difference of two random steps.
			void Jitter(Spark& s)
			{
				int a = Random();
				int b = Random();
				s.X = (uint8_t)(F.UMask & (s.X + (a & 7) - (b & 7)));
				a = Random();
				b = Random();
				s.Y = (uint8_t)(F.VMask & (s.Y + (a & 7) - (b & 7)));
			}

			// A jagged line with a ramp of heat along it, for lightning: the
			// segment packed as the original's, the start and each axis'
			// length with its sign in the lowest bit.
			void FlashRamp(uint8_t x, uint8_t y, uint8_t xs, uint8_t ys, uint8_t heat1, uint8_t heat2)
			{
				if (((ys & 1) && 2 * ys >= xs) || ((xs & 1) && 2 * ys < xs))
				{
					// Drawn from the other end.
					x = (uint8_t)(x + ((xs & 1) ? -(int)xs : (int)xs));
					y = (uint8_t)(y + ((ys & 1) ? -(int)ys : (int)ys));
					xs ^= 1;
					ys ^= 1;
					std::swap(heat1, heat2);
				}

				int steps = std::max(xs, ys) | 1;
				uint8_t wobble[256];
				int wobbleSum = 0;
				for (int i = 0; i < steps; i++)
				{
					wobble[i] = Random();
					wobbleSum += wobble[i];
				}

				int ystep = (ys & 1) ? -1 : 1;
				int xstep = (xs & 1) ? -1 : 1;
				int ylen = (ys & 1) ? -(int)ys : (int)ys;
				int xlen = (xs & 1) ? -(int)xs : (int)xs;

				int heat = heat1 << 23;
				int heatStep = ((heat2 - heat1) * (1 << 23)) / steps;
				if (xs < ys)
				{
					// Down the longer axis a pixel at a time, across by the
					// straight line's step plus the wobble less its mean,
					// in 64ths.
					int across = x << 6;
					int acrossStep = (xlen * 64 - wobbleSum) / steps;
					for (int i = 0; i < ys; i++)
					{
						across += acrossStep + wobble[i];
						int px = F.UMask & (across >> 6);
						int py = F.VMask & y;
						y = (uint8_t)(y + ystep);
						heat += heatStep;
						Pixel(px, py) = (uint8_t)(heat >> 23);
					}
				}
				else
				{
					int across = y << 6;
					int acrossStep = (ylen * 64 - wobbleSum) / steps;
					for (int i = 0; i < xs; i++)
					{
						across += acrossStep + wobble[i];
						int px = F.UMask & x;
						int py = F.VMask & (across >> 6);
						x = (uint8_t)(x + xstep);
						heat += heatStep;
						Pixel(px, py) = (uint8_t)(heat >> 23);
					}
				}
			}

			// Returns false when the spark ended and the last one took its place.
			bool Run(int index)
			{
				Spark& s = F.Sparks[index];
				switch (s.Type)
				{
				case 0: // Burn: a random heat
					Pixel(s.X, s.Y) = Random();
					break;
				case 1: // Sparkle: at a random spot of the A by B area
				{
					int x = s.X + ((Random() * s.A) >> 8);
					int y = s.Y + ((Random() * s.B) >> 8);
					Pixel(x & F.UMask, y & F.VMask) = s.Heat;
					break;
				}
				case 2: // Pulse
					Pixel(s.X, s.Y) = s.Heat;
					s.Heat += s.D;
					break;
				case 3: // Signal: shown above C, a new random start past the top
					if (s.Heat > s.C)
						Pixel(s.X, s.Y) = s.Heat;
					s.Heat += s.D;
					if (s.Heat < s.D)
						s.Heat = Random();
					break;
				case 4: // Blaze: sparks in every direction
					if (CanSpawn() && Random() < 0x80)
					{
						Spark& c = Spawn(32);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = Random();
						c.B = Random();
						c.C = s.C; c.D = s.D;
					}
					break;
				case 5: // OzHasSpoken: rising, spreading
					if (CanSpawn() && Random() < 0x80)
					{
						Spark& c = Spawn(33);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = (uint8_t)((Random() & 0x7f) - 63);
						c.B = 0x81;
						c.D = 2;
					}
					break;
				case 6: // Cone: falling sparks, a fan wide
					if (CanSpawn() && Random() < 0x40)
					{
						Spark& c = Spawn(34);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = (uint8_t)((Random() & 0x7f) - 63);
						c.B = 0;
						c.C = 50;
					}
					break;
				case 7: // BlazeRight
				case 8: // BlazeLeft
					if (CanSpawn() && Random() < 0x40)
					{
						Spark& c = Spawn(34);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = (uint8_t)((Random() & 0x3f) + (s.Type == 7 ? 63 : 0x80));
						c.B = 0xe3;
						c.C = s.C;
					}
					break;
				case 9: // Cylinder: across and back, brightest in front
				{
					int heat = std::min(s.Heat + SinTable[(uint8_t)(s.A + 64)], 255);
					int x = F.UMask & (uint8_t)(s.X + ((s.B * SinTable[s.A]) >> 8));
					Pixel(x, s.Y) = (uint8_t)heat;
					s.A += s.D;
					break;
				}
				case 10: // Cylinder3D: its front half only
				{
					if ((uint8_t)(s.A + 64) < 0x80)
					{
						int heat = std::min(s.Heat + SinTable[(uint8_t)(s.A + 64)], 255);
						int x = F.UMask & (uint8_t)(s.X + ((s.B * SinTable[s.A]) >> 8));
						Pixel(x, s.Y) = (uint8_t)heat;
					}
					s.A += s.D;
					break;
				}
				case 11: // Lissajous
				{
					int x = F.UMask & (uint8_t)(s.X + ((s.Heat * SinTable[s.A]) >> 8));
					int y = F.VMask & (uint8_t)(s.Y + ((s.Heat * SinTable[s.B]) >> 8));
					Pixel(x, y) = SinTablePlus32[(uint8_t)(s.A + 64)];
					s.A += s.C;
					s.B += s.D;
					break;
				}
				case 12: // Jugglers: up and down, brightest in front
				{
					int heat = std::min(s.Heat + SinTable[(uint8_t)(s.A + 64)], 255);
					int y = F.VMask & (uint8_t)(s.Y + ((s.B * SinTable[s.A]) >> 8));
					Pixel(s.X, y) = (uint8_t)heat;
					s.A += s.D;
					break;
				}
				case 13: // Emit: sparks with its own speed
					if (CanSpawn() && Random() < 0x40)
					{
						Spark& c = Spawn(33);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = s.A; c.B = s.B; c.D = s.D;
					}
					break;
				case 14: // Fountain: sparks thrown up, falling back
					if (CanSpawn() && Random() < 0x40)
					{
						Spark& c = Spawn(42);
						c.Heat = s.Heat; c.X = s.X; c.Y = s.Y;
						c.A = s.A; c.B = s.B; c.D = s.D;
					}
					break;
				case 15: // Flocks: circling birds, around a wandering spot
					if (CanSpawn())
					{
						Spark& c = Spawn(39);
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.B = s.A; c.A = 0; c.C = s.B; c.D = s.D; c.Heat = s.Heat;
						s.A += s.C;
					}
					Jitter(s);
					break;
				case 16: // Eels
					if (Random() < 20 && CanSpawn())
					{
						Spark& c = Spawn(38);
						c.Heat = s.Heat;
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.A = Random();
						c.B = Random();
						c.C = s.C;
					}
					Wander(s);
					break;
				case 17: // Organic: rising wisps within C of it
					if (CanSpawn() && Random() < 0x80)
					{
						Spark& c = Spawn(35);
						c.X = (uint8_t)(F.UMask & (s.X + ((s.C * Random()) >> 8)));
						c.Y = (uint8_t)(F.VMask & (s.Y + ((s.C * Random()) >> 8)));
						c.A = (uint8_t)(Random() - 127);
						c.B = 0x81;
						c.C = 0xff;
					}
					break;
				case 18: // WanderOrganic
					if (CanSpawn())
					{
						Spark& c = Spawn(35);
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.A = (uint8_t)(Random() - 127);
						c.B = 0x81;
						c.C = 0xff;
					}
					Wander(s);
					break;
				case 19: // RandomCloud
					if (CanSpawn())
					{
						Spark& c = Spawn(36);
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.A = (uint8_t)((Random() & 0x1f) - 15);
						c.B = 0x81;
						c.C = 0;
					}
					Wander(s);
					break;
				case 20: // CustomCloud
					if (CanSpawn())
					{
						Spark& c = Spawn(37);
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.A = s.A; c.B = s.B; c.C = s.D;
					}
					Jitter(s);
					break;
				case 21: // LocalCloud: within C of it, still
					if (CanSpawn())
					{
						Spark& c = Spawn(37);
						c.X = (uint8_t)(F.UMask & (s.X + ((s.C * Random()) >> 8)));
						c.Y = (uint8_t)(F.VMask & (s.Y + ((s.C * Random()) >> 8)));
						c.A = s.A; c.B = s.B; c.C = s.D;
					}
					break;
				case 22: // Stars: what the pass left under it (PostDrawSparks)
					Pixel(s.X, s.Y) = s.B;
					break;
				case 23: // LineLightning: flashes, C frames each
				case 24: // RampLightning: fading along its length
					if (s.Heat != 0)
					{
						if (s.C != 0)
						{
							s.C--;
							FlashRamp(s.X, s.Y, s.A, s.B, s.Heat, s.Type == 23 ? s.Heat : (uint8_t)(s.Heat >> 3));
						}
						else if (Random() >= s.D)
						{
							s.C = (uint8_t)((Random() + 1) & 5);
						}
					}
					break;
				case 25: // SphereLightning: bolts out to a random point within C
					if (Random() >= s.D)
					{
						uint8_t angle = Random();
						int dx = ((s.C * SinTable[angle]) >> 8) - s.C / 2;
						int dy = ((s.C * SinTable[(uint8_t)(angle + 64)]) >> 8) - s.C / 2;
						uint8_t xs = (uint8_t)(dx > 0 ? (dx & 0xfe) : (-dx | 1));
						uint8_t ys = (uint8_t)(dy > 0 ? (dy & 0xfe) : (-dy | 1));
						FlashRamp(s.X, s.Y, xs, ys, s.Heat, (uint8_t)(s.Heat >> 2));
					}
					break;
				case 26: // Wheel: circling sparks let go as it turns
					if (CanSpawn())
					{
						Spark& c = Spawn(39);
						c.X = s.X; c.Y = s.Y;
						c.B = s.A; c.A = 0; c.C = s.B; c.D = s.D; c.Heat = s.Heat;
					}
					s.A += s.C;
					break;
				case 27: // Gametes
					if (Random() < 20 && CanSpawn())
					{
						Spark& c = Spawn(43);
						c.Heat = s.Heat;
						c.X = (uint8_t)(F.UMask & (s.X + (Random() & 0x1f)));
						c.Y = (uint8_t)(F.VMask & (s.Y + (Random() & 0x1f)));
						c.A = Random();
						c.C = s.C;
						c.D = Random();
					}
					Wander(s);
					break;
				case 28: // Sprinkler: sparks thrown round as it turns
					if (CanSpawn())
					{
						Spark& c = Spawn(40);
						c.X = s.X; c.Y = s.Y; c.Heat = s.Heat;
						c.A = s.A; c.B = s.B; c.C = s.C; c.D = 2;
					}
					s.A += s.D;
					break;
				case 29: // A Lissajous with no vertical speed: across only
				{
					int x = F.UMask & (uint8_t)(s.X + ((s.Heat * SinTable[s.A]) >> 8));
					Pixel(x, s.Y) = SinTablePlus32[(uint8_t)(s.A + 64)];
					s.A += s.C;
					break;
				}
				case 30: // A Lissajous with no horizontal speed: up and down only
				{
					int y = F.VMask & (uint8_t)(s.Y + ((s.Heat * SinTable[s.B]) >> 8));
					Pixel(s.X, y) = SinTablePlus32[(uint8_t)(s.B + 64)];
					s.B += s.D;
					break;
				}

				// What the kinds above let go, each ending on its own.

				case 32: // Blaze's: cooling by 5
					s.Heat -= 5;
					if (s.Heat >= 0xfb)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					DriftY(s, (int8_t)s.B);
					break;
				case 33: // Emit's and OzHasSpoken's: cooling by D
					s.Heat -= s.D;
					if (s.Heat <= s.D)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					DriftY(s, (int8_t)s.B);
					break;
				case 34: // Cone's and the side blazes': C frames, falling ever faster
				{
					s.C--;
					if (s.C == 0)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					int8_t fall = (int8_t)s.B;
					DriftY(s, fall);
					if (fall < 122)
						s.B = (uint8_t)(fall + 3);
					break;
				}
				case 35: // Organic's: fading from 255 by 3, rising 2 rows a frame
					s.C -= 3;
					if (s.C <= 0xbe)
						return false;
					Pixel(s.X, s.Y) = s.C;
					DriftX(s, (int8_t)s.A);
					s.Y = (uint8_t)((s.Y - 2) & F.VMask);
					break;
				case 36: // RandomCloud's: brightening by 4, rising 2 rows a frame
					s.C += 4;
					if (s.C >= 0xfa)
						return false;
					Pixel(s.X, s.Y) = s.C;
					DriftX(s, (int8_t)s.A);
					s.Y = (uint8_t)((s.Y - 2) & F.VMask);
					break;
				case 37: // The clouds': brightening by 4
					s.C += 4;
					if (s.C >= 0xfa)
						return false;
					Pixel(s.X, s.Y) = s.C;
					DriftX(s, (int8_t)s.A);
					DriftY(s, (int8_t)s.B);
					break;
				case 38: // Eels': C frames
					s.C--;
					if (s.C == 0xff)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					DriftY(s, (int8_t)s.B);
					break;
				case 39: // Flocks' and Wheel's: C frames, turning by 16 D a frame
				{
					s.C--;
					if (s.C == 0xff)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					int8_t down = (int8_t)SinTablePlus128[s.B];
					int8_t across = (int8_t)SinTablePlus128[(uint8_t)(s.B + 64)];
					uint16_t heading = (uint16_t)(s.A | (s.B << 8));
					heading += 16 * s.D;
					s.A = (uint8_t)heading;
					s.B = (uint8_t)(heading >> 8);
					DriftX(s, across);
					DriftY(s, down);
					break;
				}
				case 40: // Sprinkler's: C frames, thrown out along A, turning by D
				{
					s.C--;
					if (s.C == 0xff)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					int8_t across = (int8_t)(uint8_t)(SinTable[(uint8_t)(s.A + 64)] + 0x80);
					int8_t down = (int8_t)s.B;
					s.A += s.D;
					DriftX(s, across);
					DriftY(s, down);
					break;
				}
				case 41: // Cooling by C
					s.Heat -= s.C;
					if (s.Heat >= 0xfa)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					DriftY(s, (int8_t)s.B);
					break;
				case 42: // Fountain's: cooling by D, falling faster every other frame
				{
					s.Heat -= s.D;
					if (s.Heat <= 0x32)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					DriftX(s, (int8_t)s.A);
					int8_t fall = (int8_t)s.B;
					DriftY(s, fall);
					if ((F.GlobalPhase & 1) && fall < 124)
						s.B = (uint8_t)(fall + 3);
					break;
				}
				case 43: // Gametes': C frames, swimming to and fro about D
				{
					s.C--;
					if (s.C == 0xff)
						return false;
					Pixel(s.X, s.Y) = s.Heat;
					s.A += 7;
					int swing = s.A & 0x7f;
					if (swing > 0x3f)
						swing = 127 - swing;
					uint8_t angle = (uint8_t)(swing + s.D);
					int8_t down = (int8_t)(uint8_t)(SinTable[(uint8_t)(angle + 64)] - 127);
					int8_t across = (int8_t)(uint8_t)(SinTable[angle] - 127);
					DriftX(s, across);
					DriftY(s, down);
					break;
				}
				default:
					break;
				}
				return true;
			}
		};
	}

	void RedrawSparks(Fire& fire)
	{
		fire.AuxPhase += fire.FX_Frequency;
		fire.GlobalPhase++;
		SparkPass pass{ fire };
		for (int i = 0; i < fire.NumSparks; i++)
		{
			if (!pass.Run(i))
			{
				// The last one takes its place, and waits for the next frame.
				fire.NumSparks--;
				fire.Sparks[i] = fire.Sparks[fire.NumSparks];
			}
		}
	}

	void FirePass(uint8_t* bits, const uint8_t* renderTable, int usize, int vsize, bool rising)
	{
		// Every sum reads the frame as it was before the pass: the rows
		// below are not written yet, and the wrapped ones are copies.
		int umask = usize - 1;
		std::vector<uint8_t> old(bits, bits + (size_t)usize * vsize);
		auto at = [&](int x, int y) { return (int)old[(size_t)(y % vsize) * usize + (x & umask)]; };
		for (int y = 0; y < vsize; y++)
		{
			uint8_t* line = bits + (size_t)y * usize;
			int y1 = rising ? y + 1 : y;
			int y2 = y1 + 1;
			for (int x = 0; x < usize; x++)
			{
				int sum = at(x - 1, y1) + at(x, y1) + at(x + 1, y1) + at(x, y2);
				line[x] = renderTable[sum];
			}
		}
	}

	void PostDrawSparks(Fire& fire)
	{
		if (fire.StarStatus == 0)
			return;
		bool stars = false;
		for (int i = 0; i < fire.NumSparks; i++)
		{
			Spark& s = fire.Sparks[i];
			if (s.Type != 22)
				continue;
			stars = true;
			uint8_t& pixel = fire.Bits[s.X + (s.Y << fire.UBits)];
			s.B = pixel;
			if (pixel < 0x26)
				pixel = s.A;
		}
		if (!stars)
			fire.StarStatus = 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// Water

	void BuildWaterTable(uint8_t* table)
	{
		// Half the sum -- the four neighbours less twice the old height,
		// 512 on --, as a byte.
		for (int i = 0; i < 1536; i++)
			table[i] = (uint8_t)std::max(std::min(i / 2 - 256 + (i < 256 ? 1 : 0), 255), 0);
	}

	void RedrawDrops(Water& water)
	{
		int hmask = water.UMask >> 1;
		int vmask = water.VMask >> 1;
		uint8_t* first = water.Fields;
		uint8_t* second = water.Fields + water.USize / 2;
		auto cell = [&](int x, int y) { return x + (y << water.UBits); };
		auto set = [&](int at, uint8_t value) { first[at] = value; second[at] = value; };
		auto whirl = [&](const Drop& d, uint8_t angle, int shift) {
			int x = hmask & (d.X + (SinTable[angle] >> shift));
			int y = vmask & (d.Y + (SinTable[(uint8_t)(angle + 64)] >> shift));
			set(cell(x, y), d.Depth);
		};

		water.GlobalPhase++;
		for (int i = 0; i < water.NumDrops; i++)
		{
			Drop& d = water.Drops[i];
			int at = cell(d.X, d.Y);
			switch (d.Type)
			{
			case 0: // FixedDepth
				set(at, d.D);
				break;
			case 1: // PhaseSpot
				d.Depth += d.D;
				set(at, SinTable[d.Depth]);
				break;
			case 2: // ShallowSpot
				d.Depth += d.D;
				set(at, (uint8_t)((SinTable[d.Depth] >> 1) + 64));
				break;
			case 3: // HalfAmpl: the upper half of the wave only
			{
				d.Depth += d.D;
				uint8_t value = SinTable[d.Depth];
				set(at, value < 0x80 ? 0x80 : value);
				break;
			}
			case 4: // RandomMover: a push where it was, then a step of up to 3
			{
				int a = Random();
				int b = Random();
				d.X = (uint8_t)(hmask & (d.X + (b & 3) - (a & 3)));
				a = Random();
				b = Random();
				d.Y = (uint8_t)(vmask & (d.Y + (b & 3) - (a & 3)));
				first[at] = 0xb9;
				second[at] = 71;
				break;
			}
			case 5: // FixedRandomSpot
				first[at] = Random();
				second[at] = Random();
				break;
			case 6: // WhirlyThing: circling, small
			case 7: // BigWhirly
			case 64: // the same, turning the other way
			case 65:
			{
				uint16_t angle = (uint16_t)(d.A | (d.B << 8));
				uint16_t speed = (uint16_t)(d.C | (d.D << 8));
				angle = (d.Type < 64) ? (uint16_t)(angle + speed) : (uint16_t)(angle - speed);
				d.A = (uint8_t)angle;
				d.B = (uint8_t)(angle >> 8);
				whirl(d, d.B, (d.Type == 6 || d.Type == 64) ? 4 : 3);
				break;
			}
			case 8: // HorizontalLine
			case 12: // HorizontalOsc
			{
				uint8_t value = d.Depth;
				if (d.Type == 12)
				{
					d.Depth += d.C;
					value = SinTable[d.Depth];
				}
				for (int k = 0; k <= d.D >> 1; k++)
					set(cell(hmask & (d.X + k), d.Y), value);
				break;
			}
			case 9: // VerticalLine
			case 13: // VerticalOsc
			{
				uint8_t value = d.Depth;
				if (d.Type == 13)
				{
					d.Depth += d.C;
					value = SinTable[d.Depth];
				}
				for (int k = 0; k <= d.D >> 1; k++)
					set(cell(d.X, vmask & (d.Y + k)), value);
				break;
			}
			case 10: // DiagonalLine1: /
			case 14: // DiagonalOsc1
			{
				uint8_t value = d.Depth;
				if (d.Type == 14)
				{
					d.Depth += d.C;
					value = SinTable[d.Depth];
				}
				for (int k = 0; k <= d.D >> 1; k++)
					set(cell(hmask & (d.X - k), vmask & (d.Y + k)), value);
				break;
			}
			case 11: // DiagonalLine2: backslash
			case 15: // DiagonalOsc2
			{
				uint8_t value = d.Depth;
				if (d.Type == 15)
				{
					d.Depth += d.C;
					value = SinTable[d.Depth];
				}
				for (int k = 0; k <= d.D >> 1; k++)
					set(cell(hmask & (d.X + k), vmask & (d.Y + k)), value);
				break;
			}
			case 16: // RainDrops: one frame in 16, within D of it
				if ((Random() & 0xf) == 0)
				{
					int y = vmask & (d.Y + ((d.D * Random()) >> 8));
					int x = hmask & (d.X + ((d.D * Random()) >> 8));
					first[cell(x, y)] = d.Depth;
					second[cell(x, y)] = (uint8_t)~d.Depth;
				}
				break;
			case 17: // AreaClamp: a square of D/2 held at its depth
			{
				int size = d.D >> 1;
				for (int row = 0; row < size; row++)
					for (int col = 0; col < size; col++)
						set(cell(hmask & (d.X + col), vmask & (d.Y + row)), d.Depth);
				break;
			}
			case 18: // LeakyTap: a drop each time A runs past 255 by D
			case 19: // DrippyTap: the same, starting again anywhere
				d.A += d.D;
				if (d.A <= d.D)
				{
					if (d.Type == 19)
						d.A = Random();
					first[at] = d.Depth;
					second[at] = (uint8_t)~d.Depth;
				}
				break;
			default:
				break;
			}
		}
	}

	void WaterPass(Water& water, const uint8_t* renderTable, const uint8_t* waterTable)
	{
		int usize = water.USize;
		int vsize = water.VSize;
		int half = usize / 2;
		int rows = vsize / 2;
		uint8_t* fields = water.Fields;
		water.Parity++;
		bool firstMoves = (water.Parity & 1) == 0;

		// The field that does not move gives the slopes and the neighbours.
		std::vector<uint8_t> old(fields, fields + (size_t)usize * rows);
		auto first = [&](int x, int y) { return (int)old[(size_t)((y % rows + rows) % rows) * usize + ((x % half + half) % half)]; };
		auto second = [&](int x, int y) { return (int)old[(size_t)((y % rows + rows) % rows) * usize + half + ((x % half + half) % half)]; };
		auto pixel = [&](int x, int y) -> uint8_t& { return water.Bits[(size_t)((y % vsize + vsize) % vsize) * usize + ((x % usize + usize) % usize)]; };
		auto halve = [](int v) { return v >> 1; };
		const uint8_t* rt = renderTable + 512;
		const uint8_t* wt = waterTable + 512;

		for (int y = 0; y < rows; y++)
		{
			for (int x = 0; x < half; x++)
			{
				if (firstMoves)
				{
					// The first field's cell sits between the second's (x-1..x, y-1..y).
					int sum = second(x - 1, y) + second(x - 1, y - 1) + second(x, y) + second(x, y - 1) - 2 * first(x, y);
					fields[(size_t)y * usize + x] = wt[sum];

					auto slope = [&](int sx, int sy) { return second(sx, sy) - second(sx - 2, sy); };
					int here = slope(x, y), left = slope(x - 1, y);
					int above = halve(slope(x, y) + slope(x, y - 1));
					int aboveLeft = halve(slope(x - 1, y) + slope(x - 1, y - 1));
					pixel(2 * x - 1, 2 * y) = rt[here + left];
					pixel(2 * x, 2 * y) = rt[2 * here];
					pixel(2 * x - 1, 2 * y - 1) = rt[above + aboveLeft];
					pixel(2 * x, 2 * y - 1) = rt[2 * above];
				}
				else
				{
					// The second field's cell sits between the first's (x..x+1, y..y+1).
					int sum = first(x, y) + first(x + 1, y) + first(x, y + 1) + first(x + 1, y + 1) - 2 * second(x, y);
					fields[(size_t)y * usize + half + x] = wt[sum];

					auto slope = [&](int sx, int sy) { return first(sx, sy) - first(sx - 2, sy); };
					int below = slope(x + 1, y + 1), belowLeft = slope(x, y + 1);
					int mid = halve(slope(x + 1, y + 1) + slope(x + 1, y));
					int midLeft = halve(slope(x, y + 1) + slope(x, y));
					pixel(2 * x + 1, 2 * y + 1) = rt[2 * below];
					pixel(2 * x, 2 * y + 1) = rt[below + belowLeft];
					pixel(2 * x + 1, 2 * y) = rt[2 * mid];
					pixel(2 * x, 2 * y) = rt[mid + midLeft];
				}
			}
		}
	}

	void BuildRefractionTable(uint8_t* table, int waveAmp)
	{
		for (int i = 0; i < 1024; i++)
		{
			int shift = (int)(int64_t)((i - 511) * (waveAmp * 0.001953125));
			table[i] = (uint8_t)(int8_t)std::max(std::min(shift, 127), -128);
		}
	}

	void ApplyWet(uint8_t* bits, const uint8_t* source, int usize, int vsize, int ubits, int umask)
	{
		for (int y = 0; y < vsize; y++)
		{
			uint8_t* line = bits + ((size_t)y << ubits);
			const uint8_t* sourceLine = source + ((size_t)y << ubits);
			for (int x = 0; x < usize; x++)
				line[x] = sourceLine[(x + line[x]) & umask];
		}
	}

	void BuildWaveLight(uint8_t* table, int waveAmp, int bumpMapLight, int bumpMapAngle, int phongRange, int phongSize)
	{
		double light = bumpMapLight * 0.01231997119054821;
		double angle = bumpMapAngle * 0.01231997119054821;
		for (int i = 0; i < 1024; i++)
		{
			// The slope as an angle from the flat, towards but never past a
			// right angle, lit by BumpMapLight...
			double slope = waveAmp * 0.0039215689 * (512.0 - i) * 0.0051020407;
			double tilt = slope * 1.570749998092651 / (std::abs(slope) + 1.0) + 1.570796326794897;
			int64_t value = (int64_t)((256 - (phongRange >> 1)) * std::cos(tilt - light));
			// ...with a highlight where it faces BumpMapAngle.
			double size = phongSize * 0.001953125;
			double off = tilt + tilt - light - angle;
			if (size * size > off * off)
				value += (int64_t)((size - std::abs(off)) * (double)(2 * phongRange) / size);
			table[i] = (uint8_t)std::max<int64_t>(std::min<int64_t>(value, 255), 0);
		}
	}

	/////////////////////////////////////////////////////////////////////////
	// Ice

	void MoveIce(Ice& ice, float deltaTime)
	{
		int hspeed = (int8_t)(uint8_t)(ice.HorizPanSpeed + 0x80);
		int vspeed = (int8_t)(uint8_t)(ice.VertPanSpeed + 0x80);
		ice.MasterCount = deltaTime * 120.0f + ice.MasterCount;
		ice.UDisplace = (float)(ice.UDisplace - ((double)hspeed * deltaTime + (double)hspeed * deltaTime));
		ice.VDisplace = (float)((double)vspeed * deltaTime + (double)vspeed * deltaTime + ice.VDisplace);
		double phase = (ice.Frequency + 1) * (double)ice.MasterCount;
		double size = ice.Amplitude + 1;
		switch (ice.PanningStyle)
		{
		case 0: // SLIDE_Linear
			ice.UPosition = ice.UDisplace;
			ice.VPosition = ice.VDisplace;
			break;
		case 1: // SLIDE_Circular
		case 2: // SLIDE_Gestation: its up and down a shade slower
			ice.UPosition = (float)((int)(size * std::sin(phase * 0.0012000001)) + (double)ice.UDisplace);
			ice.VPosition = (float)((int)(size * std::cos(phase * (ice.PanningStyle == 1 ? 0.0012000001 : 0.0011))) + (double)ice.VDisplace);
			break;
		case 3: // SLIDE_WavyX
			ice.VPosition = ice.VDisplace;
			ice.UPosition = (float)((int)(size * std::sin(phase * 0.0012000001) * 0.5) + (double)ice.UDisplace);
			break;
		case 4: // SLIDE_WavyY
			ice.UPosition = ice.UDisplace;
			ice.VPosition = (float)((int)(size * std::cos(phase * 0.0012000001) * 0.5) + (double)ice.VDisplace);
			break;
		default:
			break;
		}
	}

	void BlitIce(uint8_t* bits, const uint8_t* glass, const uint8_t* source, int usize, int vsize, int ubits, int umask, int vmask, int upos, int vpos, bool moveIce)
	{
		for (int y = 0; y < vsize; y++)
		{
			uint8_t* line = bits + ((size_t)y << ubits);
			if (moveIce)
			{
				const uint8_t* sourceLine = source + ((size_t)y << ubits);
				const uint8_t* glassLine = glass + ((size_t)((y + vpos) & vmask) << ubits);
				for (int x = 0; x < usize; x++)
					line[x] = sourceLine[(x + glassLine[(x + upos) & umask]) & umask];
			}
			else
			{
				const uint8_t* sourceLine = source + ((size_t)((y + vpos) & vmask) << ubits);
				const uint8_t* glassLine = glass + ((size_t)y << ubits);
				for (int x = 0; x < usize; x++)
					line[x] = sourceLine[(x + upos + glassLine[x]) & umask];
			}
		}
	}
}
