//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_PlayerHelper.c
//! BFCG_Garage - UID- и дистанс-хелперы
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//------------------------------------------------------------------------------------------------

class BFCG_GARAGE_PlayerHelper
{
	// Макс. дистанция игрок<->пульт для показа действия
	static const float MAX_INTERACTION_DISTANCE = 2.5;

	// Identity-UID игрока по entity. Server-only: GetPlayerIdentityId доступен
	// только на authority.
	static string GetUIDFromEntity(IEntity playerEntity)
	{
		if (!playerEntity)
			return string.Empty;
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		if (playerID <= 0)
			return string.Empty;
		BackendApi backend = GetGame().GetBackendApi();
		if (!backend)
			return string.Empty;
		return backend.GetPlayerIdentityId(playerID);
	}

	// UID локально управляемого игрока (клиентская сторона, для UI)
	static string GetLocalUID()
	{
		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		if (!localEntity)
			return string.Empty;
		return GetUIDFromEntity(localEntity);
	}

	// Быстрый squared-distance чек "игрок у пульта"
	static bool IsUserNearOwner(IEntity user, IEntity owner)
	{
		if (!user || !owner)
			return false;
		vector diff = user.GetOrigin() - owner.GetOrigin();
		float distSq = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];
		float maxSq = MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE;
		return distSq <= maxSq;
	}
}
