
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\w_jetpack.mdl"
$modelname "w_jetpack.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "jetpack_ref"
}

$flags 0


$hbox 0 "w_ marine_" -10.02 -8.37 -12.89 10.02 18.84 12.89

$sequence "idle" {
	"w_jetpack_anims\idle"
	ACT_IDLE 1
	fps 5
	loop
}
