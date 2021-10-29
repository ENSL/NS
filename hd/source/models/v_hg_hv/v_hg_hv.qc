
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\pistol\v_hg_hv.mdl"
$modelname "v_hg_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "pistol_ref_hv"
}

$flags 0


$attachment 0 "Bone53" 0 -3 0


$hbox 0 "Bone26" -18.485439 -4.59464 -6.128979 1.82 1.173586 1.17
$hbox 0 "Bone27" -2.14 -1.72 -0.4 0 0 1.27
$hbox 0 "Bone25" -2.97 -4.59 -2.4 3.32 10.49 2.6
$hbox 0 "Bone02" -3.39 -7.206539 -0.69 3.07 4.75 0.73
$hbox 0 "Bone49" -3.11 -0.61 -0.25 0 1.32 0.79
$hbox 0 "Bone50" -7.244637 0 -22.567659 0 1.465126 0
$hbox 0 "Bone03" -1.8 -3.41 -1.18 1.97 1.174229 2.04
$hbox 0 "Bone04" -2.33 -1.36 -1.27 0 0 0.15
$hbox 0 "Bone47" -2.98 -0.04 -2.62 3.31 10.49 2.39
$hbox 0 "Bone51" -0.42 -5.19 -0.78 1.3 2.25 0.71

$sequence "idle" {
	"v_hg_hv_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_hg_hv_anims\idle2"
	fps 10
}
$sequence "reload" {
	"v_hg_hv_anims\reload"
	{ event 5004 14 "weapons/pistol_clipout.wav" }
	{ event 5004 50 "weapons/pistol_clipin.wav" }
	fps 27
}
$sequence "reload_empty" {
	"v_hg_hv_anims\reload_empty"
	{ event 5004 10 "weapons/pistol_clipout.wav" }
	{ event 5004 50 "weapons/pistol_clipin.wav" }
	{ event 5004 71 "weapons/pistol_slide_release.wav" }
	fps 27
}
$sequence "shoot" {
	"v_hg_hv_anims\shoot"
	{ event 5001 0 "30" }
	fps 20
}
$sequence "shoot_empty" {
	"v_hg_hv_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_hg_hv_anims\draw"
	{ event 5004 2 "weapons/pistol_draw.wav" }
	fps 20
}
