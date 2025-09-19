#pragma once

#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "GameplayCueNotify_Actor.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/Interface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Interaction.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInteractionSystem, Warning, All)

UENUM(BlueprintType)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
enum class EInteractionType : uint8 {
	Press = 0 UMETA(DisplayName = "Press"),
	Hold = 1 UMETA(DisplayName = "Hold")
	//Add more types if need
	//Also add case implementation in UGAInteraction::ActivateAbility
};
//------------------------------------------------------------------------------------------------------------/IInteractable/------------------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	bool Interact(AActor* Instigator);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	EInteractionType GetInteractionType() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	float GetHoldDuration() const;  
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	TArray<UMeshComponent*> GetMeshesForOutlining() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	USceneComponent* GetTooltipPlace() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Interaction)
	FText GetTooltipText() const;
};
//------------------------------------------------------------------------------------------------------------/UGAInteraction/------------------------------------------------------------------------------------------------------------
UCLASS(Blueprintable)
class INTERACTIONSYSTEM_API UGAInteraction : public UGameplayAbility
{
	GENERATED_BODY()

	typedef UGameplayAbility Super;

public:
	UGAInteraction()
	{
		NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	}

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	/// <summary>
	/// Returns the actor that the camera is pointing at, which implements the IInteractable interface and is within reach.
	/// </summary>
	/// <param name="AvatarActor"></param>
	/// <returns></returns>
	static AActor* GetInteractionTarget(const AActor* AvatarActor);
protected:
	/// <summary>
	/// Called interaction only on server. You must create RPC or replicate variables in target to inform clients about the interaction result.
	/// </summary>
	/// <param name="ActivationInfo"></param>
	void ExecuteInteraction(const FGameplayAbilityActivationInfo* ActivationInfo);

private:
	AActor* TargetActor;
	AActor* AvatarActor;
	/// <summary>
	/// Valid only when locally controlled. Can be nullptr.
	/// </summary>
	UInteractionLocalPlayerSubsystem* InteractionSubsystem;

	//Uses in interaction implementation
	FTimerHandle HoldTimerHandle;
	float HoldDuration;
	float HoldingTime = 0.f;

	//-----Hold handlers------
	/// <summary>
	/// Callback of WaitInputReleaseTask
	/// </summary>
	/// <param name="TimeHeld"></param>
	UFUNCTION() void InterruptHolding(float TimeHeld);
	/// <summary>
	/// Handles the implementation of holding an ability. Called by ActivateAbility if the interaction type is Hold.
	/// </summary>
	/// <param name="Handle"></param>
	/// <param name="ActorInfo"></param>
	/// <param name="ActivationInfo"></param>
	/// <param name="TriggerEventData"></param>
	void HoldImplementanion(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData);
};
//------------------------------------------------------------------------------------------------------------/AGC_InteractableOutlinig/------------------------------------------------------------------------------------------------------------
//For corerect work player`s camera must have corresponding postprocess material. You can setup it in config file
//PostprocessOutliningMaterialPath=
UCLASS(Blueprintable, Category = "GameplayCueNotify")
class INTERACTIONSYSTEM_API AGC_InteractableOutlinig : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AGC_InteractableOutlinig()
	{
		PrimaryActorTick.bCanEverTick = true;
		PrimaryActorTick.bStartWithTickEnabled = true;
		PrimaryActorTick.TickInterval = 0.1f; // Adjust as needed for performance
		bAutoDestroyOnRemove = true;
	}
	/// <summary>
	/// Uses for starting the GameplayCue after ASC initiation.
	/// </summary>
	/// <param name="ASC"></param>
	/// <param name="parameters"></param>
	static void StartGameplayCue(const UAbilitySystemComponent* ASC, const FGameplayCueParameters& parameters);

protected:
	UPROPERTY(BlueprintReadOnly)
	UInteractionLocalPlayerSubsystem* InteractionSubsystem = nullptr;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
	virtual void OnTargetChanged_Implementation(AActor* OldTarget, AActor* NewTarget);

	UFUNCTION(BlueprintCallable)
	virtual void Tick(float DeltaTime) override;
	// / Called when the current interaction target changes
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnTargetChanged(AActor* OldTarget, AActor* NewTarget);

private:
	AActor* CurrentTarget = nullptr;

};
//------------------------------------------------------------------------------------------------------------/UInteractionWidget/------------------------------------------------------------------------------------------------------------
UCLASS(Blueprintable, Category = "Widget")
class INTERACTIONSYSTEM_API UInteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void TargetChanged(AActor* OldTarget, AActor* NewTarget);
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateProgressBar(float Percent);
};
//------------------------------------------------------------------------------------------------------------/UInteractionLocalPlayerSubsystem/------------------------------------------------------------------------------------------------------------
UCLASS(BlueprintType,config=Game)
class INTERACTIONSYSTEM_API UInteractionLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/// <summary>
	/// Returns the interaction widget instance. Creates it if it doesn't exist.
	/// </summary>
	/// <returns></returns>
	UFUNCTION(BlueprintCallable)
	virtual UInteractionWidget* GetWidget();

protected:
	UPROPERTY(Config)
	FSoftObjectPath InteractionWidgetClassPath;
	UPROPERTY(Config)
	FSoftObjectPath PostprocessOutliningMaterialPath;

	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	
	UFUNCTION()
	void OnPawnChanged_Callback(APawn* OldPawn, APawn* NewPawn);
private:
	UPROPERTY()
	UInteractionWidget* InteractionWidget;
};
