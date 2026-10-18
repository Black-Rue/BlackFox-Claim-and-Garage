/*
 * Stored vehicle records and their JSON representation.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

// Parking stores the vehicle prefab and ownership record, not cargo or full live vehicle state.
class BFCG_GARAGE_VehicleData
{
	protected static int s_iIDSequence;
	string m_sVehicleID;
	string m_sOwnerUID;
	string m_sVehicleName;
	ResourceName m_rPrefab;
	vector m_vPosition;
	vector m_vMatRight;
	vector m_vMatUp;
	vector m_vMatForward;
	int m_iParkedAt;
	ref array<ref BFCG_GARAGE_ItemNode> m_aTrunk;

	void BFCG_GARAGE_VehicleData()
	{
		m_sVehicleID = string.Empty;
		m_sOwnerUID = string.Empty;
		m_sVehicleName = string.Empty;
		m_vPosition = vector.Zero;
		m_vMatRight   = "1 0 0";
		m_vMatUp      = "0 1 0";
		m_vMatForward = "0 0 1";
		m_iParkedAt = 0;
		m_aTrunk = new array<ref BFCG_GARAGE_ItemNode>();
	}

	string GetVehicleID()
	{
		return m_sVehicleID;
	}

	static BFCG_GARAGE_VehicleData FromVehicle(IEntity vehicle, string ownerUID, IEntity spawnEntity)
	{
		if (!vehicle)
			return null;

		BFCG_GARAGE_VehicleData data = new BFCG_GARAGE_VehicleData();
		data.m_sOwnerUID = ownerUID;

		vector mat[4];
		vehicle.GetTransform(mat);
		data.m_vMatRight   = mat[0];
		data.m_vMatUp      = mat[1];
		data.m_vMatForward = mat[2];

		if (spawnEntity)
			data.m_vPosition = spawnEntity.GetOrigin();
		else
			data.m_vPosition = mat[3];

		int ts = System.GetUnixTime();
		data.m_iParkedAt = ts;

		// A sequence suffix prevents ID collisions when parking requests share the same Unix second.
		while (true)
		{
			s_iIDSequence++;
			data.m_sVehicleID = ownerUID + "_" + ts.ToString() + "_" + s_iIDSequence.ToString();
			if (!BFCG_GARAGE_GarageManager.GetInstance().GetVehicleByID(data.m_sVehicleID))
				break;
		}

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (prefabData)
		{
			data.m_rPrefab = prefabData.GetPrefabName();
			data.m_sVehicleName = ResourceNameToDisplayName(data.m_rPrefab);
		}

		return data;
	}

	static string ResourceNameToDisplayName(ResourceName rn)
	{
		string s = rn;

		int brace = s.IndexOf("}");
		if (brace >= 0)
			s = s.Substring(brace + 1, s.Length() - brace - 1);

		array<string> pathParts = {};
		s.Split("/", pathParts, false);
		if (!pathParts.IsEmpty())
			s = pathParts[pathParts.Count() - 1];

		array<string> extParts = {};
		s.Split(".", extParts, false);
		if (!extParts.IsEmpty())
			s = extParts[0];

		return s;
	}
}

// The trunk field remains readable for record compatibility; retrieval ignores stored cargo.
class BFCG_GARAGE_VehicleDataJson : JsonApiStruct
{
	string vehicleID;
	string ownerUID;
	string vehicleName;
	string prefab;
	float posX;
	float posY;
	float posZ;
	float matRightX;
	float matRightY;
	float matRightZ;
	float matUpX;
	float matUpY;
	float matUpZ;
	float matFwdX;
	float matFwdY;
	float matFwdZ;
	int parkedAt;
	ref array<ref BFCG_GARAGE_ItemNode> trunk;

	void BFCG_GARAGE_VehicleDataJson()
	{
		RegV("vehicleID");
		RegV("ownerUID");
		RegV("vehicleName");
		RegV("prefab");
		RegV("posX");
		RegV("posY");
		RegV("posZ");
		RegV("matRightX");
		RegV("matRightY");
		RegV("matRightZ");
		RegV("matUpX");
		RegV("matUpY");
		RegV("matUpZ");
		RegV("matFwdX");
		RegV("matFwdY");
		RegV("matFwdZ");
		RegV("parkedAt");
		RegV("trunk");
		trunk = new array<ref BFCG_GARAGE_ItemNode>();
	}

	static BFCG_GARAGE_VehicleDataJson FromData(BFCG_GARAGE_VehicleData data)
	{
		BFCG_GARAGE_VehicleDataJson j = new BFCG_GARAGE_VehicleDataJson();
		j.vehicleID   = data.m_sVehicleID;
		j.ownerUID    = data.m_sOwnerUID;
		j.vehicleName = data.m_sVehicleName;
		j.prefab      = data.m_rPrefab;
		j.posX        = data.m_vPosition[0];
		j.posY        = data.m_vPosition[1];
		j.posZ        = data.m_vPosition[2];
		j.matRightX   = data.m_vMatRight[0];
		j.matRightY   = data.m_vMatRight[1];
		j.matRightZ   = data.m_vMatRight[2];
		j.matUpX      = data.m_vMatUp[0];
		j.matUpY      = data.m_vMatUp[1];
		j.matUpZ      = data.m_vMatUp[2];
		j.matFwdX     = data.m_vMatForward[0];
		j.matFwdY     = data.m_vMatForward[1];
		j.matFwdZ     = data.m_vMatForward[2];
		j.parkedAt    = data.m_iParkedAt;
		j.trunk       = data.m_aTrunk;
		return j;
	}

	BFCG_GARAGE_VehicleData ToData()
	{
		BFCG_GARAGE_VehicleData data = new BFCG_GARAGE_VehicleData();
		data.m_sVehicleID   = vehicleID;
		data.m_sOwnerUID    = ownerUID;
		data.m_sVehicleName = vehicleName;
		data.m_rPrefab      = prefab;
		data.m_vPosition   = Vector(posX, posY, posZ);
		data.m_vMatRight   = Vector(matRightX, matRightY, matRightZ);
		data.m_vMatUp      = Vector(matUpX,    matUpY,    matUpZ);
		data.m_vMatForward = Vector(matFwdX,   matFwdY,   matFwdZ);
		data.m_iParkedAt   = parkedAt;

		if (trunk)
			data.m_aTrunk = trunk;
		return data;
	}
}

class BFCG_GARAGE_GarageStorageJson : JsonApiStruct
{
	ref array<ref BFCG_GARAGE_VehicleDataJson> vehicles;

	void BFCG_GARAGE_GarageStorageJson()
	{
		vehicles = new array<ref BFCG_GARAGE_VehicleDataJson>();
		RegV("vehicles");
	}
}
