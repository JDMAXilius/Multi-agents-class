#pragma once
// Pack float to uint16 for replication (QuantizeFloat/DequantizeFloat). Used in hit/context net serialization.

static uint16 QuantizeFloat(float P) { return (uint16)FMath::Clamp(FMath::RoundToInt(P * 10.f), 0, 65535); }
static float  DequantizeFloat(uint16 Q) { return float(Q) / 10.f; }