/*
 * Player identity and local interaction-distance helpers.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

class BFCG_GARAGE_PlayerHelper
{

	static const float MAX_INTERACTION_DISTANCE = 2.5;

	static string GetUIDFromEntity(IEntity playerEntity)
	{
		if (!Replication.IsServer() || !playerEntity)
			return string.Empty;
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		if (playerID <= 0)
			return string.Empty;
		return BFCG_VehicleClaimState.ResolvePlayerIdentity(playerID);
	}

	static string GetLocalUID()
	{
		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		if (!localEntity)
			return string.Empty;
		return GetUIDFromEntity(localEntity);
	}

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
