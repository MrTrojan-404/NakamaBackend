// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/GameBridgeSubsystem.h"

#include "MultiplayerSessionsSubsystem.h"
#include "NakamaClient.h"
#include "NakamaStorageObject.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystem/NakamaBackendSubsystem.h"

// ─── Init ─────────────────────────────────────────────────────────────────────

void UGameBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UMultiplayerSessionsSubsystem>();
    Collection.InitializeDependency<UNakamaBackendSubsystem>();

    Super::Initialize(Collection);

    // Register into MSS so Menu can call us without knowing we exist
    UMultiplayerSessionsSubsystem::RegisterPrivateMatchInterface(this);
    
    if (UNakamaBackendSubsystem* Nakama = GetNakama())
    {
        Nakama->OnAccountLoaded.AddDynamic(this, &UGameBridgeSubsystem::OnNakamaAccountLoaded);
    }
}

void UGameBridgeSubsystem::Deinitialize()
{
    UMultiplayerSessionsSubsystem::RegisterPrivateMatchInterface(nullptr);
    Super::Deinitialize();
}

// IPrivateMatchInterface — delegate to existing implementations
void UGameBridgeSubsystem::RequestJoinByCode(const FString& Code)
{
    JoinByCode(Code);
}
void UGameBridgeSubsystem::RequestRegisterMatch(const FString& Id)
{
    RegisterPrivateMatch(Id);
}
void UGameBridgeSubsystem::RequestUnregisterMatch()
{
    UnregisterPrivateMatch();
}
void UGameBridgeSubsystem::NotifyJoinByCodeFailed(const FString& Error)
{
    if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
    {
        Steam->NotifyJoinByCodeFailed(Error);
    }
}

// IPlayerBackendInterface
bool UGameBridgeSubsystem::IsBackendAuthenticated() const
{
    const UNakamaBackendSubsystem* Nakama = GetNakama();
    return Nakama && Nakama->IsLoggedIn();
}
FString UGameBridgeSubsystem::GetBackendDisplayName() const
{
    const UNakamaBackendSubsystem* Nakama = GetNakama();
    return Nakama ? Nakama->GetUsername() : FString();
}
void UGameBridgeSubsystem::RequestSetDisplayName(const FString& Name)
{
    SetDisplayName(Name);
}

// ILeaderboardInterface — In Developement, implement when ready
void UGameBridgeSubsystem::SubmitScore(const FString& LeaderboardId, int64 Score)
{
    UE_LOG(LogTemp, Warning, TEXT("[Bridge] SubmitScore not yet implemented"));
}
void UGameBridgeSubsystem::FetchTopScores(const FString& LeaderboardId, int32 Limit)
{
    UE_LOG(LogTemp, Warning, TEXT("[Bridge] FetchTopScores not yet implemented"));
}

// ─── Accessors ────────────────────────────────────────────────────────────────

UMultiplayerSessionsSubsystem* UGameBridgeSubsystem::GetSteam() const
{
    return GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>();
}

UNakamaBackendSubsystem* UGameBridgeSubsystem::GetNakama() const
{
    return GetGameInstance()->GetSubsystem<UNakamaBackendSubsystem>();
}

// ─── Feature: Private Matches ─────────────────────────────────────────────────

void UGameBridgeSubsystem::RegisterPrivateMatch(const FString& SteamSessionId)
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->IsLoggedIn()) return;

    ActiveMatchCode = GenerateCode();
    

    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("steam_session_id"), SteamSessionId);
    Json->SetStringField(TEXT("host_nakama_id"),   Nakama->GetNakamaUserId());
    Json->SetNumberField(TEXT("created_at"),       (double)FDateTime::UtcNow().ToUnixTimestamp());

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

    FNakamaStoreObjectWrite WriteObj;
    WriteObj.Collection      = TEXT("private_matches");
    WriteObj.Key             = ActiveMatchCode;
    WriteObj.Value           = Payload;
    WriteObj.PermissionRead  = ENakamaStoragePermissionRead::PUBLIC_READ;
    WriteObj.PermissionWrite = ENakamaStoragePermissionWrite::OWNER_WRITE;

    FOnStorageObjectAcks Success;
    Success.AddDynamic(this, &UGameBridgeSubsystem::HandlePrivateMatchWriteSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UGameBridgeSubsystem::HandleStorageError);

    Nakama->GetClient()->WriteStorageObjects(
        Nakama->GetSession(), { WriteObj }, Success, Error);
}

void UGameBridgeSubsystem::HandlePrivateMatchWriteSuccess(const FNakamaStoreObjectAcks& Acks)
{
    UE_LOG(LogTemp, Log, TEXT("[Bridge] Private match registered. Code: %s"), *ActiveMatchCode);
    OnPrivateMatchReady.Broadcast(ActiveMatchCode);
}

void UGameBridgeSubsystem::JoinByCode(const FString& Code)
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->IsLoggedIn())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Bridge] JoinByCode failed: backend not logged in"));

        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed(TEXT("Backend unavailable"));
        }

        return;
    }
    
    FNakamaReadStorageObjectId ReadObj;
    ReadObj.Collection = TEXT("private_matches");
    ReadObj.Key        = Code;
    ReadObj.UserId     = TEXT("");

    FOnStorageObjectsRead Success;
    Success.AddDynamic(this, &UGameBridgeSubsystem::HandlePrivateMatchReadSuccess);

    FOnError Error;
    Error.AddDynamic(this, &UGameBridgeSubsystem::HandleStorageError);

    Nakama->GetClient()->ReadStorageObjects(
        Nakama->GetSession(), { ReadObj }, Success, Error);
}

void UGameBridgeSubsystem::HandlePrivateMatchReadSuccess(const FNakamaStorageObjectList& Objects)
{
    if (Objects.Objects.Num() == 0)
    {
        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed("Invalid or expired match code.");
        }

        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Objects.Objects[0].Value);

    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed("Malformed match data.");
        }

        return;
    }

    FString SteamSessionId;
    Json->TryGetStringField(TEXT("steam_session_id"), SteamSessionId);

    if (SteamSessionId.IsEmpty())
    {
        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed("No session ID in match data.");
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Bridge] Code resolved → Steam session: %s"), *SteamSessionId);

    OnPrivateMatchJoinReady.Broadcast(SteamSessionId); // for any game-level listeners

    // Bridge calls MSS directly
    if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
    {
        Steam->FindSessionById(SteamSessionId);
        // MSS fires MultiplayerOnJoinSessionComplete → Menu.OnJoinSession → ClientTravel
        // The existing Menu flow handles the rest automatically
    }
}

void UGameBridgeSubsystem::UnregisterPrivateMatch()
{
    if (ActiveMatchCode.IsEmpty()) return;

    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->IsLoggedIn()) return;

    FNakamaDeleteStorageObjectId DeleteObj;
    DeleteObj.Collection = TEXT("private_matches");
    DeleteObj.Key        = ActiveMatchCode;

    // Use TFunction lambdas directly — no named delegate type needed
    TFunction<void()> SuccessCallback = [this]()
    {
        UE_LOG(LogTemp, Log, TEXT("[Bridge] Private match unregistered. Code: %s"), *ActiveMatchCode);
        ActiveMatchCode.Empty();
    };

    TFunction<void(const FNakamaError&)> ErrorCallback = [this](const FNakamaError& Err)
    {
        UE_LOG(LogTemp, Error, TEXT("[Bridge] Failed to unregister match: %s"), *Err.Message);
        //OnBridgeError.Broadcast(Err.Message);
        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed(Err.Message);
        }
    };

    Nakama->GetClient()->DeleteStorageObjects(
        Nakama->GetSession(), { DeleteObj }, SuccessCallback, ErrorCallback);
    
    // Clear locally immediately — cleanup is best-effort
    ActiveMatchCode.Empty();
}

// ─── Feature: Display Name Sync ───────────────────────────────────────────────

void UGameBridgeSubsystem::OnNakamaAccountLoaded()
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama) return;

    // Cache display name into MSS so CreateSession picks it up
    if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        Steam->SetHostDisplayName(Nakama->GetUsername());

    // Existing name sync logic
    if (!Nakama->HasPlayerName())
        SyncDisplayNameFromSteam();
}
void UGameBridgeSubsystem::SyncDisplayNameFromSteam()
{
    FString SteamName = GetSteamDisplayName();
    if (SteamName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Bridge] Steam display name unavailable."));
        return;
    }
    SetDisplayName(SteamName);
}

void UGameBridgeSubsystem::SetDisplayName(const FString& NewName)
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->IsLoggedIn()) return;

    PendingDisplayName = NewName;

    //Remove first — prevents stacking if called multiple times
    Nakama->OnUsernameUpdated.RemoveDynamic(
        this, &UGameBridgeSubsystem::HandleDisplayNameUpdated);
    Nakama->OnUsernameUpdated.AddDynamic(
        this, &UGameBridgeSubsystem::HandleDisplayNameUpdated);

    Nakama->SetPlayerName(NewName);
}

void UGameBridgeSubsystem::HandleDisplayNameUpdated(const FString& Name)
{
    GetNakama()->OnUsernameUpdated.RemoveDynamic(
        this, &UGameBridgeSubsystem::HandleDisplayNameUpdated);

    UE_LOG(LogTemp, Log, TEXT("[Bridge] Display name synced: %s"), *Name);
    OnDisplayNameSynced.Broadcast(Name);
}

// ─── Feature: Presence / Status ───────────────────────────────────────────────

void UGameBridgeSubsystem::UpdatePresence(const FString& StatusMessage)
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->GetRealtimeClient()) return;

    // Realtime client uses FNakamaRtError
    TFunction<void()> SuccessCallback = [StatusMessage]()
    {
        UE_LOG(LogTemp, Log, TEXT("[Bridge] Presence updated: %s"), *StatusMessage);
    };

    TFunction<void(const FNakamaRtError&)> ErrorCallback = [this](const FNakamaRtError& Err)
    {
        UE_LOG(LogTemp, Error, TEXT("[Bridge] Presence update failed: %s"), *Err.Message);

        if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
        {
            Steam->NotifyJoinByCodeFailed(Err.Message);
        }
    };

    Nakama->GetRealtimeClient()->UpdateStatus(StatusMessage, SuccessCallback, ErrorCallback);
}

// ─── Feature: Friends Sync ────────────────────────────────────────────────────

void UGameBridgeSubsystem::SyncSteamFriendsToNakama()
{
    TArray<FString> SteamIds = GetSteamFriendIds();
    if (SteamIds.Num() == 0)
    {
        OnFriendsSynced.Broadcast();
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[Bridge] Syncing %d Steam friends to Nakama"), SteamIds.Num());
    SubmitFriendBatch(SteamIds);
}

void UGameBridgeSubsystem::SubmitFriendBatch(const TArray<FString>& SteamIds)
{
    UNakamaBackendSubsystem* Nakama = GetNakama();
    if (!Nakama || !Nakama->IsLoggedIn()) return;

    FOnGetUsers Success;
    // Expand this when building friends UI: call AddFriends with the returned IDs
    FOnError Error;

    Nakama->GetClient()->GetUsers(
        Nakama->GetSession(),
        {},        // Nakama user IDs
        {},        // Usernames
        SteamIds,  // Steam IDs go in the FacebookIds slot for custom-auth users
        Success, Error
    );
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

FString UGameBridgeSubsystem::GenerateCode()
{
    // No ambiguous chars: no 0/O, 1/I/L
    static const FString Chars = TEXT("ABCDEFGHJKMNPQRSTUVWXYZ23456789");
    FString Code;
    for (int32 i = 0; i < 6; ++i)
        Code += Chars[FMath::RandRange(0, Chars.Len() - 1)];
    return Code;
}

FString UGameBridgeSubsystem::GetSteamDisplayName() const
{
    IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
    if (!OSS) return TEXT("");
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return TEXT("");
    return Identity->GetPlayerNickname(0);
}

TArray<FString> UGameBridgeSubsystem::GetSteamFriendIds() const
{
    TArray<FString> Result;
    IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
    if (!OSS) return Result;
    IOnlineFriendsPtr Friends = OSS->GetFriendsInterface();
    if (!Friends.IsValid()) return Result;

    TArray<TSharedRef<FOnlineFriend>> FriendList;
    Friends->GetFriendsList(0, TEXT(""), FriendList);
    for (const TSharedRef<FOnlineFriend>& F : FriendList)
        Result.Add(F->GetUserId()->ToString());
    return Result;
}

void UGameBridgeSubsystem::HandleStorageError(const FNakamaError& Error)
{
    UE_LOG(LogTemp, Error, TEXT("[Bridge] Error: %s"), *Error.Message);
   // OnBridgeError.Broadcast(Error.Message);
    if (UMultiplayerSessionsSubsystem* Steam = GetSteam())
    {
        Steam->NotifyJoinByCodeFailed(Error.Message);
    }
}