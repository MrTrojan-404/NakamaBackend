// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PrivateMatchInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UPrivateMatchInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MULTIPLAYERSESSIONS_API IPrivateMatchInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void RequestJoinByCode(const FString& Code) = 0;
	virtual void RequestRegisterMatch(const FString& SessionId) = 0;
	virtual void RequestUnregisterMatch() = 0;

	virtual void NotifyJoinByCodeFailed(const FString& Error) = 0;
};
