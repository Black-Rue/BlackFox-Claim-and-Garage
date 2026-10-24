/*
 * Finds a parking candidate among spots linked to the current terminal.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

// Prefer this player's active Claim, then unclaimed vehicles. Count all vehicles at the selected spot.
class BFCG_GARAGE_SpotScan
{
	protected IEntity m_NearestVehicle;
	protected IEntity m_ActiveSpot;
	protected int m_iVehicleCount;
	protected int m_iQueryCount;
	protected vector m_vQueryCenter;
	protected float m_fQueryRadius;
	protected IEntity m_PlayerEntity;
	protected bool m_bOwnClaimsOnly;

	bool Scan(BFCG_GARAGE_GarageComponent garageCmp, IEntity playerEntity)
	{
		m_NearestVehicle = null;
		m_ActiveSpot = null;
		m_iVehicleCount = 0;
		m_PlayerEntity = playerEntity;

		if (!Replication.IsServer() || !garageCmp || !playerEntity)
			return false;

		array<BFCG_GARAGE_ParkingSpotComponent> spots = BFCG_GARAGE_ParkingSpotComponent.GetSpotsForGarage(garageCmp.GetGarageID());
		if (!spots || spots.IsEmpty())
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		// Preserve spot order within each pass; unclaimed vehicles cannot displace an owned candidate.
		for (int pass = 0; pass < 2; pass++)
		{
			m_bOwnClaimsOnly = (pass == 0);
			foreach (BFCG_GARAGE_ParkingSpotComponent spot : spots)
			{
				if (!spot)
					continue;
				IEntity spotEntity = spot.GetOwnerEntity();
				if (!spotEntity)
					continue;

				m_NearestVehicle = null;
				m_iQueryCount = 0;
				m_vQueryCenter = spotEntity.GetOrigin();
				m_fQueryRadius = spot.GetParkRadius();
				world.QueryEntitiesBySphere(
					m_vQueryCenter, m_fQueryRadius, OnQueryEntity, null, EQueryEntitiesFlags.DYNAMIC
				);
				if (m_NearestVehicle)
				{
					m_ActiveSpot = spotEntity;
					m_iVehicleCount = m_iQueryCount;
					return true;
				}
			}
		}
		return false;
	}

	IEntity GetVehicle()
	{
		return m_NearestVehicle;
	}

	IEntity GetSpot()
	{
		return m_ActiveSpot;
	}

	int GetVehicleCount()
	{
		return m_iVehicleCount;
	}

	protected bool OnQueryEntity(IEntity entity)
	{
		if (!entity || !Vehicle.Cast(entity))
			return true;

		// Match the parking visual: large vehicle bounds can intersect a neighbouring spot.
		float distNew = vector.Distance(entity.GetOrigin(), m_vQueryCenter);
		if (distNew > m_fQueryRadius)
			return true;

		m_iQueryCount++;

		// Ownership filters the candidate, never the physical occupancy count.
		VehicleControllerComponent controller = VehicleControllerComponent.Cast(entity.FindComponent(VehicleControllerComponent));
		if (!controller || controller.BFCG_IsDestroyed() || BFCG_ExcludedVehicleComponent.IsExcluded(entity))
			return true;
		if (m_bOwnClaimsOnly)
		{
			if (!BFCG_VehicleClaimState.IsOwner(controller, m_PlayerEntity))
				return true;
		}
		else if (BFCG_VehicleClaimState.HasClaim(controller))
			return true;

		if (!m_NearestVehicle)
		{
			m_NearestVehicle = entity;
			return true;
		}

		float distBest = vector.Distance(m_NearestVehicle.GetOrigin(), m_vQueryCenter);
		if (distNew < distBest)
			m_NearestVehicle = entity;

		return true;
	}
}
