
// Stored modelname (without quotes): "C:\Documents and Settings\Administrator\Desktop\LMG\v_mg.mdl"
$modelname "v_mg.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "v_lmg_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$attachment 0 "Bone51" -0.3 -1 0.3


$hbox 0 "Bone24" -2.1 -11.07 -1.98 2.15 0 1.44
$hbox 0 "Bone25" -1.57 -3.32 -1.07 1.8 0.17 0.99
$hbox 0 "Bone26" -1.888534 -2.229281 -0.359631 1.469365 0.22386 1.176305
$hbox 0 "Bone27" -2.14 -1.72 -0.15 0 0.57 1.27
$hbox 0 "Bone01" -4.39 -15.036648 -1.45 3.85 7.47 1.08
$hbox 0 "Bone55" -1.48 -2.59 -1.21 0.3 1.54 0.71
$hbox 0 "Bone53" 0 -1.7 -6.69 1.04 0.7 0
$hbox 0 "Bone54" -1.12897 -2.560474 -10.608303 0 0 0
$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97

$sequence "idle" {
	"v_mg_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_mg_anims\idle2"
	fps 10
}
$sequence "reload" {
	"v_mg_anims\reload"
	{ event 5004 13 "weapons/lmg_clipout.wav" }
	{ event 5004 38 "weapons/lmg_clipin.wav" }
	fps 20
}
$sequence "shoot" {
	"v_mg_anims\shoot"
	{ event 5001 0 "20" }
	fps 20
}
$sequence "shoot_empty" {
	"v_mg_anims\shoot_empty"
	fps 20
}
$sequence "draw" {
	"v_mg_anims\draw"
	{ event 5004 0 "weapons/lmg_draw.wav" }
	fps 20
}
