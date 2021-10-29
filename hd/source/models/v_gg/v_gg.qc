
// Stored modelname (without quotes): "C:\3dmax31\Scenes\natural_selection\gl\v_gg.mdl"
$modelname "v_gg.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "GL"
{
	studio "gl_ref"
}
$bodygroup "right_hand"
{
	studio "gl_right_hand_ref"
}
$bodygroup "left_hand"
{
	studio "gl_left_hand_ref"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "hands.bmp"       }
	{ "hands_black.bmp" }
}


$attachment 0 "Bone13" 0 0 0


$hbox 0 "Bone26" -21.077509 -2.14 -10.654328 1.82 5.490737 0.71
$hbox 0 "Bone01" -24.230747 -5.54034 -5.04 3.12 2.66 3.42
$hbox 0 "Bone11" -0.22 -2.3 -11.815604 4.47 2.8 1.39
$hbox 0 "vone07" 0 -2.29 -2.2 4.02 2.24 2.25
$hbox 0 "vone08" -2.808218 -2.154317 -11.720367 0.801523 2.106357 1.52
$hbox 0 "Bone49" -0.49 -0.71 -0.71 3.17 0.71 0.71
$hbox 0 "Bone51" -0.49 -0.71 -0.71 3.17 0.7 0.71
$hbox 0 "Bone55" -0.58 -0.71 -0.71 3.08 0.7 0.71
$hbox 0 "Bone53" -0.49 -0.71 -0.71 3.17 0.7 0.71
$hbox 0 "Bone27" -1.986842 -1.07 0 0.16 0.57 1.25
$hbox 0 "Bone28" -0.506463 -1.4 -0.92 0.58 0.07 0.46
$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97

$sequence "idle-4or0" {
	"v_gg_anims\idle-4or0"
	fps 5
}
$sequence "idle-3" {
	"v_gg_anims\idle-3"
	fps 5
}
$sequence "idle-2" {
	"v_gg_anims\idle-2"
	fps 5
}
$sequence "idle-1" {
	"v_gg_anims\idle-1"
	fps 5
}
$sequence "reload-3" {
	"v_gg_anims\reload-3"
	{ event 5004 1 "weapons/gg_reload_start.wav" }
	{ event 5004 35 "weapons/gg_insert.wav" }
	{ event 5004 60 "weapons/gg_reload_end.wav" }
	fps 25
}
$sequence "reload-2" {
	"v_gg_anims\reload-2"
	{ event 5004 1 "weapons/gg_reload_start.wav" }
	{ event 5004 35 "weapons/gg_insert.wav" }
	{ event 5004 65 "weapons/gg_insert.wav" }
	{ event 5004 90 "weapons/gg_reload_end.wav" }
	fps 25
}
$sequence "reload-1" {
	"v_gg_anims\reload-1"
	{ event 5004 1 "weapons/gg_reload_start.wav" }
	{ event 5004 35 "weapons/gg_insert.wav" }
	{ event 5004 65 "weapons/gg_insert.wav" }
	{ event 5004 95 "weapons/gg_insert.wav" }
	{ event 5004 120 "weapons/gg_reload_end.wav" }
	fps 25
}
$sequence "reload-0" {
	"v_gg_anims\reload-0"
	{ event 5004 1 "weapons/gg_reload_start.wav" }
	{ event 5004 34 "weapons/gg_insert.wav" }
	{ event 5004 65 "weapons/gg_insert.wav" }
	{ event 5004 95 "weapons/gg_insert.wav" }
	{ event 5004 125 "weapons/gg_insert.wav" }
	{ event 5004 150 "weapons/gg_reload_end.wav" }
	fps 25
}
$sequence "shoot-4" {
	"v_gg_anims\shoot-4"
	{ event 5001 0 "30" }
	{ event 5004 8 "weapons/gg_rotate.wav" }
	fps 20
}
$sequence "shoot-3" {
	"v_gg_anims\shoot-3"
	{ event 5001 0 "30" }
	{ event 5004 8 "weapons/gg_rotate.wav" }
	fps 20
}
$sequence "shoot-2" {
	"v_gg_anims\shoot-2"
	{ event 5001 0 "30" }
	{ event 5004 8 "weapons/gg_rotate.wav" }
	fps 20
}
$sequence "shoot-1" {
	"v_gg_anims\shoot-1"
	{ event 5001 0 "30" }
	{ event 5004 8 "weapons/gg_rotate.wav" }
	fps 20
}
$sequence "shoot_empty" {
	"v_gg_anims\shoot_empty"
	fps 20
}
$sequence "draw-4or0" {
	"v_gg_anims\draw-4or0"
	{ event 5004 1 "weapons/gg_draw.wav" }
	fps 20
}
$sequence "draw-3" {
	"v_gg_anims\draw-3"
	{ event 5004 1 "weapons/gg_draw.wav" }
	fps 20
}
$sequence "draw-2" {
	"v_gg_anims\draw-2"
	{ event 5004 1 "weapons/gg_draw.wav" }
	fps 20
}
$sequence "draw-1" {
	"v_gg_anims\draw-1"
	{ event 5004 1 "weapons/gg_draw.wav" }
	fps 20
}
$sequence "cancel-reload-1" {
	"v_gg_anims\cancel-reload-1"
	{ event 5004 7 "weapons/gg_reload_end.wav" }
	fps 25
}
$sequence "cancel-reload-2or3" {
	"v_gg_anims\cancel-reload-2or3"
	{ event 5004 7 "weapons/gg_reload_end.wav" }
	fps 25
}
