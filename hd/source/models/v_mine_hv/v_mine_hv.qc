
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\mine\v_mine_HV.mdl"
$modelname "v_mine_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "mine_ref_HV"
}

$flags 0


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
$hbox 0 "Bone03" -1.8 -2.14 -1.18 1.83 1.174229 0.46
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.16
$hbox 0 "Bone47" -2.98 -4.59 -2.61 3.32 10.49 2.4
$hbox 0 "Bone26" -1.8 -2.14 -0.47 1.82 1.173586 1.17
$hbox 0 "Bone27" -2.14 -1.72 -0.4 0 0 1.27
$hbox 0 "Bone25" -2.97 -4.59 -2.4 3.32 10.49 2.6

$sequence "idle" {
	"v_mine_hv_anims\idle"
	fps 12
}
$sequence "idle1" {
	"v_mine_hv_anims\idle1"
	fps 7
}
$sequence "draw" {
	"v_mine_hv_anims\draw"
	{ event 5004 19 "weapons/mine_draw.wav" }
	fps 20
}
$sequence "activate" {
	"v_mine_hv_anims\activate"
	{ event 5004 19 "weapons/mine_activate.wav" }
	fps 20
}
$sequence "plant" {
	"v_mine_hv_anims\plant"
	{ event 5004 0 "weapons/mine_plant.wav" }
	fps 30
}
