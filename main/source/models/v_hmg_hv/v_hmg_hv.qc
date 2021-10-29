
$modelname "v_hmg_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "hmg_ref_hv"
}

$flags 0


$attachment 0 "Bone27" 0 0 0
$attachment 1 "Bone29" 0 0 0


$hbox 0 "Bone01" -2.04 -20.15 -6.07 1.32 12.7 10.476808
$hbox 0 "Bone32" -2.62 -1.34 -2.85 2.02 2.45 3.1695
$hbox 0 "Bone35" 0 0 -18.354189 11.733462 2.695623 0
$hbox 0 "Bone03" -1.8 -2.14 -1.18 1.83 1.174229 0.46
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.16
$hbox 0 "Bone47" -2.98 -4.59 -2.61 3.32 10.49 2.4
$hbox 0 "Bone30" -2.83 -5.25 -2.01 2.24 0 0
$hbox 0 "Bone26" -1.33 -15.645182 -0.9 0.48 0.62 0.9
$hbox 0 "Bone28" -1.33 -15.705208 -0.9 0.48 0.59 0.9

$sequence "idle" {
	"v_hmg_hv_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_hmg_hv_anims\idle2"
	fps 11
}
$sequence "idle3" {
	"v_hmg_hv_anims\idle3"
	fps 11
}
$sequence "reload" {
	"v_hmg_hv_anims\reload"
	{ event 5004 8 "weapons/hmg_open.wav" }
	{ event 5004 56 "weapons/hmg_clipout.wav" }
	{ event 5004 95 "weapons/hmg_clipin.wav" }
	{ event 5004 116 "weapons/hmg_close.wav" }
	{ event 5004 129 "weapons/hmg_slide.wav" }
	fps 22
}
$sequence "shoot_top" {
	"v_hmg_hv_anims\shoot_top"
	{ event 5001 0 "20" }
	fps 20
}
$sequence "shoot_bottom" {
	"v_hmg_hv_anims\shoot_bottom"
	{ event 5011 0 "20" }
	fps 20
}
$sequence "shoot_empty" {
	"v_hmg_hv_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_hmg_hv_anims\draw"
	{ event 5004 4 "weapons/hmg_draw.wav" }
	fps 20
}
