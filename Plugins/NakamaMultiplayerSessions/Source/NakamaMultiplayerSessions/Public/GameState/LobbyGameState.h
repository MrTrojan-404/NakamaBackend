// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchCodeReady, FString, Code);

UCLASS()
class NAKAMAMULTIPLAYERSESSIONS_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(ReplicatedUsing=OnRep_MatchCode, BlueprintReadOnly, Category="Lobby")
	FString MatchCode;

	UFUNCTION()
	void OnRep_MatchCode();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable)
	FOnMatchCodeReady OnMatchCodeReady;
	
};
