//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_NetTypes.c
//! BFCG_Garage - сетевые типы меню: запись гаража + коды результатов
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//! Changes: групповые типы удалены (только персональный гараж).
//------------------------------------------------------------------------------------------------

//--- Одна запись меню гаража ---
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

//--- Коды результатов (RPC -> клиент) ---
enum BFCG_GARAGE_EParkResult
{
	SUCCESS,
	FAILED,
	NO_VEHICLE,
	MULTI_VEHICLE,
	GARAGE_FULL,
	TOO_FAR,
	OWNED_BY_ANOTHER_PLAYER,
	VEHICLE_OCCUPIED
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
