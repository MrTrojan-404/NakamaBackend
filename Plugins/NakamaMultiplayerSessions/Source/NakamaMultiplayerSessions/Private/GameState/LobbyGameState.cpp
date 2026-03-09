// Fill out your copyright notice in the Description page of Project Settings.


#include "GameState/LobbyGameState.h"

#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALobbyGameState, MatchCode);
}

void ALobbyGameState::OnRep_MatchCode()
{
	// Broadcast so LobbyWidget can react
	OnMatchCodeReady.Broadcast(MatchCode);
}