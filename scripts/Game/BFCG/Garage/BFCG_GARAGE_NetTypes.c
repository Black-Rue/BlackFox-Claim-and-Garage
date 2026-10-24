/*
 * Garage RPC result codes and client-side list entries.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

class BFCG_GARAGE_MenuEntry
{
	string m_sVehicleID;
	string m_sPrefab;
	bool m_bAvailable;

	void BFCG_GARAGE_MenuEntry()
	{
		m_sVehicleID = string.Empty;
		m_sPrefab = string.Empty;
		m_bAvailable = true;
	}
}

// Enum ordering is part of the client/server RPC contract; keep both sides in sync.
enum BFCG_GARAGE_EParkResult
{
	SUCCESS,
	FAILED,
	NO_VEHICLE,
	MULTI_VEHICLE,
	GARAGE_FULL,
	TOO_FAR,
	OWNED_BY_ANOTHER_PLAYER,
	VEHICLE_OCCUPIED,
	CLAIMS_RESTORING,
	IDENTITY_NOT_READY,
	VEHICLE_CHANGED
}

enum BFCG_GARAGE_EUnpackResult
{
	SUCCESS,
	FAILED,
	NOT_FOUND,
	NOT_ALLOWED,
	BLOCKED,
	TOO_FAR,
	UNAVAILABLE,
	WORLD_CLAIM_LIMIT,
	CLAIMS_RESTORING,
	NO_PARKING_SPOTS
}

enum BFCG_GARAGE_EDeleteResult
{
	SUCCESS,
	FAILED,
	NOT_FOUND,
	NOT_ALLOWED,
	TOO_FAR
}
