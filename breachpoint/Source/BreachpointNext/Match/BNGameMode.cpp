#include "Match/BNGameMode.h"
#include "AIBotAdapter/BNAIBModeTags.h"
#include "AIBotAdapter/BNAIBWorldQuery.h"
#include "Characters/BNCharacter.h"
#include "Core/AIBBotManager.h"
#include "Core/BNGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BNGameState.h"
#include "Match/BNHillPoint.h"
#include "Match/BNPlayerController.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "BreachpointNext.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
// FOverlapResult is FORWARD-DECLARED by Engine/World.h, not defined there. The hill's
// OverlapMultiByChannel stores them in a TArray, which needs the complete type for
// sizeof/destruct -- so the definition has to come in explicitly.
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
// Engine/, not GameFramework/ -- APlayerStartPIE sits beside the PIE machinery, while
// its APlayerStart base is the one under GameFramework/. Easy to transpose, and the
// compiler only ever says "file not found".
#include "Engine/PlayerStartPIE.h"
#include "TimerManager.h"

ABNGameMode::ABNGameMode()
{
	// A constructor value is only a default a Blueprint child can out-serialise; InitGame below is
	// what makes it stick. Kept anyway so the bare C++ class is correct with no BP in play.
	GameStateClass = ABNGameState::StaticClass();

	// THE LINE THAT MAKES A LEVEL RESTART SURVIVABLE. HandleMatchHasEnded now calls RestartGame(),
	// which ServerTravels back to the same map so the round starts from the level's authored
	// state. Without seamless travel that would tear down and reconnect every client on a listen
	// server between rounds; with it, connections and PlayerStates are carried across and the
	// reload is invisible to anyone playing.
	bUseSeamlessTravel = true;
}

void ABNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// FORCED, after the Blueprint's serialisation and before PreInitializeComponents spawns the
	// GameState — the same precedent as the pawn path below. This is the line that closed the
	// TASK-R4-GAMESTATE-CLASS editor ticket: no dropdown on BP_BNGameMode can undo it, so the
	// match state, the clock and every score always land on the GameState the game reads.
	GameStateClass = ABNGameState::StaticClass();

	// THE TRAVEL URL OVERRIDES THE INI (front end M1, 1 Sep). The menu's whole launch
	// surface is two options — ?TargetPlayers=N?Teams=0|1 — parsed here, after the ini's
	// serialisation, before anything reads either value (the fill converges in
	// HandleMatchIsWaitingToStart; teams assign at login). Absent options change nothing,
	// so every existing boot path — PIE straight into a map, the packaged BR_Spillway
	// default, the gauntlet — is byte-identical. Clamped because a URL is user input:
	// 1 is a lonely walk which is lawful, 0 or negative is not a match.
	if (UGameplayStatics::HasOption(Options, TEXT("TargetPlayers")))
	{
		const int32 Wanted = UGameplayStatics::GetIntOption(Options, TEXT("TargetPlayers"), TargetPlayers);
		TargetPlayers = FMath::Clamp(Wanted, 1, 32);
		UE_LOG(LogBN, Log, TEXT("BNGameMode: TargetPlayers=%d from the travel URL."), TargetPlayers);
	}
	// The lobby's SCORE LIMIT / TIME LIMIT rows (founder, 2 Sep 2026: kills 10/20/30, minutes
	// 5/10/15/20). Same shape as the two above: absent = ini stands, present = clamped user input.
	if (UGameplayStatics::HasOption(Options, TEXT("ScoreLimit")))
	{
		ScoreLimit = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("ScoreLimit"), ScoreLimit), 1, 500);
		UE_LOG(LogBN, Log, TEXT("BNGameMode: ScoreLimit=%d from the travel URL."), ScoreLimit);
	}
	if (UGameplayStatics::HasOption(Options, TEXT("TimeLimit")))
	{
		// Seconds on the wire, so the URL and the ini speak the same unit.
		TimeLimit = FMath::Clamp(static_cast<float>(UGameplayStatics::GetIntOption(Options, TEXT("TimeLimit"), FMath::RoundToInt(TimeLimit))), 30.f, 3600.f);
		UE_LOG(LogBN, Log, TEXT("BNGameMode: TimeLimit=%.0fs from the travel URL."), TimeLimit);
	}
	if (UGameplayStatics::HasOption(Options, TEXT("Teams")))
	{
		bTeamsEnabled = UGameplayStatics::GetIntOption(Options, TEXT("Teams"), bTeamsEnabled ? 1 : 0) != 0;
		UE_LOG(LogBN, Log, TEXT("BNGameMode: Teams=%s from the travel URL."), bTeamsEnabled ? TEXT("on") : TEXT("off"));
	}

	// The ini names the pawn; the dropdown is only what you see. This assignment is the one the
	// DefaultGame.ini comment promises — it was found commented out on 19 Aug and restored, with
	// the guard that keeps a bad path from replacing a working dropdown value with null.
	if (UClass* PawnClass = DefaultPawnClassPath.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
	else if (DefaultPawnClassPath.IsValid())
	{
		UE_LOG(LogBN, Warning, TEXT("BNGameMode: DefaultPawnClassPath '%s' did not resolve — the Blueprint's own Default Pawn Class stands."),
			*DefaultPawnClassPath.ToString());
	}
}

bool ABNGameMode::ReadyToStartMatch_Implementation()
{
	// The gate, nothing else: the parent polls this every warmup frame, so it must not spawn,
	// fill, or log. Humans only — a lobby of four bots is not a match anyone asked for.
	return GetNumPlayers() >= MinPlayers;
}

bool ABNGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Warmup restarts skip the PARENT's InProgress-only gate but keep the GRANDPARENT's real
	// checks (valid controller, CanRestartPlayer). Everything else defers to the parent, which is
	// what keeps corpses down during WaitingPostMatch.
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		return AGameModeBase::PlayerCanRestart_Implementation(Player);
	}

	return Super::PlayerCanRestart_Implementation(Player);
}

void ABNGameMode::HandleMatchIsWaitingToStart()
{
	// Super may fire world BeginPlay (first boot); it must run before anything here spawns.
	Super::HandleMatchIsWaitingToStart();

	// PROVIDERS BEFORE BODIES (law 3, as amended 26 Aug: the host PUSHES providers into
	// the manager once; controllers PULL at possession; the module never searches). This
	// must precede EnsureBotFill because a gen-1 bot possessed in THIS warmup keeps its
	// pawn through the first match start — its possession is the only pull it ever makes.
	// Idempotent store, so the restart path re-running it is free.
	if (UWorld* ManagerWorld = GetWorld())
	{
		if (UAIBBotManager* Manager = ManagerWorld->GetSubsystem<UAIBBotManager>())
		{
			Manager->RegisterProviders(this, ManagerWorld->GetSubsystem<UBNAIBWorldQuery>());
		}
	}

	// THE HILL BEFORE THE BODIES. Gen-1 bots possess inside EnsureBotFill below, and that
	// possession is the ONLY pull they ever make — so a POI registered after it does not
	// exist as far as they are concerned, forever. Measured before this line existed: all
	// seven bots won Mode.Hold at 13.46.35:678-691 and failed the branch ("no POI of kind
	// AIBot.POI.Hill"); the hill registered at :693, two milliseconds late, and NOT ONE
	// bot re-evaluated for the remaining four minutes. Mode.Hold kept winning at 0.72, the
	// branch kept failing, and because the winner never CHANGED nothing even logged it.
	//
	// Safe to run this early: HillTick guards on IsMatchInProgress(), so no warmup squatter
	// can bank a point no matter when the timer starts. The later StartHill() call at match
	// start stays as the restart path's own — it is idempotent on !Hill.IsValid().
	StartHill();

	EnsureBotFill();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A human who logged in during EnteringMap was refused a body (PlayerCanRestart said no
	// before warmup existed). Stand them up now — warmup bodies are the design.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn() && PlayerCanRestart(PC))
		{
			RestartPlayer(PC);
		}
	}
}

void ABNGameMode::HandleMatchHasStarted()
{
	// The parent notifies the match started and stands up any pawnless human. State is already
	// InProgress when this runs — the machine set it before dispatching here.
	Super::HandleMatchHasStarted();

	ABNGameState* GS = GetGameState<ABNGameState>();
	UWorld* World = GetWorld();
	if (!GS || !World)
	{
		return;
	}

	// Every round is its own generation, so respawns armed in the last one are ignored in this
	// one. 1 is the first match; anything above is an in-place restart.
	++MatchGeneration;

	// An END STAMP, not a countdown: one replicated write that is already correct for a client
	// joining at any moment. The mode's timer is the server's own copy of the same deadline.
	GS->SetMatchEndServerTime(GS->GetServerWorldTimeSeconds() + TimeLimit);
	SetAllPlayersFrozen(false);

	World->GetTimerManager().SetTimer(MatchTimerHandle, this, &ABNGameMode::OnTimeLimitReached,
		FMath::Max(1.f, TimeLimit), /*bLoop=*/false);

	// The hill goes live with the match — scoring outside InProgress would let a warmup
	// squatter bank points. Before the rebody loop so the first post-thaw think already
	// has the POI to walk to.
	StartHill();

	// A RESTART rebodies everyone: fresh attributes at fresh start points is what "new round"
	// means, and the last round's survivors must not carry 10 health into this one. The first
	// match rebodies only the PAWNLESS — its live players were born seconds ago in this very
	// warmup and destroying them to respawn them is a visible hitch for nothing, but a bot that
	// died to a hazard DURING warmup (death ignores the freeze, its respawn was refused outside
	// InProgress, and Super above stands up PlayerControllers only) would otherwise ghost through
	// the whole first match as a scoreboard row with no body — the critic's find. Thaw ran first,
	// so the respawn's State.* sweep finds no freeze handles left to stale. Known cost, accepted:
	// on a restart, a corpse Super just stood up is destroyed and stood up again — one redundant
	// spawn per corpse per round beats a special case in the one path that must never miss anyone.
	TArray<TObjectPtr<APlayerState>> Players = GS->PlayerArray;
	for (APlayerState* PS : Players)
	{
		AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
		if (Controller && (MatchGeneration > 1 || !Controller->GetPawn()))
		{
			RespawnPlayer(TWeakObjectPtr<AController>(Controller));
		}
	}
}

void ABNGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The clock is done whichever condition fired — the score limit leaves it armed otherwise.
	World->GetTimerManager().ClearTimer(MatchTimerHandle);

	// The hill stops with the match, and the urgency cache is cleared rather than left to
	// rot: a stale "held by X" would color the first thinks of the next round.
	World->GetTimerManager().ClearTimer(HillTimerHandle);
	HillHolder = nullptr;
	bHillContested = false;

	// And the STAMP is cleared, not left to rot: a stale end time reads exactly like "no clock"
	// through GetRemainingSeconds, but any future reader distinguishing the two would conflate
	// last round's expired deadline with a clock that was never set (critic note).
	if (ABNGameState* MutableGS = GetGameState<ABNGameState>())
	{
		MutableGS->SetMatchEndServerTime(0.0);
	}

	SetAllPlayersFrozen(true);

	// The winner was written BEFORE the state flipped (FinishMatch's ordering, and its team
	// sibling's), so reading it here is reading a decision, not a race. TEAMS (BN15): a team
	// win leaves the PlayerState Winner null on purpose — announcing THAT line would read
	// "none (tie)" over a decided match, so the team record speaks first when it is set.
	const ABNGameState* GS = GetGameState<ABNGameState>();
	const uint8 WinningTeam = GS ? GS->GetWinningTeamId() : FGenericTeamId::NoTeam.GetId();
	if (WinningTeam != FGenericTeamId::NoTeam.GetId())
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: match over. Winning team: %d"), WinningTeam);
	}
	else
	{
		const ABNPlayerState* Winner = GS ? GS->GetWinner() : nullptr;
		UE_LOG(LogBN, Log, TEXT("BNGameMode: match over. Winner: %s"),
			Winner ? *Winner->GetPlayerName() : TEXT("none (tie)"));
	}

	// The post-match window is the recap's time on screen (founder: three to five seconds, the ini
	// says how many). Then everybody travels to the front end; a restart in place is only the
	// fallback for a build with no menu map configured.
	if (PostMatchMapPath.IsEmpty())
	{
		World->GetTimerManager().SetTimer(PostMatchTimerHandle, this, &ABNGameMode::RestartMatch,
			FMath::Max(1.f, PostMatchDuration), /*bLoop=*/false);
	}
	else
	{
		World->GetTimerManager().SetTimer(PostMatchTimerHandle, this, &ABNGameMode::TravelToFrontEnd,
			FMath::Max(1.f, PostMatchDuration), /*bLoop=*/false);
	}
}

void ABNGameMode::TravelToFrontEnd()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Seamless travel CARRIES PlayerStates, so the scores are wiped here exactly as a restart
	// wipes them — the next match must not open with this one's kills already on the board.
	if (ABNGameState* GS = GetGameState<ABNGameState>())
	{
		GS->SetWinner(nullptr);
		GS->SetWinningTeamId(FGenericTeamId::NoTeam.GetId());
		GS->ResetTeamScores();
		GS->ResetKillfeed();
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS))
			{
				BNPS->ResetScore();
			}
		}
	}
	// ServerTravel is the real path: one call, every connection follows (seamless). It ENDS a
	// Play-In-Editor session (measured 25 Aug), so PIE takes the lobby's own route to a map —
	// OpenLevel on the local world — and the log names which one ran (honesty ladder).
	if (!World->IsPlayInEditor())
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: post-match over — ServerTravel to %s."), *PostMatchMapPath);
		World->ServerTravel(PostMatchMapPath, /*bAbsolute*/ true);
		return;
	}
	UE_LOG(LogBN, Log, TEXT("BNGameMode: post-match over — PIE: OpenLevel %s (ServerTravel would end the session)."), *PostMatchMapPath);
	UGameplayStatics::OpenLevel(World, FName(*PostMatchMapPath), /*bAbsolute*/ true);
}

void ABNGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	// Subscribed here rather than in the PlayerState's own BeginPlay: by this point the controller
	// HAS a PlayerState, and the mode is the one object that outlives every pawn and every death,
	// so nothing has to re-subscribe on respawn. HERE and not OnPostLogin because an AIController
	// never sees OnPostLogin — the engine calls this for humans, the bot fill calls it for bots.
	// It can run more than once per controller (seamless travel does), so the guard keeps one
	// death from printing two kill lines and arming two respawns.
	ABNPlayerState* PS = C ? C->GetPlayerState<ABNPlayerState>() : nullptr;
	if (PS && !PS->OnPlayerDeath.IsBoundToObject(this))
	{
		PS->OnPlayerDeath.AddUObject(this, &ABNGameMode::HandlePlayerDeath);
	}

	// TEAMS (BN15): the side is picked HERE, before this entity's first RestartPlayer — both
	// humans and the bot fill pass through this seam, so the first spawn choice already knows
	// which tagged start serves it. Idempotent, so the re-runs noted above are free.
	AssignTeamIfNeeded(C);

	// A player initialized outside a running match joined a warmup or a post-match and must be
	// frozen like everyone already here — otherwise the one machine that joined late is the one
	// machine that can shoot. Thawing is HandleMatchHasStarted's, the moment the machine starts.
	if (PS && !IsMatchInProgress())
	{
		SetPlayerFrozen(PS, true);
	}
}

void ABNGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	// The arrival changed the seat math — a bot may have to yield. The START needs no call: the
	// parent's next Tick asks ReadyToStartMatch, which now counts this human.
	EnsureBotFill();
}

void ABNGameMode::AssignTeamIfNeeded(AController* C)
{
	// Teams off = today's FFA byte-for-byte: nobody is ever assigned and every TeamId stays
	// NoTeam, which the guard in BNTeams keeps hostile-or-neutral everywhere.
	if (!bTeamsEnabled || !HasAuthority())
	{
		return;
	}

	ABNPlayerState* PS = C ? C->GetPlayerState<ABNPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}

	// Idempotent: GenericPlayerInitialization can run more than once per controller (seamless
	// travel does), and a side once picked is kept — re-balancing a live player is a different
	// feature, deliberately not this one.
	if (PS->GetGenericTeamId() != FGenericTeamId::NoTeam)
	{
		return;
	}

	const uint8 Team = GetLowestPopulationTeam();
	PS->SetGenericTeamId(FGenericTeamId(Team));
	UE_LOG(LogBN, Log, TEXT("BNGameMode: %s assigned to team %d."), *PS->GetPlayerName(), Team);
}

void ABNGameMode::CountTeamPopulations(TArrayView<int32> OutCounts) const
{
	// NoTeam members — the caller is mid-assigning them — count for nobody.
	for (int32& Count : OutCounts)
	{
		Count = 0;
	}
	if (const ABNGameState* GS = GetGameState<ABNGameState>())
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS);
			const int32 Id = BNPS ? BNPS->GetGenericTeamId().GetId() : FGenericTeamId::NoTeam.GetId();
			if (OutCounts.IsValidIndex(Id))
			{
				++OutCounts[Id];
			}
		}
	}
}

uint8 ABNGameMode::LowestPopulationTeam(TConstArrayView<int32> TeamCounts)
{
	// Fewest members wins, ties to the lower id (the OnSight shape, BN-idiomatic).
	uint8 Best = 0;
	for (int32 Team = 1; Team < TeamCounts.Num(); ++Team)
	{
		if (TeamCounts[Team] < TeamCounts[Best]) // strict <, so a tie keeps the lower id
		{
			Best = static_cast<uint8>(Team);
		}
	}
	return Best;
}

int32 ABNGameMode::PickYieldingBotIndex(TConstArrayView<int32> TeamCounts, TConstArrayView<uint8> BotTeams)
{
	// The MIRROR of the assignment: a joiner takes the emptiest side, so the seat that yields
	// must come off the fullest one, or every second human is a permanent 5v3.
	int32 Crowded = 0;
	for (int32 Team = 1; Team < TeamCounts.Num(); ++Team)
	{
		if (TeamCounts[Team] > TeamCounts[Crowded]) // strict >, so a tie keeps the lower id
		{
			Crowded = Team;
		}
	}

	// Newest first — the tail-pop's own reasoning, which keeps the named veterans and their scores.
	for (int32 Index = BotTeams.Num() - 1; Index >= 0; --Index)
	{
		if (BotTeams[Index] == Crowded)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

uint8 ABNGameMode::GetLowestPopulationTeam() const
{
	int32 Counts[BNTeams::NumTeams];
	CountTeamPopulations(Counts);
	return LowestPopulationTeam(Counts);
}

AActor* ABNGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Teams off, or a player honestly reading NoTeam (nothing past the assignment seam should,
	// but a spawn must never fail over it): the engine's own choice stands.
	const ABNPlayerState* PS = Player ? Player->GetPlayerState<ABNPlayerState>() : nullptr;
	const FGenericTeamId Team = PS ? PS->GetGenericTeamId() : FGenericTeamId::NoTeam;
	UWorld* World = GetWorld();
	if (!bTeamsEnabled || Team == FGenericTeamId::NoTeam || !World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// A start whose PlayerStartTag is None serves anyone; "Team0"/"Team1" serves that side
	// only (the tags Tools/bn/tag_team_starts.py writes). The iterator is the StartHill
	// precedent — and the body conditionally COLLECTS, never unconditionally breaks, so the
	// increment is reachable (the AIB11 -Wunreachable-code-loop-increment lesson).
	const FName TeamTag(*FString::Printf(TEXT("Team%d"), Team.GetId()));
	TArray<APlayerStart*> Matches;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}
		// The engine's own override honors "Play from Here" before anything else; taking over
		// the choice must not take that away from a PIE session.
		if (Start->IsA<APlayerStartPIE>())
		{
			return Start;
		}
		if (Start->PlayerStartTag.IsNone() || Start->PlayerStartTag == TeamTag)
		{
			Matches.Add(Start);
		}
	}

	// No tagged or untagged start matched — a mis-tagged level is Super's problem, never a
	// failed spawn.
	if (Matches.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// THE ENGINE'S OWN PARTITION, restored (BN15 REFUTER B2): this branch skipped the
	// encroachment pass Super runs, and SpawnDefaultPawnAtTransform spawns AlwaysSpawn —
	// so a round restart rebodying a whole side from one tagged pool drew WITH
	// replacement and interpenetrated teammates, and a live respawn could land inside a
	// standing ally. Prefer starts a default pawn fits at; every candidate blocked keeps
	// the full pool (spawning clumped beats not spawning — Super's own fallback shape).
	TArray<APlayerStart*> Unblocked;
	if (UClass* PawnClass = GetDefaultPawnClassForController(Player))
	{
		if (APawn* PawnToFit = PawnClass->GetDefaultObject<APawn>())
		{
			for (APlayerStart* Start : Matches)
			{
				if (!World->EncroachingBlockingGeometry(PawnToFit, Start->GetActorLocation(), Start->GetActorRotation()))
				{
					Unblocked.Add(Start);
				}
			}
		}
	}
	TArray<APlayerStart*>& Pool = Unblocked.Num() > 0 ? Unblocked : Matches;

	// Random among the pool (the HitReact montage-pick pattern), not first-match: first-match
	// would funnel a whole side through one start every respawn, and spreading the picks is
	// what keeps spawns unclumped.
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

void ABNGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// Next tick, not now: the leaving controller is still in the world's iterators inside Logout,
	// so a fill computed here counts a seat that is already empty. Humans only — a bot's own
	// despawn also lands in Logout, and refilling the seat a yield just cleared would fight the
	// yield forever.
	if (Cast<APlayerController>(Exiting) && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ABNGameMode::EnsureBotFill);
	}
}

void ABNGameMode::HandlePlayerDeath(ABNPlayerState* Victim, ABNPlayerState* Killer, FName SourceName)
{
	if (!Victim)
	{
		return;
	}

	// CREDIT, before the line that prints it. A death always costs the victim; a kill is only a
	// kill when someone ELSE caused it. Killer == victim is their own grenade and a null killer is
	// world damage or a killer who left mid-flight — neither scores, both still cost a death.
	// Only a kill DURING the match scores. A grenade that lands after the buzzer still kills, still
	// ragdolls and still prints — but the scoreboard is final the moment the winner is announced,
	// or the post-match shows a top scorer who is not the winner, wrongly and on every machine.
	// TEAMS (BN15): a same-team kill credits NOTHING — no kill, no team point; the death still
	// counts and the feed still shows it. Compared directly on the two PlayerStates (they carry
	// the interface); the bTeamsEnabled gate keeps even the comparison off the FFA path, and the
	// NoTeam guard would answer false there anyway — belt and braces, both stated.
	const bool bTeamKill = bTeamsEnabled && Killer && Killer != Victim
		&& BNTeams::AreFriendly(Killer->GetGenericTeamId(), Victim->GetGenericTeamId());

	const bool bMatchLive = IsMatchInProgress();
	if (bMatchLive)
	{
		Victim->AddDeath();

		if (Killer && Killer != Victim && !bTeamKill)
		{
			Killer->AddKill();
		}
	}

	// The denial is COUNTABLE (BN16 harness law: no proof rests on an impression) — the
	// kill line below still prints the elimination, and without this line a team kill and
	// a credited kill read identically in a log. Exact format is load-bearing: the metrics
	// harness regex transcribes it.
	if (bTeamKill)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: team kill — %s -> %s, no credit."),
			*Killer->GetPlayerName(), *Victim->GetPlayerName());
	}

	// THE KILL LINE — the pipeline's deliverable, and the founder's test. Three wordings for three
	// real cases, decided in the research doc so nobody rediscovers them: a null killer is world
	// damage or a killer who disconnected mid-flight; killer == victim is their own grenade.
	if (!Killer)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s died. (%d deaths)"),
			*Victim->GetPlayerName(), Victim->GetDeaths());
	}
	else if (Killer == Victim)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s eliminated themselves. (%d deaths)"),
			*Victim->GetPlayerName(), Victim->GetDeaths());
	}
	else
	{
		// The cause rides the line too — the one place a wrong killfeed glyph can be told apart
		// from a wrong CAPTURE without attaching a debugger.
		UE_LOG(LogBN, Log, TEXT("BNGameMode: %s eliminated %s with '%s'. (%s: %d kills)"),
			*Killer->GetPlayerName(), *Victim->GetPlayerName(), *SourceName.ToString(),
			*Killer->GetPlayerName(), Killer->GetKills());
	}

	// R7 — the same decided kill, onto the replicated ring the killfeed renders. Unconditional
	// like the log lines above it: a post-buzzer grenade kill still prints and still shows, it
	// just does not score — one decision, told the same way to every audience.
	if (ABNGameState* FeedGS = GetGameState<ABNGameState>())
	{
		FeedGS->PushKillfeedEntry(Victim, Killer, SourceName);
	}

	// 3.2 — the score limit, checked where the kill was credited. FinishMatch itself refuses
	// unless the match is InProgress, so a second elimination inside the same frame cannot end it
	// twice. TEAMS (BN15): a credited kill is also a TEAM point, and the crossing check reads
	// the team's score — the per-player GetScore check stays FFA's own.
	if (bMatchLive && Killer && Killer != Victim && !bTeamKill)
	{
		if (bTeamsEnabled)
		{
			const uint8 KillerTeam = Killer->GetGenericTeamId().GetId();
			ABNGameState* TeamGS = GetGameState<ABNGameState>();
			if (TeamGS && KillerTeam < BNTeams::NumTeams)
			{
				TeamGS->AddTeamScore(KillerTeam, 1);
				if (TeamGS->GetTeamScore(KillerTeam) >= ScoreLimit)
				{
					FinishTeamMatch(KillerTeam);
				}
			}
		}
		else if (Killer->GetScore() >= ScoreLimit)
		{
			FinishMatch(Killer);
		}
	}

	// This mode's answer to a death is a timed respawn. Another mode's could be a spectator
	// hand-off, or nothing at all — which is the point of hearing about the death instead of being
	// called by the ability that caused it.
	RequestRespawn(Cast<AController>(Victim->GetOwner()));
}

void ABNGameMode::RequestRespawn(AController* Controller)
{
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return;
	}

	// The timer is the GAME MODE's, never the dying pawn's: the corpse is destroyed before the
	// delay is up and a timer living on it would be destroyed with it.
	//
	// It also carries the GENERATION it was armed in. A player killed near the end of a round has a
	// respawn in flight when the restart rebodies everyone; without this the stale timer fires a
	// second or two into the NEW round and unpossesses and destroys a perfectly live pawn, which
	// every other client sees as a player blinking out and reappearing at a start point. Comparing
	// generations costs one int and needs no map of handles to keep in sync.
	// R7 — the owner's death screen counts down against this stamp, computed locally the way the
	// match clock is. Stamped from the same number the timer below is armed with, so the screen
	// and the actual rebody cannot drift. InProgress-gated (critic): the score-limit kill ends
	// the match BEFORE this runs, and without the gate every match-ending kill's victim counts
	// "respawning in 3…" over the winner announcement — a countdown to a rebody the post-match
	// refuses. The timer still arms; its refusal path is the second net.
	const float Delay = FMath::Max(0.1f, RespawnDelay);
	if (IsMatchInProgress())
	{
		if (ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>())
		{
			if (const ABNGameState* GS = GetGameState<ABNGameState>())
			{
				PS->SetRespawnAtServerTime(GS->GetServerWorldTimeSeconds() + Delay);
			}
		}
	}

	const int32 ArmedGeneration = MatchGeneration;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller), ArmedGeneration]()
		{
			if (ArmedGeneration == MatchGeneration)
			{
				RespawnPlayer(WeakController);
			}
		}),
		Delay, /*bLoop=*/false);
}

void ABNGameMode::RespawnPlayer(TWeakObjectPtr<AController> WeakController)
{
	AController* Controller = WeakController.Get();
	if (!Controller)
	{
		return;
	}

	// R7 — the stamp comes off FIRST, refusals included: a respawn refused by the post-match is
	// not coming this round (the restart rebodies instead), and a death screen counting down to
	// a moment that will not happen is the HUD lying.
	ABNPlayerState* StampPS = Controller->GetPlayerState<ABNPlayerState>();
	if (StampPS)
	{
		StampPS->SetRespawnAtServerTime(0.0);
	}

	// 3.5 — a respawn armed during play can land after the match ended. Nobody stands up outside
	// InProgress; the restart is what puts everyone back on their feet.
	if (!IsMatchInProgress())
	{
		return;
	}

	// The corpse goes first, and explicitly: ABNCharacter::EndPlay is what takes the crouch GE off
	// the persistent ASC (debt A2) and UBNEquipmentComponent::EndPlay is what revokes the weapon's
	// ability set. Both must have run before the new body grants its own.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	// Then the clean slate, on the ASC — which is the PERSISTENT PlayerState's, not the pawn's.
	// Cancel ends the death ability, and that is what removes State.Dead; the sweep then takes any
	// State.* GE the old life left behind. Both happen BEFORE the new pawn exists, so it is born
	// onto an ASC with no tags and full attributes rather than being cleaned up afterwards.
	ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>();
	if (UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr)
	{
		ASC->CancelAbilities();

		FGameplayTagContainer StateTags;
		StateTags.AddTag(BNTags::State);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);

		// Through the init GE, never hand-set: that GE is the one place the starting numbers live.
		PS->ApplyInitAttributes();

		// A TRIPWIRE on an invariant that had none. The new pawn's health component watches CHANGES
		// only — reading the value at registration would kill a body on the frame it spawned — so
		// health MUST already be positive by the time that component exists. If the init GE ever
		// fails to run, the player is alive at zero and can never die again, because no change will
		// follow. Silent and permanent, and it presents as "that one player is unkillable".
		const float RestoredHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute());
		if (RestoredHealth <= 0.f)
		{
			UE_LOG(LogBN, Error, TEXT("BNGameMode: respawning %s with Health %.1f — the init GE did not restore it. "
				"This pawn will be unkillable: its health component only reports CHANGES, and none will come."),
				*GetNameSafe(PS), RestoredHealth);
		}
	}

	// Cleared explicitly, server-side. UBNGA_Death set these on the controller, and the comment
	// there says ClientRestart resets them — which it does, but ClientRestart is a CLIENT RPC, so
	// the server's copy of a remote player's controller runs no such reset. A remote client's
	// movement arrives as ServerMove into the CMC rather than through AddMovementInput, so this is
	// most likely harmless; "most likely" is not verified, the symptom would be a respawned player
	// who cannot move, and the cure is two lines.
	Controller->SetIgnoreMoveInput(false);
	Controller->SetIgnoreLookInput(false);

	// The engine's own start point. There is deliberately no ABNPlayerStart: AGameModeBase::
	// FindPlayerStart already picks an APlayerStart, and a BN subclass would hold nothing until
	// teams and spawn scoring exist — which is a later roadmap's, not this fence's.
	RestartPlayer(Controller);
}

void ABNGameMode::EnsureBotFill()
{
	if (!HasAuthority())
	{
		return;
	}

	// TWO STATES, TWO POWERS (R9). Warmup both fills and yields. A LIVE match may only YIELD:
	// R5's own limitation was that a human joining after the start left the lobby one wide for
	// the whole match (the function returned before it could count), and the fix is not to also
	// start spawning mid-fight — a bot materialising beside you is worse than a seat over. Lyra
	// draws the same line: remove on join, backfill only between matches.
	const bool bWarmup = GetMatchState() == MatchState::WaitingToStart;
	const bool bLive = GetMatchState() == MatchState::InProgress;
	if (!bWarmup && !bLive)
	{
		return;
	}

	// A lobby with no one in it needs no bots. This guard is for the DEDICATED boot, where the
	// machine enters warmup with zero humans and would otherwise spawn a full bot lobby only to
	// yield a seat when the first human arrives. In PIE and on a listen host the order is the
	// reverse — the local player's login runs BEFORE StartPlay (critic-verified against the
	// engine's LoadMap/PIE flow), so the first fill here already counts one human and prints
	// R5's own line: "filled 3 bots to reach 4".
	if (GetNumPlayers() == 0)
	{
		return;
	}

	// GetNumPlayers counts humans only (PlayerControllers), which is exactly right — the bots
	// already here are counted from our own book, pruned of anything destroyed.
	SpawnedBots.RemoveAll([](const TObjectPtr<AAIController>& Bot) { return !IsValid(Bot); });

	const int32 BotsNeeded = TargetPlayers - GetNumPlayers() - SpawnedBots.Num();

	// OVER target: humans arrived after the fill (the machine enters warmup before the local
	// player logs in, so the first fill legitimately runs at zero humans). Newest bots yield
	// their seats — popping the tail keeps the named veterans and their scores.
	if (BotsNeeded < 0)
	{
		int32 Yielded = 0;
		for (int32 i = BotsNeeded; i < 0 && SpawnedBots.Num() > 0; ++i)
		{
			// TEAMS (BN15 REFUTER B1): the seat yielded must come from the side the human
			// just crowded, or every second human makes a deterministic 5v3 — the joiner
			// runs AssignTeamIfNeeded FIRST and ties {4,4} to Team 0, while the tail bot
			// popped here is always Team 1's. Counts are re-read every iteration because
			// DespawnBot leaves PlayerArray one shorter. FFA, and a crowded side with no
			// bot on it, keep the plain tail pop.
			int32 YieldIndex = SpawnedBots.Num() - 1;
			if (bTeamsEnabled)
			{
				int32 Counts[BNTeams::NumTeams];
				CountTeamPopulations(Counts);

				TArray<uint8, TInlineAllocator<8>> BotTeams;
				BotTeams.Reserve(SpawnedBots.Num());
				for (const TObjectPtr<AAIController>& Bot : SpawnedBots)
				{
					const ABNPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABNPlayerState>() : nullptr;
					BotTeams.Add(BotPS ? BotPS->GetGenericTeamId().GetId() : FGenericTeamId::NoTeam.GetId());
				}

				const int32 Picked = PickYieldingBotIndex(Counts, BotTeams);
				if (Picked != INDEX_NONE)
				{
					YieldIndex = Picked;
				}
			}
			AAIController* Yielding = SpawnedBots[YieldIndex];
			SpawnedBots.RemoveAt(YieldIndex);
			DespawnBot(Yielding);
			++Yielded;
		}
		UE_LOG(LogBN, Log, TEXT("BNBots: %d bot(s) yielded seats to humans (%d humans, %d bots, target %d)"),
			Yielded, GetNumPlayers(), SpawnedBots.Num(), TargetPlayers);
		return;
	}

	if (BotsNeeded == 0)
	{
		return;
	}

	// Short-handed mid-match: left alone deliberately. See the two-states note above.
	if (bLive)
	{
		return;
	}

	int32 Filled = 0;
	for (int32 i = 0; i < BotsNeeded; ++i)
	{
		if (SpawnBot(NextBotNameIndex++))
		{
			++Filled;
		}
	}

	UE_LOG(LogBN, Log, TEXT("BNBots: filled %d bots to reach %d"), Filled, TargetPlayers);
}

AAIController* ABNGameMode::SpawnBot(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ControllerClass = AIBBotControllerClass.LoadSynchronous();
	if (!ControllerClass)
	{
		UE_LOG(LogBN, Error, TEXT("BNBots: AIBBotControllerClass did not resolve — no bot spawned."));
		return nullptr;
	}

	// Lyra's flags: transient, so a bot controller can never be saved into a map.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AAIController* NewBot = World->SpawnActor<AAIController>(ControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NewBot)
	{
		UE_LOG(LogBN, Error, TEXT("BNBots: SpawnActor failed for %s — no bot spawned."), *GetNameSafe(ControllerClass));
		return nullptr;
	}

	// The name lands on the PlayerState bWantsPlayerState already made — the same name the kill
	// line, the leaders query and the winner announcement all read.
	if (APlayerState* PS = NewBot->GetPlayerState<APlayerState>())
	{
		PS->SetPlayerName(BotNames.IsValidIndex(Index) ? BotNames[Index] : FString::Printf(TEXT("Bot %d"), Index));
	}

	// The ONE initialization seam (G2): death subscription + warmup freeze, same as a human.
	GenericPlayerInitialization(NewBot);
	RestartPlayer(NewBot);

	SpawnedBots.Add(NewBot);
	return NewBot;
}

void ABNGameMode::DespawnBot(AAIController* Bot)
{
	if (!Bot)
	{
		return;
	}

	// Pawn first — OnUnPossess is what stops the tree and unhooks the brain's delegates — then
	// the controller, whose destruction is what retires its PlayerState from PlayerArray and
	// every roster built on it.
	if (APawn* BotPawn = Bot->GetPawn())
	{
		Bot->UnPossess();
		BotPawn->Destroy();
	}
	Bot->Destroy();
}

void ABNGameMode::OnTimeLimitReached()
{
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	// TEAMS (BN15): the leading team at the buzzer wins. A TIE deliberately does NOT call
	// FinishTeamMatch(NoTeam) — WinningTeamId stays 255 ("undecided") and the existing
	// FinishMatch(nullptr) path runs, which is already the renderable tie outcome.
	if (bTeamsEnabled)
	{
		uint8 Best = 0;
		bool bTie = false;
		for (uint8 Team = 1; Team < BNTeams::NumTeams; ++Team)
		{
			const int32 Score = GS->GetTeamScore(Team);
			if (Score > GS->GetTeamScore(Best))
			{
				Best = Team;
				bTie = false;
			}
			else if (Score == GS->GetTeamScore(Best))
			{
				bTie = true;
			}
		}
		if (!bTie)
		{
			FinishTeamMatch(Best);
			return;
		}
		FinishMatch(nullptr);
		return;
	}

	// The sole leader wins. GetLeaders returns the whole tie set precisely so this can tell a
	// decided match from a tied one, and a tie is a null winner rather than an arbitrary pick.
	TArray<ABNPlayerState*> Leaders;
	GS->GetLeaders(Leaders);
	FinishMatch(Leaders.Num() == 1 ? Leaders[0] : nullptr);
}

void ABNGameMode::FinishMatch(ABNPlayerState* InWinner)
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS || !IsMatchInProgress())
	{
		return;
	}

	// Winner BEFORE the state, so the two land in the same bunch and no client ever sees the end
	// announced against a winner the server has not written yet. Then the machine's own verb —
	// EndMatch flips to WaitingPostMatch and dispatches HandleMatchHasEnded.
	GS->SetWinner(InWinner);
	EndMatch();
}

void ABNGameMode::FinishTeamMatch(uint8 WinningTeam)
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS || !IsMatchInProgress())
	{
		return;
	}

	// FinishMatch's own ordering, sibling'd for a team: the winning id FIRST, then the flip,
	// so both land in the same bunch. The PlayerState Winner stays null — the HUD renders
	// whichever record is set.
	GS->SetWinningTeamId(WinningTeam);
	EndMatch();
}

void ABNGameMode::RestartMatch()
{
	ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	GS->SetWinner(nullptr);
	// TEAMS (BN15): the winning-team record and the team scores clear the same way the
	// PlayerState winner and the per-player scores do — but the SIDES survive: TeamId is
	// deliberately untouched, a restart keeps every player on the team they had.
	GS->SetWinningTeamId(FGenericTeamId::NoTeam.GetId());
	GS->ResetTeamScores();
	GS->ResetKillfeed();

	// Every score to zero. Without this the restarted match inherits the old one's kills and the
	// first elimination of the new round crosses the limit again immediately.
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS))
		{
			BNPS->ResetScore();
		}
	}

	// FOUNDER, 25 Aug: "when match ends restart level to start a new match."
	//
	// This used to be SetMatchState(WaitingToStart) — a reset IN PLACE, chosen so the listen
	// server's connections survived the round. That concern is real and is NOT abandoned here:
	// bUseSeamlessTravel is enabled in the constructor, which is what lets the map actually
	// reload without dropping a single client. A bare RestartGame() with seamless travel off
	// would ServerTravel every connection through a full reconnect, which is exactly the cost
	// the old comment was avoiding.
	//
	// A real reload is what an in-place reset cannot give: the level's own actors go back to
	// their authored state — dropped weapons, spent pickups, projectiles in flight, corpses and
	// any navmesh dirtied during the round. The score wipe above still matters because seamless
	// travel CARRIES PlayerStates across, so kills would otherwise survive the reload and the
	// first elimination of the new round would cross the limit again immediately.
	//
	// BUT NOT IN PIE, AND THIS IS MEASURED, NOT ASSUMED. RestartGame() -> ServerTravel ENDS a
	// Play-In-Editor session rather than reloading it: tested 25 Aug, the match reached
	// WaitingPostMatch, the timer fired, and PIE simply stopped. Shipping the travel unguarded
	// would kill the founder's test session at the end of every single round.
	//
	// So the path is chosen by whether travel can actually work here, and it SAYS WHICH ONE IT
	// TOOK — a silent difference between what PIE exercises and what a packaged build does is
	// exactly the class of bug the honesty ladder exists to stop.
	const UWorld* MatchWorld = GetWorld();
	const bool bCanTravel = MatchWorld && !MatchWorld->IsPlayInEditor();
	if (bCanTravel)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameMode: match over — reloading the level (seamless travel)."));
		RestartGame();
		return;
	}

	UE_LOG(LogBN, Log, TEXT("BNGameMode: match over — resetting IN PLACE (PIE: ServerTravel would end the session)."));
	SetMatchState(MatchState::WaitingToStart);
}

void ABNGameMode::SetPlayerFrozen(ABNPlayerState* InPlayerState, bool bFrozen)
{
	UAbilitySystemComponent* ASC = InPlayerState ? InPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Built once: the map's key type is the weak pointer, and a raw pointer passed straight to
	// Find/Remove would hash as an address rather than as an object index and never match.
	const TWeakObjectPtr<ABNPlayerState> Key(InPlayerState);

	if (!bFrozen)
	{
		FActiveGameplayEffectHandle Handle;
		if (FrozenHandles.RemoveAndCopyValue(Key, Handle) && Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		return;
	}

	if (FrozenHandles.Contains(Key))
	{
		return;
	}

	// The PERSISTENT PlayerState's ASC, so the freeze outlives the pawn a post-match death takes.
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_State::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(BNTags::State_Match_Frozen);
		FrozenHandles.Add(Key, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data));
	}

	// And STOP what is already running. Refusing activation does nothing to an ability that keeps
	// its own loop: a player holding an Auto trigger at the buzzer would go on killing through the
	// entire post-match, with the server confirming every shot. Death and hit-react survive this.
	if (UBNAbilitySystemComponent* BNASC = InPlayerState->GetBNAbilitySystemComponent())
	{
		BNASC->CancelAbilitiesBlockedByFreeze();
	}
}

void ABNGameMode::SetAllPlayersFrozen(bool bFrozen)
{
	const ABNGameState* GS = GetGameState<ABNGameState>();
	if (!GS)
	{
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		SetPlayerFrozen(Cast<ABNPlayerState>(PS), bFrozen);
	}

	// Thawing clears the map outright, which is also what collects the entries of players who left
	// while frozen — their weak keys never come back to be removed one at a time.
	if (!bFrozen)
	{
		FrozenHandles.Reset();
	}
}

void ABNGameMode::StartHill()
{
	UWorld* World = GetWorld();
	if (!bHillEnabled || !World)
	{
		return;
	}

	// Spawn-or-adopt, idempotent: the first match spawns; an in-place restart finds the
	// hill still standing and reuses it. A hand-placed hill would be adopted the same way
	// the moment placement exists — for now the ini location is the C++-first path.
	if (!Hill.IsValid())
	{
		// "First hill in the level, or none" -- stated as a single query rather than a loop
		// that breaks on its first pass, which the compiler rejects outright under
		// -Werror,-Wunreachable-code-loop-increment because ++It can never run.
		TActorIterator<ABNHillPoint> HillIt(World);
		ABNHillPoint* Placed = HillIt ? *HillIt : nullptr;
		if (!Placed)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Placed = World->SpawnActor<ABNHillPoint>(HillLocation, FRotator::ZeroRotator, Params);
			if (Placed)
			{
				Placed->Radius = HillRadius;
			}
		}
		if (!Placed)
		{
			UE_LOG(LogBN, Warning, TEXT("BNGameMode: hill enabled but the hill failed to spawn at %s — the mode runs as Slayer."),
				*HillLocation.ToCompactString());
			return;
		}
		Hill = Placed;

		// Pushed, never hunted — the same handedness law the provider registration follows.
		if (UBNAIBWorldQuery* Query = World->GetSubsystem<UBNAIBWorldQuery>())
		{
			Query->RegisterHill(Placed);
		}
	}

	HillHolder = nullptr;
	bHillContested = false;
	World->GetTimerManager().SetTimer(HillTimerHandle, this, &ABNGameMode::HillTick,
		1.f, /*bLoop=*/true);
	UE_LOG(LogBN, Log, TEXT("BNGameMode: hill live at %s, radius %.0f, %d point(s)/s."),
		*Hill->GetActorLocation().ToCompactString(), HillRadius, HillPointsPerSecond);
}

void ABNGameMode::HillTick()
{
	UWorld* World = GetWorld();
	const ABNHillPoint* HillActor = Hill.Get();
	if (!World || !HillActor || !IsMatchInProgress())
	{
		return;
	}

	// The projectile's proven shape: one channel overlap, then dedup per ACTOR — a
	// character answers with capsule and mesh both, and a double-counted body would read
	// one occupant as a contest.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, HillActor->GetActorLocation(), FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(FMath::Max(1.f, HillActor->Radius)));

	// Distinct LIVING players on the hill. PlayerState-keyed, not pawn-keyed, because the
	// score books live there; corpses do not hold hills (the same one tag that means dead
	// everywhere else in BN).
	TSet<ABNPlayerState*> Occupants;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const APawn* PawnOnHill = Cast<APawn>(Overlap.GetActor());
		ABNPlayerState* PS = PawnOnHill ? PawnOnHill->GetPlayerState<ABNPlayerState>() : nullptr;
		const UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
		if (ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead))
		{
			Occupants.Add(PS);
		}
	}

	// TEAMS (BN15): occupants dedupe per TEAM — same-team bodies do not contest each other,
	// and contested means MORE THAN ONE distinct side present. A NoTeam occupant (nothing
	// past the assignment seam should produce one) counts as its own side, so it contests
	// rather than scoring — unknown is nobody's ally, the BNTeams default. The urgency cache
	// keeps the same shape as FFA's: any ONE member of the sole holding team is HillHolder,
	// so GetObjectiveUrgency's held-by-me check still works and its teams-on branch widens
	// "me" to "my team".
	if (bTeamsEnabled)
	{
		TSet<uint8> TeamsPresent;
		for (const ABNPlayerState* Occupant : Occupants)
		{
			TeamsPresent.Add(Occupant->GetGenericTeamId().GetId());
		}

		bHillContested = TeamsPresent.Num() > 1;
		const uint8 SoleTeam = TeamsPresent.Num() == 1
			? *TeamsPresent.CreateConstIterator() : FGenericTeamId::NoTeam.GetId();
		const bool bHeld = SoleTeam < BNTeams::NumTeams;
		HillHolder = bHeld ? *Occupants.CreateIterator() : nullptr;

		FString HillState = TEXT("empty");
		if (bHillContested)
		{
			HillState = TEXT("CONTESTED between teams, nobody scores");
		}
		else if (bHeld)
		{
			HillState = FString::Printf(TEXT("held by team %d"), SoleTeam);
		}
		UE_LOG(LogBN, Verbose, TEXT("BNGameMode: hill tick — %d occupant(s), %s."),
			Occupants.Num(), *HillState);

		if (bHeld)
		{
			if (ABNGameState* TeamGS = GetGameState<ABNGameState>())
			{
				TeamGS->AddTeamScore(SoleTeam, FMath::Max(0, HillPointsPerSecond));
				if (TeamGS->GetTeamScore(SoleTeam) >= ScoreLimit)
				{
					// The same seam the crossing kill uses: winner written first, then the flip.
					FinishTeamMatch(SoleTeam);
				}
			}
		}
		return;
	}

	// THE RULE, whole: sole occupant scores; contested means NOBODY scores. The cache is
	// what GetObjectiveUrgency answers from — written every second, whether or not points
	// moved, so "the hill emptied" is as current as "the hill scored".
	bHillContested = Occupants.Num() > 1;
	ABNPlayerState* Holder = Occupants.Num() == 1 ? *Occupants.CreateIterator() : nullptr;
	HillHolder = Holder;

	// THE OBJECTIVE'S OWN OBSERVABLE. Without this the only evidence a hill exists is the
	// absence of complaints: occupancy, contest and banked points left no trace at all, so
	// "the bots reach it but never score" and "the bots never get there" read identically
	// in a log. Verbose — one line a second is fine to ask for and wrong to always pay.
	UE_LOG(LogBN, Verbose, TEXT("BNGameMode: hill tick — %d occupant(s), %s%s."),
		Occupants.Num(),
		bHillContested ? TEXT("CONTESTED, nobody scores")
			: (Holder ? TEXT("held by ") : TEXT("empty")),
		(!bHillContested && Holder) ? *Holder->GetPlayerName() : TEXT(""));

	if (Holder)
	{
		Holder->AddObjectivePoints(FMath::Max(0, HillPointsPerSecond));
		if (Holder->GetScore() >= ScoreLimit)
		{
			// The same seam the crossing kill uses: winner written first, then the flip.
			FinishMatch(Holder);
		}
	}
}

void ABNGameMode::GetModeAmbitions(TArray<FAIBModeAmbition>& OutAmbitions) const
{
	if (bHillEnabled)
	{
		// BaseUtility 1.2 reads as "the objective can outrank a fight it is losing anyway":
		// Engage peaks at 1.0×considerations, so the hill wins only when engagement's own
		// facts sag — visible target and healthy nerve still take the fight.
		FAIBModeAmbition& Hold = OutAmbitions.AddDefaulted_GetRef();
		Hold.AmbitionTag = BNAIBTags::Ambition_Mode_Hold;
		Hold.BaseUtility = 1.2f;
		Hold.ObjectiveKind = BNAIBTags::POI_Hill;
	}

	// TEAMS (founder, 27 Aug): the regroup want, teams matches only. BaseUtility 1.0 with
	// urgency capped at 0.55 (below the EMPTY hill's 0.6) orders the wants deliberately:
	// Engage/Seek/Search with anything real to act on beat Rally, Rally beats the 0.2
	// Roam floor — an isolated bot walks toward its team instead of wandering alone,
	// which is the measured teams-ON collapse (12 kills/461 switches) attacked at its
	// mechanism: halved target density punished lone wanderers most. FFA-inert by
	// CONSTRUCTION twice over: no ambition registered here, and no POI published there.
	if (bTeamsEnabled)
	{
		FAIBModeAmbition& Rally = OutAmbitions.AddDefaulted_GetRef();
		Rally.AmbitionTag = BNAIBTags::Ambition_Mode_Rally;
		Rally.BaseUtility = 1.0f;
		Rally.ObjectiveKind = BNAIBTags::POI_Ally;
	}
}

float ABNGameMode::GetObjectiveUrgency(const AActor* Bot, FGameplayTag AmbitionTag) const
{
	// TEAMS (founder, 27 Aug): how loudly "regroup" calls = how ALONE this bot is, read
	// the way a human reads their teammate radar — HUD-grade, no perception crosses.
	// Zero inside RallyNearUU (arrival quiets the want — the POI's ReachRadiusUU is the
	// same number), scaling to the 0.55 cap at RallyFarUU. No living teammate at all
	// (everyone dead this instant) reads fully alone.
	if (AmbitionTag == BNAIBTags::Ambition_Mode_Rally)
	{
		if (!bTeamsEnabled)
		{
			return 0.f;
		}
		const APawn* RallyPawn = Cast<APawn>(Bot);
		const ABNPlayerState* RallyPS = RallyPawn ? RallyPawn->GetPlayerState<ABNPlayerState>() : nullptr;
		const FGenericTeamId RallyTeam = RallyPS ? RallyPS->GetGenericTeamId() : FGenericTeamId::NoTeam;
		UWorld* World = GetWorld();
		if (!RallyPawn || !World || RallyTeam == FGenericTeamId::NoTeam)
		{
			return 0.f;
		}
		float NearestAllySq = TNumericLimits<float>::Max();
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			const APawn* Candidate = *It;
			if (!Candidate || Candidate == RallyPawn)
			{
				continue;
			}
			const ABNPlayerState* PS = Candidate->GetPlayerState<ABNPlayerState>();
			const FGenericTeamId Team = PS ? PS->GetGenericTeamId() : FGenericTeamId::NoTeam;
			if (!BNTeams::AreFriendly(RallyTeam, Team))
			{
				continue;
			}
			const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APawn*>(Candidate));
			if (!ASC || ASC->HasMatchingGameplayTag(BNTags::State_Dead))
			{
				continue;
			}
			NearestAllySq = FMath::Min(NearestAllySq,
				static_cast<float>(FVector::DistSquared(RallyPawn->GetActorLocation(), Candidate->GetActorLocation())));
		}
		// NO living teammate = NOTHING to rally to: zero, not maximum-alone (BN22 W-REVIEW
		// L1 — a full-alone read with zero POIs published fed the fail-loudly path a
		// Warning per suppression cycle through every team-wipe window).
		if (NearestAllySq >= TNumericLimits<float>::Max())
		{
			return 0.f;
		}
		const float NearestAlly = FMath::Sqrt(NearestAllySq);
		const float Alone = FMath::Clamp(
			(NearestAlly - BNAIB::RallyNearUU) / FMath::Max(1.f, BNAIB::RallyFarUU - BNAIB::RallyNearUU), 0.f, 1.f);
		// FLOORED at 0.3 the moment it is nonzero (BN22 W-REVIEW M1): a 0-to-0.55 ramp
		// dipped below Roam's 0.2 floor at ~1470uu, so a mid-rally bot lost the want a
		// kilometre short and hovered in an annulus around its team — half a regroup.
		// Exactly 0 inside the near radius stays: arrival still quiets the want cleanly.
		return Alone > 0.f ? FMath::Lerp(0.3f, 0.55f, Alone) : 0.f;
	}

	if (!bHillEnabled || AmbitionTag != BNAIBTags::Ambition_Mode_Hold)
	{
		return 0.f; // "the mode does not care" — the contract's own zero.
	}

	// HUD-grade only: held/contested/empty is what the objective marker on every human's
	// screen says. The one per-bot term is "is the holder me", which the bot knows by
	// standing there. No position, no identity of the OTHER holder leaks through a float.
	const APawn* BotPawn = Cast<APawn>(Bot);
	const ABNPlayerState* BotPS = BotPawn ? BotPawn->GetPlayerState<ABNPlayerState>() : nullptr;
	const ABNPlayerState* Holder = HillHolder.Get();

	if (Holder && BotPS && Holder == BotPS)
	{
		return 0.35f; // Keep holding, but a fight for your life may outrank standing still.
	}
	// TEAMS (BN15): the cached holder is one member of the sole holding TEAM, and "held by
	// my team" is "held by me" for urgency — a teammate banking seconds is not the loudest
	// state. Same NoTeam guard as everywhere: unassigned never reads as an ally.
	if (bTeamsEnabled && Holder && BotPS
		&& BNTeams::AreFriendly(Holder->GetGenericTeamId(), BotPS->GetGenericTeamId()))
	{
		return 0.35f;
	}
	if (bHillContested)
	{
		return 0.75f; // You are probably in that contest — win it.
	}
	if (Holder)
	{
		return 0.9f;  // Someone else is banking seconds RIGHT NOW — loudest state.
	}
	return 0.6f;      // Empty hill: free points, worth a walk, not worth fleeing a duel.
}
