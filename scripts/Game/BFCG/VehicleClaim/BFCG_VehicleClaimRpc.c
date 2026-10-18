/*
 * BlackFox client presentation cache for vehicle claims.
 * Owner RPCs expose a personalised ownership flag, not the persistent owner identity.
 * All authoritative checks and mutations remain on the server.
 */

class BFCG_VehicleClaimClientEntry
{
	RplId m_VehicleId;
	bool m_bIsOwner;
	bool m_bLocked;
	int m_iExpiresAt;
}

class BFCG_VehicleClaimClientCache
{
	protected static ref array<ref BFCG_VehicleClaimClientEntry> s_aEntries = {};

	static void Clear()
	{
		s_aEntries.Clear();
	}

	static void Set(RplId vehicleId, bool hasClaim, bool isOwner, bool locked, int expiresAt)
	{
		if (vehicleId == RplId.Invalid())
			return;

		int index = Find(vehicleId);
		if (!hasClaim || expiresAt <= System.GetUnixTime())
		{
			if (index >= 0)
				s_aEntries.Remove(index);
			return;
		}

		BFCG_VehicleClaimClientEntry entry;
		if (index >= 0)
			entry = s_aEntries[index];
		else
		{
			entry = new BFCG_VehicleClaimClientEntry();
			entry.m_VehicleId = vehicleId;
			s_aEntries.Insert(entry);
		}

		entry.m_bIsOwner = isOwner;
		entry.m_bLocked = locked;
		entry.m_iExpiresAt = expiresAt;
	}

	static bool Get(RplId vehicleId, out bool isOwner, out bool locked, out int expiresAt)
	{
		isOwner = false;
		locked = false;
		expiresAt = 0;
		if (vehicleId == RplId.Invalid())
			return false;

		int index = Find(vehicleId);
		if (index < 0)
			return false;

		BFCG_VehicleClaimClientEntry entry = s_aEntries[index];
		if (entry.m_iExpiresAt <= System.GetUnixTime())
		{
			s_aEntries.Remove(index);
			return false;
		}

		isOwner = entry.m_bIsOwner;
		locked = entry.m_bLocked;
		expiresAt = entry.m_iExpiresAt;
		return true;
	}

	protected static int Find(RplId vehicleId)
	{
		for (int i = 0; i < s_aEntries.Count(); i++)
		{
			if (s_aEntries[i].m_VehicleId == vehicleId)
				return i;
		}

		return -1;
	}
}

modded class SCR_PlayerController
{
	void BFCG_VehicleClaimSendState(RplId vehicleId, bool hasClaim, bool isOwner, bool locked, int expiresAt)
	{
		if (!Replication.IsServer())
			return;

		Rpc(BFCG_VehicleClaimRpcDo_State, vehicleId, hasClaim, isOwner, locked, expiresAt);
	}

	void BFCG_VehicleClaimClearClientCache()
	{
		if (!Replication.IsServer())
			return;

		Rpc(BFCG_VehicleClaimRpcDo_Clear);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_VehicleClaimRpcDo_State(RplId vehicleId, bool hasClaim, bool isOwner, bool locked, int expiresAt)
	{
		BFCG_VehicleClaimClientCache.Set(vehicleId, hasClaim, isOwner, locked, expiresAt);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_VehicleClaimRpcDo_Clear()
	{
		BFCG_VehicleClaimClientCache.Clear();
	}
}
