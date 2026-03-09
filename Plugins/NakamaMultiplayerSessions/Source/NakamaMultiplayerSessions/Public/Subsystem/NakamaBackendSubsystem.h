// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NakamaClient.h"
#include "NakamaSession.h"
#include "NakamaRealtimeClient.h"
#include "NakamaBackendSubsystem.generated.h"

/**
 * 
 */

// ─── Auth Provider Enum ───────────────────────────────────────────────────────
// Add new providers here as you scale (Epic, GOG, etc.)
UENUM(BlueprintType)
enum class ENakamaAuthProvider : uint8
{
    Steam       UMETA(DisplayName = "Steam"),
    Email       UMETA(DisplayName = "Email"),
    Device      UMETA(DisplayName = "Device"),
};

// ─── Delegates ────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNakamaLoginSuccess, UNakamaSession*, Session);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNakamaLoginError,   const FNakamaError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAccountLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsernameUpdated, const FString&, Username);

UCLASS()
class NAKAMAMULTIPLAYERSESSIONS_API UNakamaBackendSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    // ── Delegates ─────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnNakamaLoginSuccess OnLoginSuccess;
    UPROPERTY(BlueprintAssignable) FOnNakamaLoginError   OnLoginError;
    UPROPERTY(BlueprintAssignable) FOnAccountLoaded      OnAccountLoaded;
    UPROPERTY(BlueprintAssignable) FOnUsernameUpdated    OnUsernameUpdated;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Auth ──────────────────────────────────────────────────────────────────
    // Primary login — auto-selects provider based on platform
    void Login();

    // Provider-specific overrides for explicit use
    void LoginWithSteam();
    void LoginWithEmail(const FString& Email, const FString& Password, const FString& Username);
    void LoginWithDevice();

    // ── Account ───────────────────────────────────────────────────────────────
    void    FetchAccount();
    bool    HasPlayerName() const;
    void    SetPlayerName(const FString& NewName);
    void    SaveSession();

    // ── Queries (Blueprint-callable) ──────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Nakama")
    bool    IsLoggedIn()      const;
    UFUNCTION(BlueprintCallable, Category="Nakama")
    FString GetUsername()     const;
    UFUNCTION(BlueprintCallable, Category="Nakama")
    FString GetNakamaUserId() const;
    UFUNCTION(BlueprintCallable, Category="Nakama")
    ENakamaAuthProvider GetActiveProvider() const { return ActiveProvider; }

    // ── Accessors for other subsystems (Storage, Leaderboard, Friends, etc.) ──
    UFUNCTION(BlueprintCallable, Category="Nakama")
    UNakamaClient* GetClient() const { return NakamaClient; }
    UFUNCTION(BlueprintCallable, Category="Nakama")
    UNakamaSession* GetSession() const { return UserSession; }
    UFUNCTION(BlueprintCallable, Category="Nakama")
    UNakamaRealtimeClient* GetRealtimeClient() const { return RealtimeClient; }

private:
    // ── Core objects ──────────────────────────────────────────────────────────
    UPROPERTY()
    UNakamaClient* NakamaClient = nullptr;
    UPROPERTY()
    UNakamaSession* UserSession = nullptr;
    UPROPERTY()
    UNakamaRealtimeClient* RealtimeClient = nullptr;

    ENakamaAuthProvider ActiveProvider = ENakamaAuthProvider::Steam;
    FString PendingUsername;

    // ── Auth callbacks ────────────────────────────────────────────────────────
    UFUNCTION()
    void HandleAuthSuccess(UNakamaSession* Session);
    UFUNCTION()
    void HandleAuthError(const FNakamaError& Error);

    // ── Account callbacks ─────────────────────────────────────────────────────
    UFUNCTION()
    void HandleGetAccountSuccess(const FNakamaAccount& Account);
    UFUNCTION()
    void HandleGetAccountError(const FNakamaError& Error);

    // ── Update callbacks ──────────────────────────────────────────────────────
    UFUNCTION()
    void HandleUsernameUpdateSuccess();
    UFUNCTION()
    void HandleUsernameUpdateError(const FNakamaError& Error);

    // ── Internal helpers ──────────────────────────────────────────────────────
    bool TryRestoreSession();
    void SetupRealtimeConnection(UNakamaSession* Session);

    // Auth provider helpers
    FString GetSteamSessionTicket() const; // Returns hex auth ticket (not SteamID)
    FString GetDeviceId() const;
};

