
$modelname "v_sg_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "shotgun_hv_ref"
}

$flags 0


$attachment 0 "Bone53" 0 -2 0


$hbox 0 "Bone51" -3.97 -18.342178 -1.44 3.18 6.732486 3.03
$hbox 0 "Bone49" -2.16 -0.52 -1.54 1.38 11.29 1.58
$hbox 0 "Bone24" -2.96 -0.95 -4.79 3.33 10.38 1.47
$hbox 0 "Bone26" -1.42 -1.04 -13.828066 9.325866 8.059951 1.11
$hbox 0 "Bone03" -1.8 -3.41 -1.18 3.79406 22.844913 2.192061
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.16
$hbox 0 "Bone47" -2.98 -0.04 -2.61 3.32 10.49 2.4

$sequence "idle" {
	"v_sg_hv_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_sg_hv_anims\idle2"
	fps 10
}
$sequence "goto_reload" {
	"v_sg_hv_anims\goto_reload"
	fps 20
}
$sequence "reload" {
	"v_sg_hv_anims\reload"
	{ event 5004 5 "weapons/shotgun_reload.wav" }
	fps 27
	loop
}
$sequence "end_reload" {
	"v_sg_hv_anims\end_reload"
	{ event 5004 11 "weapons/shotgun_pump.wav" }
	fps 20
}
$sequence "shoot3" {
	"v_sg_hv_anims\shoot3"
	{ event 5001 0 "30" }
	{ event 5004 4 "weapons/shotgun_pump.wav" }
	{ event 5001 0 "30" }
	fps 25
}
$sequence "shoot_empty" {
	"v_sg_hv_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_sg_hv_anims\draw"
	{ event 5004 0 "weapons/shotgun_draw.wav" }
	fps 20
}
