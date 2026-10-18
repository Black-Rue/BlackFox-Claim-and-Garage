//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_GarageComponent.c
//! BFCG_Garage - ScriptComponent пульта персонального гаража
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//! Changes: только персональный гараж (GroupFilter/Group-подкласс удалены).
//! Компонент хранит только конфиг (GarageID), парковочной логики в нём нет.
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "BFCG Garage", description: "Personal garage interaction point. Place on any prefab to make it a garage.")]
class BFCG_GARAGE_GarageComponentClass : ScriptComponentClass {}

class BFCG_GARAGE_GarageComponent : ScriptComponent
{
	[Attribute(defvalue: "", UIWidgets.EditBox, "Unique garage ID (must match m_sLinkedGarageID on linked ParkingSpots)")]
	protected string m_sGarageID;

	protected IEntity m_Owner;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;

		Print(string.Format("[BFCG_GARAGE_GarageComponent] OnPostInit | ID='%1'", m_sGarageID), LogLevel.NORMAL);

		if (!GetGame().InPlayMode())
			return;

		ActionsManagerComponent actMgr = ActionsManagerComponent.Cast(m_Owner.FindComponent(ActionsManagerComponent));
		if (!actMgr)
			Print("[BFCG_GARAGE_GarageComponent] WARNING: No ActionsManagerComponent on garage entity!", LogLevel.WARNING);
	}

	string GetGarageID()
	{
		return m_sGarageID;
	}
}
