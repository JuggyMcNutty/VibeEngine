
#include "Precomp.h"
#include "UWaveTexture.h"

void UWaveTexture::Prepare()
{
	UWaterTexture::Prepare();
	FireEngine::BuildWaveLight(RenderTable().data(), WaveAmp(), BumpMapLight(), BumpMapAngle(), PhongRange(), PhongSize());
}
