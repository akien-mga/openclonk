/* Sky race */

func Initialize()
{

	CreateObject(Dynamite,1050,1150,-1);
	CreateObject(Dynamite,1050,1150,-1);
	
	CreateObject(Dynamite,500,900,-1);
	CreateObject(Dynamite,500,900,-1);
	
	
	CreateObject(Dynamite,1336,1116,-1);
	CreateObject(Dynamite,1675,1075,-1);
	CreateObject(Blackpowder,1130,1007,-1);
	
	DrawMaterialQuad("Tunnel",1378,1327-5,1860,1327-5,1860,1330,1387,1330);
	for(var i = 1380; i<=1800; i+=30)
	{
		CreateObject(Blackpowder,i,1328,-1);
	
	}
	
	CreateObject(Blackpowder,2553,918,-1);
	
	CreateObject(Dynamite,3208,1188,-1);
	
	CreateObject(Dynamite,3361,749,-1);
	CreateObject(Dynamite,3243,557,-1);
	
	for(var i=0; i<=6; i++)
	CreateObject(Dynamite,3090-(3*i),564,-1);
	
	// Create the race goal.
	var pGoal = CreateObject(Goal_Parkour, 0, 0, NO_OWNER);
	pGoal->SetStartpoint(20, 1000);
	pGoal->AddCheckpoint(760,950,  PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(400,660,  PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(870,460,  PARKOUR_CP_Respawn);
	pGoal->AddCheckpoint(1200,1020,PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(1665,1070,PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(1120,1010,PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(1485,800, PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(1735,1410,PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(2110,1180,PARKOUR_CP_Respawn);
	pGoal->AddCheckpoint(3350,1240,PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(3040,720, PARKOUR_CP_Respawn);
	pGoal->AddCheckpoint(2530,520, PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(2150,510, PARKOUR_CP_Ordered);
	pGoal->AddCheckpoint(2740,350, PARKOUR_CP_Ordered);
	pGoal->SetFinishpoint(3490,100);
	
	
	
	return 1;
}


// Gamecall from Race-goal, on respawning.
protected func PlrHasRespawned(int iPlr, object cp)
{
	var clonk = GetCrew(iPlr);
	clonk->CreateContents(JarOfWinds);
	return;
}


/* Relaunch */

