#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BNGameplayCues.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"

/**
 * "WHOSE EFFECT IS THIS" — the half of the team tint that a spec can actually reach.
 *
 * THE BUG THIS PINS (28 Aug). The grenade blast cue is handled on the PROJECTILE, and the
 * tint resolver began by casting its target to a pawn. A projectile is not a pawn, so that
 * cast returned null on every blast, no colour was ever resolved, and the explosion drew
 * neutral — for weeks, while a comment in the same file said this was "the one cue where it
 * currently draws". Nothing warned, because a tint that resolves to "no answer" is
 * indistinguishable from FFA.
 *
 * The audit that found it could only prove the ASSET declares a colour parameter. That is a
 * different claim from "a colour reaches it", and the gap between those two claims is exactly
 * where this bug lived. These rows close it.
 *
 * The viewer half — ally-vs-threat — needs a local player controller and cannot run in a spec
 * world, which is why this file tests ownership rather than colour.
 */
BEGIN_DEFINE_SPEC(FBNEffectOwnerSpec, "BreachpointNext.Sim.EffectOwner",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UWorld* World = nullptr;

	// BNTeamsSpec's BuildWorld/DestroyWorld verbatim — the same machinery, not a variant.
	bool BuildWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			AddError(TEXT("UWorld::CreateWorld returned null; no actor can be spawned without a world."));
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		return true;
	}

	void DestroyWorld()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

END_DEFINE_SPEC(FBNEffectOwnerSpec)

void FBNEffectOwnerSpec::Define()
{
	AfterEach([this]() { DestroyWorld(); });

	It("finds the THROWER behind an effect handled on a projectile — the blast's lost colour", [this]()
	{
		if (!BuildWorld())
		{
			return;
		}

		APawn* Thrower = World->SpawnActor<ADefaultPawn>();
		AActor* Projectile = World->SpawnActor<AActor>();
		TestNotNull(TEXT("the spec spawned a thrower"), Thrower);
		TestNotNull(TEXT("the spec spawned a projectile"), Projectile);
		Projectile->SetInstigator(Thrower);

		// THE ROW THAT WOULD HAVE CAUGHT IT. Before the fix this resolved to null, and a null
		// owner means no PlayerState, which means no colour, which means a neutral explosion.
		TestEqual(TEXT("a non-pawn effect belongs to its instigator"),
			UBNGameplayCue_Base::ResolveEffectOwner(Projectile), (const APawn*)Thrower);
	});

	It("still prefers the target itself when the target IS a pawn", [this]()
	{
		if (!BuildWorld())
		{
			return;
		}

		APawn* Shooter = World->SpawnActor<ADefaultPawn>();
		APawn* Bystander = World->SpawnActor<ADefaultPawn>();
		Shooter->SetInstigator(Bystander); // deliberately misleading

		// The fallback must not become an override: a muzzle flash on a pawn is that PAWN's,
		// whatever its instigator field happens to say. Getting this backwards would tint every
		// weapon effect by the wrong player the moment an FX asset declared a colour.
		TestEqual(TEXT("a pawn target is its own owner"),
			UBNGameplayCue_Base::ResolveEffectOwner(Shooter), (const APawn*)Shooter);
	});

	It("answers null for an ownerless actor rather than guessing", [this]()
	{
		if (!BuildWorld())
		{
			return;
		}

		AActor* Orphan = World->SpawnActor<AActor>();
		TestNull(TEXT("no pawn, no instigator, no answer"),
			UBNGameplayCue_Base::ResolveEffectOwner(Orphan));
		TestNull(TEXT("and null in is null out"),
			UBNGameplayCue_Base::ResolveEffectOwner(nullptr));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
