
---

## Post-close change 2026-08-23 (d) — looting is a crime if you are seen

User call: *"using the loot action in a truck should be illegal if seen"*.

Built on the shipped **illegal-action window**, not on a new mechanism. `OVT_PlayerWantedComponent`
already has `BeginIllegalAction(reason, seconds)` / `EndIllegalAction()`, whose whole point is *"was
anyone watching AT ANY POINT while you did it"* rather than a one-shot at the start or the end — the
uprising and base-assault holds use it through `OVT_IllegalActionComponent`.

**Armed server-side, in the job engine, not in the user action.** `StartLootJob` opens the window and
`StepJob` **re-arms it at every LOOT chunk**, because a loot run's length is the size of the
battlefield and is not known when it starts. FINISH and ABORT close it, so the player stops being
catchable the moment the run ends rather than carrying a fixed tail. Putting this on the client action
would have made it skippable; `OVT_ContainerTransferComponent`'s RPC is the wire, and the job engine
is where the act actually happens.

`ClearLootIllegalWindow` closes the window **only when the open one is still ours** — hence the new
`GetIllegalActionReason()` accessor. Without it, a loot run finishing mid-uprising would have wiped
the uprising's window.

Deliberately unchanged: nothing about looting is refused, and being unobserved still costs nothing.
`OnIllegalActionSeen` gates on `m_bIsSeen`, so an empty field is looted for free — and per the fix
recorded in `resistance/wanted-system`, "seen" now works while you are sitting in the truck.

**Gate:** `compile-check.sh` exit 0 (6341 files). 🔴 No suite ran (same harness block as PD9).

**Owed:** the `.st` re-export (the new key renders raw until then) and a play-test.
