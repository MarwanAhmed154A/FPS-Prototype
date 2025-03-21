// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "my_bossCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UAnimMontage;
class USoundBase;
class AInteractableActor;
class AWeaponActor;
class APostProcessVolume;

UCLASS(config=Game)
class Amy_bossCharacter : public ACharacter
{
	GENERATED_BODY()

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Motion controller (right hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* R_MotionController;

	/** Motion controller (left hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* L_MotionController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* GunLoc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	UMaterialInterface* PostMat;

	UMaterialInstanceDynamic* PostProcessMat;
	APostProcessVolume* PostProcessActor;

	AWeaponActor* Weapon;

	UStaticMeshComponent* WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	float DefShaderTransitionTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	float CurrentShaderChangePercent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	int ShaderChangeState;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	float CurShaderTransitionTime;
public:
	Amy_bossCharacter();
	void TakeDamage(int Damage) { CurHealth -= Damage; if (CurHealth <= 0) Destroy(); }
protected:
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime) override;
public:
	UPROPERTY(Category = Shooting, EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AWeaponActor> WeaponClass;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Whether to use motion controller location for aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	uint8 bUsingMotionControllers : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Interaction)
	float InteractionRange = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Interaction)
	USceneComponent* HeldObjPos;

	UPROPERTY(EditAnywhere, Category = Shooting)
	TSubclassOf<AActor> MuzzleFlashClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shooting)
	float MuzzleFlashLifeTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Health)
	int DefHealth = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float DashForce = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float DashCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float DashTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float DefManaTime = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category = Health)
	float CurManaTime;

	UPROPERTY(BlueprintReadOnly, Category = Health)
	int CurHealth;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Movement)
	int DefJumpsAmount = 2;

	UPROPERTY(BlueprintReadWrite, Category = Movement)
	int CurJumpsAmount = 2;

	FCollisionQueryParams InteractionIgnore;
	AInteractableActor* HeldActor;

	AActor* MuzzleFlashActor;
	FTimerHandle DestroyMuzzleFlashHandle;

	FTimerHandle EndDashHandle;
	FTimerHandle ResetDashHandle;
	FVector VelocityBeforeDash;

	bool bHoldingActor = false;
	bool bCanDash = true;
protected:
	
	/** Fires a projectile. */
	void OnFire();
	void DestroyMuzzleFlash();
	/** Resets HMD orientation and position in VR. */
	void OnResetVR();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual bool CanJumpInternal_Implementation() const override;

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);
	void OnSlowTime();
	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	void Dash();
	void EndDash();

	void ResetDash();
	struct TouchData
	{
		TouchData() { bIsPressed = false;Location=FVector::ZeroVector;}
		bool bIsPressed;
		ETouchIndex::Type FingerIndex;
		FVector Location;
		bool bMoved;
	};
	void BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location);
	void EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location);
	void TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location);
	TouchData	TouchItem;
	
	void Interact();

	int MaxJumps = 3;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

	/* 
	 * Configures input for touchscreen devices if there is a valid touch interface for doing so 
	 *
	 * @param	InputComponent	The input component pointer to bind controls to
	 * @returns true if touch controls were enabled.
	 */
	bool EnableTouchscreenMovement(UInputComponent* InputComponent);

public:
	/** Returns Mesh1P subobject **/
	//USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

