// Copyright Epic Games, Inc. All Rights Reserved.

#include "my_bossCharacter.h"
#include "my_bossProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "InteractableActor.h"
#include "DrawDebugHelpers.h"
#include "WeaponActor.h"
#include "EnemyActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/PostProcessVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// Amy_bossCharacter

Amy_bossCharacter::Amy_bossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CurHealth = DefHealth;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	HeldObjPos = CreateDefaultSubobject<USceneComponent>(TEXT("Held Object Position"));

	GunLoc = CreateDefaultSubobject<USceneComponent>(TEXT("Gun Location"));
	GunLoc->SetupAttachment(FirstPersonCameraComponent);

	InteractionIgnore.AddIgnoredActor(this);

	Tags.Add(TEXT("Player"));

	bCanDash = true;

	CurManaTime = DefManaTime;

	CurJumpsAmount = DefJumpsAmount;
}

void Amy_bossCharacter::BeginPlay()
{
	// Call the base class 
	Super::BeginPlay();

	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 64.f));

	if (WeaponClass)
	{
		Weapon = Cast<AWeaponActor>(GetWorld()->SpawnActor(WeaponClass));

		Weapon->AttachToComponent(GunLoc, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		
		WeaponMesh = Weapon->GetMesh();
	}

	PostProcessMat = UMaterialInstanceDynamic::Create(PostMat, PostProcessActor);

	TArray<AActor*> PostProcessActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Post Process"), PostProcessActors);

	PostProcessActor = Cast<APostProcessVolume>(PostProcessActors[0]);
	PostProcessActor->Settings.AddBlendable(PostProcessMat, 1.0f);
}

void Amy_bossCharacter::Tick(float DeltaTime) 
{
	if (bHoldingActor)
	{
		FVector Look = FirstPersonCameraComponent->GetForwardVector();
		FHitResult Hit;

		//Get cammera loc in world
		FVector CameraLoc = GetTransform().TransformPosition(FirstPersonCameraComponent->GetRelativeLocation());
		FVector End = CameraLoc + (Look * InteractionRange * 0.5f);

		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, End, ECC_Visibility, InteractionIgnore);

		FVector OldLoc = HeldActor->GetActorLocation();
		FVector NewLoc;

		if(bHit)
			NewLoc = Hit.Location;
		else
			NewLoc = End;

		FVector CurLoc = FMath::Lerp(OldLoc, NewLoc, 0.65);

		HeldActor->SetActorLocation(CurLoc);
	}
	
	//Handle Shader Change
	if (ShaderChangeState == 1 && CurrentShaderChangePercent <= 1)
	{
		CurrentShaderChangePercent += DeltaTime / DefShaderTransitionTime / 3;
		PostProcessMat->SetScalarParameterValue(TEXT("Color Change Bool"), CurrentShaderChangePercent);
	}
	else if (ShaderChangeState == 2 && CurrentShaderChangePercent >= 0)
	{
		CurrentShaderChangePercent -= DeltaTime / DefShaderTransitionTime;
		PostProcessMat->SetScalarParameterValue(TEXT("Color Change Bool"), CurrentShaderChangePercent);
	}

	//Left without else so time dialation doesn't go negative for 1 frame
	if(CurrentShaderChangePercent <= 0 || CurrentShaderChangePercent >= 1)
	{
		ShaderChangeState = 0;
		CurrentShaderChangePercent = (CurrentShaderChangePercent <= 0) ? 0 : 1;
	}

	//Mana bar
	CurManaTime -= (CurrentShaderChangePercent <= 0 || ShaderChangeState == 2) ? DeltaTime : 0.0f;
	if (CurManaTime <= 0.0f)
	{
		CurManaTime = 0.01f;
		OnSlowTime();
	}

	//Handle Time Dialation
	CustomTimeDilation = (1.2f - CurrentShaderChangePercent) * 5.0f;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1 / CustomTimeDilation);

	GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Red, FString::SanitizeFloat(CurJumpsAmount, 0));
}

//////////////////////////////////////////////////////////////////////////
// Input

void Amy_bossCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &Amy_bossCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &Amy_bossCharacter::OnFire);

	// Enable touchscreen input
	EnableTouchscreenMovement(PlayerInputComponent);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &Amy_bossCharacter::OnResetVR);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &Amy_bossCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &Amy_bossCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &Amy_bossCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &Amy_bossCharacter::LookUpAtRate);

	//Bind Interact Event
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &Amy_bossCharacter::Interact);

	PlayerInputComponent->BindAction("Slow Time", IE_Pressed, this, &Amy_bossCharacter::OnSlowTime);
	PlayerInputComponent->BindAction("Dash", IE_Pressed, this, &Amy_bossCharacter::Dash);
}

void Amy_bossCharacter::Interact()
{
	FVector Look = FirstPersonCameraComponent->GetForwardVector();
	FHitResult Hit;

	//Get cammera loc in world
	FVector CameraLoc = GetTransform().TransformPosition(FirstPersonCameraComponent->GetRelativeLocation());
	FVector End = CameraLoc + (Look * InteractionRange);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, End, ECC_Visibility, InteractionIgnore);

	//check if the ray hit and the actor is interactable
	if (!bHoldingActor)
	{
		if (bHit && Hit.Actor->ActorHasTag(TEXT("Interactable")))
		{
			//Disable phys on held actor
			HeldActor = Cast<AInteractableActor>(Hit.Actor);
			HeldActor->GetSKMesh()->SetSimulatePhysics(false);
			HeldActor->GetSKMesh()->SetEnableGravity(false);
			HeldActor->GetSKMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			bHoldingActor = true;
			GEngine->AddOnScreenDebugMessage(-1, 2.50f, FColor::Red, TEXT("MSG"));
		}
	}
	else
	{
		//Enable phys on held actor
		HeldActor->GetSKMesh()->SetSimulatePhysics(true);
		HeldActor->GetSKMesh()->SetEnableGravity(true);
		HeldActor->GetSKMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		//Reset
		HeldActor = nullptr;
		bHoldingActor = false;
	}
}

void Amy_bossCharacter::OnFire()
{
	FVector Look = FirstPersonCameraComponent->GetForwardVector();
	FHitResult Hit;

	//Get cammera loc in world
	FVector CameraLoc = GetTransform().TransformPosition(FirstPersonCameraComponent->GetRelativeLocation());
	FVector End = CameraLoc + (Look * Weapon->Range * 0.5f);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, End, ECC_Visibility, InteractionIgnore);

	if(GetWorld()->GetTimerManager().GetTimerRemaining(DestroyMuzzleFlashHandle) <= 0.0f)
	{
		//spawn muzzle and start it's life-time timer
		MuzzleFlashActor = GetWorld()->SpawnActor(MuzzleFlashClass, new FTransform);
		MuzzleFlashActor->SetActorLocation(WeaponMesh->GetSocketLocation(TEXT("Muzzle")));
		MuzzleFlashActor->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Muzzle"));

		GetWorldTimerManager().SetTimer(DestroyMuzzleFlashHandle, this, &Amy_bossCharacter::DestroyMuzzleFlash, 0.2f / CustomTimeDilation, false);

		//Check if the ray hit an enemy and damage it if so
		if (bHit && Hit.Actor->ActorHasTag(TEXT("Enemy")))
		{
			Cast<AEnemyActor>(Hit.Actor)->TakeDamage(Weapon->Damage);
			CurManaTime += 1.0f * (CurrentShaderChangePercent == 1);
			CurManaTime = FMath::Clamp(CurManaTime, 0.0f, DefManaTime);
		}
	}
}

void Amy_bossCharacter::DestroyMuzzleFlash()
{
	MuzzleFlashActor->Destroy();
	MuzzleFlashActor = nullptr;
}

void Amy_bossCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void Amy_bossCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void Amy_bossCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	TouchItem.bIsPressed = false;
}


void Amy_bossCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void Amy_bossCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void Amy_bossCharacter::Jump()
{
	if (CurJumpsAmount <= 0) return;

	Super::LaunchCharacter(FVector(0, 0, GetCharacterMovement()->JumpZVelocity), false, true);
	CurJumpsAmount -= 1;
}

void Amy_bossCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	CurJumpsAmount = DefJumpsAmount;
}

bool Amy_bossCharacter::CanJumpInternal_Implementation() const
{
	return (CurJumpsAmount > 0);
}

void Amy_bossCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void Amy_bossCharacter::OnSlowTime()
{
	if (CurManaTime <= 0.01f && CurrentShaderChangePercent > 0) return;

	if (ShaderChangeState == 0)
	{
		if (CurrentShaderChangePercent == 0)
		{
			ShaderChangeState = 1;
			GetCharacterMovement()->GravityScale = 1.8f;
			GetCharacterMovement()->MaxWalkSpeed = 1200.0f;
		}
		else
		{
			ShaderChangeState = 2;
			GetCharacterMovement()->GravityScale = 1.0f;
			GetCharacterMovement()->MaxWalkSpeed = 1800.0f;
		}
	}
}

void Amy_bossCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void Amy_bossCharacter::Dash()
{
	if (!bCanDash) return;
	
	bCanDash = false;

	VelocityBeforeDash = GetCharacterMovement()->Velocity;

	//Give the character some velocity if it wasn't moving
	VelocityBeforeDash = (FMath::Abs(VelocityBeforeDash.X) <= 50 && FMath::Abs(VelocityBeforeDash.Y) <= 50) ? GetActorForwardVector() * 1250.0f : VelocityBeforeDash;

	LaunchCharacter(GetActorForwardVector() * DashForce, true, true);

	GetWorldTimerManager().SetTimer(ResetDashHandle, this, &Amy_bossCharacter::ResetDash, DashCooldown / CustomTimeDilation, false);
	GetWorldTimerManager().SetTimer(EndDashHandle, this, &Amy_bossCharacter::EndDash, DashTime / CustomTimeDilation, false);
}

void Amy_bossCharacter::EndDash()
{
	GetCharacterMovement()->Velocity = VelocityBeforeDash;
}

void Amy_bossCharacter::ResetDash()
{
	bCanDash = true;
}

bool Amy_bossCharacter::EnableTouchscreenMovement(class UInputComponent* PlayerInputComponent)
{
	if (FPlatformMisc::SupportsTouchInput() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &Amy_bossCharacter::BeginTouch);
		PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &Amy_bossCharacter::EndTouch);

		//Commenting this out to be more consistent with FPS BP template.
		return true;
	}
	
	return false;
}
