# BN27 — main does not compile: BNAdversarialAgent (contract_gap)

> STATUS: blocked — contract_gap, owner of `Source/BreachpointNext/QA/` must fix (28 Aug 2026)

## The break

```
Source/BreachpointNext/QA/BNAdversarialAgent.cpp:167:24:
  error: no matching function for call to 'CreateUObject'
```

```cpp
TM.SetTimer(EndTimer, FTimerDelegate::CreateUObject(this, &ABNAQAController::StopRun,
    FString(TEXT("duration elapsed"))), DurationSeconds, false);
```

`ABNAQAController::StopRun` is declared `void StopRun(const FString& Reason)`
(`BNAdversarialAgent.h:85`). The payload overload cannot bind an `FString` payload to a
`const FString&` parameter here. Taking the payload **by value** (`void StopRun(FString Reason)`)
is the usual fix; binding a lambda is the other.

## Why this is filed rather than fixed

`Source/BreachpointNext/QA/` is not in this session's packet (`.claude/active-packet.json`
owns Characters, Weapons, AbilitySystem, Tests, Config, Tools/bn, docs). Law 5: *"Blocked?
File a contract_gap in the ticket and STOP — never edit shared code to unblock."* The fix is
one line, and it is the QA packet's line to write.

## Provenance — this is not a merge artifact

`3b0cc8b4` ("#9: the probe that plays to lose — an adversarial QA agent inside the game")
landed on main immediately before `faaf0d94`, arriving in the `git pull --rebase` that ran
during that push. The build succeeded on the commit before it. Working tree is otherwise
clean: the only local modification is `Plugins/AIBot/Source/AIBot/Execution/AIBTreeAuthoring.cpp`,
which is this session's and unrelated.

## What it blocks

Everything that needs a compile, for every session on this branch — rung 1, rung 2, and any
editor-side verification that depends on current code. Specifically parked here: the AIBot
tree read-back that prints each Retreat task's arbitration numbers, which exists to answer
"the bots behave the same" with evidence instead of a task count.

## Done when

- [ ] `Tools/run-ubt.sh` passes on the Editor and Game targets again
