//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_SpotScan.c
//! BFCG_Garage - серверный скан спота: найти ТС на парковочных спотах гаража
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//! Инстанс-класс: Query-callback не может быть static. Только для сервера.
//------------------------------------------------------------------------------------------------

class BFCG_GARAGE_SpotScan
{
	protected IEntity m_NearestVehicle;
	protected IEntity m_ActiveSpot;
	protected int m_iVehicleCount;
	protected int m_iQueryCount;
	protected vector m_vQueryCenter;

	// true = ТС найдено; детали через GetVehicle()/GetSpot()/GetVehicleCount()
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

	// Число ТС в радиусе спота, на котором найдено ТС (>1 = парковка заблокирована)
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
