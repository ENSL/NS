
// Stored modelname (without quotes): "C:\Furreal\models\ammo\ammo.mdl"
$modelname "w_ammopack.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "shotgunammo"
	studio "lmgammo"
	studio "hmgammo"
	studio "pistolammo"
	studio "GLammo"
}

$flags 0


$hbox 0 "joint1" 0.1 0.1 0.1 0.1 0.1 0.1

$sequence "idle" {
	"w_ammopack_anims\idle"
	fps 30
}
