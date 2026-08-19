class OVT_EconomyInfo : SCR_InfoDisplay {	
	OVT_EconomyManagerComponent m_Economy;
	OVT_OccupyingFactionManager m_OccupyingFaction;
	OVT_NotificationManagerComponent m_Notify;
	OVT_RealEstateManagerComponent m_RealEstate;
	string m_playerId;
	SCR_ChimeraCharacter m_player;
	
	//! Green for money gained, red for money lost, on the HUD delta ticker.
	protected const int COLOR_DELTA_POSITIVE = 0xFF4CD964;
	protected const int COLOR_DELTA_NEGATIVE = 0xFFFF4444;

	//! Accumulating "+$500" state for the money readout. Client-side only; no new replication.
	//!
	//! STATIC ON PURPOSE (BUG-097). This info display is declared once per OCCUPIABLE PREFAB - the
	//! player character, every vehicle, every helicopter, every turret - so an instance field gave each
	//! seat its own baseline and its own countdown. Getting into a car stopped one tracker mid-count and
	//! re-seeded another from scratch, so money earned while driving showed nothing; getting out handed
	//! the display back to a tracker still holding the pre-drive balance, which then rendered the same
	//! gain again. The quantity is per-player, so the state has to outlive any one seat.
	protected static ref OVT_MoneyDeltaTracker s_MoneyDelta;

	//! Persistent ID whose balance seeded the shared tracker. Statics outlive a world, so a new session
	//! in the same process must not inherit the previous player's baseline.
	protected static string s_sDeltaOwnerId;

	//! World time (ms) of the last observation fed to the shared tracker. During a seat change both the
	//! character's and the vehicle's display can tick in the same frame, and one shared tracker fed
	//! twice would count its timeout down at double speed.
	protected static float s_fDeltaLastUpdate = -1;

	float m_fCounter = 8;
	float m_fOverrideCounter = 0;
	int m_iCurrentTownId = -1;
	bool m_bTownShowing = false;
	
	protected OVT_MainMenuContextOverrideComponent m_FoundOverride;
	protected float m_fFoundRange=-1;
	
	//------------------------------------------------------------------------------------------------
	override event void OnInit(IEntity owner)
	{
		super.OnInit(owner);

		m_Economy = OVT_Global.GetEconomy();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Notify = OVT_Global.GetNotify();
		m_RealEstate = OVT_Global.GetRealEstate();

		// Hide town panel initially until UpdateTown() populates it with correct data
		// Check if m_wRoot exists first (may not be initialized yet)
		if (m_wRoot)
		{
			Widget panel = m_wRoot.FindAnyWidget("Town");
			if(panel)
				panel.SetVisible(false);
		}
	}
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(character);
		m_playerId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
		
		m_player = character;
	}
		
	private override event void UpdateValues(IEntity owner, float timeSlice)
	{	
		m_fCounter += timeSlice;
		m_fOverrideCounter += timeSlice;
		if(!m_player){
			InitCharacter();
		}
		UpdateMoney(timeSlice);
		// ⚠ ACTIVE **AND** REVEALED (occupying/counter-attacks D15). A counter-attack siege is active
		// while it is still encircling the objective in silence, and the panel must not be the thing
		// that gives it away. A player-initiated battle is revealed from creation, so this reads
		// exactly as it did before for every battle a player starts.
		if(m_OccupyingFaction.m_bQRFActive && m_OccupyingFaction.m_bQRFRevealed)
		{
			ShowQRF();
			UpdateQRF();
		}else{
			HideQRF();
		}
		
		if(m_fOverrideCounter >= 1)
		{
			m_fOverrideCounter = 0;
			if(!UpdateVehicleHint())
			{
				UpdateOverride();
			}
		}
		
		UpdateNotification(timeSlice);
		
		
		if(m_fCounter > 10)
		{
			m_fCounter = 0;
			UpdateTown();
		}
		
#ifdef WORKBENCH
		//UpdateDebug();
#endif				
	}
	
	void UpdateOverride()
	{
		m_FoundOverride = null;
		m_fFoundRange = -1;
		GetGame().GetWorld().QueryEntitiesBySphere(m_player.GetOrigin(), 50, null, FindOverride, EQueryEntitiesFlags.ALL);		
		
		if(m_FoundOverride && m_FoundOverride.CanShow(m_player))
		{
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			SCR_UIInfo info = m_FoundOverride.m_UiInfo;
			if(info)
				text.SetTextFormat("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> %1",info.GetName());
		}else{
			if(OVT_Global.GetOverthrow().m_bHasOpenedMenu)
			{
				m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(false);
				return;
			}			
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			text.SetText("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> #OVT_Open_Overthrow_Menu");
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
		}
	}
	
	// FindOverride gets called a lot, so make it a tiny bit faster by having the pointer here
	protected Managed m_foundComponent;
	protected bool FindOverride(IEntity entity)
	{
		m_foundComponent = entity.FindComponent(OVT_MainMenuContextOverrideComponent);
		if(!m_foundComponent) return false;
		
		OVT_MainMenuContextOverrideComponent m_overrideComponent = OVT_MainMenuContextOverrideComponent.Cast(m_foundComponent);
		if(!m_overrideComponent) return false;
		
		float dist = vector.Distance(entity.GetOrigin(),m_player.GetOrigin());
		if(dist > m_overrideComponent.m_fRange) return false;
		
		if(m_fFoundRange == -1 || m_fFoundRange > dist)
		{
			bool got = true;
			if(m_overrideComponent.m_bMustOwnBase)
			{
				OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(entity.GetOrigin());
				if(!base || base.IsOccupyingFaction())
				{
					got = false;
				}
			}
			if(got)
			{
				m_FoundOverride = m_overrideComponent;
				m_fFoundRange = dist;
			}
		}
		
		return false;
	}
	
	bool UpdateVehicleHint()
	{
		if(!m_player) return false;
		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_player.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment || !compartment.IsInCompartment() || compartment.GetCompartment().GetType() != ECompartmentType.PILOT)
		{
			return false;
		}
		
		vector pos = m_player.GetOrigin();
		bool hasButtons = false;
		
		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, 40);
		if(warehouse)
		{
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(warehouse.location, 10);
			if(warehouseEntity)
			{
				EntityID id = warehouseEntity.GetID();
				bool isOwned = m_RealEstate.IsOwned(id);
				bool isOwner = m_RealEstate.IsOwner(m_playerId, id);
				bool isRented = m_RealEstate.IsRented(id);
				bool isAccessible = (!warehouse.isPrivate && isOwned && !isRented) || (warehouse.isPrivate && isOwner && !isRented) || isRented;
				if(isAccessible) hasButtons = true;
			}
		}
		
		if(!hasButtons)
		{
			RplId port = m_Economy.GetNearestPort(pos);
			if(port)
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(port));
				if(rpl)
				{
					float dist = vector.Distance(pos, rpl.GetEntity().GetOrigin());
					if(dist < 20) hasButtons = true;
				}
			}
		}
		
		if(hasButtons)
		{
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			text.SetText("<color rgba='226,168,79,255'><action name='OverthrowVehicleMenu'/></color> #OVT_Vehicle_Menu");
			return true;
		}
		
		return false;
	}
	
	void UpdateDebug()
	{
		Widget d = m_wRoot.FindAnyWidget("Debug");
		d.SetVisible(true);
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("DebugText"));
		
		CharacterPerceivableComponent percieve = OVT_ComponentFinder<CharacterPerceivableComponent>.Find(m_player);
		if(percieve)
		{
			text.SetText(percieve.GetVisualRecognitionFactor().ToString());
		}
	}
	
	void UpdateTown()
	{		
		if(m_bTownShowing) {
			Widget panel = m_wRoot.FindAnyWidget("Town");
			panel.SetVisible(false);
			m_bTownShowing = false;
			return;
		}	
		if(!m_player) return;
		OVT_TownManagerComponent tm = OVT_Global.GetTowns();	
		OVT_TownData town = tm.GetNearestTown(m_player.GetOrigin());
		int townID = OVT_Global.GetTowns().GetTownID(town);
		if(m_iCurrentTownId != townID)
		{
			m_iCurrentTownId = townID;
			ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("ControllingFaction"));
			img.LoadImageTexture(0, town.ControllingFactionData().GetUIInfo().GetIconPath());
					
			TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget("TownName"));
			widget.SetText(tm.GetTownName(townID));
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Population"));
			widget.SetText(town.population.ToString());
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stability"));
			widget.SetText(town.stability.ToString() + "%");
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Support"));		
			widget.SetText(town.support.ToString() + " (" + town.SupportPercentage().ToString() + "%)");
			
			Widget panel = m_wRoot.FindAnyWidget("Town");
			panel.SetVisible(true);
			
			m_bTownShowing = true;
		}
	}
	
	void HideQRF()
	{
		m_wRoot.FindAnyWidget("QRF").SetVisible(false);
	}
	
	void ShowQRF()
	{
		m_wRoot.FindAnyWidget("QRF").SetVisible(true);
	}
	
	//------------------------------------------------------------------------------------------------
	//! WHICH PLACE THIS BATTLE IS FOR, resolved on the CLIENT, or an empty string when it cannot be.
	//!
	//! ⚠ IT READS THE REPLICATED INDICES, NEVER THE HANDLES. m_CurrentQRFBase / m_CurrentQRFTown are
	//! server-side object handles that the finish handlers never clear - only the indices beside them
	//! are reset to -1 - so reading a handle would make this panel name whatever place was fought over
	//! LAST. The manager's own RevealQRF() carries the same warning for the same reason.
	//!
	//! ⚠ THE TOWN IS ASKED FIRST because a base battle leaves m_iCurrentQRFTown at -1 while a town
	//! battle can sit inside a base's classification radius. Same precedence the reveal notification
	//! uses, so the header and the broadcast never disagree about where the fighting is.
	//!
	//! ⚠ EMPTY IS A REAL ANSWER, NOT A FAILURE, AND THE CALLER HAS A STRING FOR IT. The indices are
	//! replicated on change but are NOT in the JIP catch-up payload - pre-existing debt recorded in the
	//! epic's JIP/client divergence entry - so a player who joined mid-battle legitimately has neither.
	//! Naming the wrong place would be worse than naming none.
	//! \return The place's display name, or "" when the client cannot tell.
	protected string ResolveQRFPlaceName()
	{
		if(!m_OccupyingFaction)
			return "";

		if(m_OccupyingFaction.m_iCurrentQRFTown > -1)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if(towns)
				return towns.GetTownName(m_OccupyingFaction.m_iCurrentQRFTown);

			return "";
		}

		int baseIndex = m_OccupyingFaction.m_iCurrentQRFBase;
		if(baseIndex < 0)
			return "";

		// ⚠ BOUNDS-CHECKED AGAINST THE CLIENT'S OWN LIST, not assumed to match the server's. m_Bases is
		// built by a server-side world query; a client whose copy is shorter (or empty) would index off
		// the end, and GetBaseByIndex dereferences m_Bases[index] with no guard of its own.
		if(!m_OccupyingFaction.m_Bases || baseIndex >= m_OccupyingFaction.m_Bases.Count())
			return "";

		OVT_BaseControllerComponent base = m_OccupyingFaction.GetBaseByIndex(baseIndex);
		if(!base)
			return "";

		return base.m_sName;
	}

	void UpdateQRF()
	{
		// THE HEADER NAMES THE PLACE, THE SUB-LINE COUNTS DOWN TO SCORING. Both battle kinds render
		// identically - a player-initiated QRF counts two minutes and a counter-attack's muster window
		// fifteen, in the same M:SS clock, under the same header.
		TextWidget header = TextWidget.Cast(m_wRoot.FindAnyWidget("QRFHeaderText"));
		if(header)
		{
			string place = ResolveQRFPlaceName();
			if(place.IsEmpty())
				header.SetText("#OVT-QRFBattleForUnknown");
			else
				header.SetText("#OVT-QRFBattleFor " + place);
		}

		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("QRFTimerText"));
		if(m_OccupyingFaction.m_iQRFTimer > 0)
		{
			// ⚠ ONE CLOCK, NOT TWO BRANCHES (2026-08-20). This used to render whole minutes above two
			// minutes and bare seconds below, through two different strings - so a single countdown
			// changed both its unit and its wording partway through, and "3" becoming "119" looked like
			// a fault. OVT_QRFSiege.FormatClock reads the same at 15:00 and at 0:07 and needs no
			// crossover. ShouldRenderMinutes and MusterMinutesRemaining are no longer called from here;
			// they are kept because the Logic tier pins them and because a mod may want the old form.
			w.SetText("#OVT-QRFScoringBeginsIn " + OVT_QRFSiege.FormatClock(m_OccupyingFaction.m_iQRFTimer));
		}else{
			w.SetText("#OVT-BattleProgress");
		}
		SliderWidget of = SliderWidget.Cast(m_wRoot.FindAnyWidget("QRFOccupying"));
		SliderWidget rf = SliderWidget.Cast(m_wRoot.FindAnyWidget("QRFResistance"));
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		int points = Math.Round(((float)m_OccupyingFaction.m_iQRFPoints / (float)config.m_Difficulty.QRFPointsToWin) * 100);
		
		if(points >= 0)
		{
			rf.SetCurrent(points);
			of.SetCurrent(0);
		}else{
			of.SetCurrent(Math.AbsFloat(points));
			rf.SetCurrent(0);
		}
	}
			
	//------------------------------------------------------------------------------------------------
	// Update Money
	//------------------------------------------------------------------------------------------------
	void UpdateMoney(float timeSlice)
	{
		if (!m_Economy)
			return;
		if(!m_wRoot)
			return;

		int money = m_Economy.GetPlayerMoney(m_playerId);

		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("MoneyText"));
		if(w) w.SetText(OVT_MoneyFormat.FormatMoney(money));

		UpdateMoneyDelta(money, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Drives the "+$500" ticker next to the money readout.
	//!
	//! Polled, not subscribed (implementation plan D11): this display already reads the balance every
	//! frame and already has a timeSlice, and its lifetime does not obviously match the economy
	//! manager's - so there is no invoker to register and, more to the point, none to unregister. The
	//! TRACKER, however, is shared by every instance (see s_MoneyDelta); the displays are pure
	//! renderers of one per-player ticker.
	//! \param[in] money The player's balance this frame.
	//! \param[in] timeSlice Seconds since the previous frame.
	protected void UpdateMoneyDelta(int money, float timeSlice)
	{
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("MoneyDeltaText"));

		// Before the local character resolves there is no balance to observe - only a 0 that would seed
		// the baseline and then read as a windfall the moment the real number arrived.
		if(m_playerId.IsEmpty())
		{
			if(w) w.SetVisible(false);
			return;
		}

		if(!s_MoneyDelta) s_MoneyDelta = new OVT_MoneyDeltaTracker();

		if(m_playerId != s_sDeltaOwnerId)
		{
			s_sDeltaOwnerId = m_playerId;
			s_MoneyDelta.Reset();
		}

		float now = GetGame().GetWorld().GetWorldTime();
		if(now != s_fDeltaLastUpdate)
		{
			s_fDeltaLastUpdate = now;
			s_MoneyDelta.Update(money, timeSlice);
		}

		if(!w) return;

		if(!s_MoneyDelta.IsVisible())
		{
			w.SetVisible(false);
			return;
		}

		w.SetText(s_MoneyDelta.GetText());

		if(s_MoneyDelta.GetDelta() > 0)
		{
			w.SetColor(Color.FromInt(COLOR_DELTA_POSITIVE));
		}else{
			w.SetColor(Color.FromInt(COLOR_DELTA_NEGATIVE));
		}

		w.SetVisible(true);
	}
	
	void UpdateNotification(float timeSlice)
	{
		if (!m_Notify)
			return;
		if(!m_wRoot)
			return;
		
		Widget notify = m_wRoot.FindAnyWidget("Notify");
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("NotificationText"));
		
		if(m_Notify.m_aNotifications.Count() == 0)
		{
			notify.SetVisible(false);
			return;
		}
		
		OVT_NotificationData data = m_Notify.m_aNotifications.Get(0);
		if(!data)
		{
			notify.SetVisible(false);
			return;
		}
		
		if(data.displayTimer <= 0)
		{
			notify.SetVisible(false);
			return;
		}
		
		data.displayTimer = data.displayTimer - timeSlice;
		
		w.SetTextFormat(data.msg.m_UIInfo.GetDescription(), data.param1, data.param2, data.param3);
		notify.SetVisible(true);
		
		if(data.msg.m_UIInfo.GetIconSetName() != "")
		{
			ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget("NotificationIcon"));
			data.msg.m_UIInfo.SetIconTo(icon);
		}			
	}
}
