// Fill out your copyright notice in the Description page of Project Settings.


#include "ServerRowWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UServerRowWidget::SetJoinEnabled(bool bEnabled)
{
	if (RootButton)
	{
		RootButton->SetIsEnabled(bEnabled);
	}
}

void UServerRowWidget::SetJoiningState(bool bJoining)
{
	if (RootButton)
	{
		RootButton->SetIsEnabled(!bJoining);
	}

	if (JoinText)
	{
		JoinText->SetText(bJoining ? FText::FromString("Joining...") 
								   : FText::FromString("Join"));
	}
}

void UServerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootButton)
	{
		RootButton->OnClicked.RemoveDynamic(this, &UServerRowWidget::OnButtonClicked);
		RootButton->OnClicked.AddDynamic(this, &UServerRowWidget::OnButtonClicked);
	}
}

void UServerRowWidget::SetRowData(const FServerRowInfo& Info, int32 Index)
{
	CachedInfo = Info;
	CachedIndex = Index;
}

void UServerRowWidget::OnButtonClicked()
{
	// Prevent Spam by Disabling and show Joining Text
	SetJoiningState(true);  
	
	// Broadcast so listeners (e.g., Menu) can react and call JoinSession
	OnServerRowClicked.Broadcast(CachedInfo, CachedIndex);

	
}