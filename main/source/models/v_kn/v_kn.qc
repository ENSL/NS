
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\knife\v_kn.mdl"
$modelname "v_kn.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "knife_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$hbox 0 "Bone26" -1.55 -2.14 -1.07 1.82 1.35 0.71
$hbox 0 "Bone27" -1.986842 -1.07 0 0.16 0.57 1.25
$hbox 0 "Bone28" -0.506463 -1.4 -0.92 0.58 0.07 0.46
$hbox 0 "Bone25" -2.2 -0.03 -1.98 2.06 11.039249 1.44
$hbox 0 "Bone01" 0 -13.035677 -10.069013 5.682652 0 0
$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97

$sequence "idle1" {
	"v_kn_anims\idle1"
	fps 10
}
$sequence "idle2" {
	"v_kn_anims\idle2"
	fps 10
}
$sequence "attack1" {
	"v_kn_anims\attack1"
	fps 25
}
$sequence "attack2" {
	"v_kn_anims\attack2"
	fps 25
}
$sequence "draw" {
	"v_kn_anims\draw"
	fps 18
}
$sequence "flourish" {
	"v_kn_anims\flourish"
	fps 22
}
