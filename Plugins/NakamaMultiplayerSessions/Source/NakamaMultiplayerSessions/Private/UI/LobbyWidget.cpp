// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/LobbyWidget.h"

#include "MultiplayerSessionsSubsystem.h"
#include "Components/TextBlock.h"
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
		VisibilityText->SetText(FText::FromString(
			bPrivate ? TEXT("Private") : TEXT("Public")));

	// Hide code text until the host receives it
	if (MatchCodeText)
		MatchCodeText->SetVisibility(ESlateVisibility::Collapsed);

	// Only host of a private match needs the code callback
	if (bPrivate && Bridge && IsHost())
	{
		Bridge->OnPrivateMatchReady.AddDynamic(
			this, &ULobbyWidget::OnMatchCodeReceived);
	}
}

void ULobbyWidget::OnMatchCodeReceived(FString Code)
{
	if (!MatchCodeText) return;

	MatchCodeText->SetText(FText::FromString(
		FString::Printf(TEXT("Code: %s"), *Code)));
	MatchCodeText->SetVisibility(ESlateVisibility::Visible);
}

bool ULobbyWidget::IsHost() const
{
	UWorld* World = GetWorld();
	// NM_ListenServer = this machine is hosting
	return World && World->GetNetMode() == NM_ListenServer;
}