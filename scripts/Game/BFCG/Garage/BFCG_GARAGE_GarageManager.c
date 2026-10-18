/*
 * Server-side personal garage storage, ownership checks and parking-spot allocation.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

class BFCG_GARAGE_GarageManager
{

	protected static ref BFCG_GARAGE_GarageManager s_Instance;

	protected ref array<ref BFCG_GARAGE_VehicleData> m_aStoredVehicles;
	protected bool m_bStorageReady = true;

	protected const string DATA_DIR     = "$profile:BlackFoxClaimGarage";
	protected const string GARAGE_FILE  = "$profile:BlackFoxClaimGarage/garage_data.txt";
	protected const string LOG_FILE     = "$profile:BlackFoxClaimGarage/garage_log.txt";

	protected void BFCG_GARAGE_GarageManager()
	{
		m_aStoredVehicles = new array<ref BFCG_GARAGE_VehicleData>();
		FileIO.MakeDirectory(DATA_DIR);
		LoadGarageData();
	}

	static BFCG_GARAGE_GarageManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new BFCG_GARAGE_GarageManager();
		return s_Instance;
	}

	// Persist the garage record before removing the live vehicle; a failed write preserves it.
	bool ParkVehicle(IEntity vehicle, string ownerUID, IEntity spawnEntity)
	{
		if (!CanUseStorage() || !vehicle || ownerUID.IsEmpty() || BFCG_ExcludedVehicleComponent.IsExcluded(vehicle))
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		VehicleControllerComponent controller = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		if ((damage && damage.GetState() == EDamageState.DESTROYED) || (controller && controller.BFCG_IsDestroyed()))
		{
			Print("[BFCG_GARAGE] Park denied: vehicle is destroyed", LogLevel.WARNING);
			return false;
		}

		if (BFCG_VehicleClaimState.IsClaimedByOtherIdentity(vehicle, ownerUID))
		{
			Print("[BFCG_GARAGE] Park denied: vehicle is claimed by another player", LogLevel.WARNING);
			return false;
		}

		if (CountForPlayer(ownerUID) >= BFCG_Settings.Get().maxGarageVehicles)
			return false;

		BFCG_GARAGE_VehicleData data = BFCG_GARAGE_VehicleData.FromVehicle(vehicle, ownerUID, spawnEntity);
		if (!data)
			return false;

		// Deleting the vehicle also deletes seated characters; reject before any storage mutation.
		if (HasOccupants(vehicle))
		{
			Print("[BFCG_GARAGE] Park denied: vehicle is occupied", LogLevel.WARNING);
			return false;
		}

		m_aStoredVehicles.Insert(data);

		if (!SaveGarageData())
		{
			int failedIndex = m_aStoredVehicles.Find(data);
			if (failedIndex >= 0)
				m_aStoredVehicles.Remove(failedIndex);
			return false;
		}
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		LogAction(ownerUID, "PARK", data.m_sVehicleName, data.m_sVehicleID);
		return true;
	}

	// Spawn and claim first; consume the stored record only after those steps succeed.
	int UnparkVehicle(string vehicleID, string playerUID, BFCG_GARAGE_GarageComponent garageCmp)
	{
		if (!Replication.IsServer() || !garageCmp || playerUID.IsEmpty() || !m_bStorageReady)
			return BFCG_GARAGE_EUnpackResult.FAILED;
		if (!BFCG_VehicleClaimPersistence.IsReady())
			return BFCG_GARAGE_EUnpackResult.CLAIMS_RESTORING;
		if (!BFCG_VehicleClaimState.CanAddWorldClaim(playerUID))
			return BFCG_GARAGE_EUnpackResult.WORLD_CLAIM_LIMIT;

		BFCG_GARAGE_VehicleData data = FindVehicleByID(vehicleID);
		if (!data)
			return BFCG_GARAGE_EUnpackResult.NOT_FOUND;

		if (data.m_sOwnerUID != playerUID)
			return BFCG_GARAGE_EUnpackResult.NOT_ALLOWED;

		Resource res = Resource.Load(data.m_rPrefab);
		if (!res.IsValid())
		{
			Print(string.Format("[BFCG_GARAGE] Prefab not found: %1", data.m_rPrefab), LogLevel.ERROR);
			return BFCG_GARAGE_EUnpackResult.UNAVAILABLE;
		}

		bool hasParkingSpots;
		IEntity parkingSpot = ReserveFreeParkingSpot(garageCmp, hasParkingSpots);
		if (!parkingSpot)
		{
			if (!hasParkingSpots)
				return BFCG_GARAGE_EUnpackResult.NO_PARKING_SPOTS;
			return BFCG_GARAGE_EUnpackResult.BLOCKED;
		}

		vector transform[4];
		parkingSpot.GetTransform(transform);

		// Discard marker scale so decorative parking prefabs cannot scale the spawned vehicle.
		vector right = transform[0];
		vector up = transform[1];
		vector forward = transform[2];
		right.Normalize();
		up.Normalize();
		forward.Normalize();
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = right;
		params.Transform[1] = up;
		params.Transform[2] = forward;
		params.Transform[3] = transform[3];

		IEntity spawned = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!spawned)
		{
			Print("[BFCG_GARAGE] Vehicle could not be spawned.", LogLevel.ERROR);
			m_SpotReservedUntil.Remove(parkingSpot);
			return BFCG_GARAGE_EUnpackResult.FAILED;
		}

		// A failed owner-claim assignment removes the new entity and leaves the stored record intact.
		if (!BFCG_VehicleClaimState.RefreshVehicleFromGarage(spawned, playerUID))
		{
			Print(string.Format("[BFCG_GARAGE] Spawned vehicle could not be claimed for player %1", playerUID), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			m_SpotReservedUntil.Remove(parkingSpot);
			return BFCG_GARAGE_EUnpackResult.FAILED;
		}

		// Retrieval uses prefab defaults, including cargo; the parked inventory is discarded.
		// On a failed garage write, restore the record and remove the spawned entity.
		int idx = m_aStoredVehicles.Find(data);
		if (idx >= 0)
			m_aStoredVehicles.Remove(idx);

		if (!SaveGarageData())
		{
			m_aStoredVehicles.Insert(data);
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			m_SpotReservedUntil.Remove(parkingSpot);
			return BFCG_GARAGE_EUnpackResult.FAILED;
		}
		LogAction(playerUID, "UNPARK", data.m_sVehicleName, data.m_sVehicleID);
		return BFCG_GARAGE_EUnpackResult.SUCCESS;
	}

	bool DeleteVehicle(string vehicleID, string playerUID)
	{
		if (!CanUseStorage())
			return false;
		BFCG_GARAGE_VehicleData data = FindVehicleByID(vehicleID);
		if (!data || data.m_sOwnerUID != playerUID)
			return false;

		int idx = m_aStoredVehicles.Find(data);
		if (idx < 0)
			return false;
		m_aStoredVehicles.Remove(idx);

		if (!SaveGarageData())
		{
			m_aStoredVehicles.Insert(data);
			return false;
		}
		LogAction(playerUID, "DELETE", data.m_sVehicleName, data.m_sVehicleID);
		return true;
	}

	// Inspect child compartment managers too, including AI occupants, before deleting a vehicle.
	static bool HasOccupants(IEntity entity)
	{
		if (!entity)
			return false;

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			entity.FindComponent(BaseCompartmentManagerComponent));
		if (compartments)
		{
			array<BaseCompartmentSlot> slots = {};
			compartments.GetCompartments(slots);
			foreach (BaseCompartmentSlot slot : slots)
			{
				if (slot && slot.GetOccupant())
					return true;
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			if (HasOccupants(child))
				return true;
			child = child.GetSibling();
		}
		return false;
	}

	static bool IsPrefabAvailable(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;
		Resource res = Resource.Load(prefab);
		return res.IsValid();
	}

	array<BFCG_GARAGE_VehicleData> GetVehiclesForPlayer(string playerUID)
	{
		array<BFCG_GARAGE_VehicleData> result = new array<BFCG_GARAGE_VehicleData>();
		foreach (BFCG_GARAGE_VehicleData data : m_aStoredVehicles)
		{
			if (data.m_sOwnerUID == playerUID)
				result.Insert(data);
		}
		return result;
	}

	int CountForPlayer(string playerUID)
	{
		int count = 0;
		foreach (BFCG_GARAGE_VehicleData data : m_aStoredVehicles)
		{
			if (data.m_sOwnerUID == playerUID)
				count++;
		}
		return count;
	}

	protected BFCG_GARAGE_VehicleData FindVehicleByID(string vehicleID)
	{
		foreach (BFCG_GARAGE_VehicleData data : m_aStoredVehicles)
		{
			if (data.m_sVehicleID == vehicleID)
				return data;
		}
		return null;
	}

	BFCG_GARAGE_VehicleData GetVehicleByID(string vehicleID)
	{
		return FindVehicleByID(vehicleID);
	}

	protected bool m_bSpawnQueryHitVehicle;
	protected ref map<IEntity, float> m_SpotReservedUntil = new map<IEntity, float>();

	// Only use spots linked to this terminal. Reserve briefly to cover concurrent requests before physics updates.
	protected IEntity ReserveFreeParkingSpot(BFCG_GARAGE_GarageComponent garageCmp, out bool hasParkingSpots)
	{
		hasParkingSpots = false;
		BaseWorld world = GetGame().GetWorld();
		if (!Replication.IsServer() || !garageCmp || !world)
			return null;
		array<BFCG_GARAGE_ParkingSpotComponent> spots = BFCG_GARAGE_ParkingSpotComponent.GetSpotsForGarage(garageCmp.GetGarageID());
		if (!spots)
			return null;

		float now = world.GetWorldTime();
		foreach (BFCG_GARAGE_ParkingSpotComponent spot : spots)
		{
			if (!spot)
				continue;
			IEntity entity = spot.GetOwnerEntity();
			if (!entity || entity.IsDeleted())
				continue;
			hasParkingSpots = true;
			if (m_SpotReservedUntil.Contains(entity) && m_SpotReservedUntil.Get(entity) > now)
				continue;
			if (IsSpawnPositionBlocked(entity.GetOrigin(), Math.Max(BFCG_Settings.Get().spawnClearanceMeters, spot.GetParkRadius())))
				continue;

			m_SpotReservedUntil.Set(entity, now + BFCG_Settings.Get().spawnReservationMilliseconds);
			return entity;
		}
		return null;
	}

	// The clearance sphere checks vehicles and characters; it is not a full geometry collision test.
	bool IsSpawnPositionBlocked(vector pos, float radius = 3.0)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!Replication.IsServer() || !world)
			return true;
		m_bSpawnQueryHitVehicle = false;
		world.QueryEntitiesBySphere(pos, radius, OnSpawnBlockQuery, null, EQueryEntitiesFlags.DYNAMIC);
		return m_bSpawnQueryHitVehicle;
	}

	protected bool OnSpawnBlockQuery(IEntity entity)
	{
		if (entity && (Vehicle.Cast(entity) || ChimeraCharacter.Cast(entity)))
			m_bSpawnQueryHitVehicle = true;
		return true;
	}

	// Block mutations until settings, garage records and native claim restoration are ready.
	bool CanUseStorage()
	{
		return Replication.IsServer() && m_bStorageReady && BFCG_VehicleClaimPersistence.IsReady();
	}

	// A failed write disables further mutations for this manager instance.
	bool SaveGarageData()
	{
		if (!Replication.IsServer() || !m_bStorageReady)
			return false;
		BFCG_GARAGE_GarageStorageJson storage = new BFCG_GARAGE_GarageStorageJson();
		foreach (BFCG_GARAGE_VehicleData data : m_aStoredVehicles)
			storage.vehicles.Insert(BFCG_GARAGE_VehicleDataJson.FromData(data));
		if (!storage.SaveToFile(GARAGE_FILE))
		{
			m_bStorageReady = false;
			Print("[BFCG] Garage write failed; storage operations disabled", LogLevel.ERROR);
			return false;
		}
		return true;
	}

	// Reject unreadable, invalid or duplicate records without replacing the existing storage file.
	void LoadGarageData()
	{
		m_aStoredVehicles.Clear();
		if (!FileIO.FileExists(GARAGE_FILE))
			return;
		BFCG_GARAGE_GarageStorageJson storage = new BFCG_GARAGE_GarageStorageJson();
		if (!storage.LoadFromFile(GARAGE_FILE))
		{
			m_bStorageReady = false;
			Print("[BFCG] Garage data cannot be read; refusing to overwrite it", LogLevel.ERROR);
			return;
		}
		foreach (BFCG_GARAGE_VehicleDataJson entry : storage.vehicles)
		{
			if (!entry || entry.vehicleID.IsEmpty() || entry.ownerUID.IsEmpty()
				|| entry.prefab.IsEmpty() || FindVehicleByID(entry.vehicleID))
			{
				m_bStorageReady = false;
				Print("[BFCG] Invalid or duplicate garage record; storage disabled", LogLevel.ERROR);
				return;
			}
			m_aStoredVehicles.Insert(entry.ToData());
		}
	}

	protected void LogAction(string playerUID, string action, string vehicleName, string vehicleID)
	{
		int ts = System.GetUnixTime();
		string line = string.Format("[%1] %2 | Player: %3 | Vehicle: %4 [%5]",
			ts, action, playerUID, vehicleName, vehicleID);

		Print("[BFCG_GARAGE] " + line, LogLevel.NORMAL);

		FileHandle f = FileIO.OpenFile(LOG_FILE, FileMode.APPEND);
		if (f)
		{
			f.WriteLine(line);
			f.Close();
		}
	}
}
