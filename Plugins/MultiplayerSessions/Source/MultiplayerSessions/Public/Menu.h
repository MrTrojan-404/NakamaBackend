// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ServerRowWidget.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"

class UScrollBox;
/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/ThirdPerson/Maps/Lobby")));

	void SetServerListEnabled(bool bEnabled);

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	//Callbacks for the custom delegates on the MultiplayerSessionSubsystem (Dynamic Multicast function callback need to be UFUNCTION())
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

	// Server row widget class to spawn (set in Blueprint or in code)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TSubclassOf<UServerRowWidget> ServerRowWidgetClass;
	
private:
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;
	UPROPERTY(meta = (BindWidget))
	UButton* FindSessionButton;

	UFUNCTION()
	void HostButtonClicked();
	UFUNCTION()
	void FindSessionButtonClicked();

	void MenuTeardown();

	//The Subsystem designed to handle all online sessions functionality
	UPROPERTY()
	UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))

	int32 NumPublicConnections{4};

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FString MatchType{TEXT("FreeForAll")};
	
	FString PathToLobby{TEXT("")};
	

	// ScrollBox in your UMG that will host the server rows
	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* ServerListScrollBox;

	// Spawns server rows into the provided ScrollBox. You can call SpawnServerRows(ServerListScrollBox);
	// after you receive OnServerListUpdated or OnFindSession callbacks.
	UFUNCTION()
	void SpawnServerRows(UScrollBox* TargetScrollBox);

	// Handler for clicks from a row widget
	UFUNCTION()
	void HandleServerRowClicked(const FServerRowInfo& Info, int32 RowIndex);

	UFUNCTION()
	void OnServerListUpdatedHandler(const TArray<FServerRowInfo>& ServerList, bool bWasSuccessful);

	UPROPERTY()
	UServerRowWidget* PendingJoinRow = nullptr;
};
