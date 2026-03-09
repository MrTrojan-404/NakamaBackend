// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LeaderboardInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class ULeaderboardInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class NAKAMAMULTIPLAYERSESSIONS_API ILeaderboardInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void SubmitScore(const FString& LeaderboardId, int64 Score) = 0;
	virtual void FetchTopScores(const FString& LeaderboardId, int32 Limit) = 0;

};
