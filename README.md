# BlackFox Claim and Garage

Standalone Arma Reforger addon. Project GUID: 6AA35166B9E79609.
Required addon: ArmaReforger (58D0FB3206B6F859) only. Both server and clients need this addon.
No Merak, Dynamic Economy, SEKA, Building System or vehicle-content mod is required.

## Installation in a scenario

1. Add this addon as a dependency to the scenario project.
2. Place Prefabs/BFCG/Garage/BFCG_GARAGE_GaragePoint_Personal.et.
3. Place one or more Prefabs/BFCG/Garage/BFCG_GARAGE_ParkingSpot.et.
4. Match the terminal Garage ID and each spot Linked Garage ID. Defaults are BFCG_Garage_01.
   Use a different ID for each independent location. Position and rotate spots for vehicle clearance.
5. Run a new game session with backend identities available. Initial claim readiness defaults to 30 seconds.
6. Test an empty spot, occupied first spot, full parking, another player's vehicle, and world limit.

GM placement/configuration tools are not included. Prefabs are placed/configured in Workbench.

## Server settings

The server creates $profile:BlackFoxClaimGarage/settings.json on first game start.
An example is included at Configs/BFCG/settings.example.json. Edit the server file with the game stopped,
then start a new session. No live reload. Invalid/unreadable settings disable garage/claim operations.
The server sends display limits to clients; client files cannot change authoritative limits.

- maxWorldClaims = 3 (1..1000): live claimed vehicles per owner; stored garage records do not count.
- maxGarageVehicles = 12 (1..1000): stored vehicles per owner.
- claimDurationHours = 72 (1..8760): absolute expiry, refreshed on garage retrieval.
- enableUnclaimedCleanup = false: optional global unclaimed-vehicle cleanup. Enable only after scenario review.
- unclaimedLifetimeHours = 6 (1..8760): cleanup deadline when enabled.
- initialRestoreDelaySeconds = 30 (0..300).
- proxyScanSeconds = 30 (5..3600).
- spawnClearanceMeters = 3 (1..50): minimum retrieval query radius; the spot radius can increase it.
- spawnReservationMilliseconds = 1000 (100..10000).
- maxGarageRequestDistanceMeters = 10 (2.5..100).

Add BFCG_ExcludedVehicleComponent to temporary/rental/event vehicles to prevent claiming, garage storage,
and optional cleanup. It does not disable native world persistence for that entity.

## Persistence contract

World vehicles use vanilla Persistence/Vehicle collection and native serializers for transforms,
fuel, damage, inventory and controllers. This addon extends the vanilla Vehicle.conf with its claim serializer
and explicit SelfSpawn. No position-based claims.json overlay is used.
The scenario must run vanilla persistence and actually save/commit its session using its normal save policy.
This addon requests transient vehicle saves; it does not force independent storage commits or enable
the scenario's autosave policy. Other mods replacing vehicle persistence rules may need integration.

Garage records live in $profile:BlackFoxClaimGarage/garage_data.txt (JSON).
Use a separate profile per server/scenario/save family. Do not copy live save data into the addon.
Unreadable/invalid/duplicate records disable writes. Failed writes do not delete a live parked vehicle.
Garage records and native world saves are not one atomic transaction: backup and crash tests are required.

## Garage behavior

Retrieval uses the first free spot at the current terminal, with the spot's position/orientation and unit scale.
Vehicles and characters block the query sphere. This is not full prefab-vs-world collision testing;
allow enough space for trucks and adjacent spots. Occupied vehicles cannot be parked.
Park (Clear Inventory) discards stored cargo; retrieval creates a fresh prefab with its default state/loadout.
The garage does not preserve fuel, damage or custom vehicle state. Native persistence preserves these
for vehicles remaining in the world, which is a separate path.

## Compatibility and overrides

All project classes/resources use BFCG_ or new resource GUIDs. Merak source files are not changed.
Do not run competing claim/garage systems on the same vehicles without an integration test.
Intentional vanilla overrides:
- Configs/System/chimeraMenus.conf (C747AFB6B750CE9A): add the BFCG_GarageMenu preset only.
- Configs/Systems/Persistence/Configuration/Vehicle/Vehicle.conf (C676762C801B4B31): retain vanilla serializers,
  add BFCG_VehicleClaimComponentSerializer and SelfSpawn 1. Car/Tracked/Helicopter inherit this configuration.
Custom game modes must preserve the SCR_BaseGameMode lifecycle calls.

Before removing this addon, retrieve/export stored garage vehicles and retain backups; disabling the addon
cannot turn garage records back into world entities automatically.

## Validation

See VALIDATION.md. Compilation is not proof of server, persistence or multiplayer correctness.
Nothing is published or deployed by this source preparation.
