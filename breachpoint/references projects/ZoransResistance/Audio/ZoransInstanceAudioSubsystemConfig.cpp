// Copyright Zollpa LLC


#include "ZoransInstanceAudioSubsystemConfig.h"

float UZoransInstanceAudioSubsystemConfig::GetFadeVolume(const float Progress, const bool bFadeIn, const float Multiplier)
{
	FRuntimeFloatCurve& Curve = bFadeIn ? FadeInCurve : FadeOutCurve;
	return MusicVolume * Curve.GetRichCurve()->Eval(Progress, 0) * Multiplier;
}
