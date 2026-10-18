# Showcase guide

[Back to README](README.md) · [Validation checklist](VALIDATION.md)

A small test scenario for vehicle claiming, personal garage storage and parking-spot allocation.

## Launch

| Environment | How to start |
|---|---|
| In game | Select **BlackFox Claim and Garage - Showcase** in the scenario list. |
| Workbench | Open [worlds/BFCG_Showcase.ent](worlds/BFCG_Showcase.ent) in World Editor and select **Play Game**. |

The scenario configuration is [Missions/BFCG_Showcase.conf](Missions/BFCG_Showcase.conf).

Use a separate test profile. On a clean Workbench profile, the engine may request script authorization for vanilla operations such as `BaseContainerTools.LoadContainer`; review those requests manually.

## Scene layout

- Automatic US player spawn.
- One garage terminal.
- Two parking spots linked to Garage ID `BFCG_Garage_01`.
- An M151A2 transport on the left spot; the right spot starts empty.
- A flat 100 × 100 m platform centred at X/Z `128/128`, Y `20`.

The world inherits the vanilla `MpTest_Empty` world and its real `MPTestTerrain`. There are no AI opponents, objectives or economy systems in the scene.

## Basic garage test

1. Start a new session and wait for data restoration: 30 seconds by default, plus any active native save loading.
2. Claim the M151A2. Leave it on its parking spot and exit the vehicle.
3. Return to the terminal, open **Garage** and select **Park (Clear Inventory)**.
4. Confirm that the vehicle disappears from the world and appears as a garage card.
5. Select the card to retrieve it. Confirm that the vehicle appears at a free spot linked to this terminal and remains owned by you.

> Parking discards inventory. Retrieval uses prefab defaults and does not restore the vehicle's previous fuel, damage or cargo state.

## Additional checks

| Check | Expected result |
|---|---|
| Block the first spot and retrieve a stored vehicle. | The second free spot is used. |
| Block both spots and request retrieval. | Retrieval is refused; the stored record remains. |
| Try parking during initial restoration. | An English restoration message is shown. |
| Try parking a vehicle claimed by another player. | Parking is refused. |
| Try parking with a character or AI still inside. | Parking is refused; occupants remain in the world. |
| Claim three world vehicles, then attempt a fourth. | The fourth claim is refused with default settings. |
| Park one of those vehicles and try claiming again. | The garage record does not consume a world claim slot. |

Add further vehicles through Workbench or the available GM editor to test the claim limit. Use real backend player identities for ownership tests; offline Workbench sessions may not have an identity available.

## Game Master and persistence

The scenario includes vanilla editor settings for local GM use. Dedicated-server GM access still follows the host's roles and permissions.

Keep the inherited terrain: GM placement needs real terrain bounds, and a platform mesh alone cannot provide them.

The showcase does not enable automatic world saves. Garage data uses `$profile:BlackFoxClaimGarage/garage_data.txt`; world persistence follows the host scenario's actual save/commit policy. See [VALIDATION.md](VALIDATION.md) for restart and recovery tests.

After changing scripts or world resources, start a fresh test session. A claim created with an earlier, incorrect test identity is not automatically reassigned to another identity.
