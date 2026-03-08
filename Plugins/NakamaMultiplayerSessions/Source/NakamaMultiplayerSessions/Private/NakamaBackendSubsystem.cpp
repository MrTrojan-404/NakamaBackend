// Fill out your copyright notice in the Description page of Project Settings.


#include "NakamaBackendSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "NakamaClient.h"
#include "OnlineSubsystemUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/NakamaSessionSaveGame.h"

// ─── Server Config ─────────────────────────────────────────────────────────
// TODO: Move to a UDeveloperSettings subclass or DataAsset for per-game config
static const FString NAKAMA_SERVER_KEY = TEXT("defaultkey");
static const FString NAKAMA_HOST       = TEXT("127.0.0.1");
static const int32   NAKAMA_PORT       = 7350;
static const bool    NAKAMA_USE_SSL    = false;
// ───────────────────────────────────────────────────────────────────────────

// ─── Lifecycle ──────────────────────────────────────────────────────────────

void UNakamaBackendSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    NakamaClient = UNakamaClient::CreateDefaultClient(
        NAKAMA_SERVER_KEY, NAKAMA_HOST, NAKAMA_PORT, NAKAMA_USE_SSL, /*bEnableDebug=*/true);

    if (!TryRestoreSession())
    {
        Login();
    }
}

void UNakamaBackendSubsystem::Deinitialize()
{
    if (RealtimeClient)
    {
        RealtimeClient->Disconnect();
        RealtimeClient = nullptr;
    }
    Super::Deinitialize();
}

// ─── Auth: Public API ────────────────────────────────────────────────────────

void UNakamaBackendSubsystem::Login()
{
    // Auto-select provider. Swap this logic for platform detection as you scale.
    // e.g. #if PLATFORM_XBOXONE → LoginWithMicrosoft()
    //      #if PLATFORM_PS4     → LoginWithSony()
    LoginWithSteam();
}

void UNakamaBackendSubsystem::LoginWithSteam()
{
    if (!NakamaClient)
    {
        UE_LOG(LogTemp, Error, TEXT("[Nakama] Client not initialized"));
        return;
    }

    // Get the Steam session ticket — this is what AuthenticateSteam expects,
    // NOT the SteamID. The ticket is verified server-side by Nakama via Steam's API.
    FString SteamTicket = GetSteamSessionTicket();
    if (SteamTicket.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[Nakama] Failed to get Steam session ticket. Is Steam running?"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Nakama] Authenticating with Steam..."));
    ActiveProvider = ENakamaAuthProvider::Steam;

    FOnAuthUpdate Success;
    Success.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthError);

    // bImportFriends = true: Nakama will mirror your Steam friends list automatically.
    // Steam friends in Nakama are queryable via the Friends API.
    const bool bCreate        = true;
    const bool bImportFriends = true;
    const FString Username    = TEXT(""); // Leave empty — Nakama generates one from Steam display name

    NakamaClient->AuthenticateSteam(
        SteamTicket,
        Username,
        bCreate,
        bImportFriends,
        {}, // Session vars — add platform/version metadata here if desired
        Success,
        Error
    );
}

void UNakamaBackendSubsystem::LoginWithEmail(
    const FString& Email, const FString& Password, const FString& Username)
{
    if (!NakamaClient) return;

    ActiveProvider = ENakamaAuthProvider::Email;

    FOnAuthUpdate Success;
    Success.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthError);

    NakamaClient->AuthenticateEmail(Email, Password, Username, true, {}, Success, Error);
}

void UNakamaBackendSubsystem::LoginWithDevice()
{
    if (!NakamaClient) return;

    ActiveProvider = ENakamaAuthProvider::Device;

    FOnAuthUpdate Success;
    Success.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UNakamaBackendSubsystem::HandleAuthError);

    NakamaClient->AuthenticateDevice(GetDeviceId(), TEXT(""), true, {}, Success, Error);
}

// ─── Auth: Callbacks ─────────────────────────────────────────────────────────

void UNakamaBackendSubsystem::HandleAuthSuccess(UNakamaSession* Session)
{
    UserSession = Session;
    SaveSession();
    SetupRealtimeConnection(Session);
    FetchAccount();

    UE_LOG(LogTemp, Log, TEXT("[Nakama] Auth success. Provider: %d | User: %s"),
        (int32)ActiveProvider, *Session->SessionData.Username);

    OnLoginSuccess.Broadcast(UserSession);
}

void UNakamaBackendSubsystem::HandleAuthError(const FNakamaError& Error)
{
    UE_LOG(LogTemp, Error, TEXT("[Nakama] Auth failed: %s"), *Error.Message);
    OnLoginError.Broadcast(Error);
}

// ─── Account ─────────────────────────────────────────────────────────────────

void UNakamaBackendSubsystem::FetchAccount()
{
    if (!UserSession || !NakamaClient) return;

    FOnUserAccountInfo Success;
    Success.AddDynamic(this, &UNakamaBackendSubsystem::HandleGetAccountSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UNakamaBackendSubsystem::HandleGetAccountError);

    NakamaClient->GetAccount(UserSession, Success, Error);
}

void UNakamaBackendSubsystem::HandleGetAccountSuccess(const FNakamaAccount& Account)
{
    if (UserSession)
    {
        UserSession->SessionData.Username = Account.User.Username;
    }
    UE_LOG(LogTemp, Log, TEXT("[Nakama] Account loaded. Username: %s | ID: %s"),
        *Account.User.Username, *Account.User.Id);

    OnAccountLoaded.Broadcast();
}

void UNakamaBackendSubsystem::HandleGetAccountError(const FNakamaError& Error)
{
    UE_LOG(LogTemp, Error, TEXT("[Nakama] GetAccount failed: %s"), *Error.Message);
}

void UNakamaBackendSubsystem::SetPlayerName(const FString& NewName)
{
    if (!UserSession || !NakamaClient) return;

    PendingUsername = NewName;

    FOnUpdateAccount Success;
    Success.AddDynamic(this, &UNakamaBackendSubsystem::HandleUsernameUpdateSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UNakamaBackendSubsystem::HandleUsernameUpdateError);

    NakamaClient->UpdateAccount(
        UserSession, NewName,
        TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
        Success, Error);
}

void UNakamaBackendSubsystem::HandleUsernameUpdateSuccess()
{
    if (UserSession) UserSession->SessionData.Username = PendingUsername;
    UE_LOG(LogTemp, Log, TEXT("[Nakama] Username updated: %s"), *PendingUsername);
    OnUsernameUpdated.Broadcast(PendingUsername);
}

void UNakamaBackendSubsystem::HandleUsernameUpdateError(const FNakamaError& Error)
{
    UE_LOG(LogTemp, Error, TEXT("[Nakama] Username update failed: %s"), *Error.Message);
}

// ─── Queries ─────────────────────────────────────────────────────────────────

bool UNakamaBackendSubsystem::IsLoggedIn() const
{
    return UserSession != nullptr && !UserSession->IsExpired();
}

FString UNakamaBackendSubsystem::GetUsername() const
{
    return UserSession ? UserSession->SessionData.Username : TEXT("");
}

FString UNakamaBackendSubsystem::GetNakamaUserId() const
{
    return UserSession ? UserSession->SessionData.UserId : TEXT("");
}

bool UNakamaBackendSubsystem::HasPlayerName() const
{
    if (!UserSession) return false;
    const FString& Name = UserSession->SessionData.Username;
    // Steam auth generates a username from your Steam display name,
    // so we check against both empty and the literal fallback Nakama uses.
    return !Name.IsEmpty() && !Name.StartsWith(TEXT("steam_"));
}

// ─── Session Persistence ─────────────────────────────────────────────────────

void UNakamaBackendSubsystem::SaveSession()
{
    if (!UserSession) return;

    auto* Save = Cast<UNakamaSessionSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UNakamaSessionSaveGame::StaticClass()));

    Save->AuthToken    = UserSession->SessionData.AuthToken;
    Save->RefreshToken = UserSession->SessionData.RefreshToken;

    UGameplayStatics::SaveGameToSlot(Save, TEXT("NakamaSession"), 0);
    UE_LOG(LogTemp, Log, TEXT("[Nakama] Session saved."));
}

bool UNakamaBackendSubsystem::TryRestoreSession()
{
    if (!UGameplayStatics::DoesSaveGameExist(TEXT("NakamaSession"), 0))
        return false;

    auto* Save = Cast<UNakamaSessionSaveGame>(
        UGameplayStatics::LoadGameFromSlot(TEXT("NakamaSession"), 0));

    if (!Save) return false;

    UserSession = UNakamaSession::RestoreSession(Save->AuthToken, Save->RefreshToken);

    if (!UserSession || UserSession->IsExpired())
    {
        UserSession = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("[Nakama] Saved session expired — re-authenticating."));
        return false;
    }

    SetupRealtimeConnection(UserSession);
    FetchAccount();

    UE_LOG(LogTemp, Log, TEXT("[Nakama] Session restored successfully."));
    return true;
}

// ─── Internal Helpers ────────────────────────────────────────────────────────

void UNakamaBackendSubsystem::SetupRealtimeConnection(UNakamaSession* Session)
{
    if (!NakamaClient || !Session) return;

    if (RealtimeClient)
    {
        // Disconnect stale connection before making a new one (e.g. after re-auth)
        RealtimeClient->Disconnect();
        RealtimeClient = nullptr;
    }

    RealtimeClient = NakamaClient->SetupRealtimeClient();
    RealtimeClient->Connect(Session, /*bCreateStatus=*/true);

    UE_LOG(LogTemp, Log, TEXT("[Nakama] Realtime client connecting..."));
}

FString UNakamaBackendSubsystem::GetSteamSessionTicket() const
{
    // Gets a Steam auth session ticket via the Unreal Online Subsystem.
    // Nakama's AuthenticateSteam sends this to Steam's backend to verify identity.
    //
    // NOTE: GetAuthToken() returns the session ticket as a hex string.
    // This is NOT the same as the SteamID — it's a one-time cryptographic token.
    // Tickets expire after ~1 minute, which is fine since we use it immediately.

    IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
    if (!OSS)
    {
        UE_LOG(LogTemp, Error, TEXT("[Nakama] No Online Subsystem found."));
        return TEXT("");
    }

    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[Nakama] Online Identity interface invalid."));
        return TEXT("");
    }

    // GetAuthToken returns the Steam session ticket (hex-encoded)
    // This is what Nakama's AuthenticateSteam expects
    FString Ticket = Identity->GetAuthToken(0);
    if (Ticket.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Nakama] Auth token empty — Steam may not be initialized yet."));
    }

    return Ticket;
}

FString UNakamaBackendSubsystem::GetDeviceId() const
{
    // Stable device ID: load from save or generate once and persist.
    // Used for the Device auth provider path.
    static FString CachedDeviceId;

    if (!CachedDeviceId.IsEmpty())
        return CachedDeviceId;

    // Try loading persisted ID
    if (UGameplayStatics::DoesSaveGameExist(TEXT("NakamaDeviceId"), 0))
    {
        // For a real implementation you'd have a small USaveGame for this.
        // For now, fall through to generate.
    }

    CachedDeviceId = FGuid::NewGuid().ToString();
    UE_LOG(LogTemp, Warning, TEXT("[Nakama] Generated new device ID: %s"), *CachedDeviceId);
    return CachedDeviceId;
}