
// Stored modelname (without quotes): "C:\Documents and Settings\Administrator\Desktop\LMG\v_mg_hv.mdl"
$modelname "v_mg_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "v_lmg_ref_hv"
}

$flags 0


$attachment 0 "Bone51" -0.3 -1 0.3


$hbox 0 "Bone24" -2.88 -15.62 -2.4 3.42 0 2.61
$hbox 0 "Bone25" -1.45 -3.31 -0.46 1.8 0 1
$hbox 0 "Bone26" -0.905261 -2.092581 -0.359631 9.66397 0.22386 15.542175
$hbox 0 "Bone27" -2.13 -1.72 -0.4 0 0.57 1.27
$hbox 0 "Bone01" -4.39 -15.03665 -1.45 3.85 7.47 1.08
$hbox 0 "Bone55" -1.48 -2.59 -1.21 0.3 1.54 0.71
$hbox 0 "Bone53" 0 -1.7 -6.69 1.04 0.7 0
$hbox 0 "Bone54" -3.665827 0 -11.629793 0 1.417501 0
$hbox 0 "Bone03" -1.8 -2.14 -1.17 1.83 1.174229 0.47
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.17
$hbox 0 "Bone47" -2.97 -4.59 -2.6 3.32 10.49 2.4

$sequence "idle" {
	"v_mg_hv_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_mg_hv_anims\idle2"
	fps 10
}
$sequence "reload" {
	"v_mg_hv_anims\reload"
	{ event 5004 13 "weapons/lmg_clipout.wav" }
	{ event 5004 38 "weapons/lmg_clipin.wav" }
	fps 20
}
$sequence "shoot" {
	"v_mg_hv_anims\shoot"
	{ event 5001 0 "20" }
	fps 20
}
$sequence "shoot_empty" {
	"v_mg_hv_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_mg_hv_anims\draw"
	{ event 5004 0 "weapons/lmg_draw.wav" }
	fps 20
}
