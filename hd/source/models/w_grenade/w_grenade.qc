
// Stored modelname (without quotes): "h:\SDK2\SDK\Weapon Models\NS\grenade.mdl"
$modelname "w_grenade.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "grenade_ref"
}

$flags 0


$hbox 0 "Bone02" -1.53 -0.23 -1.38 1.52 8.22 1.25

$sequence "idle" {
	"w_grenade_anims\idle"
	fps 1
	loop
}
