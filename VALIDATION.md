# Validation and release checks

[Back to README](README.md) · [Showcase guide](SHOWCASE.md)

This checklist separates recorded results from tests that should be repeated for each release or scenario integration.

## Recorded results

| Evidence | Result | Scope |
|---|---|---|
| Latest recorded local compilation, 18 October 2026 | 0 script errors; 15 script warnings. | 14 warnings originate in vanilla scripts; one deprecated `BackendApi.GetPlayerIdentityId` call is retained for real BI identity semantics. |
| Local showcase GM check | Player spawned; GM opened and placed an M151A2 without the previous terrain-bounds null exception. | Earlier local world validation; not a full multiplayer test. |
| Server testing reported by the project owner, 18 October 2026 | Owner confirmed the mod works correctly after the UID consistency fix. | Individual cases, client count and persistence/crash-recovery coverage were not supplied. |

Unchecked items below are a reusable test matrix. They are not a claim that a reported server test failed, or that each case was included in it.

## Build and resources

- [ ] Compile with only Arma Reforger and this addon; record game/addon versions and all diagnostics.
- [ ] Load terminal and parking prefabs, materials and garage layouts.
- [ ] Launch the [showcase](SHOWCASE.md) and confirm player spawn and GM placement within terrain bounds.

## Identity and multiplayer

- [ ] Run a dedicated server with two clients; join during/after native loading and reconnect.
- [ ] Claim, park and retrieve using the same player. Confirm ownership remains consistent.
- [ ] Verify another player cannot park the owner's claimed vehicle.
- [ ] Confirm player IDs reused after disconnect do not retain another player's cached identity.
- [ ] Verify operations with unavailable backend identity are refused rather than assigning a synthetic owner.

## Limits and configuration

- [ ] With defaults, claim three world vehicles and confirm a fourth is refused.
- [ ] Park a claimed vehicle and confirm the freed world slot can be used.
- [ ] Confirm stored garage records do not count toward the world claim limit.
- [ ] Test non-default limits, such as `2/5`; client labels must match server enforcement.
- [ ] Verify malformed and out-of-range settings block operations without rewriting the file.
- [ ] Verify optional cleanup remains inactive by default; test its timeout and exclusion marker separately when enabled.

## Parking and retrieval

- [ ] Park before restoration completes and confirm a restoration message is shown.
- [ ] Retrieve at the first free spot belonging to the current terminal.
- [ ] Block that spot and confirm the next linked free spot is used.
- [ ] Block all linked spots and confirm refusal without removing the stored record.
- [ ] Test characters/AI in the clearance area, large vehicles and simultaneous retrieval requests.
- [ ] Refuse parking of an occupied vehicle without deleting any occupant.
- [ ] Confirm parking discards inventory and retrieval uses prefab defaults.

## Persistence and recovery

Use an isolated profile and retain backups for these tests.

- [ ] Save and restart a native persistence scenario: verify world vehicle identity, ownership, expiry, fuel, damage and inventory.
- [ ] Confirm an actual save commit/reload; a transient `Save()` request alone is insufficient evidence.
- [ ] Verify garage records survive restart and remain accessible to the original real BI identity.
- [ ] Test malformed/unwritable garage storage; a failed park write must preserve the live vehicle.
- [ ] Test a forced crash around parking/retrieval and check for lost or duplicated vehicles across both storage paths.
- [ ] Test addon removal/rollback with required vehicles retrieved and garage/world/profile backups retained.

## Destroyed vehicles

The wreck lifecycle change requires the following runtime checks; earlier owner testing predates this change.

- [ ] Destroy a claimed vehicle: owner cache clears and a new world claim is immediately available.
- [ ] Reject new claims and garage parking for wrecks; keep repairable vehicles claimable and parkable.
- [ ] Remove claimed and unclaimed wrecks at `destroyedVehicleLifetimeSeconds`, including with optional unclaimed cleanup disabled.
- [ ] Save and restart before the deadline: restore a wreck with the same absolute deadline, never a usable prefab.
- [ ] Save and restart after cleanup: the removed vehicle must not return.
- [ ] Load old version-1 claim metadata and settings without the new field; preserve old limits and use the default wreck lifetime.
- [ ] Test already destroyed vehicles, missing events, occupied seats, nearby players, exclusions and two clients.
- [ ] Confirm the host scenario actually selects a rule containing both claim and damage serializers.
- [ ] Check the known crash-before-save limitation using an isolated profile.

## Release record

Record the game/addon versions, scenario, server/client configuration, tested cases, results and relevant log timestamps. Preserve unresolved warnings and limitations in the release notes. Do not publish credentials, server identity files or private player data with test evidence.
