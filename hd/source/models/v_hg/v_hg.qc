
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\pistol\v_hg.mdl"
$modelname "v_hg.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "pistol_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$attachment 0 "Bone53" 0 -3 0


$hbox 0 "Bone26" -1.55 -4.619665 -1.07 1.82 1.35 1.225403
$hbox 0 "Bone27" -1.986842 -1.07 0 0.16 0.57 1.25
$hbox 0 "Bone28" -0.506463 -1.4 -0.92 0.58 0.07 0.46
$hbox 0 "Bone25" -2.2 -0.03 -1.98 2.06 10.52 1.44
$hbox 0 "Bone02" -3.39 -7.206539 -0.69 3.07 4.75 0.73
$hbox 0 "Bone49" -3.11 -0.61 -0.26 0 1.32 0.79
$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97
$hbox 0 "Bone51" -0.42 -5.19 -0.78 1.3 2.25 0.71

$sequence "idle" {
	"v_hg_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_hg_anims\idle2"
	fps 10
}
$sequence "reload" {
	"v_hg_anims\reload"
	{ event 5004 14 "weapons/pistol_clipout.wav" }
	{ event 5004 50 "weapons/pistol_clipin.wav" }
	fps 27
}
$sequence "reload_empty" {
	"v_hg_anims\reload_empty"
	{ event 5004 10 "weapons/pistol_clipout.wav" }
	{ event 5004 50 "weapons/pistol_clipin.wav" }
	{ event 5004 71 "weapons/pistol_slide_release.wav" }
	fps 27
}
$sequence "shoot" {
	"v_hg_anims\shoot"
	{ event 5001 0 "30" }
	fps 20
}
$sequence "shoot_empty" {
	"v_hg_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_hg_anims\draw"
	{ event 5004 2 "weapons/pistol_draw.wav" }
	fps 20
}
