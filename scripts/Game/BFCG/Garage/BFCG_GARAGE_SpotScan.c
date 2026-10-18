/*
 * Finds a parking candidate among spots linked to the current terminal.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

// Use the first occupied linked spot. Vehicle count refers to that spot, not the whole terminal.
class BFCG_GARAGE_SpotScan
{
	protected IEntity m_NearestVehicle;
	protected IEntity m_ActiveSpot;
	protected int m_iVehicleCount;
	protected int m_iQueryCount;
	protected vector m_vQueryCenter;

	bool Scan(BFCG_GARAGE_GarageComponent garageCmp)
	{
		m_NearestVehicle = null;
		m_ActiveSpot = null;
		m_iVehicleCount = 0;

		if (!garageCmp)
			return false;

		array<BFCG_GARAGE_ParkingSpotComponent> spots = BFCG_GARAGE_ParkingSpotComponent.GetSpotsForGarage(garageCmp.GetGarageID());
		if (!spots || spots.IsEmpty())
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

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
			world.QueryEntitiesBySphere(
				m_vQueryCenter, spot.GetParkRadius(), OnQueryEntity, null, EQueryEntitiesFlags.DYNAMIC
			);
			if (m_NearestVehicle)
			{
				m_ActiveSpot = spotEntity;
				m_iVehicleCount = m_iQueryCount;
				return true;
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

		m_iQueryCount++;

		if (!m_NearestVehicle)
		{
			m_NearestVehicle = entity;
			return true;
		}

		float distNew  = vector.Distance(entity.GetOrigin(),           m_vQueryCenter);
		float distBest = vector.Distance(m_NearestVehicle.GetOrigin(), m_vQueryCenter);
		if (distNew < distBest)
			m_NearestVehicle = entity;

		return true;
	}
}
