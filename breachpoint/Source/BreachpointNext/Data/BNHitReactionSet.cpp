#include "Data/BNHitReactionSet.h"

const FBNHitReactionRow* UBNHitReactionSet::FindRow(EBNHitDirection Direction, EBNHitSeverity Severity) const
{
	// Exact severity, then down. Never up: promoting a Light hit to a Heavy animation reads as the
	// game lying about how hard you were hit; demoting reads as nothing worse than restraint.
	for (int32 Step = static_cast<int32>(Severity); Step >= 0; --Step)
	{
		for (const FBNHitReactionRow& Row : Rows)
		{
			if (Row.Direction == Direction && Row.Severity == static_cast<EBNHitSeverity>(Step) && !Row.Montages.IsEmpty())
			{
				return &Row;
			}
		}
	}
	return nullptr;
}

EBNHitSeverity UBNHitReactionSet::SeverityForDamage(float Damage) const
{
	if (Damage <= LightMaxDamage)
	{
		return EBNHitSeverity::Light;
	}
	return Damage <= MediumMaxDamage ? EBNHitSeverity::Medium : EBNHitSeverity::Heavy;
}
