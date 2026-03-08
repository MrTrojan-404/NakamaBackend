// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NakamaSessionSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class NAKAMAMULTIPLAYERSESSIONS_API UNakamaSessionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString AuthToken;

	UPROPERTY()
	FString RefreshToken;
	
};
