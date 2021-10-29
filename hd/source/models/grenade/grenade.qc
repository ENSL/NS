
// Stored modelname (without quotes): "h:\SDK2\SDK\Weapon Models\NS\grenade.mdl"
$modelname "grenade.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "grenade_ref"
}

$flags 0


$hbox 0 "Bone02" -1.53 -0.23 0 1.52 8.22 2.66

$sequence "idle" {
	"grenade_anims\idle"
	fps 1
	loop
}
