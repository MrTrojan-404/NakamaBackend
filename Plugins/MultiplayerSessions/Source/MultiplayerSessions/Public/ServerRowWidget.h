// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "ServerRowWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnServerRowClicked, const FServerRowInfo&, ServerInfo, int32, RowIndex);


UCLASS()
class MULTIPLAYERSESSIONS_API UServerRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called from C++ to populate the row — implement in Blueprint to set text/images
	UFUNCTION(BlueprintImplementableEvent, Category = "ServerRow")
	void Setup(const FServerRowInfo& Info, int32 Index, UMultiplayerSessionsSubsystem* SessionSubsystem);

	// Optional: call from C++ to set values and retain them
	UFUNCTION(BlueprintCallable, Category = "ServerRow")
	void SetRowData(const FServerRowInfo& Info, int32 Index);

	// Delegate fired when UI button clicked
	UPROPERTY(BlueprintAssignable, Category = "ServerRow")
	FOnServerRowClicked OnServerRowClicked;

	void SetJoinEnabled(bool bEnabled);
	void SetJoiningState(bool bJoining);


protected:
	virtual void NativeConstruct() override;

	// Optional Button:Bind in Blueprint designer to call OnButtonClicked automatically.
	// If you added in the UMG widget and bind it to this property, NativeConstruct will hook it.
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RootButton;

	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* JoinText;
	
private:
	UFUNCTION()
	void OnButtonClicked();

	// Cached
	FServerRowInfo CachedInfo;
	int32 CachedIndex = -1;
	UPROPERTY()
	UMultiplayerSessionsSubsystem* CachedSubsystem = nullptr;
	
};
