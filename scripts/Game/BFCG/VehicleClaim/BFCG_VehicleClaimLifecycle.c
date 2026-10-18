/*
 * BlackFox vehicle lifecycle and interaction-proxy integration.
 * Behavioural reference: CarKey2 by NewbKodii (Arma Public License).
 * Workshop: https://reforger.armaplatform.com/workshop/66D61CE15CC684A6
 */

[ComponentEditorProps(category: "BlackFox/Vehicle Claim", description: "Marks the runtime interaction proxy attached to a vehicle.")]
class BFCG_VehicleClaimActionProxyComponentClass : ScriptComponentClass {}

class BFCG_VehicleClaimActionProxyComponent : ScriptComponent
{
	[Attribute(defvalue: "", UIWidgets.EditBox, "Proxy prefab revision")]
	protected string m_sRevision;

	bool BFCG_IsCurrentRevision()
	{
		return m_sRevision == "native-menu-v5";
	}
}

modded class VehicleControllerComponent
{
	protected static const ResourceName BFCG_CLAIM_ACTION_PROXY = "{E96F11FBE8FD8B09}Prefabs/BFCG/VehicleClaim/BFCG_VehicleClaimActionProxy.et";

	protected string m_sBFCGClaimOwnerIdentity;

	protected bool m_bBFCGClaimLocked;

	protected int m_iBFCGClaimExpiresAt;

	protected IEntity m_pBFCGClaimActionProxy;
	protected int m_iBFCGWreckDeleteAt;
	// A positive deadline is permanent even if another serializer restores an intact prefab.
	int BFCG_GetWreckDeleteAt()
	{
		return m_iBFCGWreckDeleteAt;
	}

	void BFCG_RestoreWreckDeleteAt(int deleteAt)
	{
		m_iBFCGWreckDeleteAt = deleteAt;
		if (deleteAt > 0 && Replication.IsServer())
			BFCG_DestroyedVehicleCleanupSystem.Register(this);
	}

	bool BFCG_IsDestroyed()
	{
		if (m_iBFCGWreckDeleteAt > 0)
			return true;
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return false;
		DamageManagerComponent damage = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	override protected void OnDestroyed(IEntity ent)
	{
		super.OnDestroyed(ent);
		BFCG_ReconcileWreck();
	}

	override protected void OnDelete(IEntity owner)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(BFCG_EnsureClaimActionProxy);
		BFCG_DestroyedVehicleCleanupSystem.Unregister(this);
		super.OnDelete(owner);
	}

	void BFCG_ReconcileWreck()
	{
		if (!Replication.IsServer() || !BFCG_Settings.IsValid() || BFCG_ExcludedVehicleComponent.IsExcluded(GetOwner()))
			return;
		// Native damage and claim serializers can run in either order during loading.
		if (SCR_PersistenceSystem.IsLoadInProgress() || !BFCG_IsDestroyed())
			return;
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;
		bool firstObservation = m_iBFCGWreckDeleteAt == 0;
		if (firstObservation)
			m_iBFCGWreckDeleteAt = System.GetUnixTime() + BFCG_Settings.Get().destroyedVehicleLifetimeSeconds;
		bool hadOwner = !m_sBFCGClaimOwnerIdentity.IsEmpty();
		BFCG_VehicleClaimState.ReleaseDestroyed(this);
		BFCG_DestroyedVehicleCleanupSystem.Register(this);
		// This separate timer owns wreck lifetime; neither vanilla nor unclaimed cleanup may shorten it.
		SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(owner);
		if (garbage)
		{
			garbage.UpdateBlacklist(owner, false);
			garbage.Withdraw(owner);
		}
		if (firstObservation || hadOwner)
		{
			BFCG_VehicleClaimPersistence.SaveClaim(this);
			Print(string.Format("[BFCG] WRECK vehicle=%1 deleteAt=%2", BFCG_VehicleClaimState.BFCG_EntityName(owner), m_iBFCGWreckDeleteAt));
		}
	}


	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (owner && GetGame().InPlayMode())
			GetGame().GetCallqueue().CallLater(BFCG_EnsureClaimActionProxy, 0, false);
	}

	void BFCG_EnsureClaimActionProxy()
	{
		if (BFCG_ExcludedVehicleComponent.IsExcluded(GetOwner()))
			return;

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		BFCG_ReconcileWreck();
		if (BFCG_IsDestroyed())
			return;

		if (m_pBFCGClaimActionProxy)
		{

			// Replace stale proxy prefab revisions after a script reload.
			BFCG_VehicleClaimActionProxyComponent proxyMarker = BFCG_VehicleClaimActionProxyComponent.Cast(
				m_pBFCGClaimActionProxy.FindComponent(BFCG_VehicleClaimActionProxyComponent));
			if (proxyMarker && proxyMarker.BFCG_IsCurrentRevision())
				return;

			SCR_EntityHelper.DeleteEntityAndChildren(m_pBFCGClaimActionProxy);
			m_pBFCGClaimActionProxy = null;
		}

		IEntity owner = GetOwner();
		if (!owner)
			return;

		Resource proxyResource = Resource.Load(BFCG_CLAIM_ACTION_PROXY);
		if (!proxyResource.IsValid())
		{
			Print(string.Format("[BFCG VehicleClaim] Action proxy prefab is unavailable for %1",
				BFCG_VehicleClaimState.BFCG_EntityName(owner)), LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.Parent = owner;
		params.TransformMode = ETransformMode.LOCAL;
		params.Transform[0] = "1 0 0";
		params.Transform[1] = "0 1 0";
		params.Transform[2] = "0 0 1";

		params.Transform[3] = "0 1 0";
		m_pBFCGClaimActionProxy = GetGame().SpawnEntityPrefab(proxyResource, owner.GetWorld(), params);
		if (!m_pBFCGClaimActionProxy)
		{
			Print(string.Format("[BFCG VehicleClaim] Failed to spawn action proxy for %1",
				BFCG_VehicleClaimState.BFCG_EntityName(owner)), LogLevel.ERROR);
			return;
		}

		BFCG_VehicleClaimActionProxySystem.ReportSpawn(owner);
	}

	BFCG_ToggleVehicleLockUserAction BFCG_GetClaimAction()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		IEntity child = owner.GetChildren();
		while (child)
		{
			if (child.FindComponent(BFCG_VehicleClaimActionProxyComponent))
			{
				ActionsManagerComponent manager = ActionsManagerComponent.Cast(child.FindComponent(ActionsManagerComponent));
				if (manager)
					return BFCG_ToggleVehicleLockUserAction.Cast(manager.GetFirstAction());
			}
			child = child.GetSibling();
		}

		return null;
	}

	string BFCG_GetClaimOwnerIdentity()
	{
		return m_sBFCGClaimOwnerIdentity;
	}

	bool BFCG_IsClaimLocked()
	{
		return m_bBFCGClaimLocked;
	}

	int BFCG_GetClaimExpiresAt()
	{
		return m_iBFCGClaimExpiresAt;
	}

	void BFCG_ApplyClaimState(string ownerIdentity, bool locked, int expiresAt)
	{
		if (BFCG_IsDestroyed())
		{
			BFCG_ClearClaimState();
			return;
		}
		bool hadClaim = !m_sBFCGClaimOwnerIdentity.IsEmpty();
		m_sBFCGClaimOwnerIdentity = ownerIdentity;
		m_bBFCGClaimLocked = locked;
		m_iBFCGClaimExpiresAt = expiresAt;
		BFCG_VehicleClaimState.Register(this);
		if (!hadClaim)
			BFCG_SetGarbageProtection(true);
	}

	void BFCG_ClearClaimState()
	{
		m_sBFCGClaimOwnerIdentity = string.Empty;
		m_bBFCGClaimLocked = false;
		m_iBFCGClaimExpiresAt = 0;
		BFCG_VehicleClaimState.Unregister(this);
		BFCG_SetGarbageProtection(false);
	}

	// Active claims protect against vanilla garbage collection; releasing a claim removes that protection.
	protected void BFCG_SetGarbageProtection(bool protect)
	{
		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_GarbageSystem garbageSystem = SCR_GarbageSystem.GetByEntityWorld(owner);
		if (garbageSystem)
			garbageSystem.UpdateBlacklist(owner, protect);
	}

}

// The server attaches proxies; clients discover replicated children through the vehicle context.
class BFCG_VehicleClaimActionProxySystem
{
	protected static int s_iLoggedSpawns;

	static void EnsureAllVehicleProxies()
	{
		if (!Replication.IsServer())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere("0 0 0", 1000000, OnQueryVehicle, null, EQueryEntitiesFlags.DYNAMIC);
	}

	protected static bool OnQueryVehicle(IEntity entity)
	{
		if (!entity)
			return true;

		VehicleControllerComponent controller = VehicleControllerComponent.Cast(
			entity.FindComponent(VehicleControllerComponent));
		if (controller)
			controller.BFCG_EnsureClaimActionProxy();

		return true;
	}

	static void ReportSpawn(IEntity vehicle)
	{
		if (s_iLoggedSpawns >= 20)
			return;

		s_iLoggedSpawns++;
		Print(string.Format("[BFCG VehicleClaim] Action proxy spawned for %1",
			BFCG_VehicleClaimState.BFCG_EntityName(vehicle)), LogLevel.NORMAL);
	}
}

// Wreck deadlines are serialized with the vehicle; only wrecks are visited by this one-second tick.
class BFCG_DestroyedVehicleCleanupSystem
{
	protected static ref array<VehicleControllerComponent> s_Vehicles = {};

	static void Register(VehicleControllerComponent component)
	{
		if (component && !s_Vehicles.Contains(component))
			s_Vehicles.Insert(component);
	}

	static void Unregister(VehicleControllerComponent component)
	{
		s_Vehicles.RemoveItem(component);
	}

	static void Sweep()
	{
		if (!Replication.IsServer() || !BFCG_Settings.IsValid() || SCR_PersistenceSystem.IsLoadInProgress())
			return;
		for (int i = s_Vehicles.Count() - 1; i >= 0; i--)
		{
			VehicleControllerComponent component = s_Vehicles[i];
			if (!component || !component.GetOwner() || component.GetOwner().IsDeleted())
			{
				s_Vehicles.Remove(i);
				continue;
			}
			IEntity vehicle = component.GetOwner();
			if (BFCG_ExcludedVehicleComponent.IsExcluded(vehicle))
			{
				s_Vehicles.Remove(i);
				continue;
			}
			component.BFCG_ReconcileWreck();
			if (component.BFCG_GetWreckDeleteAt() <= 0 || component.BFCG_GetWreckDeleteAt() > System.GetUnixTime())
				continue;
			// Deleting children must never delete a seated player/AI, including after a third-party seat override.
			if (BFCG_GARAGE_GarageManager.HasOccupants(vehicle))
				continue;
			PersistenceSystem persistence = PersistenceSystem.GetInstance();
			if (persistence && persistence.IsTracked(vehicle))
			{
				if (persistence.GetState() != EPersistenceSystemState.ACTIVE || !persistence.StopTracking(vehicle, true))
					continue;
			}
			// Removal of the native record becomes durable at the scenario's next successful save commit.
			s_Vehicles.Remove(i);
			Print("[BFCG] WRECK DELETE vehicle=" + BFCG_VehicleClaimState.BFCG_EntityName(vehicle));
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
	}
}

// Optional cleanup starts a configured grace interval on first observation of an unclaimed vehicle.
// Vanilla garbage collection is suppressed during this interval to preserve the configured lifetime.
class BFCG_UnclaimedVehicleCleanupSystem
{

	protected static ref array<IEntity> s_aTrackedVehicles = {};
	protected static ref array<int> s_aUnclaimedSince = {};
	protected static ref array<IEntity> s_aSeenVehicles = {};
	protected static ref array<IEntity> s_aPendingDeletion = {};
	protected static int s_iSweepTime;

	static void Sweep()
	{
		if (!Replication.IsServer() || !BFCG_Settings.IsValid() || !BFCG_Settings.Get().enableUnclaimedCleanup)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		s_iSweepTime = System.GetUnixTime();
		s_aSeenVehicles.Clear();
		s_aPendingDeletion.Clear();
		world.QueryEntitiesBySphere("0 0 0", 1000000, OnQueryVehicle, null, EQueryEntitiesFlags.DYNAMIC);

		for (int i = s_aTrackedVehicles.Count() - 1; i >= 0; i--)
		{
			IEntity vehicle = s_aTrackedVehicles[i];
			if (!vehicle || vehicle.IsDeleted() || !s_aSeenVehicles.Contains(vehicle))
			{
				s_aTrackedVehicles.Remove(i);
				s_aUnclaimedSince.Remove(i);
			}
		}

		foreach (IEntity vehicle : s_aPendingDeletion)
		{
			if (!vehicle || vehicle.IsDeleted())
				continue;

			VehicleControllerComponent controller = VehicleControllerComponent.Cast(
				vehicle.FindComponent(VehicleControllerComponent));
			if (!controller || controller.BFCG_IsDestroyed() || BFCG_VehicleClaimState.HasClaim(controller))
				continue;

			SCR_GarbageSystem garbageSystem = SCR_GarbageSystem.GetByEntityWorld(vehicle);
			if (garbageSystem)
				garbageSystem.UpdateBlacklist(vehicle, false);

			Print(string.Format("[BFCG VehicleClaim] UNCLAIMED DELETE vehicle=%1 age=%2s",
				BFCG_VehicleClaimState.BFCG_EntityName(vehicle), (BFCG_Settings.Get().unclaimedLifetimeHours * 3600)), LogLevel.NORMAL);
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
	}

	protected static bool OnQueryVehicle(IEntity entity)
	{
		if (!entity)
			return true;

		VehicleControllerComponent controller = VehicleControllerComponent.Cast(
			entity.FindComponent(VehicleControllerComponent));
		if (!controller || BFCG_ExcludedVehicleComponent.IsExcluded(entity))
			return true;

		if (controller.BFCG_IsDestroyed())
		{
			controller.BFCG_ReconcileWreck();
			return true;
		}

		s_aSeenVehicles.Insert(entity);
		int index = s_aTrackedVehicles.Find(entity);
		if (BFCG_VehicleClaimState.HasClaim(controller))
		{
			if (index >= 0)
			{
				s_aTrackedVehicles.Remove(index);
				s_aUnclaimedSince.Remove(index);
			}
			return true;
		}

		if (index < 0)
		{
			s_aTrackedVehicles.Insert(entity);
			s_aUnclaimedSince.Insert(s_iSweepTime);
			index = s_aTrackedVehicles.Count() - 1;
			Print(string.Format("[BFCG VehicleClaim] UNCLAIMED TRACK vehicle=%1 deleteAt=%2",
				BFCG_VehicleClaimState.BFCG_EntityName(entity), s_iSweepTime + (BFCG_Settings.Get().unclaimedLifetimeHours * 3600)), LogLevel.NORMAL);
		}

		SCR_GarbageSystem garbageSystem = SCR_GarbageSystem.GetByEntityWorld(entity);
		if (garbageSystem)
			garbageSystem.UpdateBlacklist(entity, true);

		if (s_iSweepTime - s_aUnclaimedSince[index] >= (BFCG_Settings.Get().unclaimedLifetimeHours * 3600))
			s_aPendingDeletion.Insert(entity);

		return true;
	}
}

modded class SCR_BaseGameMode
{
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
		{
			BFCG_Settings.Load();
			if (!BFCG_Settings.IsValid())
				return;
			GetGame().GetCallqueue().CallLater(BFCG_VehicleClaimActionProxySystem.EnsureAllVehicleProxies, 1000, false);
			GetGame().GetCallqueue().CallLater(BFCG_VehicleClaimActionProxySystem.EnsureAllVehicleProxies, BFCG_Settings.Get().proxyScanSeconds * 1000, true);
			GetGame().GetCallqueue().CallLater(BFCG_VehicleClaimState.SweepExpiredClaims, 60000, true);
			GetGame().GetCallqueue().CallLater(BFCG_UnclaimedVehicleCleanupSystem.Sweep, 1000, false);
			GetGame().GetCallqueue().CallLater(BFCG_UnclaimedVehicleCleanupSystem.Sweep, 300000, true);
			GetGame().GetCallqueue().Remove(BFCG_DestroyedVehicleCleanupSystem.Sweep);
			GetGame().GetCallqueue().CallLater(BFCG_DestroyedVehicleCleanupSystem.Sweep, 1000, true);
			BFCG_VehicleClaimPersistence.BeginRestore();

			GetGame().GetCallqueue().CallLater(BFCG_VehicleClaimPersistence.Restore, BFCG_Settings.Get().initialRestoreDelaySeconds * 1000, false);
		}
		else
		{
			BFCG_VehicleClaimClientCache.Clear();
		}
	}

	override protected void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		if (Replication.IsServer())
		{
			BFCG_VehicleClaimState.CachePlayerIdentity(playerId);
			GetGame().GetCallqueue().CallLater(BFCG_VehicleClaimState.SyncClaimsToPlayer, 1000, false, playerId);
		}
	}

	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			BFCG_VehicleClaimState.HandlePlayerDisconnected(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
}
