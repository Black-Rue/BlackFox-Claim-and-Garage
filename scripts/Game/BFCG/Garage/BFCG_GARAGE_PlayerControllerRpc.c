/*
 * Garage request validation and reliable responses to the requesting player.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

modded class SCR_PlayerController
{
    // Send display values to this controller's owner; server rules always use server settings.
    void BFCG_SendSettings()
    {
        if (!Replication.IsServer())
            return;
        BFCG_Settings cfg = BFCG_Settings.Get();
        Rpc(BFCG_ReceiveSettings, cfg.maxWorldClaims, cfg.maxGarageVehicles, cfg.claimDurationHours);
    }
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void BFCG_ReceiveSettings(int worldLimit, int garageLimit, int hours)
    {
        BFCG_Settings.ApplyDisplaySettings(worldLimit, garageLimit, hours);
    }

	protected static const float BFCG_GARAGE_MAX_REQUEST_DISTANCE = 10.0;

	void BFCG_GARAGE_RequestContents(RplId garageId)
	{
		Rpc(BFCG_GARAGE_RpcAsk_Contents, garageId);
	}

	void BFCG_GARAGE_RequestUnpack(string vehicleID, RplId garageId)
	{
		Rpc(BFCG_GARAGE_RpcAsk_Unpack, vehicleID, garageId);
	}

	void BFCG_GARAGE_RequestDelete(string vehicleID, RplId garageId)
	{
		Rpc(BFCG_GARAGE_RpcAsk_Delete, vehicleID, garageId);
	}

	void BFCG_GARAGE_RequestPark(RplId garageId)
	{
		Rpc(BFCG_GARAGE_RpcAsk_Park, garageId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFCG_GARAGE_RpcAsk_Contents(RplId garageId)
	{
		BFCG_SendSettings();
		string playerUID = BFCG_GARAGE_GetOwnUID();
		BFCG_GARAGE_GarageComponent garageCmp = BFCG_GARAGE_ResolveGarage(garageId);
		if (playerUID.IsEmpty() || !garageCmp || !BFCG_GARAGE_IsNearGarage(garageCmp))
		{
			Rpc(BFCG_GARAGE_RpcDo_ContentsComplete, 0);
			return;
		}

		BFCG_GARAGE_GarageManager manager = BFCG_GARAGE_GarageManager.GetInstance();
		array<BFCG_GARAGE_VehicleData> vehicles = manager.GetVehiclesForPlayer(playerUID);
		foreach (BFCG_GARAGE_VehicleData data : vehicles)
		{
			bool available = BFCG_GARAGE_GarageManager.IsPrefabAvailable(data.m_rPrefab);
			if (!available)
				Print(string.Format("[BFCG_GARAGE_Rpc] Entry unavailable (prefab '%1'): %2", data.m_rPrefab, data.m_sVehicleID), LogLevel.WARNING);
			Rpc(BFCG_GARAGE_RpcDo_AddEntry, data.m_sVehicleID, data.m_rPrefab, available);
		}

		// Send a display snapshot; the parking request performs a fresh server scan.
		BFCG_GARAGE_SpotScan scan = new BFCG_GARAGE_SpotScan();
		string spotPrefab = string.Empty;
		int spotCount = 0;
		if (scan.Scan(garageCmp))
		{
			spotCount = scan.GetVehicleCount();
			EntityPrefabData prefabData = scan.GetVehicle().GetPrefabData();
			if (prefabData)
				spotPrefab = prefabData.GetPrefabName();
		}
		Rpc(BFCG_GARAGE_RpcDo_SetSpotVehicle, spotPrefab, spotCount);

		Rpc(BFCG_GARAGE_RpcDo_ContentsComplete, vehicles.Count());
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFCG_GARAGE_RpcAsk_Park(RplId garageId)
	{
		BFCG_SendSettings();
		if (!BFCG_VehicleClaimPersistence.IsReady())
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.CLAIMS_RESTORING);
			return;
		}

		string playerUID = BFCG_GARAGE_GetOwnUID();
		if (playerUID.IsEmpty())
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.IDENTITY_NOT_READY);
			return;
		}

		BFCG_GARAGE_GarageComponent garageCmp = BFCG_GARAGE_ResolveGarage(garageId);
		if (!garageCmp || !BFCG_GARAGE_IsNearGarage(garageCmp))
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.TOO_FAR);
			return;
		}

		// Rescan on the server because the client's displayed vehicle may have changed.
		BFCG_GARAGE_SpotScan scan = new BFCG_GARAGE_SpotScan();
		if (!scan.Scan(garageCmp))
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.NO_VEHICLE);
			return;
		}
		if (scan.GetVehicleCount() > 1)
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.MULTI_VEHICLE);
			return;
		}
		if (BFCG_VehicleClaimState.IsClaimedByOtherIdentity(scan.GetVehicle(), playerUID))
		{
			Print("[BFCG_GARAGE_Rpc] Park denied: vehicle is claimed by another player", LogLevel.WARNING);
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.OWNED_BY_ANOTHER_PLAYER);
			return;
		}

		BFCG_GARAGE_GarageManager manager = BFCG_GARAGE_GarageManager.GetInstance();
		if (manager.CountForPlayer(playerUID) >= BFCG_Settings.Get().maxGarageVehicles)
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.GARAGE_FULL);
			return;
		}

		if (BFCG_GARAGE_GarageManager.HasOccupants(scan.GetVehicle()))
		{
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.VEHICLE_OCCUPIED);
			return;
		}

		if (manager.ParkVehicle(scan.GetVehicle(), playerUID, scan.GetSpot()))
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.SUCCESS);
		else
			Rpc(BFCG_GARAGE_RpcDo_ParkResult, BFCG_GARAGE_EParkResult.FAILED);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFCG_GARAGE_RpcAsk_Unpack(string vehicleID, RplId garageId)
	{
		BFCG_SendSettings();
		string playerUID = BFCG_GARAGE_GetOwnUID();
		if (playerUID.IsEmpty())
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.FAILED);
			return;
		}

		BFCG_GARAGE_GarageComponent garageCmp = BFCG_GARAGE_ResolveGarage(garageId);
		if (!garageCmp || !BFCG_GARAGE_IsNearGarage(garageCmp))
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.TOO_FAR);
			return;
		}

		BFCG_GARAGE_GarageManager manager = BFCG_GARAGE_GarageManager.GetInstance();
		BFCG_GARAGE_VehicleData data = manager.GetVehicleByID(vehicleID);
		if (!data)
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.NOT_FOUND);
			return;
		}

		if (data.m_sOwnerUID != playerUID)
		{
			Print(string.Format("[BFCG_GARAGE_Rpc] Unpack denied (ownership): %1 by %2", vehicleID, playerUID), LogLevel.WARNING);
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.NOT_ALLOWED);
			return;
		}

		if (!BFCG_VehicleClaimPersistence.IsReady())
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.CLAIMS_RESTORING);
			return;
		}
		if (!BFCG_VehicleClaimState.CanAddWorldClaim(playerUID))
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.WORLD_CLAIM_LIMIT);
			return;
		}

		if (!BFCG_GARAGE_GarageManager.IsPrefabAvailable(data.m_rPrefab))
		{
			Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, BFCG_GARAGE_EUnpackResult.UNAVAILABLE);
			return;
		}

		// The server-resolved terminal selects parking spots; the client supplies no spawn coordinates.
		int result = manager.UnparkVehicle(vehicleID, playerUID, garageCmp);
		Rpc(BFCG_GARAGE_RpcDo_UnpackResult, vehicleID, result);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFCG_GARAGE_RpcAsk_Delete(string vehicleID, RplId garageId)
	{
		BFCG_SendSettings();
		string playerUID = BFCG_GARAGE_GetOwnUID();
		if (playerUID.IsEmpty())
		{
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.FAILED);
			return;
		}

		BFCG_GARAGE_GarageComponent garageCmp = BFCG_GARAGE_ResolveGarage(garageId);
		if (!garageCmp || !BFCG_GARAGE_IsNearGarage(garageCmp))
		{
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.TOO_FAR);
			return;
		}

		BFCG_GARAGE_GarageManager manager = BFCG_GARAGE_GarageManager.GetInstance();
		BFCG_GARAGE_VehicleData data = manager.GetVehicleByID(vehicleID);
		if (!data)
		{
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.NOT_FOUND);
			return;
		}

		if (data.m_sOwnerUID != playerUID)
		{
			Print(string.Format("[BFCG_GARAGE_Rpc] Delete denied (ownership): %1 by %2", vehicleID, playerUID), LogLevel.WARNING);
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.NOT_ALLOWED);
			return;
		}

		if (manager.DeleteVehicle(vehicleID, playerUID))
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.SUCCESS);
		else
			Rpc(BFCG_GARAGE_RpcDo_DeleteResult, BFCG_GARAGE_EDeleteResult.FAILED);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_AddEntry(string vehicleID, string prefab, bool available)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetAddEntry(vehicleID, prefab, available);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_ContentsComplete(int count)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetContentsComplete(count);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_SetSpotVehicle(string prefab, int count)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetSetSpotVehicle(prefab, count);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_ParkResult(int resultCode)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetParkResult(resultCode);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_UnpackResult(string vehicleID, int resultCode)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetUnpackResult(vehicleID, resultCode);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_GARAGE_RpcDo_DeleteResult(int resultCode)
	{
		BFCG_GARAGE_GarageMenuUI menu = BFCG_GARAGE_GarageMenuUI.GetOpenInstance();
		if (menu)
			menu.OnNetDeleteResult(resultCode);
	}

	// Derive identity from the requesting controller, never from a client-supplied UID.
	protected string BFCG_GARAGE_GetOwnUID()
	{
		return BFCG_VehicleClaimState.ResolvePlayerIdentity(GetPlayerId());
	}

	protected BFCG_GARAGE_GarageComponent BFCG_GARAGE_ResolveGarage(RplId garageId)
	{
		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(garageId));
		if (!rplComp)
			return null;
		IEntity garageEntity = rplComp.GetEntity();
		if (!garageEntity)
			return null;
		return BFCG_GARAGE_GarageComponent.Cast(garageEntity.FindComponent(BFCG_GARAGE_GarageComponent));
	}

	// Recheck current player position for every request, independently of the local action distance.
	protected bool BFCG_GARAGE_IsNearGarage(BFCG_GARAGE_GarageComponent garageCmp)
	{
		IEntity controlled = GetControlledEntity();
		if (!controlled)
			return false;
		IEntity garageEntity = garageCmp.GetOwner();
		if (!garageEntity)
			return false;
		vector diff = controlled.GetOrigin() - garageEntity.GetOrigin();
		float distSq = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];
		return distSq <= BFCG_Settings.Get().maxGarageRequestDistanceMeters * BFCG_Settings.Get().maxGarageRequestDistanceMeters;
	}
}
