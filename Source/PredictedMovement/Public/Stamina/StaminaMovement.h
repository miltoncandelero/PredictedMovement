// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StaminaMovement.generated.h"

struct FStaminaMoveResponseDataContainer final : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;
	
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

	float Stamina;
	bool bStaminaDrained;
};

/**
 * Add a void CalcStamina(float DeltaTime) function to your UCharacterMovementComponent and call it before Super after
 * overriding CalcVelocity.
 * 
 * You will want to implement what happens based on the stamina yourself, eg. override GetMaxSpeed to move slowly
 * when bStaminaDrained.
 *
 * Override OnStaminaChanged to call (or not) SetStaminaDrained based on the needs of your project. Most games will
 * want to make use of the drain state to prevent rapid sprint re-entry on tiny amounts of regenerated stamina. However,
 * unless you require the stamina to completely refill before sprinting again, then you'll want to override
 * OnStaminaChanged and change FMath::IsNearlyEqual(Stamina, MaxStamina) to multiply MaxStamina by the percentage
 * that must be regained before re-entry (eg. MaxStamina * 0.1f is 10%).
 *
 * Nothing is presumed about regenerating or draining stamina, if you want to implement those, do it in CalcVelocity or
 * at least PerformMovement - CalcVelocity stems from PerformMovement but exists within the physics subticks for greater
 * accuracy.
 *
 * If used with sprinting, OnStaminaDrained() should be overridden to call USprintMovement::UnSprint(). If you don't
 * do this, the greater accuracy of CalcVelocity is lost because it cannot stop sprinting between frames.
 *
 * You will need to handle any changes to MaxStamina, it is not predicted here.
 *
 * GAS can modify the Stamina (by calling SetStamina(), nothing special required) and it shouldn't desync, however
 * if you have any delay between the ability activating and the stamina being consumed it will likely desync; the
 * solution is to call GetCharacterMovement()->FlushServerMoves() from the Character.
 * It is worthwhile to expose this to blueprint.
 *
 * This is not designed to work with blueprint, at all, anything you want exposed to blueprint you will need to do it
 * Better yet, add accessors from your Character and perhaps a broadcast event for UI to use.
 *
 * This solution is provided by Cedric 'eXi' Neukirchen and has been repurposed for net predicted Stamina.
 */
UCLASS()
class PREDICTEDMOVEMENT_API UStaminaMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UStaminaMovement(const FObjectInitializer& ObjectInitializer);
	
private:
	UPROPERTY()
	float Stamina;

	UPROPERTY()
	float MaxStamina;

	UPROPERTY()
	bool bStaminaDrained;

public:
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	bool IsStaminaDrained() const { return bStaminaDrained; }

	void SetStamina(float NewStamina);

	void SetMaxStamina(float NewMaxStamina);

	void SetStaminaDrained(bool bNewValue);

protected:
	/*
	 * Drain state entry and exit is handled here. Drain state is used to prevent rapid re-entry of sprinting or other
	 * such abilities before sufficient stamina has regenerated. However, in the default implementation, 100%
	 * stamina must be regenerated. Consider overriding this and changing FMath::IsNearlyEqual(Stamina, MaxStamina)
	 * to FMath::IsNearlyEqual(Stamina, MaxStamina * 0.1f) to require 10% regeneration (or change the 0.1f to your
	 * desired value) in the else-if scope in the function body.
	 */
	virtual void OnStaminaChanged(float PrevValue, float NewValue);

	virtual void OnMaxStaminaChanged(float PrevValue, float NewValue) {}
	virtual void OnStaminaDrained() {}
	virtual void OnStaminaDrainRecovered() {}

private:
	FStaminaMoveResponseDataContainer StaminaMoveResponseDataContainer;
	
public:
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;

	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class PREDICTEDMOVEMENT_API FSavedMove_Character_Stamina : public FSavedMove_Character
{
	using Super = FSavedMove_Character;
	
public:
	FSavedMove_Character_Stamina()
		: bStaminaDrained(0)
		, Stamina(0)
	{
	}

	virtual ~FSavedMove_Character_Stamina() override
	{}

	uint32 bStaminaDrained : 1;
	float Stamina;

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void Clear() override;
	virtual void SetInitialPosition(ACharacter* C) override;
};

class PREDICTEDMOVEMENT_API FNetworkPredictionData_Client_Character_Stamina : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FNetworkPredictionData_Client_Character_Stamina(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override;
};
