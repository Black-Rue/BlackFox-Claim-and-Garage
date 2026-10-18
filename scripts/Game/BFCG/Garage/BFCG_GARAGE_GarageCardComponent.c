//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_GarageCardComponent.c
//! BFCG_Garage - виджет-компонента карточки ТС в меню гаража
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//! Changes: BFCG-нейминг, английские логи; базовая карточка, hover/focus-рамка,
//! placeholder/unavailable-режимы и отдельный клик-канал удаления основаны на оригинале.
//! BFCG addition: двухэтапное подтверждение удаления с визуальным состоянием карточки.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Клик-хендлер кнопки удаления в углу карточки. Свой класс, потому что
//! ванильные кнопки возвращают false из OnClick и клик проскакивал бы в карту
//! (выдача ТС). Здесь событие "съедается" (return true).
class BFCG_GARAGE_CardDeleteComponent : SCR_ScriptedWidgetComponent
{
	// Не ref: карта владеет компонентой, обратная ссылка намеренно слабая
	protected BFCG_GARAGE_GarageCardComponent m_Card;
	protected ImageWidget m_wIcon;
	protected bool m_bHighlighted;
	protected bool m_bArmed;

	//------------------------------------------------------------------------------------------------
	void Init(BFCG_GARAGE_GarageCardComponent card, ImageWidget icon)
	{
		m_Card = card;
		m_wIcon = icon;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		m_OnClick.Invoke(this);
		return true; // событие съедено - не пробрасываем в карту
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		super.OnMouseEnter(w, x, y);
		SetHighlight(true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		super.OnMouseLeave(w, enterW, x, y);
		SetHighlight(false);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		super.OnFocus(w, x, y);
		SetHighlight(true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocusLost(Widget w, int x, int y)
	{
		super.OnFocusLost(w, x, y);
		SetHighlight(false);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Первое нажатие оставляет кнопку в предупреждающем состоянии до подтверждения/таймаута.
	void SetArmed(bool armed)
	{
		m_bArmed = armed;
		if (m_Card)
			m_Card.SetDeleteConfirmation(armed);
		ApplyVisualState();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetHighlight(bool highlight)
	{
		m_bHighlighted = highlight;
		ApplyVisualState();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyVisualState()
	{
		bool highlighted = m_bHighlighted || m_bArmed;
		if (m_wIcon)
		{
			if (highlighted)
				m_wIcon.SetColor(GUIColors.ORANGE);
			else
				m_wIcon.SetColor(Color.White);
		}

		if (m_Card)
			m_Card.ShowBorderFromDeleteButton(highlighted);
	}
}

class BFCG_GARAGE_GarageCardComponent : SCR_ScriptedWidgetComponent
{
	protected static const string ICON_UNAVAILABLE = UIConstants.ICON_NOT_AVAILABLE;
	protected static const string ICON_DELETE = UIConstants.ICON_CANCEL;

	protected string m_sVehicleID;
	protected string m_sDisplayName;
	protected bool m_bEmptySlot;
	protected bool m_bFocused;
	protected bool m_bUnavailable;
	protected TextWidget m_wNameText;
	protected ItemPreviewWidget m_wPreviewWidget;
	protected Widget m_wHoverBorder;
	protected Widget m_wEmptyFill;
	protected Widget m_wNameBar;
	protected ImageWidget m_wCardImage;
	protected ImageWidget m_wUnavailableIcon;
	protected Widget m_wDeleteButton;
	protected ref BFCG_GARAGE_CardDeleteComponent m_DeleteComp;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wNameText = TextWidget.Cast(w.FindAnyWidget("CardName"));
		if (!m_wNameText)
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardName' not found in card layout", LogLevel.WARNING);

		m_wPreviewWidget = ItemPreviewWidget.Cast(w.FindAnyWidget("CardPreview"));
		if (!m_wPreviewWidget)
			Print("[BFCG_GARAGE_GarageCardComponent] 'CardPreview' missing or not an ItemPreviewWidget", LogLevel.WARNING);

		m_wHoverBorder = w.FindAnyWidget("CardHoverBorder");
		if (!m_wHoverBorder)
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardHoverBorder' not found - no hover border", LogLevel.WARNING);

		m_wEmptyFill = w.FindAnyWidget("CardEmptyFill");
		if (!m_wEmptyFill)
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardEmptyFill' not found - no placeholder fields", LogLevel.WARNING);

		m_wNameBar = w.FindAnyWidget("CardNameBarSize");

		m_wCardImage = ImageWidget.Cast(w.FindAnyWidget("CardImage"));
		if (!m_wCardImage)
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardImage' not found - 3D previews only", LogLevel.WARNING);

		m_wUnavailableIcon = ImageWidget.Cast(w.FindAnyWidget("CardUnavailableIcon"));
		if (m_wUnavailableIcon)
			m_wUnavailableIcon.LoadImageFromSet(0, UIConstants.ICONS_IMAGE_SET, ICON_UNAVAILABLE);
		else
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardUnavailableIcon' not found - no lock icon", LogLevel.WARNING);

		InitDeleteButton(w);
	}

	//------------------------------------------------------------------------------------------------
	protected void InitDeleteButton(Widget w)
	{
		m_wDeleteButton = w.FindAnyWidget("CardDeleteButton");
		if (!m_wDeleteButton)
		{
			Print("[BFCG_GARAGE_GarageCardComponent] Widget 'CardDeleteButton' not found - no delete", LogLevel.WARNING);
			return;
		}

		ImageWidget deleteIcon = ImageWidget.Cast(w.FindAnyWidget("CardDeleteIcon"));
		if (deleteIcon)
			deleteIcon.LoadImageFromSet(0, UIConstants.ICONS_IMAGE_SET, ICON_DELETE);

		m_DeleteComp = new BFCG_GARAGE_CardDeleteComponent();
		m_wDeleteButton.AddHandler(m_DeleteComp);
		m_DeleteComp.Init(this, deleteIcon);
	}

	//------------------------------------------------------------------------------------------------
	void EnableDeleteButton()
	{
		if (m_wDeleteButton)
			m_wDeleteButton.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	Widget GetDeleteButtonWidget()
	{
		return m_wDeleteButton;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetDeleteClickInvoker()
	{
		if (!m_DeleteComp)
			return null;
		return m_DeleteComp.m_OnClick;
	}

	//------------------------------------------------------------------------------------------------
	void ShowBorderFromDeleteButton(bool show)
	{
		if (!m_wHoverBorder || m_bEmptySlot)
			return;
		if (!show && m_bFocused)
			return;
		m_wHoverBorder.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	void SetUnavailableMode()
	{
		m_bUnavailable = true;
		if (m_wUnavailableIcon)
			m_wUnavailableIcon.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	bool IsUnavailable()
	{
		return m_bUnavailable;
	}

	//------------------------------------------------------------------------------------------------
	bool TrySetPrerenderedImage(ResourceName prefab)
	{
		if (!SetPrefabImageTo(m_wCardImage, prefab))
			return false;

		m_wCardImage.SetVisible(true);
		if (m_wPreviewWidget)
			m_wPreviewWidget.SetVisible(false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Статический вариант (панель парковки): грузит предрендеренную картинку префаба
	static bool SetPrefabImageTo(ImageWidget imageWidget, ResourceName prefab)
	{
		if (!imageWidget || prefab.IsEmpty())
			return false;

		Resource res = Resource.Load(prefab);
		if (!res.IsValid())
			return false;

		IEntityComponentSource componentSource = SCR_BaseContainerTools.FindComponentSource(res, SCR_EditableEntityComponent);
		if (!componentSource)
			return false;

		SCR_EditableEntityUIInfo info = SCR_EditableEntityComponentClass.GetInfo(componentSource);
		if (!info)
			return false;

		return info.SetAssetImageTo(imageWidget);
	}

	//------------------------------------------------------------------------------------------------
	void SetEmptySlotMode()
	{
		m_bEmptySlot = true;
		if (m_wEmptyFill)
			m_wEmptyFill.SetVisible(true);
		if (m_wNameBar)
			m_wNameBar.SetVisible(false);
		if (m_wPreviewWidget)
			m_wPreviewWidget.SetVisible(false);
		if (m_wCardImage)
			m_wCardImage.SetVisible(false);
		if (m_wDeleteButton)
		{
			m_wDeleteButton.SetVisible(false);
			m_wDeleteButton.SetFlags(WidgetFlags.NOFOCUS);
		}
		if (m_wRoot)
			m_wRoot.SetFlags(WidgetFlags.NOFOCUS);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		super.OnMouseEnter(w, x, y);
		if (m_wHoverBorder && !m_bEmptySlot)
			m_wHoverBorder.SetVisible(true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		super.OnMouseLeave(w, enterW, x, y);
		if (m_wHoverBorder && !m_bFocused)
			m_wHoverBorder.SetVisible(false);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		super.OnFocus(w, x, y);
		m_bFocused = true;
		if (m_wHoverBorder && !m_bEmptySlot)
			m_wHoverBorder.SetVisible(true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocusLost(Widget w, int x, int y)
	{
		super.OnFocusLost(w, x, y);
		m_bFocused = false;
		if (m_wHoverBorder)
			m_wHoverBorder.SetVisible(false);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void SetPreview(ItemPreviewManagerEntity manager, ResourceName prefab)
	{
		if (!manager || !m_wPreviewWidget || prefab.IsEmpty())
			return;
		m_wPreviewWidget.SetVisible(true);
		manager.SetPreviewItemFromPrefab(m_wPreviewWidget, prefab);
	}

	//------------------------------------------------------------------------------------------------
	void InitCard(string vehicleID, string displayName)
	{
		m_sVehicleID = vehicleID;
		m_sDisplayName = displayName;
		if (m_wNameText)
			m_wNameText.SetText(displayName);
	}

	//------------------------------------------------------------------------------------------------
	//! Явное подтверждение на самой карточке одинаково видно при мыши и при gamepad-фокусе.
	void SetDeleteConfirmation(bool enabled)
	{
		if (!m_wNameText)
			return;

		if (enabled)
			m_wNameText.SetText("PRESS AGAIN TO DELETE");
		else
			m_wNameText.SetText(m_sDisplayName);
	}

	//------------------------------------------------------------------------------------------------
	string GetVehicleID()
	{
		return m_sVehicleID;
	}
}
