#include "Camera/CombatFireCameraShake.h"
#include "Shakes/WaveOscillatorCameraShakePattern.h"

UCombatFireCameraShake::UCombatFireCameraShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = true;
	UWaveOscillatorCameraShakePattern* Pattern = CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(TEXT("FireImpulse"));
	Pattern->Duration = 0.13f;
	Pattern->BlendInTime = 0.012f;
	Pattern->BlendOutTime = 0.10f;
	Pattern->X.Amplitude = 0.25f;
	Pattern->Y.Amplitude = 0.12f;
	Pattern->Z.Amplitude = 0.22f;
	Pattern->Pitch.Amplitude = 0.60f;
	Pattern->Yaw.Amplitude = 0.16f;
	Pattern->Roll.Amplitude = 0.20f;
	Pattern->FOV.Amplitude = 0.f;
	Pattern->X.Frequency = Pattern->Y.Frequency = Pattern->Z.Frequency = 18.f;
	Pattern->Pitch.Frequency = Pattern->Yaw.Frequency = Pattern->Roll.Frequency = 22.f;
	SetRootShakePattern(Pattern);
}
