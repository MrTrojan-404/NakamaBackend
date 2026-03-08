// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "OnlineSubsystemUtils.h"
#include "ServerRowWidget.h"

void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly inputModeUI;
			inputModeUI.SetWidgetToFocus(TakeWidget());
			inputModeUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(inputModeUI);
			PlayerController->SetShowMouseCursor(true);
		}
	}
	
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &UMenu::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UMenu::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &UMenu::OnStartSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionComplete.AddUObject(this, &UMenu::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &UMenu::OnJoinSession);

		// Listen for server list updates
		MultiplayerSessionsSubsystem->OnServerListUpdated.AddDynamic(
			this, &UMenu::OnServerListUpdatedHandler);
	}
	
}

void UMenu::SetServerListEnabled(bool bEnabled)
{
	if (!ServerListScrollBox) return;

	for (int32 i = 0; i < ServerListScrollBox->GetChildrenCount(); ++i)
	{
		if (UServerRowWidget* Row = Cast<UServerRowWidget>(ServerListScrollBox->GetChildAt(i)))
		{
			Row->SetJoinEnabled(bEnabled);
		}
	}
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::UMenu::HostButtonClicked);
	}
	if (FindSessionButton)
	{
		FindSessionButton->OnClicked.AddDynamic(this, &ThisClass::FindSessionButtonClicked);
	}

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTeardown();
	Super::NativeDestruct();
	
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Cyan,
		FString::Printf(TEXT("Menu->OnCreateSession Called")));
				
	}
	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Menu->OnCreateSession->ServerTravel")));
				
			}
			World->ServerTravel(PathToLobby);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Hosting with path: %s"), *PathToLobby));
				
			}
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Re-Enable Host Button")));
				
		}
		HostButton->SetIsEnabled(true);
	}
	
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionSearchResults, bool bWasSuccessful)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Cyan,
		FString::Printf(TEXT("Menu->OnFindSessions Called")));
				
	}
	if (MultiplayerSessionsSubsystem == nullptr) return;

	if (!bWasSuccessful || SessionSearchResults.Num() == 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 5.f, FColor::Yellow, TEXT("No sessions found"));
		}
		FindSessionButton->SetIsEnabled(true);
		
	}

	// Auto-Join Disabled for now - Server Browser Developement----------------------------------------

	/*for (auto Result : SessionSearchResults)
	{
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		FString ID = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		Result.Session.SessionSettings.bUsesPresence = true;
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("ID : %s, User : %s"), *ID, *User)
			);
		}

		if (SettingsValue == MatchType)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Menu->OnFindSessions-> MSS->JoinSession")));
				
			}
			MultiplayerSessionsSubsystem->JoinSession(Result);
			
			return;
		}
	}*/
		
}	


void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1, 15.f, FColor::Cyan,
		FString::Printf(TEXT("OnJoinSession called. Result = %d"), (int32)Result));
	}
	
	IOnlineSessionPtr SessionInterface = Online::GetSubsystem(GetWorld())->GetSessionInterface();

	if (!SessionInterface.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1, 15.f, FColor::Red, TEXT("OnJoinSession: SessionInterface INVALID"));
		}
		return;
	}
	
	if (SessionInterface.IsValid())
	{
		FString Address;
		bool bGotAddress = SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);
		APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1, 15.f,
			bGotAddress ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("GetResolvedConnectString: %s"),
				bGotAddress ? TEXT("TRUE") : TEXT("FALSE")));

			GEngine->AddOnScreenDebugMessage(
			-1, 15.f, FColor::Yellow,
			FString::Printf(TEXT("Resolved Address: %s"), *Address));
		}

		if (!bGotAddress || Address.IsEmpty())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1, 15.f, FColor::Red,
				TEXT("Join failed: No valid connect string"));
			}
			return;
		}

		if (!PlayerController)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1, 15.f, FColor::Red,
				TEXT("OnJoinSession: PlayerController is NULL"));
			}
			return;
		}
		if (PlayerController)
		{
			
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Menu->OnJoinSession->ClientTravel Address: %s"), *Address));
				
			}
			PlayerController->ClientTravel(Address, TRAVEL_Absolute);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1, 15.f, FColor::Yellow,
				FString::Printf(TEXT("After ClientTravel - NetDriver: %s"),
					(GetWorld() && GetWorld()->GetNetDriver()) ? TEXT("VALID") : TEXT("NULL")));
			}
			
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Menu->OnJoinSession->ClientTravel Called- Did we travel?: %s"), *Address));
				
			}
		}
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Join Failed, Re-Enable Join Button")));
				
		}
		// Unlock server list
		SetServerListEnabled(true);
		
		// Re-enable the row that was clicked
		if (PendingJoinRow)
		{
			PendingJoinRow->SetJoiningState(false);
			PendingJoinRow = nullptr;
		}
		FindSessionButton->SetIsEnabled(true);
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

void UMenu::HostButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Cyan,
		FString::Printf(TEXT("HostButton Clicked")));
				
	}
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Menu->MSS- CreateSession Called")));
				
		}
	}	
}

void UMenu::FindSessionButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		-1,
		15.f,
		FColor::Cyan,
		FString::Printf(TEXT("Join Button Clicked")));
				
	}
	FindSessionButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("Menu->MSS- FindSession Called")));
		}
		MultiplayerSessionsSubsystem->FindSession(10000);
	}
}

void UMenu::MenuTeardown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly inputModeGameOnly;
			PlayerController->SetInputMode(inputModeGameOnly);
			PlayerController->SetShowMouseCursor(false);
			
		}
	}
}

void UMenu::SpawnServerRows(UScrollBox* TargetScrollBox)
{
    if (!TargetScrollBox)
    {
    	if (GEngine)
    	{
    		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("SpawnServerRows: TargetScrollBox is null")));
    	}
        return;
    }

    // Clear any previous rows
    TargetScrollBox->ClearChildren();

    if (!MultiplayerSessionsSubsystem)
    {
    	if (GEngine)
    	{
    		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("SpawnServerRows: MultiplayerSessionsSubsystem is null")));
    	}
        return;
    }

    // Use the CachedServerRows we broadcasted earlier from the subsystem
    const TArray<FServerRowInfo>& Rows = MultiplayerSessionsSubsystem->GetCachedServerRows(); 


    if (!ServerRowWidgetClass)
    {
    	if (GEngine)
    	{
    		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("SpawnServerRows: ServerRowWidgetClass not set")));
    	}
        return;
    }

    int32 Index = 0;
    for (const FServerRowInfo& Info : Rows)
    {
        // Create widget
        UServerRowWidget* RowWidget = CreateWidget<UServerRowWidget>(GetWorld(), ServerRowWidgetClass);
        if (!RowWidget)
        {
        	if (GEngine)
        	{
        		GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("SpawnServerRows: Failed to create ServerRowWidget")));
        	}
            continue;
        }

        // Store the lightweight info on the widget and call Setup (which is BlueprintImplementableEvent)
        RowWidget->SetRowData(Info, Index);
        RowWidget->Setup(Info, Index, MultiplayerSessionsSubsystem);

        // Bind to the row's click delegate
        RowWidget->OnServerRowClicked.RemoveDynamic(this, &UMenu::HandleServerRowClicked);
        RowWidget->OnServerRowClicked.AddDynamic(this, &UMenu::HandleServerRowClicked);

        // Add to scrollbox
        TargetScrollBox->AddChild(RowWidget);

        ++Index;
    }
}

void UMenu::HandleServerRowClicked(const FServerRowInfo& Info, int32 RowIndex)
{
    if (!MultiplayerSessionsSubsystem)
    {
    	if (GEngine)
    	{
    		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Cyan,
			FString::Printf(TEXT("HandleServerRowClicked: no subsystem")));
    	}
        return;
    }

	// Remember which row is attempting to join
	PendingJoinRow = Cast<UServerRowWidget>(ServerListScrollBox->GetChildAt(RowIndex));

	// Lock all rows
	SetServerListEnabled(false);
	
    // Find the raw FOnlineSessionSearchResult using SessionId
    FOnlineSessionSearchResult FoundResult;
    if (MultiplayerSessionsSubsystem->GetSearchResultBySessionId(Info.SessionId, FoundResult))
    {
    	FoundResult.Session.SessionSettings.bUsesPresence = true;
    	FoundResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
        // call JoinSession on the subsystem with the full search result
        MultiplayerSessionsSubsystem->JoinSession(FoundResult);
    }
    else
    {
    	// Unlock immediately if the session is already gone
    	SetServerListEnabled(true);
    	
    	// If session is gone, re-enable immediately
    	if (PendingJoinRow)
    	{
            PendingJoinRow->SetJoinEnabled(true);
    		PendingJoinRow = nullptr;
    	}
    	// Optionally refresh the server list or notify the player
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Session no longer available."));
        }
    }
}

void UMenu::OnServerListUpdatedHandler(const TArray<FServerRowInfo>& ServerList, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 5.f, FColor::Red, TEXT("Server search failed."));
		}
		FindSessionButton->SetIsEnabled(true);
		return;
	}

	if (!ServerListScrollBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnServerListUpdatedHandler: ServerListScrollBox is null"));
		return;
	}

	// Build the browser UI from the cached rows
	SpawnServerRows(ServerListScrollBox);
}