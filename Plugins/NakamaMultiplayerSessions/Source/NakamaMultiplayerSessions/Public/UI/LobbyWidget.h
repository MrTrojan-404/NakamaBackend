// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyWidget.generated.h"

class UTextBlock;
class UGameBridgeSubsystem;
class UMultiplayerSessionsSubsystem;
/**
 * 
 */
UCLASS()
class NAKAMAMULTIPLAYERSESSIONS_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Lobby")
	void LobbySetup();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// BindWidgetOptional — won't crash if not present in UMG blueprint
	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* VisibilityText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* MatchCodeText = nullptr;

	UPROPERTY()
	UMultiplayerSessionsSubsystem* MSS = nullptr;

	UPROPERTY()
	UGameBridgeSubsystem* Bridge = nullptr;

	// Only fires on host of a private match
	UFUNCTION()
	void OnMatchCodeReceived(FString Code);

	// True if this machine is the listen server (host)
	bool IsHost() const;
	
};
