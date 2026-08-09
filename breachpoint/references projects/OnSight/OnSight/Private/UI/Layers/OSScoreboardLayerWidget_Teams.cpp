// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSScoreboardLayerWidget_Teams.h"

#include "Core/OSGameState.h"
#include "Core/OSPlayerState.h"
#include "UI/Components/OSScoreboardComponent.h"

void UOSScoreboardLayerWidget_Teams::NativeDestruct()
{
	if (_gameState)
		_gameState->OnPlayerArrayChanged.RemoveAll(this);

	if (auto ps = GetOSPlayerState())
		ps->OnTeamAssigned.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSScoreboardLayerWidget_Teams::RebuildFromGameState()
{
	if (!TeamBoard_A || !TeamBoard_B) return;
	if (!_gameState) return;

	_gameState->OnPlayerArrayChanged.AddUObject(this, &UOSScoreboardLayerWidget_Teams::UpdateScoreboard);
	if (auto ps = GetOSPlayerState())
	{
		if (!ps->OnTeamAssigned.IsBoundToObject(this))
			ps->OnTeamAssigned.AddUObject(this, &UOSScoreboardLayerWidget_Teams::OnBoundPlayerTeamAssigned);
		
		if (ps->GetTeamIndex() != FGenericTeamId::NoTeam)
			OnBoundPlayerTeamAssigned(ps);
	}
}
UOSScoreboardComponent* UOSScoreboardLayerWidget_Teams::GetTeamScoreboard( AOSPlayerState* PS ) const
{
	if (!PS || !GetOSPlayerState()) return nullptr;
	//int32 PSTeam = PS->GetTeamIndex();
	//int32 BoundTeam = GetOSPlayerState()->GetTeamIndex();
	return PS->GetTeamIndex() == GetOSPlayerState()->GetTeamIndex() ? TeamBoard_A : TeamBoard_B;
}

void UOSScoreboardLayerWidget_Teams::OnBoundPlayerTeamAssigned( AOSPlayerState* PS )
{
	
	TeamBoard_A->ClearRows();
	TeamBoard_B->ClearRows();
	for (auto PSBase : _gameState->PlayerArray)
	{
		auto* OtherPS = Cast<AOSPlayerState>(PSBase);
		if (!IsValid(OtherPS)) continue;
		UpdateScoreboard(OtherPS, true);
	}
}

void UOSScoreboardLayerWidget_Teams::OnOtherPlayerTeamAssigned( AOSPlayerState* PS )
{
	UpdateScoreboard(PS, true);
}

void UOSScoreboardLayerWidget_Teams::UpdateScoreboard( AOSPlayerState* PlayerState, bool ToAdd )
{
	if (ToAdd && PlayerState->GetTeamIndex() ==  FGenericTeamId::NoTeam)
	{
		// team not yet known. wait.
		if (!PlayerState->OnTeamAssigned.IsBoundToObject(this))
			PlayerState->OnTeamAssigned.AddUObject(this, &UOSScoreboardLayerWidget_Teams::OnOtherPlayerTeamAssigned);
		return;
		return;
	
	}
    
	auto* component = GetTeamScoreboard(PlayerState);
	if (!component) return;
	if (ToAdd)
		component->AddRow(PlayerState);
	else
		component->RemoveRow(PlayerState);
}
