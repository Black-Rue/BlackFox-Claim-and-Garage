/*
 * BlackFox vehicle claim state and server-side ownership rules.
 * Claim, lock and absolute Unix expiry behaviour conceptually based on CarKey2 by NewbKodii (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/66D61CE15CC684A6
 * Persistent Bohemia identity, world limits and native-controller integration are BlackFox additions.
 */

class BFCG_VehicleClaimState
{

	protected static ref array<VehicleControllerComponent> s_aVehicles = {};
	protected static ref map<int, string> s_mPlayerIdentities = new map<int, string>();

	static void Register(VehicleControllerComponent component)
	{
		if (!component || s_aVehicles.Contains(component))
			return;

		s_aVehicles.Insert(component);
	}

	static void Unregister(VehicleControllerComponent component)
	{
		if (!component)
			return;

		int index = s_aVehicles.Find(component);
		if (index >= 0)
			s_aVehicles.Remove(index);

	}

	static bool Read(VehicleControllerComponent component, out string ownerIdentity, out int expiresAt, out bool locked)
	{
		ownerIdentity = string.Empty;
		expiresAt = 0;
		locked = false;

		if (!component || component.BFCG_IsDestroyed() || BFCG_ExcludedVehicleComponent.IsExcluded(component.GetOwner()))
			return false;

		if (!Replication.IsServer())
		{
			bool isOwner;
			return BFCG_VehicleClaimClientCache.Get(GetVehicleRplId(component), isOwner, locked, expiresAt);
		}

		ownerIdentity = component.BFCG_GetClaimOwnerIdentity();
		expiresAt = component.BFCG_GetClaimExpiresAt();
		locked = component.BFCG_IsClaimLocked();
		if (ownerIdentity.IsEmpty())
			return false;

		return true;
	}

	static bool HasClaim(VehicleControllerComponent component)
	{
		string ownerIdentity;
		int expiresAt;
		bool locked;
		return Read(component, ownerIdentity, expiresAt, locked) && expiresAt > System.GetUnixTime();
	}

	static bool IsLocked(VehicleControllerComponent component)
	{
		string ownerIdentity;
		int expiresAt;
		bool locked;
		return Read(component, ownerIdentity, expiresAt, locked) && expiresAt > System.GetUnixTime() && locked;
	}

	// Claim and Garage must use the same real BI identity. The vanilla utility can
	// generate a temporary Peer Tool UID before the backend identity becomes available.
	static string ResolvePlayerIdentity(int playerId)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return string.Empty;

		BackendApi backendApi = GetGame().GetBackendApi();
		string identity;
		if (backendApi)
			identity = backendApi.GetPlayerIdentityId(playerId);

		if (!identity.IsEmpty())
		{
			s_mPlayerIdentities.Set(playerId, identity);
			return identity;
		}

		// Disconnect handling may run after the backend has released the identity.
		if (s_mPlayerIdentities.Find(playerId, identity))
			return identity;
		return string.Empty;
	}

	static string ResolvePlayerIdentity(IEntity playerEntity)
	{
		if (!Replication.IsServer() || !playerEntity)
			return string.Empty;

		BackendApi backendApi = GetGame().GetBackendApi();
		if (!backendApi)
			return string.Empty;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return string.Empty;

		return ResolvePlayerIdentity(playerManager.GetPlayerIdFromControlledEntity(playerEntity));
	}

	static bool IsOwner(VehicleControllerComponent component, IEntity playerEntity)
	{
		if (!Replication.IsServer())
		{
			bool isOwner;
			bool locked;
			int expiresAt;
			return BFCG_VehicleClaimClientCache.Get(GetVehicleRplId(component), isOwner, locked, expiresAt) && isOwner;
		}

		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (!Read(component, ownerIdentity, expiresAt, locked) || expiresAt <= System.GetUnixTime())
			return false;

		string playerIdentity = ResolvePlayerIdentity(playerEntity);
		return !playerIdentity.IsEmpty() && playerIdentity == ownerIdentity;
	}

	static bool IsAccessDenied(VehicleControllerComponent component, IEntity playerEntity)
	{
		if (!IsLocked(component))
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(playerEntity);
			if (playerId > 0 && playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR))
				return false;
		}

		return !IsOwner(component, playerEntity);
	}

	// Count authoritative live claims, excluding garage records and the optional target being refreshed.
	static int CountWorldClaims(string playerIdentity, VehicleControllerComponent exclude = null)
	{
		if (!Replication.IsServer() || playerIdentity.IsEmpty())
			return 0;

		BFCG_VehicleClaimPersistence.ReconcileWorldClaims();
		array<VehicleControllerComponent> claimed = {};
		GetClaimedComponents(claimed);
		int count;
		foreach (VehicleControllerComponent component : claimed)
		{
			if (component != exclude && !BFCG_ExcludedVehicleComponent.IsExcluded(component.GetOwner())
				&& component.BFCG_GetClaimOwnerIdentity() == playerIdentity)
				count++;
		}
		return count;
	}

	static bool CanAddWorldClaim(string playerIdentity, VehicleControllerComponent exclude = null)
	{
		return Replication.IsServer() && !playerIdentity.IsEmpty()
			&& BFCG_VehicleClaimPersistence.IsReady()
			&& CountWorldClaims(playerIdentity, exclude) < BFCG_Settings.Get().maxWorldClaims;
	}

	static string WorldClaimLimitMessage()
	{
		return string.Format("World vehicle limit reached (%1 claimed vehicles). Park one in the garage first.", BFCG_Settings.Get().maxWorldClaims);
	}

	static string RestorePendingMessage()
	{
		return "Vehicle data is being restored. Claiming and garage retrieval are temporarily unavailable. Please try again later.";
	}

	static bool TryClaim(VehicleControllerComponent component, IEntity playerEntity)
	{
		if (!Replication.IsServer() || !component || !playerEntity)
			return false;

		string playerIdentity = ResolvePlayerIdentity(playerEntity);
		if (playerIdentity.IsEmpty())
		{
			Print("[BFCG VehicleClaim] Claim requested, but player identity is unavailable", LogLevel.WARNING);
			return false;
		}

		if (!BFCG_VehicleClaimPersistence.IsReady())
		{
			BFCG_Notify.ToPlayer(playerEntity, RestorePendingMessage());
			return false;
		}
		if (!CanAddWorldClaim(playerIdentity))
		{
			BFCG_Notify.ToPlayer(playerEntity, WorldClaimLimitMessage());
			return false;
		}

		return TryClaimIdentity(component, playerIdentity);
	}

	static bool RefreshVehicleFromGarage(IEntity vehicle, string playerIdentity)
	{
		if (!Replication.IsServer() || !vehicle || playerIdentity.IsEmpty())
			return false;

		VehicleControllerComponent component = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		if (!component)
		{
			Print(string.Format("[BFCG VehicleClaim] Cannot claim entity without VehicleControllerComponent: %1",
				BFCG_EntityName(vehicle)), LogLevel.ERROR);
			return false;
		}

		return RefreshFromGarageIdentity(component, playerIdentity);
	}

	static bool IsClaimedByOtherIdentity(IEntity vehicle, string playerIdentity)
	{
		if (!Replication.IsServer() || !vehicle || playerIdentity.IsEmpty())
			return false;

		VehicleControllerComponent component = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (!Read(component, ownerIdentity, expiresAt, locked) || expiresAt <= System.GetUnixTime())
			return false;

		return ownerIdentity != playerIdentity;
	}

	protected static bool TryClaimIdentity(VehicleControllerComponent component, string playerIdentity)
	{
		if (!Replication.IsServer() || !component || component.BFCG_IsDestroyed() || playerIdentity.IsEmpty() || BFCG_ExcludedVehicleComponent.IsExcluded(component.GetOwner()))
			return false;

		ReleaseIfExpired(component);

		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (Read(component, ownerIdentity, expiresAt, locked))
			return false;

		if (!CanAddWorldClaim(playerIdentity))
			return false;

		Apply(component, playerIdentity, true, System.GetUnixTime() + (BFCG_Settings.Get().claimDurationHours * 3600));
		EjectUnauthorizedPilot(component);
		Print(string.Format("[BFCG VehicleClaim] CLAIM vehicle=%1 owner=%2 expires=%3",
			BFCG_EntityName(component.GetOwner()), playerIdentity, component.BFCG_GetClaimExpiresAt()));
		return true;
	}

	protected static bool RefreshFromGarageIdentity(VehicleControllerComponent component, string playerIdentity)
	{
		if (!Replication.IsServer() || !component || component.BFCG_IsDestroyed() || playerIdentity.IsEmpty() || BFCG_ExcludedVehicleComponent.IsExcluded(component.GetOwner()))
			return false;

		ReleaseIfExpired(component);

		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (Read(component, ownerIdentity, expiresAt, locked) && ownerIdentity != playerIdentity)
			return false;

		if (!CanAddWorldClaim(playerIdentity, component))
			return false;

		Apply(component, playerIdentity, true, System.GetUnixTime() + (BFCG_Settings.Get().claimDurationHours * 3600));
		Print(string.Format("[BFCG VehicleClaim] GARAGE REFRESH vehicle=%1 owner=%2 expires=%3",
			BFCG_EntityName(component.GetOwner()), playerIdentity, component.BFCG_GetClaimExpiresAt()));
		return true;
	}

	static bool Toggle(VehicleControllerComponent component, IEntity playerEntity)
	{
		if (!Replication.IsServer() || !IsOwner(component, playerEntity))
			return false;

		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (!Read(component, ownerIdentity, expiresAt, locked))
			return false;

		bool newLocked = !locked;
		Apply(component, ownerIdentity, newLocked, expiresAt);
		if (newLocked)
			EjectUnauthorizedPilot(component);
		Print(string.Format("[BFCG VehicleClaim] TOGGLE vehicle=%1 owner=%2 locked=%3",
			BFCG_EntityName(component.GetOwner()), ownerIdentity, newLocked));
		return true;
	}

	protected static void EjectUnauthorizedPilot(VehicleControllerComponent component)
	{
		if (!component || !IsLocked(component))
			return;

		Vehicle vehicle = Vehicle.Cast(component.GetOwner());
		if (!vehicle)
			return;

		BaseCompartmentSlot pilotSlot = vehicle.GetPilotCompartmentSlot();
		if (!pilotSlot)
			return;

		IEntity occupant = pilotSlot.GetOccupant();
		if (!occupant || !IsAccessDenied(component, occupant))
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(
			occupant.FindComponent(SCR_CompartmentAccessComponent));
		if (access)
			access.AskOwnerToGetOutFromVehicle(EGetOutType.TELEPORT, -1,
				ECloseDoorAfterActions.LEAVE_OPEN, true, true);
	}

	static void CachePlayerIdentity(int playerId)
	{
		ResolvePlayerIdentity(playerId);
	}

	static void SyncClaimsToPlayer(int playerId)
	{
		if (!Replication.IsServer())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
		if (!controller)
			return;

		controller.BFCG_SendSettings();
		controller.BFCG_VehicleClaimClearClientCache();
		string playerIdentity = ResolvePlayerIdentity(playerId);
		foreach (VehicleControllerComponent component : s_aVehicles)
			PushStateToController(component, controller, playerIdentity);
	}

	// Relock owned vehicles without extending expiry, then release the transient player-ID cache.
	static void HandlePlayerDisconnected(int playerId)
	{
		if (!Replication.IsServer())
			return;

		string identity = ResolvePlayerIdentity(playerId);
		if (!identity.IsEmpty())
		{
			foreach (VehicleControllerComponent component : s_aVehicles)
			{
				string ownerIdentity;
				int expiresAt;
				bool locked;
				if (Read(component, ownerIdentity, expiresAt, locked) && ownerIdentity == identity)
					Apply(component, ownerIdentity, true, expiresAt);
			}
		}

		s_mPlayerIdentities.Remove(playerId);
	}

	// Also reconcile proxies for editor-placed and third-party vehicles during periodic sweeps.
	static void SweepExpiredClaims()
	{
		if (!Replication.IsServer())
			return;

		BFCG_VehicleClaimActionProxySystem.EnsureAllVehicleProxies();

		for (int i = s_aVehicles.Count() - 1; i >= 0; i--)
		{
			VehicleControllerComponent component = s_aVehicles[i];
			if (!component || !component.GetOwner() || component.GetOwner().IsDeleted())
			{
				if (component)
					PushClearedState(GetVehicleRplId(component));
				s_aVehicles.Remove(i);
				continue;
			}

			component.BFCG_ReconcileWreck();
			ReleaseIfExpired(component);
		}

		BFCG_VehicleClaimPersistence.Snapshot();
	}

	static void RestoreLocked(VehicleControllerComponent component)
	{
		if (!Replication.IsServer())
			return;

		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (Read(component, ownerIdentity, expiresAt, locked) && expiresAt > System.GetUnixTime())
			Apply(component, ownerIdentity, true, expiresAt);
	}

	// Destruction revokes the owner immediately, independently of restore/readiness gates.
	static void ReleaseDestroyed(VehicleControllerComponent component)
	{
		if (!Replication.IsServer() || !component || !component.BFCG_IsDestroyed())
			return;
		bool hadOwner = !component.BFCG_GetClaimOwnerIdentity().IsEmpty();
		component.BFCG_ClearClaimState();
		if (hadOwner)
			PushClearedState(GetVehicleRplId(component));
	}

	protected static void ReleaseIfExpired(VehicleControllerComponent component)
	{
		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (!Read(component, ownerIdentity, expiresAt, locked) || expiresAt > System.GetUnixTime())
			return;

		RplId vehicleId = GetVehicleRplId(component);
		component.BFCG_ClearClaimState();
		PushClearedState(vehicleId);
		BFCG_VehicleClaimPersistence.SaveClaim(component);
		Print(string.Format("[BFCG VehicleClaim] EXPIRED vehicle=%1", BFCG_EntityName(component.GetOwner())));
	}

	protected static void Apply(VehicleControllerComponent component, string ownerIdentity, bool locked, int expiresAt)
	{
		component.BFCG_ApplyClaimState(ownerIdentity, locked, expiresAt);
		PushStateToClients(component);
		BFCG_VehicleClaimPersistence.SaveClaim(component);
	}

	static void GetClaimedComponents(out array<VehicleControllerComponent> outComponents)
	{
		outComponents.Clear();
		if (!Replication.IsServer())
			return;

		foreach (VehicleControllerComponent component : s_aVehicles)
		{
			if (!component || !component.GetOwner() || component.GetOwner().IsDeleted())
				continue;

			if (component.BFCG_IsDestroyed() || component.BFCG_GetClaimOwnerIdentity().IsEmpty())
				continue;

			if (component.BFCG_GetClaimExpiresAt() <= System.GetUnixTime())
				continue;

			outComponents.Insert(component);
		}
	}

	static int GetExpiresAt(VehicleControllerComponent component)
	{
		string ownerIdentity;
		int expiresAt;
		bool locked;
		if (!Read(component, ownerIdentity, expiresAt, locked))
			return 0;

		return expiresAt;
	}

	protected static RplId GetVehicleRplId(VehicleControllerComponent component)
	{
		if (!component || !component.GetOwner())
			return RplId.Invalid();

		RplComponent rplComponent = RplComponent.Cast(component.GetOwner().FindComponent(RplComponent));
		if (!rplComponent)
			return RplId.Invalid();

		return rplComponent.Id();
	}

	protected static void PushStateToClients(VehicleControllerComponent component)
	{
		if (!Replication.IsServer() || !component)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
			if (controller)
				PushStateToController(component, controller, ResolvePlayerIdentity(playerId));
		}
	}

	protected static void PushStateToController(VehicleControllerComponent component, SCR_PlayerController controller, string playerIdentity)
	{
		if (!component || !controller)
			return;

		RplId vehicleId = GetVehicleRplId(component);
		if (vehicleId == RplId.Invalid())
			return;

		string ownerIdentity;
		int expiresAt;
		bool locked;
		bool hasClaim = Read(component, ownerIdentity, expiresAt, locked) && expiresAt > System.GetUnixTime();
		controller.BFCG_VehicleClaimSendState(vehicleId, hasClaim,
			hasClaim && !playerIdentity.IsEmpty() && playerIdentity == ownerIdentity, locked, expiresAt);
	}

	protected static void PushClearedState(RplId vehicleId)
	{
		if (!Replication.IsServer())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
			if (controller)
				controller.BFCG_VehicleClaimSendState(vehicleId, false, false, false, 0);
		}
	}

	static string BFCG_EntityName(IEntity entity)
	{
		if (!entity || !entity.GetPrefabData())
			return "<?>";

		return entity.GetPrefabData().GetPrefabName();
	}
}
