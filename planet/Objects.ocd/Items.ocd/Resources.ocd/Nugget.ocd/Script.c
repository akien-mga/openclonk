/*--- Nugget ---*/

#include Library_CarryHeavy

public func GetCarryMode(clonk) { return CARRY_BothHands; }
public func GetCarryPhase() { return 800; }

protected func Hit(x, y)
{
	StonyObjectHit(x,y);
	return true;
}

public func IsFoundryIngredient() { return true; }
public func IsValuable(){ return true; }

func Definition(def) {
	SetProperty("PictureTransformation", Trans_Mul(Trans_Rotate(30,0,0,1),Trans_Rotate(-30,1,0,0),Trans_Scale(1300)),def);
}
local Name = "$Name$";
local Description = "$Description$";
<<<<<<< HEAD
local Touchable = 2;
=======
local Plane = 470;
>>>>>>> master
