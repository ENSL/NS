
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\mine\v_mine.mdl"
$modelname "v_mine.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "mine_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97
$hbox 0 "Bone26" -1.55 -2.14 -1.07 1.82 1.35 0.71
$hbox 0 "Bone27" -1.986842 -1.07 0 0.16 0.57 1.25
$hbox 0 "Bone28" -0.506463 -1.4 -0.92 0.58 0.07 0.46
$hbox 0 "Bone25" -2.2 -0.03 -1.98 2.06 10.52 1.44
$hbox 0 "Bone02" -4.89 -4.86 -0.13 4.86 4.89 2.06
$hbox 0 "Bone51" -0.471403 -1.00264 0 0.764 0.111117 3.287294
$hbox 0 "Bone71" -1.64 -0.68 -0.53 0.731561 0.66 0.55
$hbox 0 "Bone72" -2.34 -0.54 -0.82 0.48 0.52 0.42
$hbox 0 "Bone65" -0.65 -0.701438 -0.52 0.69 1.64 0.55
$hbox 0 "Bone66" -0.51 -0.48 -0.81 0.55 2.35 0.43
$hbox 0 "Bone62" -0.67 -1.64 -0.53 0.68 0.705147 0.54
$hbox 0 "Bone63" -0.52 -2.34 -0.81 0.54 0.49 0.43
$hbox 0 "Bone68" -0.696426 -0.6 -0.53 1.64 0.76 0.55
$hbox 0 "Bone69" -0.48 -0.44 -0.82 2.35 0.62 0.42

$sequence "idle" {
	"v_mine_anims\idle"
	fps 12
}
$sequence "idle2" {
	"v_mine_anims\idle2"
	fps 7
}
$sequence "draw" {
	"v_mine_anims\draw"
	{ event 5004 19 "weapons/mine_draw.wav" }
	fps 20
}
$sequence "activate" {
	"v_mine_anims\activate"
	{ event 5004 19 "weapons/mine_activate.wav" }
	fps 20
}
$sequence "plant" {
	"v_mine_anims\plant"
	{ event 5004 0 "weapons/mine_plant.wav" }
	fps 30
}
