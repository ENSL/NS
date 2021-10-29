
$modelname "b_resourcetower.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "resource_tower_reference"
}

$flags 0

$externaltextures


$attachment 0 "D_particle_help" 0 4 0

$hbox 0 "D_tower_body" -30.009998 -7.25 -30.16 30.009998 10.18 30.860001
$hbox 0 "D_particle_help" -5.27 0 -5.48 5.27 7.4 5.33
$hbox 0 "D_Tube_top" -4.54 -24.72481 -4.21 4.7 0 5.65
$hbox 0 "D_Claw_body" -8.21 -3.5 -7.92 8.21 3.5 9.62
$hbox 0 "D_upper_claw03" 0 -4.34 -0.78 11.01 2.06 1.9
$hbox 0 "D_upper_claw01" -0.99 -4.34 -1.05 9.36 2.06 1.58
$hbox 0 "D_upper_claw02" -0.09 -4.34 -2.2 10.28 2.06 0.48
$hbox 0 "D_UpperLeg02" -0.06 -9.13 -6.72 23.459999 6.15 4.17
$hbox 0 "D_LowerLeg02" -1.9 -16.34 -3.93 16.780001 2.17 1.47
$hbox 0 "D_Foot02" -4.84 -35.369999 -11.29 13.11 8.19 8.26
$hbox 0 "D_UpperLeg03" -1.05 -9.13 -5.44 22.42 6.15 5.45
$hbox 0 "D_LowerLeg03" -2.89 -16.34 -2.66 15.79 2.17 2.75
$hbox 0 "D_Foot03" -5.83 -35.369999 -10.02 12.12 8.19 9.53
$hbox 0 "D_UpperLeg01" -0.02 -9.13 -4.36 23.59 6.15 6.53
$hbox 0 "D_LowerLeg01" -1.68 -16.34 -1.56 16.99 2.17 3.84
$hbox 0 "D_Foot01" -4.62 -35.369999 -8.93 13.33 8.19 10.62
$hbox 0 "D_UpperLeg04" 0 -9.13 -4.34 24.629999 6.15 6.56
$hbox 0 "D_LowerLeg04" -1.14 -16.34 -1.53 17.540001 2.17 3.87
$hbox 0 "D_Foot04" -4.08 -35.369999 -8.89 13.87 8.19 10.66

$sequence "spawn" {
	"b_resourcetower_anims\spawn"
	fps 5
	loop
}
$sequence "deploy" {
	"b_resourcetower_anims\deploy"
	fps 7
}
$sequence "idle1" {
	"b_resourcetower_anims\idle1"
	fps 5
	loop
}
$sequence "idle2" {
	"b_resourcetower_anims\idle2"
	fps 5
	loop
}
$sequence "dummy_researching" {
	"b_resourcetower_anims\dummy_researching"
	fps 1
}
$sequence "active" {
	"b_resourcetower_anims\active"
	fps 6
	loop
}
$sequence "dummy_fire" {
	"b_resourcetower_anims\dummy_fire"
	fps 1
}
$sequence "take_damage_dummy" {
	"b_resourcetower_anims\take_damage_dummy"
	fps 1
}
$sequence "die_forward" {
	"b_resourcetower_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"b_resourcetower_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"b_resourcetower_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"b_resourcetower_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"b_resourcetower_anims\dummy_special"
	fps 1
}
