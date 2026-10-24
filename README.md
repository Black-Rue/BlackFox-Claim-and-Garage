# BlackFox Claim and Garage

Vehicle claiming and personal garage storage for **Arma Reforger**. A standalone addon for scenario creators and server owners, with configurable limits and an included showcase.

[Workshop](https://reforger.armaplatform.com/workshop/6AA35166B9E79609-BlackFoxClaimandGarage) · [GitHub](https://github.com/Black-Rue/BlackFox-Claim-and-Garage) · [Showcase guide](SHOWCASE.md) · [Testing](VALIDATION.md) · [Credits and licence notices](NOTICE.md)

## Features

- Claim vehicles and control their locks using persistent Bohemia player identities.
- Store vehicles in a personal garage and retrieve them through configured terminals.
- Retrieve at a free parking spot linked to the **current terminal**; refuse retrieval if all linked spots are occupied.
- Reject parking of occupied or destroyed vehicles and vehicles claimed by another player.
- Release destroyed vehicles from claims and remove their wrecks after a separate persistent deadline.
- Enforce limits on the server, with English notifications and mouse/gamepad garage controls.
- Integrate world vehicles and claim metadata with vanilla persistence.

| Default rule | Value |
|---|---|
| Claimed vehicles outside the garage, per player | 3 |
| Vehicles stored in the garage, per player | 12 |
| Claim lifetime | 72 hours |
| Initial data restoration delay | 30 seconds |

Garage records do not count toward the world claim limit. Retrieval renews the claim lifetime. These values can be changed in the server settings.

> **Park (Clear Inventory) discards the parked vehicle's inventory.** Retrieval creates a fresh vehicle with its prefab defaults. Garage storage does not preserve fuel, damage or custom vehicle state.

## Requirements

Both server and clients need this addon and the base game. Merak, Dynamic Economy, SEKA, Building System and the original credited mods are not required dependencies.

Addon ID: `6AA35166B9E79609`.

## Quick test

Launch **BlackFox Claim and Garage - Showcase** from the scenario list, or open [worlds/BFCG_Showcase.ent](worlds/BFCG_Showcase.ent) in Workbench and select **Play Game**.

The scene includes a player spawn, one terminal, two parking spots and an M151A2. Wait for initial restoration, claim the vehicle, then use **Park (Clear Inventory)** at the terminal and select its garage card to retrieve it.

See the [showcase guide](SHOWCASE.md) for blocked-spot and limit checks.

## Add a garage to your scenario

1. Add BlackFox Claim and Garage as a dependency of your scenario project.
2. Place [BFCG_GARAGE_GaragePoint_Personal.et](Prefabs/BFCG/Garage/BFCG_GARAGE_GaragePoint_Personal.et).
3. Place one or more [BFCG_GARAGE_ParkingSpot.et](Prefabs/BFCG/Garage/BFCG_GARAGE_ParkingSpot.et) prefabs.
4. Match the terminal's **Garage ID** and each spot's **Linked Garage ID**. The default is `BFCG_Garage_01`; use a different ID for each independent location.
5. Position and rotate the spots with enough clearance for the vehicles your scenario uses.
6. Start a new session with backend player identities available and test parking, retrieval and ownership checks.

Garage prefabs are placed and configured in Workbench. Runtime GM garage placement/configuration tools are not included.

## Server configuration

The server creates this file on first game start:

```text
$profile:BlackFoxClaimGarage/settings.json
```

See [settings.example.json](Configs/BFCG/settings.example.json) for the full example. Edit the server file with the game stopped, then start a new session. Settings are not reloaded live. Invalid or unreadable settings block garage/claim operations without overwriting the existing file.

| Setting | Default | Allowed range | Purpose |
|---|---:|---|---|
| `maxWorldClaims` | `3` | 1–1000 | Live claimed vehicles per player; garage records excluded. |
| `maxGarageVehicles` | `12` | 1–1000 | Stored vehicles per player. |
| `claimDurationHours` | `72` | 1–8760 | Claim lifetime in hours; renewed on retrieval. |
| `enableUnclaimedCleanup` | `false` | Boolean | Enable optional global cleanup of unclaimed vehicles. |
| `unclaimedLifetimeHours` | `6` | 1–8760 | Unclaimed-vehicle lifetime when cleanup is enabled. |
| `destroyedVehicleLifetimeSeconds` | `1200` | 60–86400 | Wreck lifetime after final destruction, independent of optional unclaimed cleanup. |
| `initialRestoreDelaySeconds` | `30` | 0–300 | Initial delay before checking native restore readiness. |
| `proxyScanSeconds` | `30` | 5–3600 | Vehicle interaction-proxy scan interval. |
| `spawnClearanceMeters` | `3.0` | 1–50 | Minimum retrieval clearance radius; spot radius may increase it. |
| `spawnReservationMilliseconds` | `1000` | 100–10000 | Short reservation to prevent overlapping retrieval requests. |
| `maxGarageRequestDistanceMeters` | `10.0` | 2.5–100 | Maximum player-to-terminal request distance. |

Clients receive display values; their local files cannot change the server's limits.

## Persistence and compatibility

**Vehicles in the world** use vanilla vehicle persistence for transforms, fuel, damage and inventory, with the addon extending the native vehicle controller serializer for claim metadata. The host scenario must run vanilla persistence and commit saves through its normal save policy. This addon does not enable autosave or force independent save commits.

Destroyed vehicles immediately lose their claim and stop counting toward the world limit. Only the final `DESTROYED` damage state starts wreck cleanup; repairable damage does not. Wrecks use a separate server timer (20 minutes by default), with an absolute deadline serialized in claim metadata. A successful native save commit preserves that deadline across restart. Cleanup waits while a character occupies a seat. Nearby players do not extend this separate timer. Excluded vehicles retain their existing lifecycle.

The scenario must preserve this addon serializer and native damage serializers in its active persistence rules. A saved wreck marker prevents renewed claims and garage parking even if another persistence override loses damage. The marker retains the original cleanup deadline; it does not apply damage, reignite or delete a vehicle early. Correct visual and mechanical wreck restoration requires the native damage serializers. Legacy records without damage or a wreck marker cannot identify earlier destroyed vehicles. A crash before the scenario commits its save can still restore the previous state; this addon does not force a global save per destruction.

**Garage records** are stored separately as JSON:

```text
$profile:BlackFoxClaimGarage/garage_data.txt
```

Use a separate profile per server/scenario/save family. Keep backups of both garage records and native saves: these two storage paths are not one atomic transaction. Unreadable, invalid or duplicate garage records disable writes; failed writes do not delete the live vehicle being parked.

Additional integration notes:

- Retrieval clearance checks vehicles and characters within a sphere; allow room for large vehicles and surrounding geometry.
- Add `BFCG_ExcludedVehicleComponent` to rental/event vehicles to exclude them from Claim, Garage and optional cleanup. It does not disable vanilla persistence.
- Custom game modes must preserve the `SCR_BaseGameMode` lifecycle calls.
- Mods replacing vehicle persistence or providing competing claim/garage systems require integration testing.
- Before removing the addon, retrieve the vehicles you need and back up the profile. Garage records do not automatically become world entities when the addon is disabled.

The addon intentionally extends `Configs/System/chimeraMenus.conf` with its menu preset and `Configs/Systems/Persistence/Configuration/Vehicle/Vehicle.conf` with its claim serializer and `SelfSpawn` setting.

## Original authors and acknowledgements

| Original project | Author / team | Contribution |
|---|---|---|
| [GTG_Garage](https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2) | **Maj0rrush / GTG-Team** | Original garage implementation adapted for this addon. |
| [CarKey2](https://reforger.armaplatform.com/workshop/66D61CE15CC684A6) | **NewbKodii** | Adapted vehicle actions and serializer patterns, plus behavioural references for claiming, locking and expiry. |

Standalone integration and additional development by **BlackFox**. Adapted code, behavioural references and original BlackFox code are distinguished in source headers. See [NOTICE.md](NOTICE.md) for attribution and Arma Public License notices.

## Test status and issue reports

The project owner reported successful server testing after the UID consistency fix on **18 October 2026**. The detailed test matrix is maintained in [VALIDATION.md](VALIDATION.md); this report does not mark every persistence or crash-recovery case as tested.

Report problems through [GitHub Issues](https://github.com/Black-Rue/BlackFox-Claim-and-Garage/issues). Include the game/addon version, scenario, reproduction steps and relevant log excerpts. Remove credentials and private player/server data before posting.

## Parking candidates by active Claim

The terminal first searches for vehicles with the requesting player's active Claim; if none are found, it offers the first unclaimed vehicle. Linked spots retain their existing iteration order; arrival time is not tracked. After a successful park, the menu reloads and shows the next owned candidate. Vehicles with an expired Claim are treated as unclaimed. Vehicles with another player's active Claim, destroyed vehicles and excluded vehicles are not candidates. All vehicles still count toward the multiple-vehicles-on-one-spot restriction; passengers still prevent parking. The server rechecks ownership, occupancy and the displayed vehicle replication ID on every park request. A changed candidate requires another click after refresh. Client and server must use the same updated addon because the garage RPC signatures changed.
