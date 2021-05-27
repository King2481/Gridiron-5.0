// Created by Bruce Crum


#include "Gridiron/Player/GridironPlayerController.h"
#include "Gridiron/Player/GridironPlayerState.h"
#include "Gridiron/UI/GridironHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Gridiron/GameModes/GridironGameModeBase.h"
#include "Gridiron/GameModes/GridironGameState.h"
#include "Gridiron/Characters/GridironCharacter.h"
#include "Blueprint/UserWidget.h"

AGridironPlayerController::AGridironPlayerController()
{
	bIsChatting = false;
	bIsInGameMenu = false;
	bIsLookingAtScoreboard = false;

	InGameMenuWidget = nullptr;
	ScoreboardWidget = nullptr;

#if UE_SERVER
	InGameMenuWidgetClass = nullptr;
	ScoreboardWidgetClass = nullptr;
#else
	// InGameMenu
	static ConstructorHelpers::FClassFinder<UUserWidget> InGameMenuWidgetFinder(TEXT("/Game/UI/Widgets/HUD/BP_InGameMenu"));
	InGameMenuWidgetClass = InGameMenuWidgetFinder.Succeeded() ? InGameMenuWidgetFinder.Class : nullptr;

	// Scoreboard
	static ConstructorHelpers::FClassFinder<UUserWidget> ScoreboardWidgetFinder(TEXT("/Game/UI/Widgets/HUD/Scoreboard/BP_Scoreboard"));
	ScoreboardWidgetClass = ScoreboardWidgetFinder.Class;
#endif
}

void AGridironPlayerController::ConstructWidgets()
{
	if (InGameMenuWidgetClass)
	{
		InGameMenuWidget = CreateWidget<UUserWidget>(this, InGameMenuWidgetClass);
		if (InGameMenuWidget)
		{
			// We add to viewport then hide.
			InGameMenuWidget->AddToViewport(1);
			InGameMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Error, attempted to create InGameMenu widget, but InGameMenu class is null"));
	}

	if (ScoreboardWidgetClass)
	{
		ScoreboardWidget = CreateWidget<UUserWidget>(this, ScoreboardWidgetClass);
		if (ScoreboardWidget)
		{
			// We add to viewport then hide.
			ScoreboardWidget->AddToViewport();
			ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Error, attempted to create Scoreboard widget, but Scoreboard class is null"));
	}
}

void AGridironPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("StartChat", IE_Pressed, this, &AGridironPlayerController::StartChat);
	InputComponent->BindAction("StartTeamChat", IE_Pressed, this, &AGridironPlayerController::StartTeamChat);
	InputComponent->BindAction("InGameMenu", IE_Pressed, this, &AGridironPlayerController::ToggleInGameMenu);

	InputComponent->BindAction("Scoreboard", IE_Pressed, this, &AGridironPlayerController::ShowScoreboard);
	InputComponent->BindAction("Scoreboard", IE_Released, this, &AGridironPlayerController::HideScoreboard);
}

void AGridironPlayerController::StartChat()
{
	const auto GridironHUD = Cast<AGridironHUD>(GetHUD());
	if (GridironHUD)
	{
		GridironHUD->StartChatInput();
	}
}

void AGridironPlayerController::StartTeamChat()
{
	const auto GridironHUD = Cast<AGridironHUD>(GetHUD());
	if (GridironHUD)
	{
		GridironHUD->StartChatInput(true);
	}
}

void AGridironPlayerController::ServerSendChatMessage_Implementation(const FText& Message, const bool bTeamMessage)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const auto PC = Cast<AGridironPlayerController>(*Iterator);
		if (!PC)
		{
			continue;
		}

		if (bTeamMessage && PC->GetTeamId() != GetTeamId())
		{
			continue;
		}

		PC->ClientTeamMessage(PlayerState, Message.ToString(), bTeamMessage ? TEXT("Team") : TEXT(""));
	}
}

bool AGridironPlayerController::ServerSendChatMessage_Validate(const FText& Message, const bool bTeamMessage)
{
	return true;
}

void AGridironPlayerController::ClientTeamMessage_Implementation(APlayerState* SenderPlayerStateBase, const FString& S, FName Type, float MsgLifeTime)
{
	Super::ClientTeamMessage_Implementation(SenderPlayerStateBase, S, Type, MsgLifeTime);

	const auto SenderPlayerState = Cast<AGridironPlayerState>(SenderPlayerStateBase);;

	const bool bGamemodeSay = Type == FName(TEXT("Gamemode"));
	const bool bHostSay = Type == FName(TEXT("Host"));
	const bool bTeamSay = Type == FName(TEXT("Team"));

	static FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), FText::FromString(SenderPlayerState ? SenderPlayerState->GetPlayerName() : TEXT("")));

	if (bGamemodeSay)
	{
		Arguments.Add(TEXT("Title"), FText::FromString(TEXT("")));
	}
	else
	{
		Arguments.Add(TEXT("Title"), FText::FromString(bTeamSay ? TEXT("(Team):") : TEXT("(All):")));
	}
	
	Arguments.Add(TEXT("Message"), FText::FromString(S));

	OnChatMessageReceived(FText::Format(NSLOCTEXT("HUD", "ChatMessageFormat", "{Name} {Title} {Message}"), Arguments), SenderPlayerState);
}

void AGridironPlayerController::OnChatMessageReceived(const FText& Message, AGridironPlayerState* SenderPlayerState /*= nullptr*/)
{
	const auto GridironHUD = Cast<AGridironHUD>(GetHUD());
	if (GridironHUD)
	{
		GridironHUD->OnChatMessageReceived(Message, SenderPlayerState);
	}
}

void AGridironPlayerController::OnChatInputStarted()
{
	bIsChatting = true;
	UpdateInputMode();
}

void AGridironPlayerController::OnChatInputEnded()
{
	bIsChatting = false;
	UpdateInputMode();
}

void AGridironPlayerController::ToggleInGameMenu()
{
	SetShowInGameMenu(!bIsInGameMenu);
}

void AGridironPlayerController::SetShowInGameMenu(const bool NewIsInGameMenu)
{
	if (InGameMenuWidget)
	{
		bIsInGameMenu = NewIsInGameMenu;

		const ESlateVisibility Visibility = bIsInGameMenu ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		InGameMenuWidget->SetVisibility(Visibility);

		UpdateInputMode();
	}
}

void AGridironPlayerController::ShowScoreboard()
{
	if (ScoreboardWidget)
	{
		bIsLookingAtScoreboard = true;

		ScoreboardWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UpdateInputMode();
	}
}

void AGridironPlayerController::HideScoreboard()
{
	if (ScoreboardWidget)
	{
		bIsLookingAtScoreboard = false;

		ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);

		UpdateInputMode();
	}
}

void AGridironPlayerController::UpdateInputMode()
{
	if (bIsChatting || bIsInGameMenu)
	{
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		return;
	}

	if (bIsLookingAtScoreboard)
	{
		SetInputMode(FInputModeGameAndUI());
		bShowMouseCursor = true;
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AGridironPlayerController::OnRoundWon(AGridironPlayerState* WinningPlayerState, uint8 WinningTeam)
{
	ClientOnRoundWon(WinningPlayerState, WinningTeam);
}

void AGridironPlayerController::ClientOnRoundWon_Implementation(AGridironPlayerState* WinningPlayerState, uint8 WinningTeam)
{
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	OnRoundWonDelegate.Broadcast(WinningPlayerState, WinningTeam);
}

void AGridironPlayerController::ClientRecieveKillFeedNotice_Implementation(const FKillfeedNotice& Notice)
{
	KillFeedNoticeDelegate.Broadcast(Notice);
}

void AGridironPlayerController::ClientPlaySound2D_Implementation(USoundBase* SoundToPlay)
{
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySound2D(this, SoundToPlay);
	}
}

void AGridironPlayerController::RespawnPlayer()
{
	if (GetPawn())
	{
		return;
	}

	auto GM = GetWorld()->GetAuthGameMode<AGridironGameModeBase>();
	if (GM)
	{
		AActor* PlayerStart = GM->ChoosePlayerStart(this);
		if (PlayerStart)
		{
			GM->RestartPlayerAtPlayerStart(this, PlayerStart);
		}
	}
}

void AGridironPlayerController::QueueRespawnDelay(float Delay)
{
	GetWorldTimerManager().SetTimer(DelayRespawn_TimerHandle, this, &AGridironPlayerController::OnQueueRespawnDelayFinished, Delay);
}

void AGridironPlayerController::OnQueueRespawnDelayFinished()
{
	GetWorldTimerManager().ClearTimer(DelayRespawn_TimerHandle);
	RespawnPlayer();
}

uint8 AGridironPlayerController::GetTeamId() const
{
	const auto PS = Cast<AGridironPlayerState>(PlayerState);
	return PS ? PS->GetTeamId() : ITeamInterface::InvalidId;
}

void AGridironPlayerController::JoinTeam(uint8 NewTeam)
{
	const auto World = GetWorld();
	if (!World)
	{
		return;
	}

	const auto GS = Cast<AGridironGameState>(World->GetGameState());
	const auto PS = Cast<AGridironPlayerState>(PlayerState);

	if (NewTeam != ITeamInterface::InvalidId)
	{
		if (GS && PS)
		{
			GS->AddPlayerForTeam(PS, NewTeam);
		}
	}
}

void AGridironPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	const auto NewCharacter = Cast<AGridironCharacter>(InPawn);
	if (NewCharacter)
	{
		NewCharacter->SetTeamId(GetTeamId());
	}
}

void AGridironPlayerController::ClientStartCameraFade_Implementation(float FromAlpha, float ToAlpha, float Duration, const FLinearColor& Color, bool bShouldFadeAudio, bool bHoldWhenFinished)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, Duration, Color, bShouldFadeAudio, bHoldWhenFinished);
	}
}