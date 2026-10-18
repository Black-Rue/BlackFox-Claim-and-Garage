/*
 * Parking-spot registration and occupancy visuals, grouped by GarageID.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

[ComponentEditorProps(category: "BFCG Garage", description: "Parking spot marker: players leave the vehicle here. Parking is done at the garage point menu.")]
class BFCG_GARAGE_ParkingSpotComponentClass : ScriptComponentClass {}

class BFCG_GARAGE_ParkingSpotComponent : ScriptComponent
{
	protected static ref map<string, ref array<BFCG_GARAGE_ParkingSpotComponent>> s_mSpotsByGarageID;

	protected const int VISUAL_STATE_HIDDEN  = 0;
	protected const int VISUAL_STATE_NEUTRAL = 1;
	protected const int VISUAL_STATE_ACTIVE  = 2;
	protected const int VISUAL_STATE_ERROR   = 3;
	protected const int PROXIMITY_TICK_MS    = 250;

	[Attribute(defvalue: "", UIWidgets.EditBox, "Garage ID of the linked garage point (must match m_sGarageID on the GarageComponent)")]
	protected string m_sLinkedGarageID;

	[Attribute(defvalue: "5", UIWidgets.Slider, "Radius in which a vehicle is searched for parking (meters)", params: "1 50 1")]
	protected float m_fParkRadius;

	[Attribute(defvalue: "15", UIWidgets.Slider, "Approach radius: circle appears when a vehicle is within this distance (meters)", params: "1 100 1")]
	protected float m_fApproachRadius;

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Visual prefab for approach (white/neutral)", params: "et")]
	protected ResourceName m_rNeutralVisualPrefab;

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Visual prefab for park radius (green/active)", params: "et")]
	protected ResourceName m_rActiveVisualPrefab;

	[Attribute(defvalue: "", UIWidgets.ResourceNamePicker, "Visual prefab for error (red, multiple vehicles on spot)", params: "et")]
	protected ResourceName m_rErrorVisualPrefab;

	protected IEntity m_Owner;
	protected IEntity m_NeutralVisual;
	protected IEntity m_ActiveVisual;
	protected IEntity m_ErrorVisual;
	protected int m_iVisualState;

	protected bool m_bAnyVehicleInApproach;
	protected int  m_iVehiclesInPark;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		m_iVisualState = VISUAL_STATE_HIDDEN;

		// Show the placement visual in Workbench without registering a runtime parking spot.
		if (!GetGame().InPlayMode())
		{

			if (!m_rActiveVisualPrefab.IsEmpty())
				m_ActiveVisual = SpawnVisualAtOwner(m_rActiveVisualPrefab, m_fParkRadius * 2.0);
			return;
		}

		RegisterInRegistry();
		SpawnVisuals();
		SetVisualState(VISUAL_STATE_HIDDEN);

		GetGame().GetCallqueue().CallLater(TickProximity, PROXIMITY_TICK_MS, true);

		Print(string.Format("[BFCG_GARAGE_ParkingSpot] Registered | GarageID='%1' | ParkR=%2 | ApproachR=%3",
			m_sLinkedGarageID, m_fParkRadius, m_fApproachRadius), LogLevel.NORMAL);
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue cq = GetGame().GetCallqueue();
			if (cq)
				cq.Remove(TickProximity);
		}

		if (m_NeutralVisual)
			SCR_EntityHelper.DeleteEntityAndChildren(m_NeutralVisual);
		if (m_ActiveVisual)
			SCR_EntityHelper.DeleteEntityAndChildren(m_ActiveVisual);
		if (m_ErrorVisual)
			SCR_EntityHelper.DeleteEntityAndChildren(m_ErrorVisual);

		UnregisterFromRegistry();
	}

	static array<BFCG_GARAGE_ParkingSpotComponent> GetSpotsForGarage(string garageID)
	{
		if (garageID.IsEmpty() || !s_mSpotsByGarageID || !s_mSpotsByGarageID.Contains(garageID))
			return null;
		return s_mSpotsByGarageID.Get(garageID);
	}

	IEntity GetOwnerEntity()
	{
		return m_Owner;
	}

	float GetParkRadius()
	{
		return m_fParkRadius;
	}

	protected void RegisterInRegistry()
	{
		if (m_sLinkedGarageID.IsEmpty())
			return;
		if (!s_mSpotsByGarageID)
			s_mSpotsByGarageID = new map<string, ref array<BFCG_GARAGE_ParkingSpotComponent>>();
		if (!s_mSpotsByGarageID.Contains(m_sLinkedGarageID))
			s_mSpotsByGarageID.Set(m_sLinkedGarageID, new array<BFCG_GARAGE_ParkingSpotComponent>());
		s_mSpotsByGarageID.Get(m_sLinkedGarageID).Insert(this);
	}

	protected void UnregisterFromRegistry()
	{
		if (m_sLinkedGarageID.IsEmpty() || !s_mSpotsByGarageID || !s_mSpotsByGarageID.Contains(m_sLinkedGarageID))
			return;
		array<BFCG_GARAGE_ParkingSpotComponent> spots = s_mSpotsByGarageID.Get(m_sLinkedGarageID);
		int idx = spots.Find(this);
		if (idx >= 0)
			spots.Remove(idx);
	}

	protected void SpawnVisuals()
	{
		if (!m_Owner)
			return;

		float diameter = m_fParkRadius * 2.0;

		if (!m_rNeutralVisualPrefab.IsEmpty())
			m_NeutralVisual = SpawnVisualAtOwner(m_rNeutralVisualPrefab, diameter);
		if (!m_rActiveVisualPrefab.IsEmpty())
			m_ActiveVisual = SpawnVisualAtOwner(m_rActiveVisualPrefab, diameter);
		if (!m_rErrorVisualPrefab.IsEmpty())
			m_ErrorVisual = SpawnVisualAtOwner(m_rErrorVisualPrefab, diameter);
	}

	protected IEntity SpawnVisualAtOwner(ResourceName prefab, float scale)
	{
		Resource res = Resource.Load(prefab);
		if (!res.IsValid())
		{
			Print(string.Format("[BFCG_GARAGE_ParkingSpot] Resource invalid: '%1'", prefab), LogLevel.ERROR);
			return null;
		}

		vector mat[4];
		m_Owner.GetTransform(mat);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = mat;

		IEntity visual = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!visual)
		{
			Print(string.Format("[BFCG_GARAGE_ParkingSpot] SpawnEntityPrefab returned null for '%1'", prefab), LogLevel.ERROR);
			return null;
		}

		return visual;
	}

	protected void TickProximity()
	{
		if (!m_Owner)
			return;

		m_bAnyVehicleInApproach = false;
		m_iVehiclesInPark = 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float queryRadius = m_fApproachRadius;
		if (m_fParkRadius > queryRadius)
			queryRadius = m_fParkRadius;

		world.QueryEntitiesBySphere(
			m_Owner.GetOrigin(), queryRadius, OnProximityQuery, null, EQueryEntitiesFlags.DYNAMIC
		);

		int newState = VISUAL_STATE_HIDDEN;
		if (m_iVehiclesInPark > 1)
			newState = VISUAL_STATE_ERROR;
		else if (m_iVehiclesInPark == 1)
			newState = VISUAL_STATE_ACTIVE;
		else if (m_bAnyVehicleInApproach)
			newState = VISUAL_STATE_NEUTRAL;

		if (newState != m_iVisualState)
			SetVisualState(newState);
	}

	protected bool OnProximityQuery(IEntity entity)
	{
		if (!entity || !Vehicle.Cast(entity))
			return true;
		float dist = vector.Distance(entity.GetOrigin(), m_Owner.GetOrigin());
		if (dist <= m_fApproachRadius)
			m_bAnyVehicleInApproach = true;
		if (dist <= m_fParkRadius)
			m_iVehiclesInPark++;
		return true;
	}

	protected void SetVisualState(int state)
	{
		m_iVisualState = state;
		ShowEntity(m_NeutralVisual, state == VISUAL_STATE_NEUTRAL);
		ShowEntity(m_ActiveVisual,  state == VISUAL_STATE_ACTIVE);
		ShowEntity(m_ErrorVisual,   state == VISUAL_STATE_ERROR);
	}

	protected void ShowEntity(IEntity entity, bool show)
	{
		if (!entity)
			return;
		if (show)
			entity.SetFlags(EntityFlags.VISIBLE, true);
		else
			entity.ClearFlags(EntityFlags.VISIBLE, true);
	}
}
