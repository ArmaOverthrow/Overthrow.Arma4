modded class SCR_DSSessionCallback {
	override void LoadSession(string fileName)
	{
		SessionStorage storage = GetGame().GetBackendApi().GetStorage();
		storage.AssignFileIDCallback(fileName, this);
		
#ifdef WORKBENCH
		storage.LocalLoad(fileName);
		PrintFormat("LocalLoad: $profile:.backed\\%1.json", fileName);
		return;
#endif
		
		// TODO: Solve for Listen servers
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			//--- MP
			storage.RequestLoad(fileName);
			PrintFormat("RequestLoad: $profile:.backed\\%1.json", fileName);
		}
		else if (RplSession.Mode() == RplMode.None)
		{
			//--- SP
			storage.LocalLoad(fileName);
			PrintFormat("LocalLoad: $profile:.backed\\%1.json", fileName);
		}
	}
}