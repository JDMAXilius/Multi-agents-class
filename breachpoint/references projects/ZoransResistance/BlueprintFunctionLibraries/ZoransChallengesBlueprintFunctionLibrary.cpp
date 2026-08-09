// Copyright Zollpa LLC


#include "ZoransChallengesBlueprintFunctionLibrary.h"

FText UZoransChallengesBlueprintFunctionLibrary::GetChallengeName(const FChallengeData& Challenge)
{
	FString ResultString = "";
	FStringFormatNamedArguments NamedArgs;
	NamedArgs.Add("Num", Challenge.Required);
	switch (Challenge.Type)
	{
		case EChallengeType::Kills:
			{
				ResultString = FString("Get {Num} kills");
				break;
			}
		case EChallengeType::Headshots:
			{
				ResultString = FString("Get {Num} weakpoint hits");
				break;
			}
		case EChallengeType::Mitigation:
			{
				ResultString = FString("Mitigate {Num} damage");
				break;
			}
		case EChallengeType::Objective:
			{
				for (auto [Type, Value] : Challenge.Conditions)
				{
					if(Type == EChallengeConditionType::Gamemode)
					{
						if(Value == "Data Dash")
						{
							ResultString = Challenge.Required == 1 ? FString("Capture a beacon") : FString("Capture {Num} beacons");
							break;
						}
					}
				}
				if(!ResultString.IsEmpty())
					break;
				ResultString = Challenge.Required == 1 ? FString("Get an objective score") : FString("Get {Num} objective score");
				break;
			}
		case EChallengeType::WinGame:
			{
				ResultString = Challenge.Required == 1 ? FString("Win a game") : FString("Win {Num} games");
				break;
			}
		case EChallengeType::PlayGame:
			{
				ResultString = Challenge.Required == 1 ? FString("Play a game") : FString("Play {Num} games");
				break;
			}
		case EChallengeType::CompleteChallenges:
			{
				ResultString = FString("Complete {Num} challenges");
				break;
			}
		default: ;
	}

	ResultString = FString::Format(*ResultString, NamedArgs);

	
	if(Challenge.Required > 1 && Challenge.Type != EChallengeType::CompleteChallenges)
	{
		switch (Challenge.AggregateFunc)
		{
			case EChallengeAggregateType::Consecutive:
				{
					ResultString = FString::Printf(TEXT("%s in a row"), *ResultString);
					break;
				}
			case EChallengeAggregateType::Max:
				{
					ResultString = FString::Printf(TEXT("%s in a match"), *ResultString);
					break;
				}
			default: ;
		}

	}

	return FText::FromString(ResultString);
}

FText UZoransChallengesBlueprintFunctionLibrary::GetConditionText(const FChallengeData& Challenge, FWeaponNameToDisplayName WeaponNameFunc)
{
	FString ResultString;
	for (auto& [Type, Value] : Challenge.Conditions)
	{
		switch (Type)
		{
			case EChallengeConditionType::Map:
				ResultString = FString::Printf(TEXT("%sin %s "), *ResultString, *Value);
				break;
			case EChallengeConditionType::Weapon:
				{
					FString WeaponName = "";
					WeaponNameFunc.Execute(Value, WeaponName);
					ResultString = FString::Printf(TEXT("%swith %s "), *ResultString, *WeaponName);
					break;
				}
			case EChallengeConditionType::Gamemode:
				ResultString = FString::Printf(TEXT("%son %s "), *ResultString, *Value);
				break;
			case EChallengeConditionType::LessThanDeaths:
				ResultString = FString::Printf(TEXT("%swith less than %s deaths "), *ResultString, *Value);
				break;
			default: ;
		}
	}
	return FText::FromString(ResultString);
}
