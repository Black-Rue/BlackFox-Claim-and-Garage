/*
 * Client-side personal garage view. Server RPCs provide records and operation results.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

// Components created with new do not receive Attribute defaults; initialise flags before attachment.
class BFCG_GARAGE_GamepadScrollComponent : SCR_GamepadScrollComponent
{

	override void HandlerAttached(Widget w)
	{
		m_bTryFindNewFocus = true;
		m_bScrollEnabled = true;
		m_bIsVerticalScroll = true;
		m_bAllowScrollWheel = false;
		super.HandlerAttached(w);
	}
}

class BFCG_GARAGE_GarageMenuUI : ChimeraMenuBase
{
	protected static const ResourceName CARD_LAYOUT = "{E44B148A7EA37AF4}UI/layouts/Menus/BFCG_GarageCard.layout";

	protected static const ResourceName PREVIEW_MANAGER_PREFAB = "{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et";

	protected static const ResourceName TAB_BUTTON_LAYOUT = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";

	protected static const ResourceName HINT_BUTTON_LAYOUT = "{08CF3B69CB1ACBC4}UI/layouts/WidgetLibrary/WLib_NavigationButton.layout";

	protected static const int GRID_COLUMNS = 4;
	protected static const int DELETE_CONFIRM_TIMEOUT_MS = 3000;

	// OpenMenu has no terminal argument, so the local action supplies the context before opening.
	protected static RplId s_ContextGarageId;
	protected static bool s_bContextSet = false;

	protected static BFCG_GARAGE_GarageMenuUI s_OpenInstance;

	protected ItemPreviewManagerEntity m_PreviewManager;
	protected Widget m_wRoot;
	protected Widget m_wCardGrid;
	protected Widget m_wTabBar;
	protected TextWidget m_wErrorText;

	protected ImageWidget m_wParkImage;
	protected ItemPreviewWidget m_wParkPreview;
	protected TextWidget m_wParkNameText;
	protected Widget m_wParkHoverBorder;
	protected SCR_ButtonTextComponent m_ParkButton;
	protected string m_sSpotPrefab;
	protected int m_iSpotVehicleCount;
	protected bool m_bParkPending;

	// Buffer owner-RPC entries until ContentsComplete, then rebuild the view.
	protected ref array<ref BFCG_GARAGE_MenuEntry> m_aEntries = {};

	protected ref array<SCR_ButtonTextComponent> m_aTabButtons = {};
	protected string m_sActiveTabName;

	protected ref array<ref BFCG_GARAGE_GarageCardComponent> m_aCards = {};

	// Parallel arrays map each delete handler to its stored vehicle ID.
	protected ref array<BFCG_GARAGE_CardDeleteComponent> m_aDeleteComps = {};
	protected ref array<string> m_aDeleteVehicleIDs = {};

	// Suppress repeated requests while awaiting server results; these flags do not grant authority.
	protected bool m_bUnpackPending = false;
	protected bool m_bDeletePending = false;
	protected BFCG_GARAGE_CardDeleteComponent m_ArmedDeleteComp;

	protected ref BFCG_GARAGE_GamepadScrollComponent m_GamepadScroll;
	protected bool m_bBackHintWired;
	protected bool m_bBackListenerRegistered;
	protected bool m_bDeviceListenerRegistered;

	protected SCR_ButtonTextComponent m_CloseButton;
	protected bool m_bDefaultStyleCached;
	protected ref Color m_DefaultBg;
	protected ref Color m_DefaultBgHover;
	protected ref Color m_DefaultBgSelected;
	protected ref Color m_DefaultBgSelectedHover;
	protected ref Color m_DefaultBgClicked;
	protected ref Color m_DefaultTextColor;

	static void SetGarageContext(RplId garageId)
	{
		s_ContextGarageId = garageId;
		s_bContextSet = true;
	}

	static BFCG_GARAGE_GarageMenuUI GetOpenInstance()
	{
		return s_OpenInstance;
	}

	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		s_OpenInstance = this;

		m_wRoot = GetRootWidget();
		if (!m_wRoot)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] OnMenuOpen: root widget not found", LogLevel.ERROR);
			return;
		}

		m_wCardGrid = m_wRoot.FindAnyWidget("CardGrid");
		if (!m_wCardGrid)
			Print("[BFCG_GARAGE_GarageMenuUI] Widget 'CardGrid' not found in layout", LogLevel.WARNING);

		m_wTabBar = m_wRoot.FindAnyWidget("TabBar");
		if (m_wTabBar)
			LayoutSlot.SetPadding(m_wTabBar, 40, 0, 0, 0);
		Widget separatorSize = m_wRoot.FindAnyWidget("TabSeparatorSize");
		if (separatorSize)
		{
			LayoutSlot.SetHorizontalAlign(separatorSize, LayoutHorizontalAlign.Stretch);
			LayoutSlot.SetPadding(separatorSize, 40, 0, 30, 0);
		}

		InitParkPanel();

		m_wErrorText = TextWidget.Cast(m_wRoot.FindAnyWidget("ErrorText"));
		if (!m_wErrorText)
			Print("[BFCG_GARAGE_GarageMenuUI] Widget 'ErrorText' not found - no error line", LogLevel.WARNING);

		Widget closeButton = m_wRoot.FindAnyWidget("ButtonClose");
		if (closeButton)
		{
			SCR_ButtonBaseComponent buttonComp = SCR_ButtonBaseComponent.Cast(closeButton.FindHandler(SCR_ButtonBaseComponent));
			if (buttonComp)
			{
				buttonComp.m_OnClicked.Insert(OnCloseClicked);

				SCR_ButtonTextComponent textComp = SCR_ButtonTextComponent.Cast(buttonComp);
				StyleButtonOrange(textComp);
				if (textComp)
				{
					textComp.SetText("Close");
					m_CloseButton = textComp;
					textComp.m_bShowBorderOnFocus = false;
					textComp.m_OnFocus.Insert(OnMenuButtonFocus);
					textComp.m_OnFocusLost.Insert(OnMenuButtonFocusLost);
				}
			}
		}

		InitGamepadSupport();

		// Fallback for layouts where the Back hint could not register its own action.
		if (!m_bBackHintWired)
		{
			GetGame().GetInputManager().AddActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnMenuBack);
			m_bBackListenerRegistered = true;
		}

		RequestContents();
		Print("[BFCG_GARAGE_GarageMenuUI] Menu opened", LogLevel.NORMAL);
	}

	// Unregister input listeners and cancel the delete timer before releasing menu references.
	override void OnMenuClose()
	{
		if (s_OpenInstance == this)
			s_OpenInstance = null;
		ResetDeleteConfirmation();
		m_aCards.Clear();
		m_aDeleteComps.Clear();
		m_aDeleteVehicleIDs.Clear();
		m_aEntries.Clear();
		m_aTabButtons.Clear();
		m_bUnpackPending = false;
		m_bParkPending = false;
		m_bDeletePending = false;
		if (m_bBackListenerRegistered)
		{
			GetGame().GetInputManager().RemoveActionListener(UIConstants.MENU_ACTION_BACK, EActionTrigger.DOWN, OnMenuBack);
			m_bBackListenerRegistered = false;
		}
		if (m_bDeviceListenerRegistered)
		{
			GetGame().OnInputDeviceUserChangedInvoker().Remove(OnInputDeviceUserChanged);
			m_bDeviceListenerRegistered = false;
		}
		super.OnMenuClose();
	}

	protected void InitParkPanel()
	{
		Widget cardScroll = m_wRoot.FindAnyWidget("CardScroll");
		if (cardScroll)
		{
			LayoutSlot.SetSizeMode(cardScroll, LayoutSizeMode.Fill);
			LayoutSlot.SetFillWeight(cardScroll, 2);
			HorizontalLayoutSlot.SetVerticalAlign(cardScroll, LayoutVerticalAlign.Stretch);
		}

		Widget parkArea = m_wRoot.FindAnyWidget("ParkArea");
		if (parkArea)
		{
			LayoutSlot.SetSizeMode(parkArea, LayoutSizeMode.Fill);
			LayoutSlot.SetFillWeight(parkArea, 1);
			HorizontalLayoutSlot.SetVerticalAlign(parkArea, LayoutVerticalAlign.Stretch);
		}

		Widget parkPanel = m_wRoot.FindAnyWidget("ParkPanel");
		if (!parkPanel)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Widget 'ParkPanel' not found - no park panel", LogLevel.WARNING);
			return;
		}

		Widget imageSize = m_wRoot.FindAnyWidget("ParkImageSize");
		if (imageSize)
			LayoutSlot.SetHorizontalAlign(imageSize, LayoutHorizontalAlign.Center);

		m_wParkImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("ParkImage"));
		m_wParkPreview = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("ParkPreview"));
		m_wParkHoverBorder = m_wRoot.FindAnyWidget("ParkHoverBorder");
		m_wParkNameText = TextWidget.Cast(m_wRoot.FindAnyWidget("ParkVehicleName"));
		if (m_wParkNameText)
		{
			LayoutSlot.SetHorizontalAlign(m_wParkNameText, LayoutHorizontalAlign.Center);
			LayoutSlot.SetPadding(m_wParkNameText, 0, 12, 0, 0);
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget parkButton = workspace.CreateWidgets(TAB_BUTTON_LAYOUT, parkPanel);
		if (!parkButton)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Park button could not be created", LogLevel.ERROR);
			return;
		}
		parkButton.SetName("BFCG_ParkButton");
		LayoutSlot.SetHorizontalAlign(parkButton, LayoutHorizontalAlign.Center);
		LayoutSlot.SetPadding(parkButton, 0, 16, 0, 0);

		m_ParkButton = SCR_ButtonTextComponent.Cast(parkButton.FindHandler(SCR_ButtonTextComponent));
		if (!m_ParkButton)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Park button without SCR_ButtonTextComponent", LogLevel.ERROR);
			return;
		}
		m_ParkButton.SetText("Park (Clear Inventory)");
		StyleButtonOrange(m_ParkButton);
		m_ParkButton.m_OnClicked.Insert(OnParkClicked);
		m_ParkButton.m_OnFocus.Insert(OnParkButtonFocus);
		m_ParkButton.m_OnFocusLost.Insert(OnParkButtonFocusLost);
		m_ParkButton.m_bShowBorderOnFocus = false;
		m_ParkButton.m_OnFocus.Insert(OnMenuButtonFocus);
		m_ParkButton.m_OnFocusLost.Insert(OnMenuButtonFocusLost);
	}

	protected void OnParkButtonFocus(Widget w)
	{
		if (m_wParkHoverBorder)
			m_wParkHoverBorder.SetVisible(true);
	}

	protected void OnParkButtonFocusLost(Widget w)
	{
		if (m_wParkHoverBorder)
			m_wParkHoverBorder.SetVisible(false);
	}

	protected void StyleButtonOrange(SCR_ButtonTextComponent buttonComp)
	{
		if (!buttonComp)
			return;

		buttonComp.SetBackgroundColors(GUIColors.ORANGE, GUIColors.ORANGE, GUIColors.ORANGE, GUIColors.ORANGE, GUIColors.ORANGE);
		buttonComp.ColorizeBackground(false);
		TextWidget textWidget = buttonComp.GetTextWidget();
		if (textWidget)
		{
			AlignableSlot.SetPadding(textWidget, 14, 6, 14, 6);
			textWidget.SetColor(Color.White);
		}
	}

	protected void StyleButtonFocus(SCR_ButtonTextComponent buttonComp)
	{
		if (!buttonComp)
			return;

		buttonComp.SetBackgroundColors(Color.White, Color.White, Color.White, Color.White, Color.White);
		buttonComp.ColorizeBackground(false);
		TextWidget textWidget = buttonComp.GetTextWidget();
		if (textWidget)
			textWidget.SetColor(GUIColors.ORANGE);
	}

	protected void StyleButtonDefault(SCR_ButtonTextComponent buttonComp)
	{
		if (!buttonComp || !m_bDefaultStyleCached)
			return;

		buttonComp.SetBackgroundColors(m_DefaultBg, m_DefaultBgHover, m_DefaultBgSelected, m_DefaultBgSelectedHover, m_DefaultBgClicked);
		buttonComp.ColorizeBackground(false);
		TextWidget textWidget = buttonComp.GetTextWidget();
		if (textWidget && m_DefaultTextColor)
			textWidget.SetColor(m_DefaultTextColor);
	}

	protected SCR_ButtonTextComponent FindTextButton(Widget w)
	{
		if (!w)
			return null;
		return SCR_ButtonTextComponent.Cast(w.FindHandler(SCR_ButtonTextComponent));
	}

	protected bool IsOrangeStyledButton(SCR_ButtonTextComponent buttonComp)
	{
		return (buttonComp == m_ParkButton || buttonComp == m_CloseButton);
	}

	protected void OnMenuButtonFocus(Widget w)
	{
		StyleButtonFocus(FindTextButton(w));
	}

	protected void OnMenuButtonFocusLost(Widget w)
	{
		SCR_ButtonTextComponent buttonComp = FindTextButton(w);
		if (!buttonComp)
			return;

		if (IsOrangeStyledButton(buttonComp))
			StyleButtonOrange(buttonComp);
		else
			StyleButtonDefault(buttonComp);
	}

	protected void InitGamepadSupport()
	{
		Widget cardScroll = m_wRoot.FindAnyWidget("CardScroll");
		if (cardScroll)
		{
			m_GamepadScroll = new BFCG_GARAGE_GamepadScrollComponent();
			cardScroll.AddHandler(m_GamepadScroll);
		}

		InitHintBar();

		GetGame().OnInputDeviceUserChangedInvoker().Insert(OnInputDeviceUserChanged);
		m_bDeviceListenerRegistered = true;
	}

	protected void InitHintBar()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget hintBar = workspace.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_wRoot);
		if (!hintBar)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Hint bar could not be created", LogLevel.WARNING);
			return;
		}
		AlignableSlot.SetHorizontalAlign(hintBar, LayoutHorizontalAlign.Right);
		AlignableSlot.SetVerticalAlign(hintBar, LayoutVerticalAlign.Bottom);
		AlignableSlot.SetPadding(hintBar, 0, 0, 30, 30);

		CreateHintButton(workspace, hintBar, "Select", UIConstants.MENU_ACTION_SELECT, false);
		SCR_InputButtonComponent backHint = CreateHintButton(workspace, hintBar, "Back", UIConstants.MENU_ACTION_BACK, true);
		if (backHint)
			m_bBackHintWired = true;
	}

	protected SCR_InputButtonComponent CreateHintButton(WorkspaceWidget workspace, Widget parent, string label, string actionName, bool wireBack)
	{
		Widget hint = workspace.CreateWidgets(HINT_BUTTON_LAYOUT, parent);
		if (!hint)
		{
			Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Hint button '%1' could not be created", label), LogLevel.WARNING);
			return null;
		}
		AlignableSlot.SetPadding(hint, 20, 0, 0, 0);

		SCR_InputButtonComponent comp = SCR_InputButtonComponent.Cast(hint.FindHandler(SCR_InputButtonComponent));
		if (!comp)
		{
			Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Hint button '%1' without SCR_InputButtonComponent", label), LogLevel.WARNING);
			return null;
		}

		comp.SetLabel(label);
		comp.SetAction(actionName);
		if (wireBack)
			comp.m_OnActivated.Insert(OnBackHintActivated);
		return comp;
	}

	protected void OnBackHintActivated()
	{
		Close();
	}

	// Defer focus until the newly built widgets have completed a layout pass.
	protected void FocusInitialWidget()
	{
		GetGame().GetCallqueue().CallLater(ApplyFocus, 0, false);
	}

	protected void ApplyFocus()
	{
		if (s_OpenInstance != this)
			return;

		if (GetGame().GetInputManager().GetLastUsedInputDevice() == EInputDeviceType.MOUSE)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget target = GetPreferredFocusWidget();
		if (target)
			workspace.SetFocusedWidget(target);
	}

	protected Widget GetPreferredFocusWidget()
	{
		foreach (BFCG_GARAGE_GarageCardComponent card : m_aCards)
		{
			if (!card.GetVehicleID().IsEmpty())
				return card.GetRootWidget();
		}

		if (m_ParkButton)
		{
			Widget parkRoot = m_ParkButton.GetRootWidget();
			if (parkRoot && parkRoot.IsEnabled())
				return parkRoot;
		}

		if (!m_aTabButtons.IsEmpty() && m_aTabButtons[0])
			return m_aTabButtons[0].GetRootWidget();

		return null;
	}

	protected void OnInputDeviceUserChanged(EInputDeviceType oldDevice, EInputDeviceType newDevice)
	{
		if (newDevice == EInputDeviceType.MOUSE || newDevice == EInputDeviceType.TRACK_IR || newDevice == EInputDeviceType.JOYSTICK)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && !workspace.GetFocusedWidget())
			ApplyFocus();
	}

	protected void RequestContents()
	{
		if (!s_bContextSet)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] No garage context set (debug open?) - no request", LogLevel.WARNING);
			return;
		}

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] No local PlayerController - no request", LogLevel.ERROR);
			return;
		}

		m_aEntries.Clear();
		m_sSpotPrefab = string.Empty;
		m_iSpotVehicleCount = 0;
		pc.BFCG_GARAGE_RequestContents(s_ContextGarageId);
	}

	void OnNetAddEntry(string vehicleID, string prefab, bool available)
	{
		BFCG_GARAGE_MenuEntry entry = new BFCG_GARAGE_MenuEntry();
		entry.m_sVehicleID = vehicleID;
		entry.m_sPrefab = prefab;
		entry.m_bAvailable = available;
		m_aEntries.Insert(entry);
	}

	void OnNetContentsComplete(int count)
	{
		if (m_aEntries.Count() != count)
			Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Entry mismatch: %1 received, %2 announced", m_aEntries.Count(), count), LogLevel.WARNING);

		BuildTabs();
		BuildGrid();
		UpdateParkPanel();
		FocusInitialWidget();
	}

	void OnNetSetSpotVehicle(string prefab, int count)
	{
		m_sSpotPrefab = prefab;
		m_iSpotVehicleCount = count;
	}

	void OnNetParkResult(int resultCode)
	{
		m_bParkPending = false;

		if (resultCode == BFCG_GARAGE_EParkResult.SUCCESS)
		{

			Print("[BFCG_GARAGE_GarageMenuUI] Parked - reloading contents", LogLevel.NORMAL);
			RequestContents();
			return;
		}

		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Park failed (code %1)", resultCode), LogLevel.WARNING);
		ShowError(GetParkErrorText(resultCode));
		if (resultCode == BFCG_GARAGE_EParkResult.NO_VEHICLE || resultCode == BFCG_GARAGE_EParkResult.MULTI_VEHICLE)
			RequestContents();
	}

	void OnNetUnpackResult(string vehicleID, int resultCode)
	{
		m_bUnpackPending = false;

		if (resultCode == BFCG_GARAGE_EUnpackResult.SUCCESS)
		{

			Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Unpacked: %1 - closing menu", vehicleID), LogLevel.NORMAL);
			Close();
			return;
		}

		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Unpack failed: %1 (code %2)", vehicleID, resultCode), LogLevel.WARNING);
		ShowError(GetUnpackErrorText(resultCode));
		if (resultCode == BFCG_GARAGE_EUnpackResult.NOT_FOUND)
			RequestContents();
	}

	// An already missing record is also a completed deletion from the view's perspective.
	void OnNetDeleteResult(int resultCode)
	{
		m_bDeletePending = false;

		if (resultCode == BFCG_GARAGE_EDeleteResult.SUCCESS || resultCode == BFCG_GARAGE_EDeleteResult.NOT_FOUND)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Entry deleted - reloading contents", LogLevel.NORMAL);
			RequestContents();
			return;
		}

		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Delete failed (code %1)", resultCode), LogLevel.WARNING);
		ShowError(GetDeleteErrorText(resultCode));
	}

	protected void UpdateParkPanel()
	{
		if (!m_wParkNameText)
			return;

		bool hasVehicle = (m_iSpotVehicleCount == 1) && !m_sSpotPrefab.IsEmpty();

		if (m_wParkImage)
			m_wParkImage.SetVisible(false);
		if (m_wParkPreview)
			m_wParkPreview.SetVisible(false);

		if (m_iSpotVehicleCount > 1)
		{
			m_wParkNameText.SetText(string.Format("%1 vehicles at the parking spot - only one allowed", m_iSpotVehicleCount));
		}
		else if (!hasVehicle)
		{
			m_wParkNameText.SetText("No vehicle at the parking spot");
		}
		else
		{
			ResourceName prefab = m_sSpotPrefab;
			m_wParkNameText.SetText(BFCG_GARAGE_VehicleData.ResourceNameToDisplayName(prefab));

			if (BFCG_GARAGE_GarageCardComponent.SetPrefabImageTo(m_wParkImage, prefab))
			{
				m_wParkImage.SetVisible(true);
			}
			else if (m_wParkPreview && m_PreviewManager)
			{
				m_wParkPreview.SetVisible(true);
				m_PreviewManager.SetPreviewItemFromPrefab(m_wParkPreview, prefab);
			}
		}

		if (m_ParkButton)
		{
			Widget buttonRoot = m_ParkButton.GetRootWidget();
			if (buttonRoot)
			{
				buttonRoot.SetEnabled(hasVehicle);
				if (hasVehicle)
					buttonRoot.ClearFlags(WidgetFlags.NOFOCUS);
				else
					buttonRoot.SetFlags(WidgetFlags.NOFOCUS);
			}
		}
	}

	protected void OnParkClicked(SCR_ButtonBaseComponent comp)
	{
		if (m_bParkPending)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		HideError();
		m_bParkPending = true;
		Print("[BFCG_GARAGE_GarageMenuUI] Park request", LogLevel.NORMAL);
		pc.BFCG_GARAGE_RequestPark(s_ContextGarageId);
	}

	protected string GetParkErrorText(int resultCode)
	{
		switch (resultCode)
		{
			case BFCG_GARAGE_EParkResult.NO_VEHICLE:
				return "No vehicle at the parking spot";
			case BFCG_GARAGE_EParkResult.MULTI_VEHICLE:
				return "Multiple vehicles at the parking spot - only one allowed";
			case BFCG_GARAGE_EParkResult.GARAGE_FULL:
				return "Garage is full - limit reached";
			case BFCG_GARAGE_EParkResult.TOO_FAR:
				return "Too far from the garage";
			case BFCG_GARAGE_EParkResult.OWNED_BY_ANOTHER_PLAYER:
				return "Cannot park: this vehicle belongs to another player";
			case BFCG_GARAGE_EParkResult.IDENTITY_NOT_READY:
				return "Player identity is not ready. Please try again shortly.";
			case BFCG_GARAGE_EParkResult.CLAIMS_RESTORING:
				return "Vehicle data is being restored. Parking is temporarily unavailable. Please try again shortly.";
			case BFCG_GARAGE_EParkResult.VEHICLE_OCCUPIED:
				return "Cannot park: vehicle is occupied. All occupants must exit first";
		}
		return "Parking failed (internal error)";
	}

	protected string GetUnpackErrorText(int resultCode)
	{
		switch (resultCode)
		{
			case BFCG_GARAGE_EUnpackResult.NOT_FOUND:
				return "Vehicle is no longer in the garage - list was reloaded";
			case BFCG_GARAGE_EUnpackResult.NOT_ALLOWED:
				return "No permission for this vehicle";
			case BFCG_GARAGE_EUnpackResult.BLOCKED:
				return "Cannot retrieve vehicle: all parking spots at this garage are occupied.";
			case BFCG_GARAGE_EUnpackResult.TOO_FAR:
				return "Too far from the garage";
			case BFCG_GARAGE_EUnpackResult.UNAVAILABLE:
				return "Vehicle is no longer available - can only be deleted";
			case BFCG_GARAGE_EUnpackResult.WORLD_CLAIM_LIMIT:
				return BFCG_VehicleClaimState.WorldClaimLimitMessage();
			case BFCG_GARAGE_EUnpackResult.CLAIMS_RESTORING:
				return BFCG_VehicleClaimState.RestorePendingMessage();
			case BFCG_GARAGE_EUnpackResult.NO_PARKING_SPOTS:
				return "Cannot retrieve vehicle: no parking spots are linked to this garage.";
		}
		return "Unparking failed (internal error)";
	}

	protected string GetDeleteErrorText(int resultCode)
	{
		switch (resultCode)
		{
			case BFCG_GARAGE_EDeleteResult.NOT_ALLOWED:
				return "No permission for this vehicle";
			case BFCG_GARAGE_EDeleteResult.TOO_FAR:
				return "Too far from the garage";
		}
		return "Deletion failed (internal error)";
	}

	protected void ShowError(string message)
	{
		if (!m_wErrorText)
			return;
		m_wErrorText.SetText(message);
		m_wErrorText.SetVisible(true);
	}

	protected void HideError()
	{
		if (!m_wErrorText)
			return;
		m_wErrorText.SetVisible(false);
	}

	// Display the server-provided garage limit instead of assuming the default capacity.
	protected void BuildTabs()
	{
		if (!m_wTabBar)
			return;

		m_aTabButtons.Clear();
		Widget child = m_wTabBar.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		string label = string.Format("Personal (%1/%2)", m_aEntries.Count(), BFCG_Settings.Get().maxGarageVehicles);

		Widget tab = workspace.CreateWidgets(TAB_BUTTON_LAYOUT, m_wTabBar);
		if (!tab)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Tab button could not be created", LogLevel.ERROR);
			return;
		}
		tab.SetName("BFCG_Tab0");
		m_sActiveTabName = tab.GetName();
		LayoutSlot.SetPadding(tab, 4, 0, 4, 0);

		SCR_ButtonTextComponent buttonComp = SCR_ButtonTextComponent.Cast(tab.FindHandler(SCR_ButtonTextComponent));
		if (!buttonComp)
		{
			Print("[BFCG_GARAGE_GarageMenuUI] Tab button without SCR_ButtonTextComponent", LogLevel.ERROR);
			return;
		}
		buttonComp.SetText(label);

		// Capture the original palette before applying focus or active-tab colours.
		if (!m_bDefaultStyleCached)
		{
			m_DefaultBg = buttonComp.m_BackgroundDefault;
			m_DefaultBgHover = buttonComp.m_BackgroundHovered;
			m_DefaultBgSelected = buttonComp.m_BackgroundSelected;
			m_DefaultBgSelectedHover = buttonComp.m_BackgroundSelectedHovered;
			m_DefaultBgClicked = buttonComp.m_BackgroundClicked;
			TextWidget defaultText = buttonComp.GetTextWidget();
			if (defaultText)
				m_DefaultTextColor = defaultText.GetColor();
			m_bDefaultStyleCached = true;
		}

		TextWidget textWidget = buttonComp.GetTextWidget();
		if (textWidget)
			AlignableSlot.SetPadding(textWidget, 14, 6, 14, 6);

		StyleButtonOrange(buttonComp);

		buttonComp.m_bShowBorderOnFocus = false;
		buttonComp.m_OnFocus.Insert(OnMenuButtonFocus);
		buttonComp.m_OnFocusLost.Insert(OnMenuButtonFocusLost);
		m_aTabButtons.Insert(buttonComp);
	}

	protected void ClearGrid()
	{
		ResetDeleteConfirmation();
		m_aCards.Clear();
		m_aDeleteComps.Clear();
		m_aDeleteVehicleIDs.Clear();
		if (!m_wCardGrid)
			return;

		Widget child = m_wCardGrid.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	protected void BuildGrid()
	{
		if (!m_wCardGrid)
			return;

		ClearGrid();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_PreviewManager = GetOrSpawnPreviewManager();
		if (!m_PreviewManager)
			Print("[BFCG_GARAGE_GarageMenuUI] No ItemPreviewManager available - cards without 3D preview", LogLevel.WARNING);

		int i = 0;
		foreach (BFCG_GARAGE_MenuEntry entry : m_aEntries)
		{
			Widget card = workspace.CreateWidgets(CARD_LAYOUT, m_wCardGrid);
			if (!card)
			{
				Print("[BFCG_GARAGE_GarageMenuUI] Card layout could not be created", LogLevel.ERROR);
				return;
			}

			card.SetName(string.Format("BFCG_Card%1", i));

			int row = i / GRID_COLUMNS;
			int col = i - (i / GRID_COLUMNS) * GRID_COLUMNS;
			GridSlot.SetRow(card, row);
			GridSlot.SetColumn(card, col);
			GridSlot.SetPadding(card, 8, 8, 8, 8);

			BFCG_GARAGE_GarageCardComponent cardComp = new BFCG_GARAGE_GarageCardComponent();
			card.AddHandler(cardComp);
			m_aCards.Insert(cardComp);

			ResourceName prefab = entry.m_sPrefab;
			cardComp.InitCard(entry.m_sVehicleID, BFCG_GARAGE_VehicleData.ResourceNameToDisplayName(prefab));

			if (!cardComp.TrySetPrerenderedImage(prefab))
				cardComp.SetPreview(m_PreviewManager, prefab);

			if (!entry.m_bAvailable)
				cardComp.SetUnavailableMode();

			cardComp.m_OnClick.Insert(OnCardClicked);

			cardComp.EnableDeleteButton();
			Widget deleteButton = cardComp.GetDeleteButtonWidget();
			ScriptInvoker deleteInvoker = cardComp.GetDeleteClickInvoker();
			if (deleteButton && deleteInvoker)
			{
				deleteButton.SetName(string.Format("BFCG_CardDel%1", i));
				deleteInvoker.Insert(OnCardDeleteClicked);
				m_aDeleteComps.Insert(BFCG_GARAGE_CardDeleteComponent.Cast(deleteButton.FindHandler(BFCG_GARAGE_CardDeleteComponent)));
				m_aDeleteVehicleIDs.Insert(entry.m_sVehicleID);
			}
			i++;
		}
		int cardCount = i;

		while (i < BFCG_Settings.Get().maxGarageVehicles)
		{
			Widget slot = workspace.CreateWidgets(CARD_LAYOUT, m_wCardGrid);
			if (!slot)
			{
				Print("[BFCG_GARAGE_GarageMenuUI] Placeholder layout could not be created", LogLevel.ERROR);
				break;
			}

			int slotRow = i / GRID_COLUMNS;
			int slotCol = i - (i / GRID_COLUMNS) * GRID_COLUMNS;
			GridSlot.SetRow(slot, slotRow);
			GridSlot.SetColumn(slot, slotCol);
			GridSlot.SetPadding(slot, 8, 8, 8, 8);

			BFCG_GARAGE_GarageCardComponent slotComp = new BFCG_GARAGE_GarageCardComponent();
			slot.AddHandler(slotComp);
			slotComp.SetEmptySlotMode();
			m_aCards.Insert(slotComp);
			i++;
		}

		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Grid filled (%1 cards, %2 placeholders)", cardCount, i - cardCount), LogLevel.NORMAL);

		WireNavigation(cardCount);
	}

	// Explicit links cross layout containers; rebuild them whenever cards are recreated.
	protected void WireNavigation(int cardCount)
	{
		string firstTarget = "BFCG_ParkButton";
		if (cardCount > 0)
			firstTarget = "BFCG_Card0";

		foreach (SCR_ButtonTextComponent tabComp : m_aTabButtons)
		{
			Widget tabRoot = tabComp.GetRootWidget();
			if (tabRoot)
				tabRoot.SetNavigation(WidgetNavigationDirection.DOWN, WidgetNavigationRuleType.EXPLICIT, firstTarget);
		}

		int lastRow = 0;
		if (cardCount > 0)
			lastRow = (cardCount - 1) / GRID_COLUMNS;
		for (int c = 0; c < cardCount; c++)
		{
			Widget cardRoot = m_aCards[c].GetRootWidget();
			if (!cardRoot)
				continue;

			int row = c / GRID_COLUMNS;
			int col = c - (c / GRID_COLUMNS) * GRID_COLUMNS;

			Widget deleteRoot = GetDeleteButtonRoot(c);
			bool isRowEnd = (col == GRID_COLUMNS - 1 || c == cardCount - 1);
			if (deleteRoot)
				cardRoot.SetNavigation(WidgetNavigationDirection.RIGHT, WidgetNavigationRuleType.EXPLICIT, deleteRoot.GetName());
			else if (isRowEnd)
				cardRoot.SetNavigation(WidgetNavigationDirection.RIGHT, WidgetNavigationRuleType.EXPLICIT, "BFCG_ParkButton");

			if (row == 0 && !m_sActiveTabName.IsEmpty())
				cardRoot.SetNavigation(WidgetNavigationDirection.UP, WidgetNavigationRuleType.EXPLICIT, m_sActiveTabName);
			if (row == lastRow)
				cardRoot.SetNavigation(WidgetNavigationDirection.DOWN, WidgetNavigationRuleType.EXPLICIT, "ButtonClose");

			if (!deleteRoot)
				continue;

			deleteRoot.SetNavigation(WidgetNavigationDirection.LEFT, WidgetNavigationRuleType.EXPLICIT, cardRoot.GetName());
			if (isRowEnd)
				deleteRoot.SetNavigation(WidgetNavigationDirection.RIGHT, WidgetNavigationRuleType.EXPLICIT, "BFCG_ParkButton");
			else
				deleteRoot.SetNavigation(WidgetNavigationDirection.RIGHT, WidgetNavigationRuleType.EXPLICIT, string.Format("BFCG_Card%1", c + 1));
			if (row == 0 && !m_sActiveTabName.IsEmpty())
				deleteRoot.SetNavigation(WidgetNavigationDirection.UP, WidgetNavigationRuleType.EXPLICIT, m_sActiveTabName);
			if (row == lastRow)
				deleteRoot.SetNavigation(WidgetNavigationDirection.DOWN, WidgetNavigationRuleType.EXPLICIT, "ButtonClose");
		}

		if (m_ParkButton)
		{
			Widget parkRoot = m_ParkButton.GetRootWidget();
			if (parkRoot)
			{
				if (cardCount > 0)
				{
					int nearestIdx = GRID_COLUMNS - 1;
					if (cardCount - 1 < nearestIdx)
						nearestIdx = cardCount - 1;
					parkRoot.SetNavigation(WidgetNavigationDirection.LEFT, WidgetNavigationRuleType.EXPLICIT, string.Format("BFCG_Card%1", nearestIdx));
				}
				else if (!m_sActiveTabName.IsEmpty())
				{
					parkRoot.SetNavigation(WidgetNavigationDirection.LEFT, WidgetNavigationRuleType.EXPLICIT, m_sActiveTabName);
				}
				if (!m_sActiveTabName.IsEmpty())
					parkRoot.SetNavigation(WidgetNavigationDirection.UP, WidgetNavigationRuleType.EXPLICIT, m_sActiveTabName);
				parkRoot.SetNavigation(WidgetNavigationDirection.DOWN, WidgetNavigationRuleType.EXPLICIT, "ButtonClose");
			}
		}

		if (m_CloseButton)
		{
			Widget closeRoot = m_CloseButton.GetRootWidget();
			if (closeRoot)
			{
				closeRoot.SetNavigation(WidgetNavigationDirection.UP, WidgetNavigationRuleType.EXPLICIT, firstTarget);
				closeRoot.SetNavigation(WidgetNavigationDirection.RIGHT, WidgetNavigationRuleType.EXPLICIT, "BFCG_ParkButton");
			}
		}
	}

	protected Widget GetDeleteButtonRoot(int cardIndex)
	{
		if (cardIndex < 0 || cardIndex >= m_aDeleteComps.Count())
			return null;
		if (!m_aDeleteComps[cardIndex])
			return null;
		return m_aDeleteComps[cardIndex].GetRootWidget();
	}

	// Follow SCR_LoadoutPreviewComponent: reuse the world manager or create a local one.
	protected ItemPreviewManagerEntity GetOrSpawnPreviewManager()
	{
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
		{
			Resource res = Resource.Load(PREVIEW_MANAGER_PREFAB);
			if (res.IsValid())
				GetGame().SpawnEntityPrefabLocal(res, world);
			manager = world.GetItemPreviewManager();
		}
		return manager;
	}

	protected void OnCardClicked(SCR_ScriptedWidgetComponent comp)
	{
		if (m_bUnpackPending || m_bDeletePending)
			return;
		ResetDeleteConfirmation();

		BFCG_GARAGE_GarageCardComponent card = BFCG_GARAGE_GarageCardComponent.Cast(comp);
		if (!card || card.GetVehicleID().IsEmpty())
			return;

		if (card.IsUnavailable())
		{
			ShowError(GetUnpackErrorText(BFCG_GARAGE_EUnpackResult.UNAVAILABLE));
			return;
		}

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		HideError();
		m_bUnpackPending = true;
		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Unpack request: %1", card.GetVehicleID()), LogLevel.NORMAL);
		pc.BFCG_GARAGE_RequestUnpack(card.GetVehicleID(), s_ContextGarageId);
	}

	// First click arms this record; a second click within the timeout sends the delete request.
	protected void OnCardDeleteClicked(SCR_ScriptedWidgetComponent comp)
	{
		if (m_bUnpackPending || m_bDeletePending)
			return;

		BFCG_GARAGE_CardDeleteComponent deleteComp = BFCG_GARAGE_CardDeleteComponent.Cast(comp);
		if (!deleteComp)
			return;

		int idx = m_aDeleteComps.Find(deleteComp);
		if (idx < 0 || idx >= m_aDeleteVehicleIDs.Count())
			return;

		string vehicleID = m_aDeleteVehicleIDs[idx];
		if (vehicleID.IsEmpty())
			return;

		if (m_ArmedDeleteComp != deleteComp)
		{
			ResetDeleteConfirmation();
			m_ArmedDeleteComp = deleteComp;
			m_ArmedDeleteComp.SetArmed(true);
			GetGame().GetCallqueue().CallLater(ResetDeleteConfirmation, DELETE_CONFIRM_TIMEOUT_MS, false);
			Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Delete confirmation armed: %1", vehicleID), LogLevel.NORMAL);
			return;
		}

		ResetDeleteConfirmation();

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		HideError();
		m_bDeletePending = true;
		Print(string.Format("[BFCG_GARAGE_GarageMenuUI] Delete request: %1", vehicleID), LogLevel.NORMAL);
		pc.BFCG_GARAGE_RequestDelete(vehicleID, s_ContextGarageId);
	}

	protected void ResetDeleteConfirmation()
	{
		GetGame().GetCallqueue().Remove(ResetDeleteConfirmation);
		if (m_ArmedDeleteComp)
			m_ArmedDeleteComp.SetArmed(false);
		m_ArmedDeleteComp = null;
	}

	protected void OnCloseClicked(SCR_ButtonBaseComponent button)
	{
		Close();
	}

	protected void OnMenuBack(float value, EActionTrigger reason)
	{
		Close();
	}
}
