#include "Core/OSHUD.h"

#if !UE_BUILD_SHIPPING

#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Characters/OSCharacter.h"
#include "Core/OSPlayerState.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GAS/Abilities/GA_OSAttack.h"
#include "GAS/Abilities/GA_OSGrab.h"
#include "GAS/Abilities/GA_OSmantleability.h"
#include "Components/BoxComponent.h"
#include "Components/OSCharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Utilities/AbilityHelper.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

// ---------------------------------------------------------------------------
// Global bools — viewport-level settings (shared across all players in process).
// Player cheats are per-player, read from ASC tags in DrawDebugStatusOverlay.
// ---------------------------------------------------------------------------

#include "Core/OSDebugGlobals.h"

// Shared with UGA_OSGrab (defined there); extern'd here for the HUD overlay gate in DrawHUD.
extern TAutoConsoleVariable<int32> CVarGrabDiagLogging;

// ---------------------------------------------------------------------------
// Drawing constants
// ---------------------------------------------------------------------------

namespace OSDebugHUD
{
	constexpr float BarWidth        = 80.f;
	constexpr float BarHeight       = 8.f;
	constexpr float BarBorderWidth  = 1.f;
	constexpr float VerticalOffset  = 100.f;  // world-space units above actor origin
	constexpr float TextPadding     = 2.f;
	constexpr float MaxDrawDistance  = 4000.f; // skip characters beyond 40m
}

#endif // !UE_BUILD_SHIPPING

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AOSHUD::AOSHUD()
	: Super()
	, DebugWarpTargetNames({
		FName("AttackTarget"),
		FName("AlignmentTarget"),
		FName("root"),
		FName("warp")
	})
{
}

// ---------------------------------------------------------------------------
// DrawHUD
// ---------------------------------------------------------------------------

void AOSHUD::DrawHUD()
{
	Super::DrawHUD();
#if !UE_BUILD_SHIPPING
	DrawDebugStatusOverlay();

	if (GDebugShowHealthBars || GDebugShowNameplates)
	{
		DrawDebugOverheadInfo();
	}

	if (GDebugRotationViz)
	{
		DrawRotationDebugOverlay();
	}

	if (GDebugTargetingViz)
	{
		DrawTargetingDebugOverlay();
	}

	if (GDebugGrabViz)
	{
		DrawGrabDebugOverlay();
	}

	if (GDebugMantleViz)
	{
		DrawMantleDebugOverlay();
	}

	// Unified gate: cheat-menu Grab checkbox OR console CVar. Both enable the full grab
	// diagnostic surface (logs + this overlay). No verbosity distinction — one flag, all on.
	if (GDebugGrabViz || CVarGrabDiagLogging.GetValueOnAnyThread() > 0)
	{
		DrawGrabDiagnostics();
	}
#endif
}

#if !UE_BUILD_SHIPPING

// ---------------------------------------------------------------------------
// DrawDebugOverheadInfo — iterate all characters, project to screen, draw
// ---------------------------------------------------------------------------

void AOSHUD::DrawDebugOverheadInfo()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	if (!Canvas) return;
	
	const APawn* LocalPawn = PC->GetPawn();
	const FVector CameraLoc = LocalPawn ? LocalPawn->GetActorLocation() : PC->GetFocalLocation();
	const float MaxDistSq = OSDebugHUD::MaxDrawDistance * OSDebugHUD::MaxDrawDistance;

	for (TActorIterator<AOSCharacter> It(World); It; ++It)
	{
		AOSCharacter* Character = *It;
		if (!IsValid(Character)) continue;

		const FVector CharLoc = Character->GetActorLocation();
		if (FVector::DistSquared(CameraLoc, CharLoc) > MaxDistSq) continue;

		// Project a point above the character's head to screen space
		const FVector WorldPos = CharLoc + FVector(0.f, 0.f, OSDebugHUD::VerticalOffset);
		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(WorldPos, ScreenPos)) continue;

		float DrawY = ScreenPos.Y;

		// Draw nameplate above health bar so it doesn't overlap.
		if (GDebugShowNameplates)  DrawNameplate(Character, ScreenPos.X, DrawY);
		if (GDebugShowHealthBars)  DrawHealthBar(Character, ScreenPos.X, DrawY);
	}
}

// ---------------------------------------------------------------------------
// DrawHealthBar — colored bar + HP text for a single character
// ---------------------------------------------------------------------------

void AOSHUD::DrawHealthBar(const AOSCharacter* Character, float ScreenX, float& DrawY)
{
	auto* ASC = AbilityHelper::GetOSASCFromActor(Character);
	if (!ASC) return;

	const float Health    = ASC->GetNumericAttribute(UOSAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UOSAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f) return;

	const float Pct = FMath::Clamp(Health / MaxHealth, 0.f, 1.f);

	// Bar position (centered on projected X)
	const float BarX = ScreenX - OSDebugHUD::BarWidth * 0.5f;
	const float BarY = DrawY;

	// Border (dark background)
	Canvas->SetDrawColor(FColor(0, 0, 0, 180));
	Canvas->DrawTile(
		Canvas->DefaultTexture,
		BarX - OSDebugHUD::BarBorderWidth,
		BarY - OSDebugHUD::BarBorderWidth,
		OSDebugHUD::BarWidth + OSDebugHUD::BarBorderWidth * 2.f,
		OSDebugHUD::BarHeight + OSDebugHUD::BarBorderWidth * 2.f,
		0.f, 0.f, 1.f, 1.f);

	// Filled portion — green to yellow to red
	FLinearColor BarColor;
	if (Pct > 0.5f)
		BarColor = FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, (Pct - 0.5f) * 2.f);
	else
		BarColor = FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Yellow, Pct * 2.f);

	Canvas->SetDrawColor(BarColor.ToFColor(true));
	Canvas->DrawTile(
		Canvas->DefaultTexture,
		BarX,
		BarY,
		OSDebugHUD::BarWidth * Pct,
		OSDebugHUD::BarHeight,
		0.f, 0.f, 1.f, 1.f);

	// HP text centered below bar
	DrawY += OSDebugHUD::BarHeight + OSDebugHUD::TextPadding;
	const FString HpText = FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth);
	float TextW, TextH;
	Canvas->StrLen(GEngine->GetSmallFont(), HpText, TextW, TextH);
	Canvas->SetDrawColor(FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), HpText, ScreenX - TextW * 0.5f, DrawY);
	DrawY += TextH + OSDebugHUD::TextPadding;
}

// ---------------------------------------------------------------------------
// DrawNameplate — player name for a single character
// ---------------------------------------------------------------------------

void AOSHUD::DrawNameplate(const AOSCharacter* Character, float ScreenX, float& DrawY)
{
	const AOSPlayerState* PS = Character->GetPlayerState<AOSPlayerState>();
	if (!PS) return;

	const FString PlayerName = PS->GetPlayerName();
	if (PlayerName.IsEmpty()) return;

	FColor NameColor = FColor::White;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const AOSPlayerState* LocalPS = PC->GetPlayerState<AOSPlayerState>())
		{
			const int32 LocalTeam = LocalPS->GetTeamIndex();
			const int32 TargetTeam = PS->GetTeamIndex();
			NameColor = (LocalTeam == TargetTeam) ? FColor::Cyan : FColor::Red;
		}
	}

	float TextW, TextH;
	Canvas->StrLen(GEngine->GetSmallFont(), PlayerName, TextW, TextH);
	Canvas->SetDrawColor(NameColor);
	Canvas->DrawText(GEngine->GetSmallFont(), PlayerName, ScreenX - TextW * 0.5f, DrawY);
	DrawY += TextH + OSDebugHUD::TextPadding;
}

// ---------------------------------------------------------------------------
// DrawDebugStatusOverlay — compact cheat indicator strip in top-left
// ---------------------------------------------------------------------------

void AOSHUD::DrawDebugStatusOverlay()
{
	if (!Canvas) return;

	// Collect active cheat labels.
	TArray<FString> ActiveCheats;

	// Player cheats — per-player, read from local player's ASC tags (not global bools).
	APlayerController* PC = GetOwningPlayerController();
	if (PC)
	{
		if (AOSPlayerState* PS = PC->GetPlayerState<AOSPlayerState>())
		{
			if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
			{
				const FOSGameplayTags& DebugTags = FOSGameplayTags::Get();
				if (ASC->HasMatchingGameplayTag(DebugTags.Debug_NoHealthDamage))    ActiveCheats.Add(TEXT("NO_HP_DMG"));
				if (ASC->HasMatchingGameplayTag(DebugTags.Debug_NoStaminaDrain))    ActiveCheats.Add(TEXT("NO_STA_DRAIN"));
				if (ASC->HasMatchingGameplayTag(DebugTags.Debug_BlockHealthRegen))  ActiveCheats.Add(TEXT("BLOCK_HP_REGEN"));
				if (ASC->HasMatchingGameplayTag(DebugTags.Debug_BlockStaminaRegen)) ActiveCheats.Add(TEXT("BLOCK_STA_REGEN"));
			}
		}
	}

	// Viewport-level settings — global bools (shared across all players in process).
	if (GDebugShowHealthBars)     ActiveCheats.Add(TEXT("BARS"));
	if (GDebugShowNameplates)     ActiveCheats.Add(TEXT("NAMES"));
	if (GDebugPunchingBag)        ActiveCheats.Add(TEXT("BAG"));
	if (GDebugMantleViz)          ActiveCheats.Add(TEXT("MANTLE"));
	if (GDebugAttackTraceViz)     ActiveCheats.Add(TEXT("TRACE"));
	if (GDebugRotationViz)        ActiveCheats.Add(TEXT("ROTATION"));
	if (GDebugTargetingViz)       ActiveCheats.Add(TEXT("TARGET"));
	if (GDebugGrabViz)            ActiveCheats.Add(TEXT("GRAB"));

	if (ActiveCheats.IsEmpty()) return;

	const FString StatusText = FString::Printf(TEXT("[DEBUG] %s"), *FString::Join(ActiveCheats, TEXT(" | ")));

	const UFont* Font = GEngine->GetSmallFont();
	float TextW, TextH;
	Canvas->StrLen(Font, StatusText, TextW, TextH);

	// Dark background behind text for readability.
	const float PadX = 6.f;
	const float PadY = 3.f;
	const float DrawX = 8.f;
	const float DrawY = 8.f;

	Canvas->SetDrawColor(FColor(0, 0, 0, 160));
	Canvas->DrawTile(Canvas->DefaultTexture,
		DrawX - PadX, DrawY - PadY,
		TextW + PadX * 2.f, TextH + PadY * 2.f,
		0.f, 0.f, 1.f, 1.f);

	Canvas->SetDrawColor(FColor::Yellow);
	Canvas->DrawText(Font, StatusText, DrawX, DrawY);
}

// ---------------------------------------------------------------------------
// DrawRotationDebugOverlay — High-Fidelity Diagnostics
// Color legend:
//   WHITE  = Actor forward
//   GREEN  = MW warp target direction (active)
//   YELLOW = Raw input direction (stick)
//   CYAN   = Ideal lunge path tube
//   AMBER  = Subtle search cone pulse
//   DASHED WHITE = Raw root motion projection
//   RED/BLUE AXES = Orientation history ghosts
// ---------------------------------------------------------------------------

void AOSHUD::DrawRotationDebugOverlay()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	const APawn* LocalPawn = PC->GetPawn();
	const FVector CameraLoc = LocalPawn ? LocalPawn->GetActorLocation() : PC->GetFocalLocation();

	for (TActorIterator<AOSCharacter> It(World); It; ++It)
	{
		AOSCharacter* Char = *It;
		if (!IsValid(Char)) continue;

		if (FVector::DistSquared(CameraLoc, Char->GetActorLocation()) > 16000000.f) continue;

		UOSCharacterMovementComponent* CMC = Cast<UOSCharacterMovementComponent>(Char->GetCharacterMovement());
		UMotionWarpingComponent* MWC = Char->GetMotionWarpingComponent();
		UAbilitySystemComponent* ASC = AbilityHelper::GetOSASCFromActor(Char);
		UGA_OSAttack* AttackAbility = ASC ? Cast<UGA_OSAttack>(ASC->GetAnimatingAbility()) : nullptr;

		const FVector Base = Char->GetActorLocation() + FVector(0.f, 0.f, 100.f);
		constexpr float ArrowLen = 150.f;
		constexpr float ArrowSize = 15.f;
		constexpr float Duration = 0.f;

		// --- WHITE: Actor forward ---
		const FVector ActorFwd = Char->GetActorForwardVector().GetSafeNormal2D();
		DrawDebugDirectionalArrow(World, Base, Base + ActorFwd * ArrowLen,
			ArrowSize, FColor::White, false, Duration, 0, 2.f);
		DrawDebugString(World, Base + ActorFwd * ArrowLen, TEXT("[FORWARD]"), nullptr, FColor::White, Duration);

		// --- ANGULAR OFFSETS between direction sources ---
		if (AttackAbility && Char == LocalPawn)
		{
			const FVector CharFwd = ActorFwd;
			const FVector CamFwd = UOSCombatBlueprintLibrary::GetCameraForwardDirection2D(Char);
			const FVector BlendedDir = AttackAbility->GetStoredBlendedDirection2D();
			const FVector GroundBase = Char->GetActorLocation() - FVector(0, 0, 85);
			constexpr float ArcRadius = 80.f;

			// Helper: draw an angular offset arc between two directions with a degree label
			auto DrawAngleArc = [&](const FVector& From, const FVector& To, const FColor& Color, const TCHAR* Label)
			{
				if (From.IsNearlyZero() || To.IsNearlyZero()) return;
				const float FromYaw = FMath::Atan2(From.Y, From.X);
				const float ToYaw = FMath::Atan2(To.Y, To.X);
				float DeltaRad = FMath::FindDeltaAngleRadians(FromYaw, ToYaw);
				const float DeltaDeg = FMath::RadiansToDegrees(DeltaRad);
				if (FMath::Abs(DeltaDeg) < 1.f) return;

				const int32 Steps = FMath::Max(4, FMath::CeilToInt(FMath::Abs(DeltaDeg) / 10.f));
				const float StepRad = DeltaRad / Steps;
				for (int32 i = 0; i < Steps; ++i)
				{
					const float A0 = FromYaw + StepRad * i;
					const float A1 = FromYaw + StepRad * (i + 1);
					const FVector P0 = GroundBase + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * ArcRadius;
					const FVector P1 = GroundBase + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * ArcRadius;
					DrawDebugLine(World, P0, P1, Color, false, Duration, 0, 2.0f);
				}
				const float MidAngle = FromYaw + DeltaRad * 0.5f;
				const FVector LabelPos = GroundBase + FVector(FMath::Cos(MidAngle), FMath::Sin(MidAngle), 0.f) * (ArcRadius + 15.f);
				DrawDebugString(World, LabelPos, FString::Printf(TEXT("%s %.0f°"), Label, DeltaDeg), nullptr, Color, Duration, false, 0.8f);
			};

			// Char→Camera offset (amber)
			if (!CamFwd.IsNearlyZero())
			{
				DrawAngleArc(CharFwd, CamFwd, FColor(255, 191, 0), TEXT("CAM"));
			}
			// Char→Attack direction offset (magenta) — shows how far the attack deviates from facing
			if (!BlendedDir.IsNearlyZero())
			{
				DrawAngleArc(CharFwd, BlendedDir, FColor::Magenta, TEXT("ATK"));
			}
		}

		// --- MW state ---
		bool bMWActive = false;
		float MWYaw = 0.f;
		if (MWC)
		{
			for (const FName& TName : DebugWarpTargetNames)
			{
				if (const FMotionWarpingTarget* WT = MWC->FindWarpTarget(TName))
				{
					const FVector WLoc = WT->GetTargetTrasform().GetLocation();
					const float WYaw = WT->GetTargetTrasform().Rotator().Yaw;

					bMWActive = true;
					if (MWYaw == 0.f) MWYaw = WYaw;

					// Target Sphere & Name
					DrawDebugSphere(World, WLoc, 20.f, 8, FColor::Green, false, Duration, 0, 1.0f);
					DrawDebugString(World, WLoc + FVector(0, 0, 30), TName.ToString(), nullptr, FColor::Green, Duration, false, 1.2f);
					
					// LUNGE Arrow
					const FVector WTDir = FRotator(0.f, WYaw, 0.f).Vector();
					DrawDebugDirectionalArrow(World, Base + FVector(0,0,5), Base + FVector(0,0,5) + WTDir * ArrowLen, 
						ArrowSize, FColor::Green, false, Duration, 0, 3.0f);
					DrawDebugString(World, Base + WTDir * ArrowLen + FVector(0,0,5), TEXT("[LUNGE]"), nullptr, FColor::Green, Duration);

	#if !UE_BUILD_SHIPPING
					// DIAGNOSTIC SUITE — all data sources are compile-guarded on the ability side
					if (AttackAbility)
					{
						const FVector CurrentLoc = Char->GetActorLocation();
						const FVector Start = AttackAbility->DebugWarpPathStart;
						const FVector End = AttackAbility->DebugWarpPathEnd;

						if (FVector::DistSquared(Start, End) > 100.f)
						{
							// Ideal Path Tube (Cyan)
							DrawDebugLine(World, Start, End, FColor::Cyan, false, Duration, 0, 4.0f);

							// DRIFT & SCALE
							const FVector IdealDir = (End - Start).GetSafeNormal2D();
							const FVector ToCurrent = CurrentLoc - Start;
							const FVector PointOnIdeal = Start + IdealDir * FVector::DotProduct(ToCurrent, IdealDir);
							const float DriftCm = FVector::Dist2D(CurrentLoc, PointOnIdeal);

							DrawDebugLine(World, CurrentLoc, PointOnIdeal, FColor::Yellow, false, Duration, 0, 2.0f);
							DrawDebugString(World, CurrentLoc + FVector(0,0,20), FString::Printf(TEXT("Drift: %.1f cm"), DriftCm), nullptr, FColor::Yellow, Duration);

							// GHOST TRAIL: visualize rotation history
							for (const FTransform& Ghost : AttackAbility->DebugOrientationGhosts)
							{
								const FVector GLoc = Ghost.GetLocation() - FVector(0,0,90);
								const FVector GFwd = Ghost.GetRotation().GetForwardVector();
								DrawDebugLine(World, GLoc, GLoc + GFwd * 40.f, FColor::Red, false, Duration, 0, 2.0f);
								DrawDebugLine(World, GLoc, GLoc + Ghost.GetRotation().GetRightVector() * 20.f, FColor::Blue, false, Duration, 0, 2.0f);
							}

							// RAW ROOT MOTION PROJECTION (Dashed White)
							const FVector AnimBase = Start + ActorFwd * FVector::Dist2D(Start, End);
							const int32 DashSteps = 10;
							for(int32 i=0; i<DashSteps; ++i)
							{
								if (i % 2 == 0) continue;
								FVector P0 = Start + (AnimBase - Start) * (float)i / DashSteps;
								FVector P1 = Start + (AnimBase - Start) * (float)(i+1) / DashSteps;
								DrawDebugLine(World, P0 + FVector(0,0,5), P1 + FVector(0,0,5), FColor::White, false, Duration, 0, 1.5f);
							}
							DrawDebugString(World, AnimBase, TEXT("[RAW_PROJECTED]"), nullptr, FColor::White, Duration);
						}

						// Standoff ring
						AActor* CurrentTarget = AttackAbility->GetCurrentSoftTarget();
						if (IsValid(CurrentTarget))
						{
							const FVector TargetCenter = CurrentTarget->GetActorLocation();
							const float Standoff = AttackAbility->GetOptimalMeleeDistance();
							DrawDebugPoint(World, TargetCenter + FVector(0,0,50), 15.f, FColor::Red, false, Duration);
							const int32 RingSteps = 24;
							for(int32 i=0; i<RingSteps; ++i)
							{
								if (i % 2 == 0) continue;
								float Angle0 = 360.f * i / RingSteps;
								float Angle1 = 360.f * (i+1) / RingSteps;
								FVector P0 = TargetCenter + FRotator(0.f, Angle0, 0.f).Vector() * Standoff;
								FVector P1 = TargetCenter + FRotator(0.f, Angle1, 0.f).Vector() * Standoff;
								DrawDebugLine(World, P0 + FVector(0,0,5), P1 + FVector(0,0,5), FColor::Green, false, Duration, 0, 2.5f);
							}
						}
					}
#endif
				}
			}
		}

		// --- Only draw inputs for the local player ---
		if (Char == LocalPawn)
		{
			// --- YELLOW: Input direction ---
			const FVector InputDir = UOSCombatBlueprintLibrary::GetInputDirection2D(Char);
			if (!InputDir.IsNearlyZero())
			{
				DrawDebugDirectionalArrow(World, Base + FVector(0, 0, 15), Base + FVector(0, 0, 15) + InputDir * (ArrowLen * 0.7f),
					ArrowSize, FColor::Yellow, false, Duration, 0, 2.f);
				DrawDebugString(World, Base + InputDir * (ArrowLen * 0.7f), TEXT("[STICK]"), nullptr, FColor::Yellow, Duration);
			}

			// --- CYAN: Aim direction (camera forward) ---
			const FVector AimDir = UOSCombatBlueprintLibrary::GetCameraForwardDirection2D(Char);
			if (!AimDir.IsNearlyZero())
			{
				DrawDebugDirectionalArrow(World, Base + FVector(0, 0, 20), Base + FVector(0, 0, 20) + AimDir * (ArrowLen * 0.6f),
					ArrowSize * 0.7f, FColor::Cyan, false, Duration, 0, 1.5f);
				DrawDebugString(World, Base + AimDir * (ArrowLen * 0.6f), TEXT("[AIM]"), nullptr, FColor::Cyan, Duration);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// DrawTargetingDebugOverlay — live targeting decision visualization
// CVar: OS.Debug.TargetingViz
// Shows: candidates with status colors, scores, radar sweep, connection beam,
//        final attack direction, and cone search extents.
// ---------------------------------------------------------------------------

void AOSHUD::DrawTargetingDebugOverlay()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	AOSCharacter* LocalChar = Cast<AOSCharacter>(PC->GetPawn());
	if (!IsValid(LocalChar)) return;

	UAbilitySystemComponent* LocalASC = AbilityHelper::GetOSASCFromActor(LocalChar);
	UGA_OSAttack* AttackAbility = LocalASC ? Cast<UGA_OSAttack>(LocalASC->GetAnimatingAbility()) : nullptr;
	if (!AttackAbility || !AttackAbility->HasSoftTargeting()) return;

	const float ConeAngle = AttackAbility->GetSoftTargetConeAngle();
	const float MaxRange = AttackAbility->GetSoftTargetMaxRange();
	const FVector CharLoc = LocalChar->GetActorLocation();
	const FVector GroundBase = CharLoc - FVector(0, 0, 85);
	constexpr float Duration = 0.f;
	const float Time = World->GetTimeSeconds();

	// Compute primary search direction (mirrors ResolveAttackDirectionInCone)
	FVector PrimaryDir = UOSCombatBlueprintLibrary::GetInputDirection2D(LocalChar);
	if (PrimaryDir.IsNearlyZero())
	{
		PrimaryDir = LocalChar->GetActorForwardVector();
		PrimaryDir.Z = 0.f;
		PrimaryDir = PrimaryDir.GetSafeNormal();
	}
	if (PrimaryDir.IsNearlyZero()) PrimaryDir = FVector::ForwardVector;

	// ConeAngle is halved again in FindBestTargetInCone (ConeAngleDegrees * 0.5f).
	// So the actual search half-angle is ConeAngle * 0.5.
	const float HalfAngle = ConeAngle * 0.5f;
	const float ConeHalfCos = FMath::Cos(FMath::DegreesToRadians(HalfAngle));

	// --- RADAR SWEEP: rotating line through the cone ---
	{
		const float SweepSpeed = 120.f; // degrees per second
		const float SweepAngle = FMath::Fmod(Time * SweepSpeed, HalfAngle * 2.f) - HalfAngle;
		const FVector SweepDir = FRotator(0.f, SweepAngle, 0.f).RotateVector(PrimaryDir);
		const FColor SweepColor = FColor(0, 255, 100, 180);
		DrawDebugLine(World, GroundBase, GroundBase + SweepDir * MaxRange, SweepColor, false, Duration, 0, 1.0f);
	}

	// --- CONE EDGES: subtle boundary lines ---
	{
		const FVector Left = FRotator(0.f, -HalfAngle, 0.f).RotateVector(PrimaryDir);
		const FVector Right = FRotator(0.f, HalfAngle, 0.f).RotateVector(PrimaryDir);
		const FColor EdgeColor = FColor(80, 80, 80, 60);
		DrawDebugLine(World, GroundBase, GroundBase + Left * MaxRange, EdgeColor, false, Duration, 0, 0.5f);
		DrawDebugLine(World, GroundBase, GroundBase + Right * MaxRange, EdgeColor, false, Duration, 0, 0.5f);
	}

	// --- CANDIDATES: sphere overlap all characters in range ---
	AActor* ChosenTarget = AttackAbility->GetCurrentSoftTarget();
	FOSAttackDirectionParams Params;
	AttackAbility->FillSoftTargetParams(Params);

	for (TActorIterator<AOSCharacter> It(World); It; ++It)
	{
		AOSCharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == LocalChar) continue;

		const FVector CandLoc = Candidate->GetActorLocation();
		const float Dist = FVector::Dist2D(CharLoc, CandLoc);
		if (Dist > MaxRange * 1.5f) continue; // show slightly beyond range for context

		const FVector ToCandidate = (CandLoc - CharLoc).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(PrimaryDir, ToCandidate);
		const bool bInCone = Dot >= ConeHalfCos && Dist <= MaxRange;
		const bool bIsChosen = Candidate == ChosenTarget;
		const bool bIsDead = UOSCombatBlueprintLibrary::IsIncapacitated(Candidate);
		const bool bIsFriendly = UOSCombatBlueprintLibrary::AreActorsFriendly(LocalChar, Candidate);
		const bool bOutOfRange = Dist > MaxRange;

		// Status color
		FColor SphereColor;
		FString StatusText;
		if (bIsChosen)
		{
			// Pulsing green for chosen target
			const float Pulse = 0.7f + 0.3f * FMath::Sin(Time * 6.f);
			SphereColor = FColor(0, FMath::Clamp(static_cast<int32>(255 * Pulse), 100, 255), 0);
			StatusText = TEXT("LOCKED");
		}
		else if (bIsDead)
		{
			SphereColor = FColor(100, 0, 0); // Dark red
			StatusText = TEXT("DEAD");
		}
		else if (bIsFriendly)
		{
			SphereColor = FColor(0, 100, 200); // Blue
			StatusText = TEXT("FRIENDLY");
		}
		else if (bOutOfRange)
		{
			SphereColor = FColor(80, 80, 80); // Gray
			StatusText = FString::Printf(TEXT("%.0fcm"), Dist);
		}
		else if (!bInCone)
		{
			SphereColor = FColor(200, 100, 0); // Orange — in range but outside cone
			StatusText = FString::Printf(TEXT("OUT %.0f°"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f))));
		}
		else
		{
			// In cone, valid — amber, show score
			const float Score = UOSCombatBlueprintLibrary::ScoreTargetCandidate(LocalChar, PrimaryDir, Candidate, MaxRange);
			SphereColor = FColor(255, 200, 0); // Amber
			StatusText = FString::Printf(TEXT("%.2f"), Score);
		}

		const float SphereSize = bIsChosen ? 25.f : 15.f;
		DrawDebugSphere(World, CandLoc - FVector(0, 0, 50), SphereSize, 8, SphereColor, false, Duration, 0, 1.0f);
		DrawDebugString(World, CandLoc + FVector(0, 0, 120), StatusText, nullptr, SphereColor, Duration, false, 1.0f);

		// Connection beam to chosen target
		if (bIsChosen)
		{
			const float BeamPulse = 1.5f + 1.0f * FMath::Sin(Time * 4.f);
			DrawDebugLine(World, CharLoc, CandLoc, FColor::Green, false, Duration, 0, BeamPulse);
		}
	}

	// --- FINAL ATTACK DIRECTION: magenta arrow ---
	const FVector BlendedDir = AttackAbility->GetStoredBlendedDirection2D();
	if (!BlendedDir.IsNearlyZero())
	{
		const FVector ArrowBase = CharLoc + FVector(0, 0, 5);
		DrawDebugDirectionalArrow(World, ArrowBase, ArrowBase + BlendedDir * 150.f,
			15.f, FColor::Magenta, false, Duration, 0, 3.0f);
		DrawDebugString(World, ArrowBase + BlendedDir * 155.f,
			ChosenTarget ? TEXT("[ATK→TGT]") : TEXT("[ATK→MISS]"),
			nullptr, ChosenTarget ? FColor::Magenta : FColor(200, 100, 200), Duration);
	}
}

// ---------------------------------------------------------------------------
// DrawGrabDebugOverlay — per-frame grab warp + socket visualization
// CVar: os.debug.GrabViz
// Color legend:
//   GRAY LINE    = Root-to-root centerline
//   MAGENTA      = Attacker MW warp target (from MWC)
//   YELLOW       = Victim MW warp target (from MWC)
//   BLUE DOT     = Attacker socket position (live from mesh)
//   ORANGE DOT   = Victim socket position (live from mesh)
//   GREEN/RED    = Socket-to-socket line (green = close, red = far)
//   WHITE        = Forward arrows + summary label
// ---------------------------------------------------------------------------

void AOSHUD::DrawGrabDebugOverlay()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	const FOSGameplayTags& OSTags = FOSGameplayTags::Get();
	constexpr float Duration = 0.f; // single frame

	for (TActorIterator<AOSCharacter> It(World); It; ++It)
	{
		AOSCharacter* Attacker = *It;
		if (!IsValid(Attacker)) continue;

		// Find active grab ability via GetAnimatingAbility (same pattern as rotation overlay).
		UAbilitySystemComponent* ASC = AbilityHelper::GetOSASCFromActor(Attacker);
		if (!ASC) continue;

		UGA_OSGrab* GrabAbility = Cast<UGA_OSGrab>(ASC->GetAnimatingAbility());
		if (!GrabAbility) continue;

		AOSCharacter* Victim = GrabAbility->GetGrabVictim();
		if (!IsValid(Victim)) continue;

		const TArray<FOSGrabSocketPair>& Pairs = GrabAbility->GetGrabSocketPairs();
		UMotionWarpingComponent* AttackerMWC = Attacker->GetMotionWarpingComponent();
		UMotionWarpingComponent* VictimMWC = Victim->GetMotionWarpingComponent();
		USkeletalMeshComponent* AttackerMesh = Attacker->GetMesh();
		USkeletalMeshComponent* VictimMesh = Victim->GetMesh();

		const FVector AttackerRoot = Attacker->GetActorLocation();
		const FVector VictimRoot = Victim->GetActorLocation();
		const float RootToRoot = FVector::Dist2D(AttackerRoot, VictimRoot);

		// --- Centerline between roots ---
		DrawDebugLine(World, AttackerRoot, VictimRoot,
			FColor(80, 80, 80), false, Duration, 0, 0.5f);

		// --- Per socket pair ---
		float LabelOffsetZ = 30.f;
		for (int32 i = 0; i < Pairs.Num(); ++i)
		{
			const FOSGrabSocketPair& Pair = Pairs[i];

			// Attacker MW warp target (magenta)
			if (AttackerMWC && !Pair.AttackerWarpName.IsNone())
			{
				if (const FMotionWarpingTarget* WT = AttackerMWC->FindWarpTarget(Pair.AttackerWarpName))
				{
					const FVector WarpLoc = WT->GetTargetTrasform().GetLocation();
					DrawDebugSphere(World, WarpLoc, 12.f, 6, FColor::Magenta, false, Duration, 0, 1.0f);
					DrawDebugDirectionalArrow(World, AttackerRoot + FVector(0, 0, 5.f), WarpLoc + FVector(0, 0, 5.f),
						10.f, FColor::Magenta, false, Duration, 0, 1.0f);
					DrawDebugString(World, WarpLoc + FVector(0, 0, 15.f),
						FString::Printf(TEXT("ATK:%s"), *Pair.AttackerWarpName.ToString()),
						nullptr, FColor::Magenta, Duration, false, 0.7f);
				}
			}

			// Victim MW warp target (yellow)
			if (VictimMWC && !Pair.VictimWarpName.IsNone())
			{
				if (const FMotionWarpingTarget* WT = VictimMWC->FindWarpTarget(Pair.VictimWarpName))
				{
					const FVector WarpLoc = WT->GetTargetTrasform().GetLocation();
					DrawDebugSphere(World, WarpLoc, 12.f, 6, FColor::Yellow, false, Duration, 0, 1.0f);
					DrawDebugDirectionalArrow(World, VictimRoot + FVector(0, 0, 5.f), WarpLoc + FVector(0, 0, 5.f),
						10.f, FColor::Yellow, false, Duration, 0, 1.0f);
					DrawDebugString(World, WarpLoc + FVector(0, 0, 15.f),
						FString::Printf(TEXT("VIC:%s"), *Pair.VictimWarpName.ToString()),
						nullptr, FColor::Yellow, Duration, false, 0.7f);
				}
			}

			// Attacker socket live position (blue dot)
			FVector AttackerSocketLoc = AttackerRoot;
			float AttackerReach = 0.f;
			if (AttackerMesh && !Pair.GrabberSocket.IsNone() && AttackerMesh->DoesSocketExist(Pair.GrabberSocket))
			{
				AttackerSocketLoc = AttackerMesh->GetSocketLocation(Pair.GrabberSocket);
				DrawDebugPoint(World, AttackerSocketLoc, 8.f, FColor::Blue, false, Duration);
				DrawDebugLine(World, AttackerRoot, AttackerSocketLoc,
					FColor(60, 60, 140), false, Duration, 0, 0.5f);

				const FVector Offset = AttackerSocketLoc - AttackerRoot;
				AttackerReach = FMath::Max(0.f, FVector::DotProduct(Offset, Attacker->GetActorForwardVector()));
			}

			// Victim socket live position (orange dot)
			FVector VictimSocketLoc = VictimRoot;
			float VictimReach = 0.f;
			if (VictimMesh && !Pair.VictimSocket.IsNone() && VictimMesh->DoesSocketExist(Pair.VictimSocket))
			{
				VictimSocketLoc = VictimMesh->GetSocketLocation(Pair.VictimSocket);
				DrawDebugPoint(World, VictimSocketLoc, 8.f, FColor::Orange, false, Duration);
				DrawDebugLine(World, VictimRoot, VictimSocketLoc,
					FColor(140, 90, 0), false, Duration, 0, 0.5f);

				const FVector Offset = VictimSocketLoc - VictimRoot;
				VictimReach = FMath::Max(0.f, FVector::DotProduct(Offset, Victim->GetActorForwardVector()));
			}

			// Line between the two sockets (how close the contact points are)
			const float SocketGap = FVector::Dist(AttackerSocketLoc, VictimSocketLoc);
			DrawDebugLine(World, AttackerSocketLoc, VictimSocketLoc,
				SocketGap < 30.f ? FColor::Green : FColor::Red, false, Duration, 0, 1.0f);

			// Summary label above centerline midpoint
			const FVector MidPoint = (AttackerRoot + VictimRoot) * 0.5f + FVector(0, 0, LabelOffsetZ);
			DrawDebugString(World, MidPoint,
				FString::Printf(TEXT("[%d] AtkFwd:%.0f VicFwd:%.0f R2R:%.0f Gap:%.0f SockDist:%.0f"),
					i, AttackerReach, VictimReach, RootToRoot,
					RootToRoot - AttackerReach - VictimReach, SocketGap),
				nullptr, FColor::White, Duration, false, 0.6f);

			LabelOffsetZ += 18.f;
		}

		// --- Actor forward arrows (thin, short) ---
		DrawDebugDirectionalArrow(World, AttackerRoot, AttackerRoot + Attacker->GetActorForwardVector() * 80.f,
			8.f, FColor::White, false, Duration, 0, 1.0f);
		DrawDebugDirectionalArrow(World, VictimRoot, VictimRoot + Victim->GetActorForwardVector() * 80.f,
			8.f, FColor::White, false, Duration, 0, 1.0f);
	}
}

// ---------------------------------------------------------------------------
// DrawMantleDebugOverlay — continuous mantle trace visualization
// CVar: os.debug.MantleViz
// Color legend:
//   CYAN LINE       = Forward trace (sphere sweep)
//   YELLOW LINE     = Downward trace (from above)
//   GREEN CAPSULE   = Clearance check passed
//   RED CAPSULE     = Clearance check blocked
//   ORANGE LINE     = Landing floor verification trace
//   MAGENTA SPHERE  = Edge MW warp target
//   GREEN SPHERE    = Landing MW warp target
//   WHITE DASHED    = Height reference lines (knee/waist/max)
//   LIGHT BLUE ARROW = Blended trace direction
//   YELLOW ARROW    = Velocity direction
//   WHITE TEXT      = Status + metrics
// ---------------------------------------------------------------------------

void AOSHUD::DrawMantleDebugOverlay()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	APawn* LocalPawn = PC->GetPawn();
	AOSCharacter* Character = Cast<AOSCharacter>(LocalPawn);
	if (!Character) return;

	UAbilitySystemComponent* ASC = AbilityHelper::GetOSASCFromActor(Character);
	if (!ASC) return;

	constexpr float Duration = 0.f; // single frame

	// Find the mantle ability instance from granted abilities
	UGA_OSmantleability* MantleAbility = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		MantleAbility = Cast<UGA_OSmantleability>(Spec.GetPrimaryInstance());
		if (MantleAbility) break;
	}

	if (!MantleAbility) return;

	// Run trace and get debug data
	const FOSMantleDebugData D = MantleAbility->DebugRunMantleTrace();

	// Shorthand refs to geometry profiles
	const FOSCharacterGeo& CG = D.CharGeo;
	const FOSObstacleGeo& OG = D.ObstGeo;

	// --- Height reference lines at character position ---
	// These show the valid mantle range as horizontal lines extending from the character forward.
	// Dashed white lines at knee height, waist height (montage switch), and max mantle height.
	if (CG.FullHeight > 0.f)
	{
		const FVector RefBase = CG.Feet;
		const FVector RefDir = CG.Forward;
		constexpr float RefLen = 80.f;
		const FColor RefColor(180, 180, 180, 200);
		const FColor WaistColor(200, 200, 100, 200);

		// Knee height — minimum mantleable
		const FVector KneeStart = RefBase + FVector(0, 0, D.DerivedMinHeight);
		DrawDebugLine(World, KneeStart, KneeStart + RefDir * RefLen,
			RefColor, false, Duration, 0, 1.0f);
		DrawDebugString(World, KneeStart + RefDir * (RefLen + 5.f),
			FString::Printf(TEXT("Knee %.0f"), D.DerivedMinHeight),
			nullptr, RefColor, Duration, false, 0.5f);

		// Waist height — low/high montage switch threshold
		const FVector WaistStart = RefBase + FVector(0, 0, D.DerivedHighThreshold);
		DrawDebugLine(World, WaistStart, WaistStart + RefDir * RefLen,
			WaistColor, false, Duration, 0, 1.0f);
		DrawDebugString(World, WaistStart + RefDir * (RefLen + 5.f),
			FString::Printf(TEXT("Waist %.0f"), D.DerivedHighThreshold),
			nullptr, WaistColor, Duration, false, 0.5f);

		// Jump clearance height — climb/jump boundary (blue dashed)
		if (D.DerivedJumpClearanceHeight > 0.f)
		{
			const FColor JumpColor(80, 160, 255, 200);
			const FVector JumpStart = RefBase + FVector(0, 0, D.DerivedJumpClearanceHeight);
			constexpr int32 JumpDashes = 6;
			for (int32 i = 0; i < JumpDashes; i += 2)
			{
				const FVector P0 = JumpStart + RefDir * (RefLen * (float)i / JumpDashes);
				const FVector P1 = JumpStart + RefDir * (RefLen * (float)(i + 1) / JumpDashes);
				DrawDebugLine(World, P0, P1, JumpColor, false, Duration, 0, 1.0f);
			}
			DrawDebugString(World, JumpStart + RefDir * (RefLen + 5.f),
				FString::Printf(TEXT("JumpClear %.0f"), D.DerivedJumpClearanceHeight),
				nullptr, JumpColor, Duration, false, 0.5f);
		}

		// Max mantle height -- gameplay cap (red dashed)
		if (D.MaxMantleHeightUsed > 0.f)
		{
			const FColor MaxColor(255, 80, 80, 200);
			const FVector MaxStart = RefBase + FVector(0, 0, D.MaxMantleHeightUsed);
			constexpr int32 Dashes = 6;
			for (int32 i = 0; i < Dashes; i += 2)
			{
				const FVector P0 = MaxStart + RefDir * (RefLen * (float)i / Dashes);
				const FVector P1 = MaxStart + RefDir * (RefLen * (float)(i + 1) / Dashes);
				DrawDebugLine(World, P0, P1, MaxColor, false, Duration, 0, 1.0f);
			}
			DrawDebugString(World, MaxStart + RefDir * (RefLen + 5.f),
				FString::Printf(TEXT("Max %.0f"), D.MaxMantleHeightUsed),
				nullptr, MaxColor, Duration, false, 0.5f);
		}
	}

	// --- Gravity prediction indicator (white dashed line from actual to predicted) ---
	if (D.bUsingPrediction)
	{
		DrawDebugPoint(World, D.ActualLocation, 6.f, FColor::White, false, Duration);
		DrawDebugPoint(World, D.PredictedLocation, 8.f, FColor(200, 200, 255), false, Duration);
		// Dashed line: actual -> predicted
		const FVector Delta = D.PredictedLocation - D.ActualLocation;
		constexpr int32 DashCount = 6;
		for (int32 i = 0; i < DashCount; i += 2)
		{
			const FVector P0 = D.ActualLocation + Delta * ((float)i / DashCount);
			const FVector P1 = D.ActualLocation + Delta * ((float)(i + 1) / DashCount);
			DrawDebugLine(World, P0, P1, FColor(200, 200, 255), false, Duration, 0, 1.0f);
		}
		DrawDebugString(World, D.PredictedLocation + FVector(0, 0, 15.f),
			TEXT("[PREDICTED]"), nullptr, FColor(200, 200, 255), Duration, false, 0.6f);
	}

	// --- Blended trace direction (light blue arrow — always shown) ---
	if (!D.TraceDirection.IsZero() && !D.ForwardStart.IsZero())
	{
		const FVector TraceArrowStart = D.ForwardStart;
		DrawDebugDirectionalArrow(World, TraceArrowStart, TraceArrowStart + D.TraceDirection * 70.f,
			8.f, FColor(100, 180, 255), false, Duration, 0, 1.5f);
		DrawDebugString(World, TraceArrowStart + D.TraceDirection * 70.f + FVector(0, 0, 8.f),
			TEXT("[TRACE DIR]"), nullptr, FColor(100, 180, 255), Duration, false, 0.5f);
	}

	// --- Velocity direction (yellow arrow — shown when blend is active) ---
	if (D.VelocityBlendUsed > 0.f && !D.VelocityDirection.IsZero() && !D.ForwardStart.IsZero())
	{
		const FVector VelArrowStart = D.ForwardStart + FVector(0, 0, 10.f);
		DrawDebugDirectionalArrow(World, VelArrowStart, VelArrowStart + D.VelocityDirection * 50.f,
			6.f, FColor::Yellow, false, Duration, 0, 1.0f);
	}

	// --- Forward trace (cyan) ---
	if (!D.ForwardStart.IsZero() || !D.ForwardEnd.IsZero())
	{
		DrawDebugLine(World, D.ForwardStart, D.ForwardEnd,
			D.bForwardHit ? FColor::Cyan : FColor(80, 80, 80), false, Duration, 0, 2.0f);

		if (D.bForwardHit)
		{
			DrawDebugPoint(World, D.ForwardHitPoint, 10.f, FColor::Cyan, false, Duration);
			// Wall normal arrow
			DrawDebugDirectionalArrow(World, D.ForwardHitPoint,
				D.ForwardHitPoint + D.ForwardHitNormal * 40.f,
				8.f, FColor::Cyan, false, Duration, 0, 1.5f);
			// Face angle label
			DrawDebugString(World, D.ForwardHitPoint + FVector(0, 0, 15.f),
				FString::Printf(TEXT("Wall %.0f\xB0"), D.ForwardHitAngle),
				nullptr, FColor::Cyan, Duration, false, 0.7f);
		}
	}

	// --- Downward trace (yellow) ---
	if (D.bForwardHit && (!D.DownStart.IsZero() || !D.DownEnd.IsZero()))
	{
		DrawDebugLine(World, D.DownStart, D.DownEnd,
			D.bDownHit ? FColor::Yellow : FColor(80, 80, 0), false, Duration, 0, 2.0f);

		// Show sphere sweep radius at trace endpoints (down trace uses sphere sweep, not line)
		if (D.DownSweepRadius > 0.f)
		{
			DrawDebugSphere(World, D.DownStart, D.DownSweepRadius, 8,
				FColor(255, 255, 0, 80), false, Duration, 0, 0.5f);
		}

		if (D.bDownHit)
		{
			// Show sphere at hit point to visualize sweep contact
			if (D.DownSweepRadius > 0.f)
			{
				DrawDebugSphere(World, D.DownHitPoint, D.DownSweepRadius, 8,
					FColor::Yellow, false, Duration, 0, 1.0f);
			}
			else
			{
				DrawDebugPoint(World, D.DownHitPoint, 10.f, FColor::Yellow, false, Duration);
			}
			// Surface normal arrow
			DrawDebugDirectionalArrow(World, D.DownHitPoint,
				D.DownHitPoint + D.DownHitNormal * 30.f,
				6.f, FColor::Yellow, false, Duration, 0, 1.0f);

			// Height line from feet to ledge (shows measured height visually)
			const FVector HeightBase = FVector(D.DownHitPoint.X, D.DownHitPoint.Y, CG.Feet.Z);
			DrawDebugLine(World, HeightBase, D.DownHitPoint,
				FColor(255, 255, 0, 140), false, Duration, 0, 1.0f);
			DrawDebugString(World, (HeightBase + D.DownHitPoint) * 0.5f + FVector(15, 0, 0),
				FString::Printf(TEXT("%.0f cm"), D.MantleHeight),
				nullptr, FColor::Yellow, Duration, false, 0.6f);
		}
	}

	// --- Ledge edge samples (green dots = passed, red dots = failed) ---
	if (D.LedgeSamplesTotal > 1)
	{
		for (int32 i = 0; i < D.LedgeSamplePoints.Num(); ++i)
		{
			const bool bPassed = D.LedgeSampleResults.IsValidIndex(i) && D.LedgeSampleResults[i];
			const FColor SampleColor = bPassed ? FColor::Green : FColor::Red;
			DrawDebugPoint(World, D.LedgeSamplePoints[i], 8.f, SampleColor, false, Duration);
			// Small cross on failed samples
			if (!bPassed)
			{
				constexpr float Sz = 6.f;
				DrawDebugLine(World, D.LedgeSamplePoints[i] + FVector(-Sz, 0, 0), D.LedgeSamplePoints[i] + FVector(Sz, 0, 0),
					FColor::Red, false, Duration, 0, 2.0f);
				DrawDebugLine(World, D.LedgeSamplePoints[i] + FVector(0, -Sz, 0), D.LedgeSamplePoints[i] + FVector(0, Sz, 0),
					FColor::Red, false, Duration, 0, 2.0f);
			}
		}

		// Sample summary label
		if (D.bDownHit)
		{
			DrawDebugString(World, D.DownHitPoint + FVector(0, 0, -15.f),
				FString::Printf(TEXT("Ledge: %d/%d"), D.LedgeSamplesPassed, D.LedgeSamplesTotal),
				nullptr, D.LedgeSamplesPassed >= FMath::CeilToInt(D.LedgeSamplesTotal * 0.5f) ? FColor::Green : FColor::Red,
				Duration, false, 0.7f);
		}
	}

	// --- Vertical wall profile (multi-height traces) ---
	if (D.bProfileRan && D.ProfileSlices.Num() > 0)
	{
		for (int32 i = 0; i < D.ProfileSlices.Num(); ++i)
		{
			const auto& Slice = D.ProfileSlices[i];
			const float SliceZ = CG.Feet.Z + Slice.Height;
			const FVector SliceStart = FVector(CG.Center.X, CG.Center.Y, SliceZ);

			if (Slice.bHit)
			{
				FColor SliceColor;
				if (i == D.ProfileWallTopIndex)
					SliceColor = FColor::Green;  // wall top
				else if (D.bProfileInsetDetected
					&& D.ProfileWallTopIndex != INDEX_NONE
					&& i < D.ProfileWallTopIndex
					&& Slice.Distance > D.ProfileSlices[D.ProfileWallTopIndex].Distance + 5.f)
					SliceColor = FColor(255, 165, 0);  // protruding base = orange
				else
					SliceColor = FColor::Cyan;  // normal wall face

				DrawDebugLine(World, SliceStart, Slice.HitPoint, SliceColor, false, Duration, 0, 1.f);
				DrawDebugPoint(World, Slice.HitPoint, 6.f, SliceColor, false, Duration);

				// Per-slice distance label (helps spot insets visually)
				DrawDebugString(World, Slice.HitPoint + FVector(5, 0, 3.f),
					FString::Printf(TEXT("%.0f"), Slice.Distance),
					nullptr, SliceColor, Duration, false, 0.4f);
			}
			else
			{
				// Miss: dim gray line in the profile trace direction
				const FVector MissEnd = SliceStart + D.ProfileTraceDirection * 150.f;
				DrawDebugLine(World, SliceStart, MissEnd, FColor(60, 60, 60), false, Duration, 0, 0.5f);
			}
		}

		// Consensus normal arrow at wall top
		if (D.ProfileWallTopIndex != INDEX_NONE && D.ProfileWallTopIndex < D.ProfileSlices.Num())
		{
			const auto& TopSlice = D.ProfileSlices[D.ProfileWallTopIndex];
			if (TopSlice.bHit)
			{
				DrawDebugDirectionalArrow(World,
					TopSlice.HitPoint,
					TopSlice.HitPoint + D.ProfileConsensusNormal * 40.f,
					10.f, FColor::Green, false, Duration, 0, 2.f);
			}
		}
	}

	// --- Capsule clearance check ---
	if (D.bDownHit && D.CapsuleRadius > 0.f)
	{
		const FColor CapsuleColor = D.bCapsuleBlocked ? FColor::Red : FColor::Green;
		DrawDebugCapsule(World, D.CapsuleCheckCenter, D.CapsuleHalfHeight, D.CapsuleRadius,
			FQuat::Identity, CapsuleColor, false, Duration, 0, 1.5f);

		// Show pull-over distance label on the capsule
		if (!D.bCapsuleBlocked && OG.bLedgeFound)
		{
			// Line from ledge edge to capsule center (horizontal) shows pull-over distance
			const FVector LedgeEdgeAtCapsuleZ = FVector(OG.LedgePoint.X, OG.LedgePoint.Y, D.CapsuleCheckCenter.Z);
			const float PullOverDist = FVector::Dist2D(LedgeEdgeAtCapsuleZ, D.CapsuleCheckCenter);
			DrawDebugLine(World, LedgeEdgeAtCapsuleZ, D.CapsuleCheckCenter,
				FColor(0, 200, 0, 160), false, Duration, 0, 1.0f);
			DrawDebugString(World, D.CapsuleCheckCenter + FVector(0, 0, D.CapsuleHalfHeight + 8.f),
				FString::Printf(TEXT("Pull: %.0f cm"), PullOverDist),
				nullptr, FColor::Green, Duration, false, 0.6f);
		}
	}

	// --- Approach path overhead (detects overhangs blocking climb from below) ---
	if (D.bApproachOverheadCheckRan)
	{
		const FColor ApproachOHColor = D.bApproachOverheadBlocked ? FColor::Red : FColor(100, 180, 100);
		DrawDebugLine(World, D.ApproachOverheadStart,
			D.bApproachOverheadBlocked ? D.ApproachOverheadHitPoint : D.ApproachOverheadEnd,
			ApproachOHColor, false, Duration, 0, 1.5f);
		if (D.bApproachOverheadBlocked)
		{
			DrawDebugPoint(World, D.ApproachOverheadHitPoint, 10.f, FColor::Red, false, Duration);
			DrawDebugString(World, D.ApproachOverheadHitPoint + FVector(15, 0, 0),
				TEXT("OVERHANG"), nullptr, FColor::Red, Duration, false, 0.7f);
		}
	}

	// --- Overhead clearance: edge position (where character passes during climb) ---
	if (D.bEdgeOverheadCheckRan)
	{
		const FColor EdgeOHColor = D.bEdgeOverheadBlocked ? FColor::Red : FColor(80, 200, 80);
		DrawDebugLine(World, D.EdgeOverheadStart,
			D.bEdgeOverheadBlocked ? D.EdgeOverheadHitPoint : D.EdgeOverheadEnd,
			EdgeOHColor, false, Duration, 0, 1.5f);
		if (D.bEdgeOverheadBlocked)
		{
			DrawDebugPoint(World, D.EdgeOverheadHitPoint, 10.f, FColor::Red, false, Duration);
			DrawDebugString(World, D.EdgeOverheadHitPoint + FVector(15, 0, 0),
				FString::Printf(TEXT("CEILING@EDGE %.0f cm"), D.EdgeOverheadClearance),
				nullptr, FColor::Red, Duration, false, 0.7f);
		}
	}

	// --- Overhead clearance: landing position (where character ends up) ---
	if (D.bOverheadCheckRan)
	{
		const FColor OverheadColor = D.bOverheadBlocked ? FColor::Red : FColor(80, 200, 80);
		DrawDebugLine(World, D.OverheadTraceStart, D.bOverheadBlocked ? D.OverheadHitPoint : D.OverheadTraceEnd,
			OverheadColor, false, Duration, 0, 1.5f);
		if (D.bOverheadBlocked)
		{
			DrawDebugPoint(World, D.OverheadHitPoint, 10.f, FColor::Red, false, Duration);
			DrawDebugString(World, D.OverheadHitPoint + FVector(15, 0, 0),
				FString::Printf(TEXT("CEILING@LAND %.0f cm"), D.OverheadClearance),
				nullptr, FColor::Red, Duration, false, 0.7f);
		}
	}

	// --- Landing floor verification (orange) ---
	if (D.bFloorCheckRan)
	{
		const FColor FloorColor = D.bFloorWalkable ? FColor::Orange : FColor::Red;
		DrawDebugLine(World, D.FloorTraceStart, D.FloorTraceEnd,
			D.bFloorHit ? FloorColor : FColor(80, 40, 0), false, Duration, 0, 1.5f);
		if (D.bFloorHit)
		{
			DrawDebugPoint(World, D.FloorHitPoint, 8.f, FloorColor, false, Duration);
			// Floor normal + walkability
			DrawDebugDirectionalArrow(World, D.FloorHitPoint,
				D.FloorHitPoint + D.FloorHitNormal * 25.f,
				5.f, FloorColor, false, Duration, 0, 1.0f);
			DrawDebugString(World, D.FloorHitPoint + FVector(0, 0, 10.f),
				FString::Printf(TEXT("Floor %.0f\xB0 %s"), D.FloorAngle, D.bFloorWalkable ? TEXT("OK") : TEXT("STEEP")),
				nullptr, FloorColor, Duration, false, 0.7f);
		}
	}

	// --- Rejection marker (red X + reason at failure point) ---
	if (!D.bSuccess && !D.RejectionReason.IsEmpty())
	{
		// Place the rejection label at the most advanced trace point we reached.
		FVector RejectionPoint = Character->GetActorLocation() + FVector(0, 0, 50.f);
		if (D.bDownHit)
		{
			RejectionPoint = D.DownHitPoint + FVector(0, 0, 30.f);
		}
		else if (D.bForwardHit)
		{
			RejectionPoint = D.ForwardHitPoint + FVector(0, 0, 30.f);
		}

		// Red X cross at rejection point
		constexpr float XSize = 15.f;
		DrawDebugLine(World, RejectionPoint + FVector(-XSize, 0, -XSize), RejectionPoint + FVector(XSize, 0, XSize),
			FColor::Red, false, Duration, 0, 3.0f);
		DrawDebugLine(World, RejectionPoint + FVector(XSize, 0, -XSize), RejectionPoint + FVector(-XSize, 0, XSize),
			FColor::Red, false, Duration, 0, 3.0f);
		DrawDebugLine(World, RejectionPoint + FVector(0, -XSize, -XSize), RejectionPoint + FVector(0, XSize, XSize),
			FColor::Red, false, Duration, 0, 3.0f);
		DrawDebugLine(World, RejectionPoint + FVector(0, XSize, -XSize), RejectionPoint + FVector(0, -XSize, XSize),
			FColor::Red, false, Duration, 0, 3.0f);

		DrawDebugString(World, RejectionPoint + FVector(0, 0, 20.f),
			FString::Printf(TEXT("X %s"), *D.RejectionReason),
			nullptr, FColor::Red, Duration, false, 1.0f);
	}

	// --- Edge warp target (magenta — phase 1: climb to wall top) ---
	if (D.bHasEdgeWarp)
	{
		DrawDebugSphere(World, D.EdgeWarpLocation, 12.f, 8, FColor::Magenta, false, Duration, 0, 2.0f);
		// Rotation arrow shows which way the character will face at the edge
		const FVector EdgeFwd = D.EdgeWarpRotation.Vector();
		DrawDebugDirectionalArrow(World, D.EdgeWarpLocation,
			D.EdgeWarpLocation + EdgeFwd * 35.f,
			6.f, FColor::Magenta, false, Duration, 0, 1.5f);
		DrawDebugString(World, D.EdgeWarpLocation + FVector(0, 0, 18.f),
			FString::Printf(TEXT("[EDGE] delta:%.0f cm  rot:%.0f\xB0"),
				D.EdgeWarpDelta, D.RotationDelta),
			nullptr, FColor::Magenta, Duration, false, 0.7f);

		// Delta line from character feet to edge target
		DrawDebugLine(World, CG.Feet, D.EdgeWarpLocation,
			FColor(200, 0, 200, 120), false, Duration, 0, 1.0f);
	}

	// --- Landing warp target (green --- phase 2: pull-over to landing) ---
	if (D.bHasLandingWarp)
	{
		DrawDebugSphere(World, D.LandingWarpLocation, 15.f, 8, FColor(0, 255, 128), false, Duration, 0, 2.0f);
		const FVector LandFwd = D.LandingWarpRotation.Vector();
		DrawDebugDirectionalArrow(World, D.LandingWarpLocation,
			D.LandingWarpLocation + LandFwd * 50.f,
			10.f, FColor(0, 255, 128), false, Duration, 0, 2.0f);
		DrawDebugString(World, D.LandingWarpLocation + FVector(0, 0, 25.f),
			FString::Printf(TEXT("[LANDING] delta:%.0f cm"),
				D.LandingWarpDelta),
			nullptr, FColor(0, 255, 128), Duration, false, 0.7f);

		// Delta line from edge to landing
		if (D.bHasEdgeWarp)
		{
			DrawDebugLine(World, D.EdgeWarpLocation, D.LandingWarpLocation,
				FColor(0, 200, 100, 120), false, Duration, 0, 1.0f);
		}
	}

	// --- Arc path between edge and landing (white curve, climb only) ---
	if (D.bHasEdgeWarp && D.bHasLandingWarp)
	{
		// Draw a simple arc (quadratic Bezier: edge -> apex -> landing)
		const FVector MidPoint = (D.EdgeWarpLocation + D.LandingWarpLocation) * 0.5f
			+ FVector(0, 0, 30.f); // apex lifted above midpoint
		constexpr int32 ArcSegments = 8;
		for (int32 i = 0; i < ArcSegments; ++i)
		{
			const float T0 = (float)i / ArcSegments;
			const float T1 = (float)(i + 1) / ArcSegments;
			// Quadratic Bezier: P = (1-t)^2*A + 2(1-t)t*M + t^2*B
			const FVector P0 = (1-T0)*(1-T0)*D.EdgeWarpLocation + 2*(1-T0)*T0*MidPoint + T0*T0*D.LandingWarpLocation;
			const FVector P1 = (1-T1)*(1-T1)*D.EdgeWarpLocation + 2*(1-T1)*T1*MidPoint + T1*T1*D.LandingWarpLocation;
			DrawDebugLine(World, P0, P1, FColor::White, false, Duration, 0, 1.0f);
		}
	}

	// --- Vault detection traces (orange = depth probe, cyan = far-side floor) ---
	if (D.bVaultTraceRan)
	{
		const FColor ProbeColor = D.bVaultFarSideClear ? FColor(255, 165, 0) : FColor(100, 50, 0);
		DrawDebugLine(World, D.VaultTraceStart, D.VaultTraceEnd,
			ProbeColor, false, Duration, 0, 1.5f);

		// Label showing what the depth probe found
		const FVector ProbeMid = (D.VaultTraceStart + D.VaultTraceEnd) * 0.5f;
		if (OG.bDepthProbed)
		{
			if (OG.bFarSideOpen)
			{
				DrawDebugString(World, ProbeMid + FVector(0, 0, 12.f),
					TEXT("Open"), nullptr, FColor(255, 165, 0), Duration, false, 0.5f);
			}
			else
			{
				DrawDebugString(World, ProbeMid + FVector(0, 0, 12.f),
					FString::Printf(TEXT("Depth: %.0f cm"), OG.EstimatedDepth),
					nullptr, FColor(100, 50, 0), Duration, false, 0.5f);
			}
		}

		if (D.bVaultFarSideClear)
		{
			// Far-side floor trace
			const FColor FarFloorColor = D.bVaultFloorFound ? FColor(0, 200, 255) : FColor(0, 60, 80);
			DrawDebugLine(World, D.VaultFloorStart, D.VaultFloorEnd,
				FarFloorColor, false, Duration, 0, 1.5f);

			if (D.bVaultFloorFound)
			{
				DrawDebugPoint(World, D.VaultFloorHitPoint, 10.f, FColor(0, 200, 255), false, Duration);

				// Show drop distance and classification reason
				const bool bIsPlatform = (OG.FarDrop < CG.KneeHeight());
				const FString ClassLabel = bIsPlatform
					? FString::Printf(TEXT("[PLATFORM] Drop: %.0f < Knee: %.0f -> CLIMB"), OG.FarDrop, CG.KneeHeight())
					: FString::Printf(TEXT("[FAR FLOOR] Drop: %.0f cm -> VAULT"), OG.FarDrop);
				const FColor ClassColor = bIsPlatform ? FColor::Green : FColor(0, 200, 255);
				DrawDebugString(World, D.VaultFloorHitPoint + FVector(0, 0, 15.f),
					ClassLabel, nullptr, ClassColor, Duration, false, 0.7f);
			}
		}
	}

	// --- Intent filtering debug (input direction arrow + approach speed) ---
	if (D.bIntentFilterRan && D.bForwardHit)
	{
		// Input direction arrow (blue = aligned, orange = misaligned)
		if (!CG.InputDirection.IsNearlyZero())
		{
			const bool bAligned = D.IntentInputAlignment >= D.MinVaultInputAlignmentUsed;
			const FColor InputColor = bAligned ? FColor(80, 160, 255) : FColor(255, 140, 40);
			const FVector ArrowStart = CG.Feet + FVector(0, 0, 30.f);
			DrawDebugDirectionalArrow(World, ArrowStart,
				ArrowStart + CG.InputDirection * 60.f,
				8.f, InputColor, false, Duration, 0, 2.0f);
			DrawDebugString(World, ArrowStart + CG.InputDirection * 60.f + FVector(0, 0, 10.f),
				FString::Printf(TEXT("Input %.2f"), D.IntentInputAlignment),
				nullptr, InputColor, Duration, false, 0.6f);
		}

		// Approach speed indicator (shown for vault candidates)
		if (D.IntentApproachSpeed != 0.f || D.MinApproachSpeedUsed > 0.f)
		{
			const bool bFastEnough = D.IntentApproachSpeed >= D.MinApproachSpeedUsed;
			const FColor SpeedColor = bFastEnough ? FColor(80, 200, 80) : FColor(255, 80, 80);
			const FVector SpeedPos = CG.Feet + FVector(0, 0, 15.f);
			DrawDebugString(World, SpeedPos,
				FString::Printf(TEXT("Speed %.0f/%.0f"), D.IntentApproachSpeed, D.MinApproachSpeedUsed),
				nullptr, SpeedColor, Duration, false, 0.6f);
		}
	}

	// --- Screen-space HUD text ---
	if (Canvas)
	{
		const UFont* Font = GEngine->GetSmallFont();
		const float TextX = 8.f;
		float TextY = 30.f;
		float TextW, TextH;

		// Helper lambda: draw a line of text with dark background
		auto DrawHUDLine = [&](const FString& Text, const FColor& Color)
		{
			Canvas->StrLen(Font, Text, TextW, TextH);
			Canvas->SetDrawColor(FColor(0, 0, 0, 140));
			Canvas->DrawTile(Canvas->DefaultTexture,
				TextX - 4.f, TextY - 2.f,
				TextW + 8.f, TextH + 4.f,
				0.f, 0.f, 1.f, 1.f);
			Canvas->SetDrawColor(Color);
			Canvas->DrawText(Font, Text, TextX, TextY);
			TextY += TextH + 3.f;
		};

		// Line 1: Status
		{
			FString StatusText;
			FColor StatusColor;
			if (D.bSuccess)
			{
				const TCHAR* TypeStr = TEXT("CLIMB");
				const TCHAR* MontageStr = TEXT("Low");
				switch (D.MantleType)
				{
				case EOSMantleType::Vault:
					TypeStr = TEXT("VAULT");
					MontageStr = TEXT("Vault");
					StatusColor = FColor(0, 200, 255);
					break;
				case EOSMantleType::Climb:
					MontageStr = (D.MantleHeight >= D.DerivedHighThreshold) ? TEXT("ClimbHigh") : TEXT("ClimbLow");
					StatusColor = FColor::Green;
					break;
				case EOSMantleType::Catch:
					TypeStr = TEXT("CATCH");
					{
						const float FeetBelowLedge = OG.LedgePoint.Z - CG.Feet.Z;
						MontageStr = (FeetBelowLedge >= CG.WaistHeight()) ? TEXT("CatchHigh") : TEXT("CatchLow");
					}
					StatusColor = FColor(255, 165, 0);
					break;
				}
				StatusText = FString::Printf(TEXT("[MANTLE] %s READY  Height: %.0f cm  Angle: %.0f\xB0  Montage: %s"),
					TypeStr, D.MantleHeight, D.SurfaceAngle, MontageStr);
			}
			else if (!D.RejectionReason.IsEmpty())
			{
				StatusText = FString::Printf(TEXT("[MANTLE] BLOCKED: %s"), *D.RejectionReason);
				StatusColor = FColor::Red;
			}
			else
			{
				StatusText = TEXT("[MANTLE] Scanning...");
				StatusColor = FColor(128, 128, 128);
			}
			DrawHUDLine(StatusText, StatusColor);
		}

		// Line 2: Character state
		{
			const FString CharText = FString::Printf(
				TEXT("  Char: %s  VelZ: %.0f  Capsule: R%.0f H%.0f  Feet: %.0f,%.0f,%.0f"),
				CG.bAirborne ? TEXT("AIRBORNE") : TEXT("Grounded"),
				CG.VerticalVelocity,
				CG.CapsuleRadius, CG.CapsuleHalfHeight,
				CG.Feet.X, CG.Feet.Y, CG.Feet.Z);
			const FColor CharColor = CG.bAirborne ? FColor(200, 160, 255) : FColor(140, 200, 140);
			DrawHUDLine(CharText, CharColor);
		}

		// Line 3a: Geometry thresholds
		{
			const FString GeoText = FString::Printf(
				TEXT("  Heights: Knee=%.0f  Waist=%.0f  JumpClear=%.0f  Max=%.0f  PullOver=%.0f  DownOff=%.0f"),
				D.DerivedMinHeight, D.DerivedHighThreshold, D.DerivedJumpClearanceHeight,
				D.MaxMantleHeightUsed, D.DerivedIdealPullOver, D.DerivedDownTraceOffset);
			DrawHUDLine(GeoText, FColor(160, 160, 200));
		}

		// Line 3b: Arbitration thresholds
		{
			const FString ArbiText = FString::Printf(
				TEXT("  Arbitration: MaxVDepth=%.0f  MaxVDrop=%.0f  MinApproach=%.0f  MinAlign=%.2f"),
				D.DerivedMaxVaultDepth, D.DerivedMaxVaultDrop,
				D.MinApproachSpeedUsed, D.MinVaultInputAlignmentUsed);
			DrawHUDLine(ArbiText, FColor(160, 160, 200));
		}

		// Line 4: Physics context
		{
			const FString PhysText = FString::Printf(
				TEXT("  VelBlend: %.0f%%  Predict: %s  Samples: %d/%d"),
				D.VelocityBlendUsed * 100.f,
				D.bUsingPrediction ? TEXT("ON") : TEXT("off"),
				D.LedgeSamplesPassed, FMath::Max(D.LedgeSamplesTotal, 1));
			DrawHUDLine(PhysText, FColor(140, 200, 140));
		}

		// Line 5: Wall geometry (if forward trace hit)
		if (D.bForwardHit)
		{
			const FString WallText = FString::Printf(
				TEXT("  Wall: %.0f\xB0  Normal: (%.2f,%.2f,%.2f)  WalkableZ: %.2f (%.0f\xB0)"),
				D.ForwardHitAngle,
				OG.WallNormal.X, OG.WallNormal.Y, OG.WallNormal.Z,
				D.WalkableFloorZ, D.WalkableFloorAngle);
			DrawHUDLine(WallText, FColor(120, 200, 220));
		}

		// Line 5b: Vertical wall profile
		if (D.bProfileRan && D.ProfileSlices.Num() > 0)
		{
			int32 HitCount = 0;
			for (const auto& S : D.ProfileSlices) { if (S.bHit) ++HitCount; }

			const float WallTopCm = (D.ProfileWallTopIndex != INDEX_NONE && D.ProfileWallTopIndex < D.ProfileSlices.Num())
				? D.ProfileSlices[D.ProfileWallTopIndex].Height : 0.f;

			FString ProfileText;
			if (D.bProfileInsetDetected)
			{
				ProfileText = FString::Printf(
					TEXT("  Profile: %d/%d  WallTop=%.0fcm  INSET (%.0fcm)"),
					HitCount, D.ProfileSlices.Num(), WallTopCm, D.ProfileInsetDepth);
			}
			else
			{
				ProfileText = FString::Printf(
					TEXT("  Profile: %d/%d  WallTop=%.0fcm  flat"),
					HitCount, D.ProfileSlices.Num(), WallTopCm);
			}
			DrawHUDLine(ProfileText, D.bProfileInsetDetected ? FColor(255, 165, 0) : FColor::Cyan);
		}

		// Line 6: Obstacle geometry (if ledge found)
		if (D.bForwardHit && D.bDownHit)
		{
			const FString ObsText = FString::Printf(
				TEXT("  Ledge: H=%.1f  Surf=%.1f\xB0  Floor: %s(%s)  Capsule: %s(%d/%d)"),
				D.MantleHeight, D.SurfaceAngle,
				D.bFloorHit ? TEXT("YES") : TEXT("NO"),
				D.bFloorWalkable ? TEXT("walk") : TEXT("steep"),
				D.bCapsuleBlocked ? TEXT("BLOCKED") : TEXT("CLEAR"),
				D.ClearanceOverlapTotal - D.ClearanceWalkableFiltered, D.ClearanceOverlapTotal);
			DrawHUDLine(ObsText, FColor(180, 180, 180));
		}

		// Line 7: Classification detail
		{
			FString ClassText;
			FColor ClassColor;

			if (D.MantleType == EOSMantleType::Catch)
			{
				// Catch: show airborne state, falling velocity, and feet-below-ledge distance
				const float FeetBelowLedge = OG.LedgePoint.Z - CG.Feet.Z;
				ClassText = FString::Printf(
					TEXT("  Classify: CATCH  Airborne=%s  VelZ=%.0f  FeetBelowLedge=%.0f  Waist=%.0f -> %s"),
					CG.bAirborne ? TEXT("YES") : TEXT("no"),
					CG.VerticalVelocity,
					FeetBelowLedge, CG.WaistHeight(),
					(FeetBelowLedge >= CG.WaistHeight()) ? TEXT("CatchHigh") : TEXT("CatchLow"));
				ClassColor = FColor(255, 165, 0);
			}
			else if (OG.bDepthProbed)
			{
				if (OG.bFarSideOpen)
				{
					if (OG.bFarGroundFound)
					{
						const TCHAR* ClassResult;
						if (!OG.bFarGroundWalkable)
							ClassResult = TEXT("CLIMB (far floor not walkable)");
						else if (OG.FarDrop < 0.f)
							ClassResult = TEXT("CLIMB (far ground above feet)");
						else if (OG.FarDrop <= CG.KneeHeight())
							ClassResult = TEXT("CLIMB (platform edge)");
						else
							ClassResult = TEXT("VAULT");

						ClassText = FString::Printf(
							TEXT("  Classify: FarSide=OPEN  FarDrop=%.0f  Knee=%.0f  %s -> %s"),
							OG.FarDrop, CG.KneeHeight(),
							OG.bFarGroundWalkable ? TEXT("Walkable") : TEXT("NotWalkable"),
							ClassResult);
					}
					else
					{
						ClassText = TEXT("  Classify: FarSide=OPEN  No far ground -> VAULT (gravity landing)");
					}
				}
				else
				{
					ClassText = FString::Printf(
						TEXT("  Classify: FarSide=BLOCKED  Depth=%.0f cm -> CLIMB"),
						OG.EstimatedDepth);
				}
				ClassColor = (D.MantleType == EOSMantleType::Vault) ? FColor(0, 200, 255) : FColor(0, 220, 100);
			}

			if (!ClassText.IsEmpty())
			{
				DrawHUDLine(ClassText, ClassColor);
			}
		}

		// Line 7b: Intent filtering (vault gates / climb deferral)
		if (D.bIntentFilterRan)
		{
			FString IntentText;
			FColor IntentColor;
			if (D.MantleType == EOSMantleType::Vault || (!D.bSuccess && D.RejectionReason.Contains(TEXT("Vault"))))
			{
				const bool bAlignOk = D.IntentInputAlignment >= D.MinVaultInputAlignmentUsed;
				const bool bSpeedOk = D.IntentApproachSpeed >= D.MinApproachSpeedUsed;
				IntentText = FString::Printf(
					TEXT("  Intent: Align=%.2f/%s  Speed=%.0f/%s"),
					D.IntentInputAlignment, bAlignOk ? TEXT("OK") : TEXT("FAIL"),
					D.IntentApproachSpeed, bSpeedOk ? TEXT("OK") : TEXT("FAIL"));
				IntentColor = (bAlignOk && bSpeedOk) ? FColor(80, 200, 80) : FColor(255, 140, 40);
			}
			else if (D.IntentClearanceHeight > 0.f)
			{
				const bool bAbove = D.MantleHeight > D.IntentClearanceHeight;
				IntentText = FString::Printf(
					TEXT("  Intent: Height=%.0f vs JumpClear=%.0f -> %s"),
					D.MantleHeight, D.IntentClearanceHeight,
					bAbove ? TEXT("MANTLE") : TEXT("JUMP"));
				IntentColor = bAbove ? FColor(80, 200, 80) : FColor(80, 160, 255);
			}
			if (!IntentText.IsEmpty())
			{
				DrawHUDLine(IntentText, IntentColor);
			}
		}

		// Line 8: Warp analysis (if success)
		if (D.bSuccess && D.bHasEdgeWarp)
		{
			FString WarpText = FString::Printf(
				TEXT("  Warp: EdgeDelta=%.0f cm  Rot=%.0f\xB0"),
				D.EdgeWarpDelta, D.RotationDelta);
			if (D.bHasLandingWarp)
			{
				WarpText += FString::Printf(TEXT("  LandDelta=%.0f cm"), D.LandingWarpDelta);
			}
			if (D.bEdgeOverheadCheckRan && D.bEdgeOverheadBlocked)
			{
				WarpText += FString::Printf(TEXT("  CEILING@EDGE=%.0f cm"), D.EdgeOverheadClearance);
			}
			else if (D.bOverheadCheckRan && D.bOverheadBlocked)
			{
				WarpText += FString::Printf(TEXT("  CEILING@LAND=%.0f cm"), D.OverheadClearance);
			}
			DrawHUDLine(WarpText, FColor(200, 160, 255));
		}

		// Line 9: Blocker details (if capsule blocked)
		if (!D.BlockerDetails.IsEmpty())
		{
			const FString BlockText = FString::Printf(TEXT("  Blockers: %s"), *D.BlockerDetails);
			DrawHUDLine(BlockText, FColor(255, 100, 100));
		}
	}
}

// ---------------------------------------------------------------------------
// DrawGrabDiagnostics — per-grabbing/grabbed character, stacked top-right.
// Gated on OnSight.Grab.DiagLogging >= 2 (call site in DrawHUD).
// ---------------------------------------------------------------------------

void AOSHUD::DrawGrabDiagnostics()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas) return;

	const FOSGameplayTags& GTags = FOSGameplayTags::Get();

	float DrawY = 20.f;
	const float AnchorX = Canvas->SizeX - 520.f;

	for (TActorIterator<AOSCharacter> It(World); It; ++It)
	{
		AOSCharacter* Char = *It;
		if (!IsValid(Char)) continue;

		UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent();
		if (!ASC) continue;

		const bool bIsGrabbing     = ASC->HasMatchingGameplayTag(GTags.IsGrabbing);
		const bool bIsGrabbed      = ASC->HasMatchingGameplayTag(GTags.IsGrabbed);
		const bool bIsBeingGrabbed = ASC->HasMatchingGameplayTag(GTags.IsBeingGrabbed);
		if (!bIsGrabbing && !bIsGrabbed && !bIsBeingGrabbed) continue;

		const FVector LocalPos  = Char->GetActorLocation();
		// GetReplicatedMovement().Location is the last replicated transform. On autonomous
		// proxies (this client's own pawn) the client is the authority, so the replicated-
		// movement buffer is not populated for it — would report (0,0,0) and a nonsense
		// delta. Skip the server/delta line on autonomous proxies.
		const bool bIsAutonomous = (Char->GetLocalRole() == ROLE_AutonomousProxy);
		const FVector ServerPos = bIsAutonomous ? LocalPos : Char->GetReplicatedMovement().Location;
		const float DeltaCm = bIsAutonomous ? 0.f : static_cast<float>(FVector::Dist(LocalPos, ServerPos));

		const FString RoleStr =
			(Char->GetLocalRole() == ROLE_Authority)       ? TEXT("Server") :
			bIsAutonomous                                   ? TEXT("AutonomousProxy") :
			                                                 TEXT("SimulatedProxy");

		const FString Line1 = FString::Printf(TEXT("GRAB: %s %s"), *RoleStr, *Char->GetName());
		const FString Line2 = bIsAutonomous
			? FString::Printf(TEXT("  local=(%.0f %.0f %.0f)  [local authority — no replicated delta]"),
				LocalPos.X, LocalPos.Y, LocalPos.Z)
			: FString::Printf(TEXT("  local=(%.0f %.0f %.0f)  server=(%.0f %.0f %.0f)  delta=%.1fcm"),
				LocalPos.X, LocalPos.Y, LocalPos.Z, ServerPos.X, ServerPos.Y, ServerPos.Z, DeltaCm);
		const FString Line3 = FString::Printf(
			TEXT("  NetFreq=%.1f NetPrio=%.2f  IsGrabbing=%d IsGrabbed=%d IsBeingGrabbed=%d"),
			Char->GetNetUpdateFrequency(), Char->NetPriority,
			bIsGrabbing ? 1 : 0, bIsGrabbed ? 1 : 0, bIsBeingGrabbed ? 1 : 0);
		// Phase 1: replicated-warp arrival state. Shows whether OnRep has landed the warp
		// target for this character as of last check. Useful to see if the owning client
		// got the server-computed warp before the montage progressed into the MW window.
		const FString Line4 = FString::Printf(
			TEXT("  rep: atkWarp=%s  vicWarp=%s"),
			Char->HasReplicatedGrabAttackerWarp() ? TEXT("yes") : TEXT("no"),
			Char->HasReplicatedGrabVictimWarp()   ? TEXT("yes") : TEXT("no"));

		UFont* Font = GEngine ? GEngine->GetTinyFont() : nullptr;
		Canvas->DrawText(Font, Line1, AnchorX, DrawY);
		Canvas->DrawText(Font, Line2, AnchorX, DrawY + 14.f);
		Canvas->DrawText(Font, Line3, AnchorX, DrawY + 28.f);
		Canvas->DrawText(Font, Line4, AnchorX, DrawY + 42.f);
		DrawY += 62.f;
	}
}

#endif // !UE_BUILD_SHIPPING
