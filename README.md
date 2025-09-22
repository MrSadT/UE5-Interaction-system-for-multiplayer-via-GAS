# UE5-Interaction-system-for-multiplayer-via-GAS
Interaction system for Unreal Engine 5, designed for multiplayer projects using Gameplay Ability System (GAS).

**[Showcase](https://youtu.be/zi3RGkh3Wkc)**

## Features:
*  Multiplayer-ready interaction logic
*  GAS-driven abilities and cues
*  Extensible interaction modular system
*  Support for both instant and hold interactions
*  Easy integration into existing UE5 projects

## Problems of this system:
*	Interaction and outliling of interactable objects are not linked. You can have an outline, but you cannot interact if ability isn`t granted.
*	Prediction is not supported.

Perfect as a foundation for cooperative or competitive games requiring flexible interaction mechanics.

## 1 How to setup
### 1.1 Сonnection of the interaction module
Add InteractionSystem module to /Game/Source/

Content files replace to your project.

Note: `AGC_InteractionOutliling` must be located in directory for GameplayCues

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
### 1.2 Setup outlining and interaction bar
Enabling Custom Depth-Stencil Pass:
[Project Settings → Rendering → Postprocessing → Custom Depth-Stencil Pass]
Set "Enabled with Stencil"

Connecting GameplayCue to GameplayTag:
[Project Settings → GameplayTags → Manage Gameplay Tags...]
Add "GameplayCue.InteractableOutlining"

In `DefaultGame.ini` add this code
```c#
[/Script/InteractionSystem.InteractionLocalPlayerSubsystem]
InteractionWidgetClassPath=..._C
PostprocessOutliningMaterialPath=...
```
Note:
	`InteractionWidgetClassPath` is path to object widget with suffix _C
	`PostprocessOutliningMaterialPath` is path to material
	
## 2 Using the system
### 2.1 Add to character and interactable actors include:
```cpp
#include "InteractionSystem/Public/Interaction.h"
```	
### 2.2 Start GameplayCue for Outlining and interaction tooltip
After AbilitySystemComponent initialization start GameplayCue locally for outlining and tooltip from `AGC_InteractableOutlinig::StartGameplayCue(const UAbilitySystemComponent* ASC, const FGameplayCueParameters& parameters)`:
```cpp
	if (IsLocallyControlled() && AbilitySystemComponent)
{
	FGameplayCueParameters parameters;
	parameters.Instigator = this;
	AGC_InteractableOutlinig::StartGameplayCue(AbilitySystemComponent, parameters);
}
```
Note: `FGameplayCueParameters&` parameter must contain instigator which is player`s pawn.

### 2.3 Grant UGAInteraction
You can do it in BeginPlay or anywhere else.

Note: You must set the same input ID as when calling the ability [2.4]

### 2.4 Connecting ability to player input
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
Add to player's character cpp file `SetupPlayerInputComponent()`:
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
	Change `APlayer_Character_Base` to your own name of character class
	Change `EAbilityInputID::IA_Interaction` to your own enumerator for AbilitiesInputID or set int value. This value also must be set when ability granting in "Input ID".
	`IA_Interaction` is InputAction with trigger seted to "Down"
	
### 2.5 Creating interactable objects
Interactable objects must implement IInteractable interface.
Note: `IInteractable::Interact(AActor* Instigator)` executes only on server, so you must use RPC or replication

## 3 How to extend
### 3.1 Adding interaction types
Add your type to enumerator `EInteractionType` in `Interaction.h`

In `UGAInteraction::ActivateAbility()` add implementation to your case

### 3.2 Overriding of chosing target
By defoult choses target in front of the camera.

You may need blocking if something between the character and the target. 
You can make it using override
`AActor* UGAInteraction::GetInteractionTarget(const AActor* AvatarActor)`

### 3.3 Adding new functionality to all interactable objects
You may need to add methods to interface while creates new interaction types.
As exaple if you will add tapping you may need count of taps.
Add it in `IInteractable` in `Interaction.h`

### 3.4 Overriding interaction tooltip
Interaction tooltip must be inherited from `UInteractionWidget`
You can set your own widget in `DefaultGame.ini`:
```c#
[/Script/InteractionSystem.InteractionLocalPlayerSubsystem]
InteractionWidgetClassPath=[path_to_widget]_C
```
Note: suffix _C is necessary
### 3.5 Overriding outlining
For change outlining color you must edit postprocess material from Content `M_PostProcessOutlining`
GameplayCue for tooltip inherited from GameplayCue for outlining to decrese count of raycast checks. If you wanna disable outlining, you must rewrite tooltip or only disable material
