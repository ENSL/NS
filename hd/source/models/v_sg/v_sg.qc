
$modelname "v_sg.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "shotgun_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$attachment 0 "Bone53" 0 -2 0


$hbox 0 "Bone51" -3.97 -18.342178 -1.4 3.18 6.91 1.79
$hbox 0 "Bone49" -2.16 -0.52 -1.54 1.38 11.29 1.58
$hbox 0 "Bone24" -2.15 -0.02 -1.98 2.11 7.58 1.44
$hbox 0 "Bone26" -1.42 -1.04 -5.868253 14.316846 9.377198 1.11
$hbox 0 "Bone03" -1.55 -2.14 -1 24.669785 10.3586 10.135379
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97

$sequence "idle" {
	"v_sg_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_sg_anims\idle2"
	fps 10
}
$sequence "goto_reload" {
	"v_sg_anims\goto_reload"
	fps 20
}
$sequence "reload" {
	"v_sg_anims\reload"
	{ event 5004 5 "weapons/shotgun_reload.wav" }
	fps 27
	loop
}
$sequence "end_reload" {
	"v_sg_anims\end_reload"
	{ event 5004 11 "weapons/shotgun_pump.wav" }
	fps 20
}
$sequence "shoot3" {
	"v_sg_anims\shoot3"
	{ event 5001 0 "30" }
	{ event 5004 4 "weapons/shotgun_pump.wav" }
	{ event 5001 0 "30" }
	fps 25
}
$sequence "shoot_empty" {
	"v_sg_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_sg_anims\draw"
	{ event 5004 0 "weapons/shotgun_draw.wav" }
	fps 20
}
