# UE5-Interaction-system-for-multiplayer-via-GAS
Interaction system for Unreal Engine 5, designed for multiplayer projects using Gameplay Ability System (GAS).

**[Showcase](https://youtu.be/zi3RGkh3Wkc)**

## Features:
*  Multiplayer-ready interaction logic
*  GAS-driven abilities and attributes
*  Extensible interaction component system
*  Support for both instant and hold interactions
*  Easy integration into existing UE5 projects

Perfect as a foundation for cooperative or competitive games requiring flexible interaction mechanics.

## How to setup
### 1 Сonnection of the interaction module
Add InteractionSystem module to /Game/Source/

Content files replace to your project.
	Note:    AGC_InteractionOutliling must be located in directory for GameplayCues

In [project].Target.cs, [project]Editor.Target.cs add this:
```c#
	ExtraModuleNames.Add("InteractionSystem");
```
In [project].Build.cs
```c#
	PublicDependencyModuleNames.AddRange(new string[] {... ,"InteractionSystem" });
	//or
	//PrivateDependencyModuleNames.AddRange(new string[] {... ,"InteractionSystem" });
```
### 2 Setup outlining and interaction bar
Enabling Custom Depth-Stencil Pass:
[Project Settings → Rendering → Postprocessing → Custom Depth-Stencil Pass]
Set "Enabled with Stencil"

Connecting GameplayCue to GameplayTag:
[Project Settings → GameplayTags → Manage Gameplay Tags...]
Add "GameplayCue.InteractableOutlining"

In DefaultGame.ini add this code
```c#
[/Script/InteractionSystem.InteractionLocalPlayerSubsystem]
InteractionWidgetClassPath=..._C
PostprocessOutliningMaterialPath=...
```
Note:
	InteractionWidgetClassPath is path to object widget with suffix _C
	PostprocessOutliningMaterialPath is path to material
	
### 3 Using the system
Add to character and interactable actors include:
```cpp
#include "InteractionSystem/Public/Interaction.h"
```	
Grant UGAInteraction

After AbilitySystemComponent initialization start GameplayCue locally for outlining and tooltip from AGC_InteractableOutlinig::StartGameplayCue(const UAbilitySystemComponent* ASC, const FGameplayCueParameters& parameters):
```cpp
	if (IsLocallyControlled() && AbilitySystemComponent)
{
	FGameplayCueParameters parameters;
	parameters.Instigator = this;
	AGC_InteractableOutlinig::StartGameplayCue(AbilitySystemComponent, parameters);
}
```
Note: FGameplayCueParameters& parameter must contain instigator which is player`s pawn.
	Add to player's character header:
```cpp
private:
	void PressAbilityIA_Interaction(const FInputActionInstance& Instance) 
	{
		AbilitySystemComponent->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::IA_Interaction));
	}
	void ReleaseAbilityIA_Interaction(const FInputActionInstance& Instance) 
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::IA_Interaction));
	}
```
Add to player's character cpp file SetupPlayerInputComponent():
```cpp
if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
{
	...
	EnhancedInputComponent->BindAction(IA_Interaction, ETriggerEvent::Started, this, &APlayer_Character_Base::PressAbilityIA_Interaction);
	EnhancedInputComponent->BindAction(IA_Interaction, ETriggerEvent::Completed, this, &APlayer_Character_Base::ReleaseAbilityIA_Interaction);
	...
}
```
Note:
	Change APlayer_Character_Base to your own name of character class
	Change EAbilityInputID::IA_Interaction to your own enumerator for AbilitiesInputID or set int value.
	IA_Interaction is InputAction with trigger seted to "Down"

Interactable objects must implement IInteractable interface.
Note: IInteractable::Interact(AActor* Instigator) executes only on server, so you must use RPC or replication

## Problems of this system:
*	Interaction and outliling of interactable objects are not linked. You can have an outline, but you cannot interact if ability isn`t granted.
*	Prediction is not supported.
