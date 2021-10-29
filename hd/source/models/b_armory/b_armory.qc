
$modelname "b_armory.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "armory_ref"
}

$flags 0

$externaltextures


$attachment 0 "Bone02" 20 15 15

$hbox 0 "Bone02" -25.034986 -24.537804 -9.938429 24.468674 23.996393 30.00993
$hbox 0 "INTERFACE A04" 0 -8.45 0 38.470001 8.45 26.4
$hbox 0 "INTERFACE A02" -38.470001 -8.45 0 0 8.45 26.4
$hbox 0 "INTERFACE A03" -8.45 -38.470001 0 8.45 0 26.4
$hbox 0 "INTERFACE A01" -8.45 0 0 8.45 38.470001 26.4
$hbox 0 "TOP" -20.4 -20.4 0 20.4 20.4 26.1
$hbox 0 "ADVANCED CORE03" -6.49 -32.330002 -7.14 6.59 0 8.7
$hbox 0 "ADVANCED CORE01" -6.59 0 -7.14 6.49 32.330002 8.7
$hbox 0 "ADVANCED CORE04" 0 -6.49 -7.14 32.330002 6.59 8.7
$hbox 0 "ADVANCED CORE02" -32.330002 -6.59 -7.14 0 6.49 8.7
$hbox 0 "BASE" -24.42 -24.42 0 24.42 24.42 36.139999

$sequence "spawn" {
	"b_armory_anims\spawn"
	fps 5
	loop
}
$sequence "deploy" {
	"b_armory_anims\deploy"
	fps 8
}
$sequence "idle_basic" {
	"b_armory_anims\idle_basic"
	fps 5
	loop
}
$sequence "idle_adv" {
	"b_armory_anims\idle_adv"
	fps 5
	loop
}
$sequence "adv_upg" {
	"b_armory_anims\adv_upg"
	fps 8
}
$sequence "active_basic" {
	"b_armory_anims\active_basic"
	fps 25
	loop
}
$sequence "dummy_fire" {
	"b_armory_anims\dummy_fire"
	fps 1
}
$sequence "take_damage_dummy" {
	"b_armory_anims\take_damage_dummy"
	fps 1
}
$sequence "die_forward" {
	"b_armory_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"b_armory_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"b_armory_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"b_armory_anims\die_right"
	fps 17
}
$sequence "active_adv" {
	"b_armory_anims\active_adv"
	fps 25
	loop
}
