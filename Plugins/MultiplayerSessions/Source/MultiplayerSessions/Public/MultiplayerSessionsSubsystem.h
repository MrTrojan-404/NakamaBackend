// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/PrivateMatchInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MultiplayerSessionsSubsystem.generated.h"

class IPlayerBackendInterface;
class IPrivateMatchInterface;
//Declaring our own system delegates for the Menu class to bind callbacks to
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionComplete, const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);

//For UI------------------------
USTRUCT(BlueprintType)
struct FServerRowInfo
{
	GENERATED_BODY()


	UPROPERTY(BlueprintReadOnly) FString ServerName;
	UPROPERTY(BlueprintReadOnly) FString MapName = "Default";
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 PingMs = -1;
	UPROPERTY(BlueprintReadOnly) FString HostUserName;
	UPROPERTY(BlueprintReadOnly) FString NakamaDisplayName;
	UPROPERTY(BlueprintReadOnly) FString SessionId;

	// Can add other fields (tags, custom settings)
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnServerListUpdated, const TArray<FServerRowInfo>&, ServerList, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDisplayNameReady, const FString&, Name);

UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UMultiplayerSessionsSubsystem();

	//To Handle Session functionality, Menu class will call these

	void CreateSession(int32 NumPublicConnections, FString MatchType, bool bPrivate = false);
	void FindSession(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	void StartSession();

	void NotifyJoinByCodeFailed(const FString& Error);
	

	//Our own custom delegates for the Menu class to bind callbacks to
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnFindSessionComplete MultiplayerOnFindSessionComplete;
	
	int32 DesiredNumPublicConnections{};
	FString DesiredMatchType{};
	bool IsSessionPrivate() const { return bLastSessionWasPrivate; }

	UPROPERTY(BlueprintAssignable)
	FOnServerListUpdated OnServerListUpdated;

	/** 
	 * Finds an FOnlineSessionSearchResult by session id string (Result.GetSessionIdStr()).
	 * Returns true and fills OutResult when found; otherwise returns false.
	 */
	bool GetSearchResultBySessionId(const FString& SessionId, FOnlineSessionSearchResult& OutResult) const;
	void BroadcastDisplayName(const FString& Name);


	UFUNCTION(BlueprintCallable)
	const TArray<FServerRowInfo>& GetCachedServerRows() const { return CachedServerRows; }

	// Finds a specific session by its ID string, then fires MultiplayerOnJoinSessionComplete
	// Used by GameBridgeSubsystem after resolving a private match code
	void FindSessionById(const FString& SessionId);

	// Called by Bridge after Nakama account loads
	void SetHostDisplayName(const FString& Name) { PendingHostDisplayName = Name; }

	FString GetCurrentSessionId() const;
	FOnlineSessionSettings* GetCurrentSessionSettings() const;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerDisplayNameReady OnDisplayNameReady;
	
protected:
	//Internal Callbacks for our Delegates we'll add to the online session interface delegates list
	//These don't need to be called outside the classes

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	//To add to the Online Session Interface Delegate list
	//We will bind our MultiplayerSessionSubsystem internal callbacks to these.

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate; 
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate; 
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	bool bCreateSessionOnDestroy{false};
	int32 LastNumPublicConnections;
	FString LastMatchType;

	//Function to Create info about server for UI from SearchResult
	static FServerRowInfo MakeServerRowInfoFromSearchResult(const FOnlineSessionSearchResult& Result);

	TArray<FServerRowInfo> CachedServerRows;

	FString PendingJoinSessionId;
	bool    bFindingBySessionId = false;
	bool bLastSessionWasPrivate = false;
	
	FString PendingHostDisplayName;

public:
	static void RegisterPrivateMatchInterface(IPrivateMatchInterface* Handler)
	{
		PrivateMatchHandler = Handler;
	}
	static IPrivateMatchInterface* GetPrivateMatchInterface()
	{
		return PrivateMatchHandler;
	}

	static void RegisterPlayerBackendInterface(IPlayerBackendInterface* Handler)
	{
		PlayerBackendHandler = Handler;
	}
	static IPlayerBackendInterface* GetPlayerBackendInterface()
	{
		return PlayerBackendHandler;
	}

	FString GetCachedDisplayName() const { return CachedDisplayName; }


private:
	static IPrivateMatchInterface* PrivateMatchHandler;
	static IPlayerBackendInterface* PlayerBackendHandler;
	FString CachedDisplayName;
};
