// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/LeaderboardInterface.h"
#include "Interface/PlayerBackendInterface.h"
#include "Interface/PrivateMatchInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameBridgeSubsystem.generated.h"

/**
 * 
 */

struct FNakamaStoreObjectAcks;
struct FNakamaStorageObjectList;
struct FNakamaError;
class UNakamaBackendSubsystem;
class UMultiplayerSessionsSubsystem;
// ─── Delegates ────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrivateMatchReady, FString, Code);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrivateMatchJoinReady, FString, SteamSessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBridgeError, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDisplayNameSynced,FString, DisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFriendsSynced);

UCLASS()
class NAKAMAMULTIPLAYERSESSIONS_API UGameBridgeSubsystem :
public UGameInstanceSubsystem,
public IPrivateMatchInterface,
public IPlayerBackendInterface,
public ILeaderboardInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;  // ← add this

	// IPrivateMatchInterface
	virtual void RequestJoinByCode(const FString& Code) override;
	virtual void RequestRegisterMatch(const FString& SessionId) override;
	virtual void RequestUnregisterMatch() override;

	// IPlayerBackendInterface
	virtual bool    IsBackendAuthenticated() const override;
	virtual FString GetBackendDisplayName() const override;
	virtual void    RequestSetDisplayName(const FString& Name) override;

	// ILeaderboardInterface
	virtual void SubmitScore(const FString& LeaderboardId, int64 Score) override;
	virtual void FetchTopScores(const FString& LeaderboardId, int32 Limit) override;

	
    // ── Accessors ─────────────────────────────────────────────────────────────
    // Shorthand getters so features don't need to reach into GameInstance themselves
    UMultiplayerSessionsSubsystem* GetSteam()  const;
    UNakamaBackendSubsystem*       GetNakama() const;

    // ── Feature: Private Matches ──────────────────────────────────────────────
    // HOST: Call once Steam session is created
    UFUNCTION(BlueprintCallable, Category="Bridge|PrivateMatch")
    void RegisterPrivateMatch(const FString& SteamSessionId);

    // CLIENT: Player submits a code in UI
    UFUNCTION(BlueprintCallable, Category="Bridge|PrivateMatch")
    void JoinByCode(const FString& Code);

    // HOST: Call when lobby closes / host quits
    UFUNCTION(BlueprintCallable, Category="Bridge|PrivateMatch")
    void UnregisterPrivateMatch();

    // ── Feature: Display Name Sync ────────────────────────────────────────────
    // Syncs Steam display name → Nakama on first login if no name is set yet
    UFUNCTION(BlueprintCallable, Category="Bridge|Identity")
    void SyncDisplayNameFromSteam();

    // Explicit override — player picks a custom name
    UFUNCTION(BlueprintCallable, Category="Bridge|Identity")
    void SetDisplayName(const FString& NewName);

    // ── Feature: Friends Sync ─────────────────────────────────────────────────
    // Mirrors Steam friends list into Nakama friends
    // (bImportFriends=true on AuthenticateSteam handles initial import,
    //  call this for a manual refresh)
    UFUNCTION(BlueprintCallable, Category="Bridge|Friends")
    void SyncSteamFriendsToNakama();

    // ── Feature: Presence / Status ────────────────────────────────────────────
    // Writes current match state to Nakama presence so friends can see it
    UFUNCTION(BlueprintCallable, Category="Bridge|Presence")
    void UpdatePresence(const FString& StatusMessage);

    // ── Delegates ─────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable)
	FOnPrivateMatchReady OnPrivateMatchReady;
    UPROPERTY(BlueprintAssignable)
	FOnPrivateMatchJoinReady OnPrivateMatchJoinReady;
    UPROPERTY(BlueprintAssignable)
	FOnBridgeError OnBridgeError;
    UPROPERTY(BlueprintAssignable)
	FOnDisplayNameSynced OnDisplayNameSynced;
    UPROPERTY(BlueprintAssignable)
	FOnFriendsSynced OnFriendsSynced;

private:
	FString ActiveMatchCode;
	FString PendingDisplayName;

	static FString GenerateCode();

	// Storage callbacks
	UFUNCTION()
	void HandlePrivateMatchWriteSuccess(const FNakamaStoreObjectAcks& Acks);
	UFUNCTION()
	void HandlePrivateMatchReadSuccess(const FNakamaStorageObjectList& Objects);
	UFUNCTION()
	void HandleStorageError(const FNakamaError& Error);

	// Display name callbacks
	UFUNCTION()
	void OnNakamaAccountLoaded();
	UFUNCTION()
	void HandleDisplayNameUpdated(const FString& Name);

	// Friends helpers
	void SubmitFriendBatch(const TArray<FString>& SteamIds);

	// OSS helpers
	FString GetSteamDisplayName() const;
	TArray<FString> GetSteamFriendIds() const;
	
};
