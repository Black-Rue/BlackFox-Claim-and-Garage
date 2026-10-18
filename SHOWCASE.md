# Claim and Garage showcase

World: `worlds/BFCG_Showcase.ent`
Scenario: `Missions/BFCG_Showcase.conf`

Open the world in Workbench World Editor and use Play Game. In the game scenario list,
look for **BlackFox Claim and Garage - Showcase**.

The scene inherits the vanilla MpTest_Empty world and its real MPTestTerrain, required
by GM terrain bounds checks. A flat 100 x 100 m platform is centered at X/Z 128/128, Y 20.
It contains automatic US player spawn,
one garage terminal, two linked parking spots and an M151A2 transport on the left spot.
Both spots use Garage ID `BFCG_Garage_01`. The right spot starts empty.
No AI opponents, objectives, economy or Merak resources are included.

On a clean Workbench profile, the engine may request script authorization for vanilla
operations such as BaseContainerTools.LoadContainer. Review those prompts manually.

## Manual test

1. Start a new session. Allow the configured initial restore delay (30 seconds by default).
2. Walk to the M151A2 and claim it. Return to the terminal while the empty vehicle remains on its spot.
3. Open Garage and choose **Park (Clear Inventory)**. This removes cargo and stores the vehicle.
4. Retrieve the stored vehicle. It must use a free spot linked to this terminal.
5. Block the first spot and repeat: retrieval must use the second spot.
6. Block both spots: retrieval must refuse without removing the stored record.
7. Add further vanilla vehicles in WB to check the world limit of three claims and the fourth-claim refusal.

The showcase does not change server limits and does not enable automatic world saves.
Use a separate test profile. Garage data uses `$profile:BlackFoxClaimGarage/garage_data.txt`.
Claim identities in an offline WB session depend on the available backend identity.

The test world is intended for garage/claim interaction checks. Full persistence restart,
crash recovery and multiplayer acceptance still follow `VALIDATION.md`.

## GM compatibility

The real terrain comes from the vanilla parent world. Do not remove it or replace it
with a mesh collider: GM placement calls GetTerrain(0, 0).GetTerrainBoundBox().
The showcase keeps its original automatic player spawn and garage systems.
A vanilla SCR_EditorSettingsEntity marks GM as part of this test scenario rules.
In local Workbench play mode, use the available editor controls to open GM.
Dedicated-server GM roles and permissions still follow the host's settings.

If the world was already open when updated, close the game session and reload the world.
This scene update does not reset profile data or saved garage records.

Validated locally: automatic player spawn, terrain bounds check (inside=true, outside=false),
opening GM and placing an M151A2 on the free parking spot without the previous null exception.
