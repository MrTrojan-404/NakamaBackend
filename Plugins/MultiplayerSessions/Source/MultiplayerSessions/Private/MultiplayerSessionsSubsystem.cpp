// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"

IPrivateMatchInterface* UMultiplayerSessionsSubsystem::PrivateMatchHandler = nullptr;


UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
{
	
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, bool bPrivate)
{
	
	DesiredNumPublicConnections = NumPublicConnections;
	DesiredMatchType = MatchType;
	bLastSessionWasPrivate = bPrivate;

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("CreateSession: Online subsystem not available")));
		}
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Yellow,
		FString::Printf(TEXT("CreateSession: OSS Name = %s"),
			OSS ? *OSS->GetSubsystemName().ToString() : TEXT("NULL")));
	}
	
	SessionInterface = OSS->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("CreateSession: SessionInterface invalid")));
		}
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}	

	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Session already exists- Destroying")));
		}
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		
		DestroySession();
		// **Important**: return here so we don't try to create while destroy is in-progress
		return;

	}

	

	//Store the Delegate in a FDelegateHandle so we can later remove it from the delegate list
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Green,
		TEXT("CreateSession: Binding CreateSessionComplete delegate"));
	}
	
	
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch             = (OSS->GetSubsystemName() == "NULL");
	LastSessionSettings->NumPublicConnections     = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress     = true;
	LastSessionSettings->bAllowJoinViaPresence    = !bPrivate;  // ← 
	LastSessionSettings->bShouldAdvertise         = !bPrivate;  // ← private = not advertised
	LastSessionSettings->bUsesPresence            = true;
	LastSessionSettings->bUseLobbiesIfAvailable   = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId            = 1;
	LastSessionSettings->Set(FName("MatchType"), MatchType,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	//store host's display name so server browser can show it
	if (!PendingHostDisplayName.IsEmpty())
	{
		LastSessionSettings->Set(FName("HostDisplayName"), PendingHostDisplayName,
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("CreateSession: No LocalPlayer available")));
		}
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}
	const FUniqueNetIdRepl UniqueId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UniqueId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("CreateSession: Invalid UniqueNetId")));
		}
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}
	
	bool bCreateCalled = SessionInterface->CreateSession(*UniqueId.GetUniqueNetId(), NAME_GameSession, *LastSessionSettings);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		bCreateCalled ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("CreateSession: CreateSession() returned %s"),
			bCreateCalled ? TEXT("TRUE") : TEXT("FALSE")));
	}

	if (!bCreateCalled)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
	
}

void UMultiplayerSessionsSubsystem::NotifyJoinByCodeFailed(const FString& Error)
{
	MultiplayerOnJoinSessionComplete.Broadcast(
	EOnJoinSessionCompleteResult::SessionDoesNotExist);
}

void UMultiplayerSessionsSubsystem::FindSession(int32 MaxSearchResults)
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("FindSession: Online subsystem not available")));
		}
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
    SessionInterface = OSS->GetSessionInterface();
	
	if (!SessionInterface.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("FindSession: SessionInterface invalid")));
		}
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = FMath::Clamp(MaxSearchResults, 1, 200); // limit to something reasonable
    LastSessionSearch->bIsLanQuery = (OSS->GetSubsystemName() == "NULL");
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("FindSession: No LocalPlayer available")));
		}
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	const FUniqueNetIdRepl UniqueId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UniqueId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("FindSession: Invalid UniqueNetId")));
		}
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	if (!SessionInterface->FindSessions(*UniqueId.GetUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
	
}

void UMultiplayerSessionsSubsystem::FindSessionById(const FString& SessionId)
{
	PendingJoinSessionId = SessionId;
	bFindingBySessionId = true;
	
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		MultiplayerOnFindSessionComplete.Broadcast({}, false);
		return;
	}

	SessionInterface = OSS->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnFindSessionComplete.Broadcast({}, false);
		return;
	}

	// Clear old results immediately
	CachedServerRows.Reset();
	OnServerListUpdated.Broadcast(CachedServerRows, true);

	FindSessionsCompleteDelegateHandle =
		SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = 200;
	LastSessionSearch->bIsLanQuery = (OSS->GetSubsystemName() == "NULL");

	LastSessionSearch->QuerySettings.Set(
		SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast({}, false);
		return;
	}

	const FUniqueNetIdRepl UniqueId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UniqueId.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast({}, false);
		return;
	}

	if (!SessionInterface->FindSessions(
		*UniqueId.GetUniqueNetId(),
		LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast({}, false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	SessionInterface = OSS ? OSS->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("JoinSession: No LocalPlayer")));
		}
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	const FUniqueNetIdRepl UniqueId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UniqueId.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("JoinSession: Invalid UniqueNetId")));
		}
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	if (!SessionInterface->JoinSession(*UniqueId.GetUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
	SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnStartSessionComplete.Broadcast(false);
		return;
	}
	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerOnStartSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Yellow,
		TEXT("DestroySession: Called"));
	}
	
	SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
	
}

// ----------------------------- Callbacks -----------------------------

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();
	
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	if (bWasSuccessful)
	{
		StartSession();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("MSS_OnCreateSessionComplete : StartSession Called")));
		}
	}
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionComplete(bool bWasSuccessful)
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	SessionInterface = OSS ? OSS->GetSessionInterface() : nullptr;
	
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
	}
	// Defensive: ensure search pointer is valid
	if (!LastSessionSearch.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("OnFindSessionComplete: LastSessionSearch is invalid")));
		}
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	if (bFindingBySessionId)
	{
		bFindingBySessionId = false;

		if (bWasSuccessful && LastSessionSearch->SearchResults.Num() > 0)
		{
			// Find the specific session matching our target ID
			for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
			{
				if (Result.GetSessionIdStr() == PendingJoinSessionId)
				{
					// Found it — hand off to normal join flow
					JoinSession(Result);
					PendingJoinSessionId.Empty();
					return;
				}
			}
		}

		// Session not found or already gone
		UE_LOG(LogTemp, Warning, TEXT("[MSS] FindSessionById: session %s not found"),
			*PendingJoinSessionId);
		PendingJoinSessionId.Empty();

		// Reuse the join complete delegate to signal failure back to Menu
		MultiplayerOnJoinSessionComplete.Broadcast(
			EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return;
	}

	// ── Broad-search path (server browser) ──────────────────────────

	if (LastSessionSearch->SearchResults.Num() <= 0)	//BroadCast false if result is Empty
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("0 Sessions Found")));
		}
		
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>() ,false);
		return;
	}
	
	// Cache and convert results to UI-friendly struct
	CachedServerRows.Reset();
	for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
	{
		if (!Result.IsValid()) continue;

		if (Result.Session.NumOpenPublicConnections <= 0)
			continue;

		CachedServerRows.Add(MakeServerRowInfoFromSearchResult(Result));
	}

	
	// Broadcast both raw and converted results for backwards compat
	MultiplayerOnFindSessionComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	OnServerListUpdated.Broadcast(CachedServerRows, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();
	if (SessionInterface)
	{
		  SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Yellow,
		FString::Printf(TEXT("OnDestroySessionComplete: %s"),
			bWasSuccessful ? TEXT("SUCCESS") : TEXT("FAILED")));
	}
	SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	SessionInterface = OSS ? OSS->GetSessionInterface() : nullptr;

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	MultiplayerOnStartSessionComplete.Broadcast(bWasSuccessful);
}

FServerRowInfo UMultiplayerSessionsSubsystem::MakeServerRowInfoFromSearchResult(const FOnlineSessionSearchResult& Result)
{
	FServerRowInfo Row;
	// SessionId
	Row.SessionId = Result.GetSessionIdStr();

	// Server name: try "Session.SessionSettings" first if you stored a name, otherwise OwningUserName
	FString FoundName;
	if (Result.Session.SessionSettings.Get(FName("ServerName"), FoundName))
	{
		Row.ServerName = FoundName;
	}
	else if (!Result.Session.OwningUserName.IsEmpty())
	{
		Row.ServerName = Result.Session.OwningUserName;
	}
	else
	{
		Row.ServerName = TEXT("Unknown");
	}

	// Map name (if you stored it in session settings)
	FString MapName;
	if (Result.Session.SessionSettings.Get(FName("MAPNAME"), MapName))
	{
		Row.MapName = MapName;
	}
	else
	{
		Row.MapName = TEXT("Unknown");
	}

	// Players
	int32 NumOpenPublic = Result.Session.NumOpenPublicConnections;
	int32 MaxPublic = Result.Session.SessionSettings.NumPublicConnections;
	Row.MaxPlayers = MaxPublic;
	Row.CurrentPlayers = FMath::Clamp(MaxPublic - NumOpenPublic, 0, MaxPublic);

	// Ping
	Row.PingMs = Result.PingInMs;

	// Host user
	// Host user
	Row.HostUserName = Result.Session.OwningUserName;

	FString DisplayName;
	if (Result.Session.SessionSettings.Get(FName("HostDisplayName"), DisplayName))
	{
		Row.NakamaDisplayName = DisplayName;
	}
	else
	{
		Row.NakamaDisplayName = Row.HostUserName;
	}
	
	return Row;
}

FString UMultiplayerSessionsSubsystem::GetCurrentSessionId() const
{
	if (!SessionInterface.IsValid()) return TEXT("");
	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session || !Session->SessionInfo.IsValid()) return TEXT("");
	return Session->GetSessionIdStr();
}

// Read current settings (for LobbyWidget to check initial visibility)
FOnlineSessionSettings* UMultiplayerSessionsSubsystem::GetCurrentSessionSettings() const
{
	return LastSessionSettings.Get();
}

bool UMultiplayerSessionsSubsystem::GetSearchResultBySessionId(const FString& SessionId, FOnlineSessionSearchResult& OutResult) const
{
	if (!LastSessionSearch.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("GetSearchResultBySessionId: LastSessionSearch invalid")));
		}
		return false;
	}

	for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
	{
		if (Result.GetSessionIdStr() == SessionId)
		{
			OutResult = Result;
			return true;
		}
	}

	// Not found
	return false;
}
