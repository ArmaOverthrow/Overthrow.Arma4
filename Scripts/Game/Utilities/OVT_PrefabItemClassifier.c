//------------------------------------------------------------------------------------------------
//! Classifies a prefab into the arsenal (type, mode) the price configs speak, from its component
//! sources alone - for gear that is in nobody's entity catalogue and so has no SCR_ArsenalItem.
//!
//! Weapons read WeaponType off WeaponComponent, clothing reads the slot off BaseLoadoutClothComponent,
//! magazines/consumables/gadgets/mines classify by component class. Mode mirrors how vanilla files the
//! same things (guns WEAPON, magazines AMMUNITION, medical CONSUMABLE, everything else DEFAULT).
class OVT_PrefabItemClassifier : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] source The prefab's entity source.
	//! \param[out] type The arsenal type.
	//! \param[out] mode The arsenal mode.
	//! \return False when nothing on the prefab says what it is.
	static bool Classify(IEntitySource source, out SCR_EArsenalItemType type, out SCR_EArsenalItemMode mode)
	{
		if (!source)
			return false;

		mode = SCR_EArsenalItemMode.DEFAULT;

		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component)
				continue;

			typename componentType = component.GetClassName().ToType();
			if (!componentType)
				continue;

			if (componentType.IsInherited(BaseWeaponComponent))
			{
				EWeaponType weaponType;
				if (!component.Get("WeaponType", weaponType))
					continue;

				if (ClassifyWeapon(weaponType, type, mode))
					return true;

				continue;
			}

			if (componentType.IsInherited(BaseLoadoutClothComponent))
			{
				LoadoutAreaType area;
				if (component.Get("AreaType", area) && area && ClassifyClothing(area.Type(), type))
					return true;

				continue;
			}

			if (componentType.IsInherited(MagazineComponent))
			{
				type = SCR_EArsenalItemType.RIFLE;
				mode = SCR_EArsenalItemMode.AMMUNITION;
				return true;
			}

			if (componentType.IsInherited(SCR_ConsumableItemComponent))
			{
				type = SCR_EArsenalItemType.HEAL;
				mode = SCR_EArsenalItemMode.CONSUMABLE;
				return true;
			}

			if (componentType.IsInherited(SCR_MineWeaponComponent) || componentType.IsInherited(SCR_MineInventoryItemComponent))
			{
				type = SCR_EArsenalItemType.EXPLOSIVES;
				return true;
			}

			if (componentType.IsInherited(SCR_GadgetComponent))
			{
				type = SCR_EArsenalItemType.EQUIPMENT;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ClassifyWeapon(EWeaponType weaponType, out SCR_EArsenalItemType type, out SCR_EArsenalItemMode mode)
	{
		mode = SCR_EArsenalItemMode.WEAPON;
		switch (weaponType)
		{
			case EWeaponType.WT_RIFLE:
			case EWeaponType.WT_GRENADELAUNCHER:
				type = SCR_EArsenalItemType.RIFLE;
				return true;
			case EWeaponType.WT_SNIPERRIFLE:
				type = SCR_EArsenalItemType.SNIPER_RIFLE;
				return true;
			case EWeaponType.WT_MACHINEGUN:
				type = SCR_EArsenalItemType.MACHINE_GUN;
				return true;
			case EWeaponType.WT_HANDGUN:
				type = SCR_EArsenalItemType.PISTOL;
				return true;
			case EWeaponType.WT_ROCKETLAUNCHER:
				type = SCR_EArsenalItemType.ROCKET_LAUNCHER;
				return true;
			case EWeaponType.WT_FRAGGRENADE:
				type = SCR_EArsenalItemType.LETHAL_THROWABLE;
				mode = SCR_EArsenalItemMode.DEFAULT;
				return true;
			case EWeaponType.WT_SMOKEGRENADE:
				type = SCR_EArsenalItemType.NON_LETHAL_THROWABLE;
				mode = SCR_EArsenalItemMode.DEFAULT;
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ClassifyClothing(typename area, out SCR_EArsenalItemType type)
	{
		if (area.IsInherited(LoadoutHeadCoverArea)) { type = SCR_EArsenalItemType.HEADWEAR; return true; }
		if (area.IsInherited(LoadoutJacketArea)) { type = SCR_EArsenalItemType.TORSO; return true; }
		if (area.IsInherited(LoadoutVestArea) || area.IsInherited(LoadoutArmoredVestSlotArea)) { type = SCR_EArsenalItemType.VEST_AND_WAIST; return true; }
		if (area.IsInherited(LoadoutPantsArea)) { type = SCR_EArsenalItemType.LEGS; return true; }
		if (area.IsInherited(LoadoutBootsArea)) { type = SCR_EArsenalItemType.FOOTWEAR; return true; }
		if (area.IsInherited(LoadoutHandwearSlotArea)) { type = SCR_EArsenalItemType.HANDWEAR; return true; }
		if (area.IsInherited(LoadoutBackpackArea)) { type = SCR_EArsenalItemType.BACKPACK; return true; }
		return false;
	}
}
