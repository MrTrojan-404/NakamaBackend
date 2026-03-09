// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/LobbyWidget.h"

#include "MultiplayerSessionsSubsystem.h"
#include "Components/TextBlock.h"
#include "GameState/LobbyGameState.h"
#include "Subsystem/GameBridgeSubsystem.h"

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LobbySetup();
}

void ULobbyWidget::NativeDestruct()
{
	// Always unbind to avoid callbacks on a destroyed widget
	if (Bridge)
		Bridge->OnPrivateMatchReady.RemoveDynamic(
			this, &ULobbyWidget::OnMatchCodeReceived);

	Super::NativeDestruct();
}

void ULobbyWidget::LobbySetup()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	MSS    = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();
	Bridge = GI->GetSubsystem<UGameBridgeSubsystem>();

	const bool bPrivate = MSS && MSS->IsSessionPrivate();

	if (VisibilityText)
		VisibilityText->SetText(FText::FromString(bPrivate ? TEXT("Private") : TEXT("Public")));

	if (MatchCodeText)
		MatchCodeText->SetVisibility(ESlateVisibility::Collapsed);

	if (!Bridge) return;

	// Bind to Bridge — fires on host when Nakama write completes
	Bridge->OnPrivateMatchReady.AddDynamic(this, &ULobbyWidget::OnMatchCodeReceived);

	// Race condition fix — code may already be ready
	FString ExistingCode = Bridge->GetActiveMatchCode();
	if (!ExistingCode.IsEmpty())
		OnMatchCodeReceived(ExistingCode);
}

void ULobbyWidget::OnMatchCodeReceived(FString Code)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green,
		FString::Printf(TEXT("[Lobby] OnMatchCodeReceived: '%s'"), *Code));

	// Update GameState from here — we're guaranteed to be in the correct world
	if (ALobbyGameState* GS = GetWorld()->GetGameState<ALobbyGameState>())
	{
		GS->MatchCode = Code;
		// OnRep won't fire on host, so broadcast manually for any other local listeners
		GS->OnMatchCodeReady.Broadcast(Code);
	}

	if (!MatchCodeText) return;
	MatchCodeText->SetText(FText::FromString(FString::Printf(TEXT("Code: %s"), *Code)));
	MatchCodeText->SetVisibility(ESlateVisibility::Visible);
}

bool ULobbyWidget::IsHost() const
{
	UWorld* World = GetWorld();
	// NM_ListenServer = this machine is hosting
	return World && World->GetNetMode() == NM_ListenServer;
}