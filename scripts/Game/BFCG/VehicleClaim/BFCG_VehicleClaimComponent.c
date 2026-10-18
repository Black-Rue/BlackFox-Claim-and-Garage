/*
 * BFCG Vehicle Claim for Arma Reforger 1.8.
 * Forked in concept from the replicated lock and absolute Unix expiry
 * behaviour of CarKey2 by NewbKodii. Claim fields now extend the standard
 * VehicleControllerComponent and contain no CarKey2 runtime dependency.
 * CarKey2 Workshop ID: 66D61CE15CC684A6; license: Arma Public License.
 */

// Claim data is stored on the engine-provided VehicleControllerComponent.
// See BFCG_VehicleClaimLifecycle.c. This avoids requiring every third-party
// vehicle prefab to inherit a custom component from Vehicle_Base.et.
