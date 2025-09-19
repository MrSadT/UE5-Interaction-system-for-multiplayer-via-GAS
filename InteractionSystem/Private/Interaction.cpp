// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction.h"
#include "Engine/OverlapResult.h"

#include "Camera/CameraComponent.h"

#include "AbilitySystemInterface.h"
#include "GameplayAbilities/Public/Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"

DEFINE_LOG_CATEGORY(LogInteractionSystem)
//------------------------------------------------------------------------------------------------------------/ActivateAbility/------------------------------------------------------------------------------------------------------------
void UGAInteraction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		AvatarActor = ActorInfo->AvatarActor.Get();
		TargetActor = GetInteractionTarget(AvatarActor);

		if (IsLocallyControlled())
			InteractionSubsystem = AvatarActor->GetInstigatorController<APlayerController>()->GetLocalPlayer()->GetSubsystem<UInteractionLocalPlayerSubsystem>();

		if (TargetActor)
		{
			//=====================================================================================================/Implementation of different interaction types/=====================================================================================================
			switch (IInteractable::Execute_GetInteractionType(TargetActor))
			{	//----------------------------------------------------------------------------------------------------------------------------------------------/ Press
			case EInteractionType::Press:
				// any interaction logic for other types can be added here
				ExecuteInteraction(&ActivationInfo);
				EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
				break;
				//----------------------------------------------------------------------------------------------------------------------------------------------/ Hold
			case EInteractionType::Hold:
				HoldImplementanion(Handle, ActorInfo, ActivationInfo, TriggerEventData);
				break;
				//----------------------------------------------------------------------------------------------------------------------------------------------/ Uknown
			default:
				UE_LOG(LogInteractionSystem, Warning, TEXT("GAInteraction: Uknown interaction type. Update interaction gameplay ability"));
				EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
				break;
			}//=========================================================================================================================================================================================================================================================
		}
		else EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
	else
	{
		UE_LOG(LogInteractionSystem, Warning, TEXT("GAInteraction: Invalid ActorInfo or AvatarActor"));
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
}
//------------------------------------------------------------------------------------------------------------/GetInteractionTarget/------------------------------------------------------------------------------------------------------------
AActor* UGAInteraction::GetInteractionTarget(const AActor* AvatarActor)
{
	const float InteractionRadius = 50.f; // Radius of the interaction sphere
	const float InteractionDistance = 500.f;// Distance from the camera to check for interactable objects

	if (AvatarActor)
		if (UCameraComponent* camera = AvatarActor->FindComponentByClass<UCameraComponent>())
		{
			// Trace settings
			FVector cameraLocation = camera->GetComponentLocation();
			FVector cameraForward = camera->GetForwardVector();
			FVector traceEnd = cameraLocation + (cameraForward * InteractionDistance);
			FCollisionQueryParams queryParams;
			queryParams.AddIgnoredActor(AvatarActor);
			FHitResult hitResult;

			if (AvatarActor->GetWorld()->LineTraceSingleByChannel(hitResult, cameraLocation, traceEnd, ECC_Visibility, queryParams))
			{
				TArray<FOverlapResult> overlaps;

				FVector sphereCenter = hitResult.bBlockingHit ? hitResult.ImpactPoint : traceEnd;
				FCollisionShape sphereShape = FCollisionShape::MakeSphere(InteractionRadius);
				if (AvatarActor->GetWorld()->OverlapMultiByChannel(overlaps, sphereCenter, FQuat::Identity, ECC_Visibility, sphereShape, queryParams))
				{
					TArray<AActor*> interactableActors;
					for (const FOverlapResult& overlap : overlaps)
					{
						if (overlap.GetActor() && overlap.GetActor()->Implements<UInteractable>())
						{
							interactableActors.Add(overlap.GetActor());
						}
					}
					if (interactableActors.Num() > 0)
					{
						// Sort interactable actors by distance to the camera
						interactableActors.Sort([sphereCenter](const AActor& A, const AActor& B)
							{
								return FVector::DistSquared(A.GetActorLocation(), sphereCenter) < FVector::DistSquared(B.GetActorLocation(), sphereCenter);
							});
						return interactableActors[0];
					}
				}
				else
					UE_LOG(LogInteractionSystem, Warning, TEXT("GAInteraction: OverlapMultiByChannel failed"));
			}
		}
		else
			UE_LOG(LogInteractionSystem, Warning, TEXT("GAInteraction: AvatarActor does not have a CameraComponent"));
	return nullptr;
}
void UGAInteraction::ExecuteInteraction(const FGameplayAbilityActivationInfo* ActivationInfo)
{
	if (HasAuthority(ActivationInfo))
	{
		IInteractable::Execute_Interact(TargetActor, AvatarActor);
	}
}
//------------------------------------------------------------------------------------------------------------/OnReleaseHold/------------------------------------------------------------------------------------------------------------
void UGAInteraction::InterruptHolding(float TimeHeld)
{
	AvatarActor->GetWorld()->GetTimerManager().ClearTimer(HoldTimerHandle);
	if (InteractionSubsystem)
		if (UInteractionWidget* widget = InteractionSubsystem->GetWidget())
			widget->UpdateProgressBar(0.f);
	HoldingTime = 0.f;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
//------------------------------------------------------------------------------------------------------------//------------------------------------------------------------------------------------------------------------
void UGAInteraction::HoldImplementanion(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const float tickRate = 0.02f; // Timer tick rate

	HoldDuration = IInteractable::Execute_GetHoldDuration(TargetActor);

	GetWorld()->GetTimerManager().SetTimer(HoldTimerHandle, [this, tickRate]()
		{
			if (HoldDuration <= HoldingTime && IsValid(TargetActor) && IsValid(AvatarActor))
			{
				//Holding sucessfully finished
				AvatarActor->GetWorld()->GetTimerManager().ClearTimer(HoldTimerHandle);
				FGameplayAbilityActivationInfo activationInfo = GetCurrentActivationInfo();
				ExecuteInteraction(&activationInfo);
				HoldingTime = 0.f;
				if(InteractionSubsystem)
					if (UInteractionWidget* widget = InteractionSubsystem->GetWidget())
						widget->UpdateProgressBar(0.f);
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
			else
			{   //Interrupt execution if the target has changed
				if (GetInteractionTarget(AvatarActor) != TargetActor)
				{
					AvatarActor->GetWorld()->GetTimerManager().ClearTimer(HoldTimerHandle);
					HoldingTime = 0.f;
					if (InteractionSubsystem)
						if (UInteractionWidget* widget = InteractionSubsystem->GetWidget())
							widget->UpdateProgressBar(0.f);
					EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
				}
				else
				{
					HoldingTime += tickRate;
					if (InteractionSubsystem)
						if (UInteractionWidget* widget = InteractionSubsystem->GetWidget())
							widget->UpdateProgressBar(HoldingTime / HoldDuration);
				}
			}

		}, tickRate, true);

	UAbilityTask_WaitInputRelease* WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitInputReleaseTask->OnRelease.AddDynamic(this, &UGAInteraction::InterruptHolding);
	WaitInputReleaseTask->ReadyForActivation();
}
//------------------------------------------------------------------------------------------------------------//------------------------------------------------------------------------------------------------------------
void AGC_InteractableOutlinig::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CueInstigator == nullptr) UE_LOG(LogTemp, Warning, TEXT("GC_InteractableOutlinig: CueInstigator is null. Check if the Gameplay Cue called with setting it"));
	//AActor* avatarActor = Cast<AMyPlayerState>(CueInstigator.Get())->GetAbilitySystemComponent()->GetAvatarActor();
	AActor* NewTarget = UGAInteraction::GetInteractionTarget(CueInstigator.Get());
	if (CurrentTarget != NewTarget)
	{
		// Remove highlight from the last target
		if (CurrentTarget != nullptr)
		{
			for (auto mesh : IInteractable::Execute_GetMeshesForOutlining(CurrentTarget))
			{
				mesh->SetRenderCustomDepth(false);
			}
		}
		// Add highlight to the current target
		if (NewTarget != nullptr)
		{
			for (auto mesh : IInteractable::Execute_GetMeshesForOutlining(NewTarget))
			{
				mesh->SetRenderCustomDepth(true);
				mesh->SetCustomDepthStencilValue(1);
			}
		}
		// Broadcast the target change event
		OnTargetChanged(CurrentTarget, NewTarget);

		CurrentTarget = NewTarget;
	}
}
void AGC_InteractableOutlinig::StartGameplayCue(const UAbilitySystemComponent* ASC, const FGameplayCueParameters& parameters)
{
	FGameplayTag gameplayCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.InteractableOutlining"));
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(ASC->GetOwner(), gameplayCueTag, EGameplayCueEvent::Type::OnActive, parameters);
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(ASC->GetOwner(), gameplayCueTag, EGameplayCueEvent::Type::WhileActive, parameters);
}

void AGC_InteractableOutlinig::HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	Super::HandleGameplayCue(MyTarget, EventType, Parameters);

	if (IAbilitySystemInterface* playerController = Cast<IAbilitySystemInterface>(Parameters.GetInstigator()))
		InteractionSubsystem = Cast<APlayerController>(playerController->GetAbilitySystemComponent()->GetOwnerActor()->GetOwner())->GetLocalPlayer()->GetSubsystem<UInteractionLocalPlayerSubsystem>();
	else UE_LOG(LogInteractionSystem, Warning, TEXT("AGC_InteractableOutlinig: Instigator don`t implement IAbilitySystemInterface"));
}

void AGC_InteractableOutlinig::OnTargetChanged_Implementation(AActor* OldTarget, AActor* NewTarget)
{
	if (InteractionSubsystem)
		InteractionSubsystem->GetWidget()->TargetChanged(CurrentTarget, NewTarget);
}
//------------------------------------------------------------------------------------------------------------//------------------------------------------------------------------------------------------------------------
UInteractionWidget* UInteractionLocalPlayerSubsystem::GetWidget()
{
	if (!InteractionWidget)
	{
		if (UClass* LoadedObject = Cast<UClass>(InteractionWidgetClassPath.TryLoad()))
			InteractionWidget = CreateWidget<UInteractionWidget>(GetWorld(), LoadedObject);
		else
		{
			UE_LOG(LogInteractionSystem, Error, TEXT("UInteractionLocalPlayerSubsystem: InteractionWidgetClass is not set. Please set it in the config DefaultGame.ini file."));
			return nullptr;
		}
	}
	return InteractionWidget;
}
//------------------------------------------------------------------------------------------------------------//------------------------------------------------------------------------------------------------------------
void UInteractionLocalPlayerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	if (NewPlayerController)
		NewPlayerController->OnPossessedPawnChanged.AddDynamic(this, &UInteractionLocalPlayerSubsystem::OnPawnChanged_Callback);
}

void UInteractionLocalPlayerSubsystem::OnPawnChanged_Callback(APawn* OldPawn, APawn* NewPawn)
{
	if (NewPawn)
	{
		if (UCameraComponent* CameraComponent = NewPawn->FindComponentByClass<UCameraComponent>())
		{
			if (UMaterialInterface* PPMaterial = Cast<UMaterialInterface>(PostprocessOutliningMaterialPath.TryLoad()))
			{
				CameraComponent->PostProcessSettings.AddBlendable(PPMaterial, 1.0f);
				CameraComponent->PostProcessBlendWeight = 1.0f;
			}
			else
			{
				UE_LOG(LogInteractionSystem, Error, TEXT("UInteractionLocalPlayerSubsystem: PostprocessOutliningMaterial is not set. Please set it in the config DefaultGame.ini file."));
			}
		}
		else
		{
			UE_LOG(LogInteractionSystem, Error, TEXT("UInteractionLocalPlayerSubsystem: Player's Pawn does not have a CameraComponent."));
		}
	}
}
