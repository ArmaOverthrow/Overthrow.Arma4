[BaseContainerProps()]
class OVT_MapRestrictedAreas : OVT_MapCanvasLayer
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES - APPEARANCE
	//-----------------------------------------------------------------------
	//
	// GEOMETRY IS UNCHANGED BY EVERYTHING BELOW. The centres and the radii are exactly what they have
	// always been - the same baseCloseRange + FOB deploy buffer for bases, the same tower range for
	// towers, the same QRF constants - and nothing here may ever move one. Only the COLOUR each circle
	// is filled with, and whether a texture is attached at all, are decided in this file.
	//
	// WHY THE RINGS ARE FACTION-COLOURED. A restriction ring is meant to read as "the same territory,
	// denser" over the territory overlay beneath it - that denser reading is what makes the two layers
	// legible together instead of fighting. It only works if the ring and the ground under it are the
	// SAME HUE: the hardcoded red and blue these rings used to draw produced a muddy third colour over
	// faction-coloured territory rather than a heavier version of the colour underneath. The hue is
	// resolved through the SAME shared helper the location markers and the territory fill use, so there
	// is exactly one answer in the codebase to "what colour is faction N".
	//
	// AND THE ALPHA IS WHAT MAKES IT DENSER, so the two tiers stay: an occupier-held ring is heavier
	// than a resistance-held one, the same split this layer has always drawn.

	[Attribute("0", UIWidgets.Auto, "Fill the restriction rings with a texture instead of flat colour. Defaults OFF: textured polygons are known to work in this build, but the hatch art does not exist yet.")]
	protected bool m_bUseTexture;

	[Attribute("", UIWidgets.ResourceNamePicker, "Hatch/bar texture for restricted zones, visually distinct from the territory overlay's fill so the two overlays cannot be confused where they overlap. A failed load degrades to the flat fill this layer has always drawn.", "edds")]
	protected ResourceName m_sRestrictedTexture;

	[Attribute("0.01", UIWidgets.Auto, "UV scale for the restricted-zone texture. Units are undocumented and unused in vanilla, so this is a sane guess to be dialled in by eye once the art exists.")]
	protected float m_fUVScale;

	[Attribute("50", UIWidgets.Auto, "Alpha (0-255) of a ring around an OCCUPIER-held base or radio tower. The alpha is the whole point of the two tiers: the hue now matches the territory underneath, so density is what tells a ring apart from the ground it sits on.")]
	protected int m_iRestrictedAlpha;

	[Attribute("40", UIWidgets.Auto, "Alpha (0-255) of a ring around a RESISTANCE-held base or radio tower. These block FOB deployment too, but they are the player's own ground and read a little lighter, which is the split this layer has always drawn.")]
	protected int m_iFriendlyRestrictedAlpha;

	[Attribute("50", UIWidgets.Auto, "QRF circle alpha (0-255). Unchanged from what this layer has always drawn. The QRF's distinction is carried by its HATCH, not by this - see ResolveQRFColour.")]
	protected int m_iQRFAlpha;

	[Attribute("255", UIWidgets.Auto, "QRF circle colour - red component. Unchanged default; it is the secondary signal now, so it no longer has to carry the distinction alone.")]
	protected int m_iQRFRed;

	[Attribute("0", UIWidgets.Auto, "QRF circle colour - green component.")]
	protected int m_iQRFGreen;

	[Attribute("0", UIWidgets.Auto, "QRF circle colour - blue component.")]
	protected int m_iQRFBlue;

	[Attribute("1", UIWidgets.Auto, "Hatch the QRF circles. This is what makes a QRF distinguishable from the rings and the territory around it, and it works whatever colour the occupying faction turns out to be - see ResolveQRFColour. Off falls back to a flat coloured circle.")]
	protected bool m_bUseQRFTexture;

	[Attribute("{B7E8255E75EE66ED}UI/Textures/Map/overthrow_map_diagonal.edds", UIWidgets.ResourceNamePicker, "Diagonal hatch for the QRF circles. The SAME art the territory layer draws over contested regions, at a different scale: hatch means 'this ground is being fought over', and a QRF is the most literal case of that. A failed load degrades to a flat circle.", "edds")]
	protected ResourceName m_sQRFTexture;

	[Attribute("0.003", UIWidgets.Auto, "UV scale for the QRF hatch, set DISTINCTLY apart from the territory layer's contested scale (0.01) so a QRF and a contested town read as different weights of the same idea. STARTING POINT, NOT A MEASUREMENT: the units are undocumented, so which direction is coarser is unverified - if this reads FINER than the contested fill rather than coarser, the mapping is inverted and 0.03 is the value to try.")]
	protected float m_fQRFUVScale;

	protected ref SharedItemRef m_TextureRef;
	protected bool m_bTextureAttempted;
	protected bool m_bTextureValid;

	//! The QRF hatch is loaded and tracked SEPARATELY from the ring texture above, because the two are
	//! independent decisions: the rings ship solid and the QRF ships hatched, so one switch could not
	//! express both.
	protected ref SharedItemRef m_QRFTextureRef;
	protected bool m_bQRFTextureAttempted;
	protected bool m_bQRFTextureValid;

	protected ref array<vector> m_Centers;
	protected ref array<int> m_Ranges;
	//! Controlling faction of each occupier-held ring, collected alongside its centre and radius.
	//! Collected rather than looked up in Draw: Draw runs every frame and a faction manager query per
	//! ring per frame would be the only per-frame manager lookup in this layer.
	protected ref array<int> m_FactionIndices;
	//! Packed ARGB per occupier-held ring, resolved once from m_FactionIndices at map open.
	protected ref array<int> m_Colors;

	// Resistance-held bases/towers also block FOB deployment - drawn at the lighter alpha tier
	protected ref array<vector> m_FriendlyCenters;
	protected ref array<int> m_FriendlyRanges;
	protected ref array<int> m_FriendlyFactionIndices;
	protected ref array<int> m_FriendlyColors;

	protected bool m_bQRFActive = false;
	protected vector m_QRFCenter;

	override void Draw()
	{
		if(!m_Centers || !m_Ranges || !m_FriendlyCenters || !m_FriendlyRanges)
			return;

		// The colour arrays are built in the same pass as the centres, so a shorter one means the collect
		// and the draw disagree about this frame's rings and every colour after the mismatch would belong
		// to the wrong circle
		if(!m_Colors || !m_FriendlyColors)
			return;

		if(m_Colors.Count() != m_Centers.Count() || m_FriendlyColors.Count() != m_FriendlyCenters.Count())
			return;

		m_Commands.Clear();

		EnsureTexture();

		// Null unless a texture was configured AND loaded AND validated. DrawCircle only touches the
		// command's texture fields when this is non-null, so the untextured path is unchanged
		SharedItemRef tex = null;
		if (m_bTextureValid)
			tex = m_TextureRef;

		// Colours are ARRAY READS, not lookups. Resolving a faction colour here would put a faction
		// manager query on the one path in this layer that runs every single frame the map is open
		foreach(int i, vector center : m_Centers)
		{
			DrawCircle(center, m_Ranges[i], m_Colors[i], 36, tex, m_fUVScale);
		}

		foreach(int i, vector center : m_FriendlyCenters)
		{
			DrawCircle(center, m_FriendlyRanges[i], m_FriendlyColors[i], 36, tex, m_fUVScale);
		}

		if(m_bQRFActive)
		{
			// DISTINGUISHED BY STRUCTURE, not by hue - see ResolveQRFColour. Its own texture and its own
			// UV scale, independent of the ring texture above, which ships off
			EnsureQRFTexture();

			SharedItemRef qrfTex = null;
			if(m_bQRFTextureValid)
				qrfTex = m_QRFTextureRef;

			int qrfColour = ResolveQRFColour();

			DrawCircle(m_QRFCenter, OVT_QRFControllerComponent.QRF_RANGE, qrfColour, 36, qrfTex, m_fQRFUVScale);
			DrawCircle(m_QRFCenter, OVT_QRFControllerComponent.QRF_POINT_RANGE, qrfColour, 36, qrfTex, m_fQRFUVScale);
		}
	}

	//! The QRF circles' colour - and the reason it is NOT the thing that makes a QRF stand out.
	//!
	//! WHY THE FACTION RULE HELPS THE BASE RINGS AND WOULD HURT THIS ONE. A ring is faction-coloured so
	//! it reads as its own territory, denser. A QRF fires when the occupier counter-attacks, so it
	//! appears INSIDE occupier territory almost by definition - painting it the occupier's colour would
	//! dissolve it into its own background at precisely the moment it matters most.
	//!
	//! THEY ARE ALSO DIFFERENT CATEGORIES OF INFORMATION. A base ring is reference state the player
	//! consults ("can I build here?") and is true for the whole campaign; a QRF is an alert the player
	//! reacts to, and it is the only thing this layer draws that appears and vanishes on a minute
	//! timescale. That earns it its own visual channel.
	//!
	//! 🔴 AND THE CHANNEL CANNOT BE HUE, WHICH IS THE PART NOBODY WILL RECOVER FROM THE CODE. The obvious
	//! fix - pick a contrasting alarm colour - assumes a fixed palette to contrast AGAINST, and there is
	//! none: the occupying faction is configurable, and faction colours run from red to blue. Orange
	//! reads well against a blue occupier and disappears against a red one. No fixed hue is reliably
	//! contrasting, so hue was rejected as this circle's distinguishing channel. DO NOT "simplify" this
	//! back to a hardcoded alarm colour.
	//!
	//! WHAT CARRIES IT INSTEAD IS STRUCTURE: the QRF circles are HATCHED, and structure is
	//! hue-independent. The semantic overlap with the territory layer's contested hatch is deliberate
	//! rather than a collision - hatch means "this ground is being fought over", and a QRF is the most
	//! literal case of that. The two are kept apart by SCALE, coarse bars for a QRF against the fine
	//! hatch of a contested town, and both scales are attributes so they can be separated by eye.
	//! \return Packed ARGB for both QRF circles.
	protected int ResolveQRFColour()
	{
		return ARGB(Math.ClampInt(m_iQRFAlpha, 0, 255), Math.ClampInt(m_iQRFRed, 0, 255), Math.ClampInt(m_iQRFGreen, 0, 255), Math.ClampInt(m_iQRFBlue, 0, 255));
	}

	//! Turns the collected faction indices into the packed colours Draw reads.
	//!
	//! Run once per map open, off the same pass that collects the geometry, because that is the layer's
	//! whole refresh model: the rings are gathered when the map opens and are not re-gathered until it is
	//! opened again. Resolving colour on the same schedule keeps the two exactly in step and keeps the
	//! faction manager off the per-frame path.
	protected void ResolveRingColours()
	{
		m_Colors.Clear();
		m_FriendlyColors.Clear();

		foreach(int factionIndex : m_FactionIndices)
		{
			m_Colors.Insert(ResolveRingColour(factionIndex, m_iRestrictedAlpha));
		}

		foreach(int factionIndex : m_FriendlyFactionIndices)
		{
			m_FriendlyColors.Insert(ResolveRingColour(factionIndex, m_iFriendlyRestrictedAlpha));
		}
	}

	//! One ring's packed colour: the owning faction's hue at the requested alpha.
	//!
	//! THE HUE IS NOT IMPLEMENTED HERE. It comes from the same shared helper the location markers and
	//! the territory fill call, so a ring cannot drift away from the colour of the marker at its centre
	//! or the territory under it - that single source is the only reason the "same territory, denser"
	//! reading holds at all.
	//!
	//! An unresolvable faction falls back to WHITE, matching the territory layer's fallback rather than
	//! reintroducing a hardcoded red: a pale ring over a coloured region still reads as a ring, whereas
	//! a red one would read as a faction that may not be there.
	//! \param[in] factionIndex The faction owning the base or tower at the ring's centre.
	//! \param[in] alpha Ring alpha, 0-255.
	//! \return Packed ARGB.
	protected int ResolveRingColour(int factionIndex, int alpha)
	{
		int red = 255;
		int green = 255;
		int blue = 255;

		Color colour = OVT_MapLocationType.GetFactionColorByIndex(factionIndex);
		if(colour)
		{
			red = Math.ClampInt(Math.Round(colour.R() * 255), 0, 255);
			green = Math.ClampInt(Math.Round(colour.G() * 255), 0, 255);
			blue = Math.ClampInt(Math.Round(colour.B() * 255), 0, 255);
		}

		return ARGB(Math.ClampInt(alpha, 0, 255), red, green, blue);
	}

	//! Loads the restricted-zone texture once per map open, guarded by IsValid before it can ever reach
	//! a draw command. Every failure path leaves m_bTextureValid false, so a missing or broken texture
	//! degrades to the flat rings this layer has always drawn and logs why, rather than crashing.
	protected void EnsureTexture()
	{
		if(m_bTextureAttempted)
			return;

		m_bTextureAttempted = true;

		if(!m_bUseTexture)
			return;

		if(m_sRestrictedTexture.IsEmpty())
		{
			Print("[OVT_MapRestrictedAreas] m_bUseTexture is on but no texture is configured. Drawing flat rings.", LogLevel.ERROR);
			return;
		}

		if(!m_Canvas)
		{
			Print("[OVT_MapRestrictedAreas] No CanvasWidget - cannot load the restricted-zone texture. Drawing flat rings.", LogLevel.ERROR);
			return;
		}

		m_TextureRef = m_Canvas.LoadTexture(m_sRestrictedTexture);

		if(!m_TextureRef)
		{
			Print(string.Format("[OVT_MapRestrictedAreas] LoadTexture returned null for '%1'. Drawing flat rings.", m_sRestrictedTexture), LogLevel.ERROR);
			return;
		}

		if(!m_TextureRef.IsValid())
		{
			m_TextureRef = null;
			Print(string.Format("[OVT_MapRestrictedAreas] LoadTexture returned an invalid SharedItemRef for '%1'. Drawing flat rings.", m_sRestrictedTexture), LogLevel.ERROR);
			return;
		}

		m_bTextureValid = true;
	}

	//! Loads the QRF hatch once per map open, on exactly the same terms as the ring texture: attempted at
	//! most once, never assigned to a command before IsValid has passed, and every failure path leaves
	//! m_bQRFTextureValid false so the circles keep drawing as the flat rings they always were.
	//!
	//! Attempted from Draw rather than OnMapOpen because a QRF can start while the map is already open -
	//! but still at most once per map session, so an active QRF costs no repeated load attempts.
	protected void EnsureQRFTexture()
	{
		if(m_bQRFTextureAttempted)
			return;

		m_bQRFTextureAttempted = true;

		if(!m_bUseQRFTexture)
			return;

		if(m_sQRFTexture.IsEmpty())
		{
			Print("[OVT_MapRestrictedAreas] m_bUseQRFTexture is on but no QRF texture is configured. Drawing flat QRF circles.", LogLevel.ERROR);
			return;
		}

		if(!m_Canvas)
		{
			Print("[OVT_MapRestrictedAreas] No CanvasWidget - cannot load the QRF texture. Drawing flat QRF circles.", LogLevel.ERROR);
			return;
		}

		m_QRFTextureRef = m_Canvas.LoadTexture(m_sQRFTexture);

		if(!m_QRFTextureRef)
		{
			Print(string.Format("[OVT_MapRestrictedAreas] LoadTexture returned null for '%1'. Drawing flat QRF circles.", m_sQRFTexture), LogLevel.ERROR);
			return;
		}

		if(!m_QRFTextureRef.IsValid())
		{
			m_QRFTextureRef = null;
			Print(string.Format("[OVT_MapRestrictedAreas] LoadTexture returned an invalid SharedItemRef for '%1'. Drawing flat QRF circles.", m_sQRFTexture), LogLevel.ERROR);
			return;
		}

		m_bQRFTextureValid = true;
	}

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		m_Centers = new array<vector>;
		m_Ranges = new array<int>;
		m_FactionIndices = new array<int>;
		m_Colors = new array<int>;
		m_FriendlyCenters = new array<vector>;
		m_FriendlyRanges = new array<int>;
		m_FriendlyFactionIndices = new array<int>;
		m_FriendlyColors = new array<int>;

		m_TextureRef = null;
		m_bTextureAttempted = false;
		m_bTextureValid = false;

		m_QRFTextureRef = null;
		m_bQRFTextureAttempted = false;
		m_bQRFTextureValid = false;

		OVT_OccupyingFactionManager factionMgr = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();

		if(factionMgr.m_bQRFActive)
		{
			m_bQRFActive = true;
			m_QRFCenter = factionMgr.m_vQRFLocation;
		}else{
			m_bQRFActive = false;
		}

		// Draw the radii the FOB deploy check actually enforces (it rejects near ANY base/tower)
		int baseRestrictedRange = otconfig.m_Difficulty.baseCloseRange + OVT_ResistanceFactionManager.FOB_DEPLOY_BASE_BUFFER;

		foreach(OVT_BaseData base : factionMgr.m_Bases)
		{
			if(!base.IsOccupyingFaction()) {
				m_FriendlyCenters.Insert(base.location);
				m_FriendlyRanges.Insert(baseRestrictedRange);
				m_FriendlyFactionIndices.Insert(base.faction);
				continue;
			};
			if(m_bQRFActive && factionMgr.m_iCurrentQRFBase > -1 && factionMgr.m_iCurrentQRFBase == base.id) continue;

			m_Centers.Insert(base.location);
			m_Ranges.Insert(baseRestrictedRange);
			m_FactionIndices.Insert(base.faction);
		}

		foreach(OVT_RadioTowerData tower : factionMgr.m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) {
				m_FriendlyCenters.Insert(tower.location);
				m_FriendlyRanges.Insert(OVT_ResistanceFactionManager.FOB_DEPLOY_TOWER_RANGE);
				m_FriendlyFactionIndices.Insert(tower.faction);
				continue;
			}
			m_Centers.Insert(tower.location);
			m_Ranges.Insert(OVT_ResistanceFactionManager.FOB_DEPLOY_TOWER_RANGE);
			m_FactionIndices.Insert(tower.faction);
		}

		// AFTER every ring is collected, never during: the colour arrays are index-parallel to the
		// centres, so building them in one pass at the end is what guarantees they cannot fall out of step
		// with an early-continue in either loop above
		ResolveRingColours();
	}

	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);

		m_Ranges.Clear();
		m_Ranges = null;
		m_Centers.Clear();
		m_Centers = null;
		m_FactionIndices.Clear();
		m_FactionIndices = null;
		m_Colors.Clear();
		m_Colors = null;
		m_FriendlyRanges.Clear();
		m_FriendlyRanges = null;
		m_FriendlyCenters.Clear();
		m_FriendlyCenters = null;
		m_FriendlyFactionIndices.Clear();
		m_FriendlyFactionIndices = null;
		m_FriendlyColors.Clear();
		m_FriendlyColors = null;

		m_TextureRef = null;
		m_bTextureAttempted = false;
		m_bTextureValid = false;

		m_QRFTextureRef = null;
		m_bQRFTextureAttempted = false;
		m_bQRFTextureValid = false;
	}
}
