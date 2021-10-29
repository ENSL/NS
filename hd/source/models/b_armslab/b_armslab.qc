
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\armory.mdl"
$modelname "b_armslab.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "armory_reference"
}

$flags 0

$externaltextures


$attachment 0 "Dummy02" 0 0 0

$hbox 0 "Dummy02" -17.917629 -15.916123 -0.793561 24.622334 0 24.051064
$hbox 0 "cover hinge" -1.26 -1.25 -15.68 1.26 1.26 25.41
$hbox 0 "D_screen01" 0 0 -19.781185 11.302593 12.870768 17.287529
$hbox 0 "D_screen02" -13.33926 -9.196434 -18.419069 0.008091 0.225137 15.756766
$hbox 0 "Box19" -17.93 -1.86 0 0.96 2.02 1.63
$hbox 0 "arm rotater 01" -2.79 -2.41 0 2.79 2.41 4.34
$hbox 0 "Box38" -17.93 -1.86 0 0.96 2.02 1.63
$hbox 0 "arm rotater 02" -2.79 -2.41 0 2.79 2.41 3.95
$hbox 0 "Box39" -0.59 -1.43 0 14.07 1.54 1.63
$hbox 0 "Cylinder38" -3.4 -2.95 0 3.4 2.95 4.38
$hbox 0 "Box18" -0.59 -1.43 0 14.07 1.54 1.63
$hbox 0 "Cylinder36" -3.4 -2.95 0 3.4 2.95 4.38
$hbox 0 "Dummy04" -0.95 -14.0789 -1.76 0.98 0.98 8.672063
$hbox 0 "Dummy05" -1.39 -13.99 -0.92 1.11 0.61 2.26
$hbox 0 "Dummy06" -25.969999 -6.23 -1.01 0 0 1.05
$hbox 0 "Line01" -22.09 -45.77 -50.98 45.110001 13.42 3.3

$sequence "spawn" {
	"b_armslab_anims\spawn"
	fps 5
	loop
}
$sequence "deploy" {
	"b_armslab_anims\deploy"
	fps 7
}
$sequence "idle1" {
	"b_armslab_anims\idle1"
	fps 5
	loop
}
$sequence "idle2" {
	"b_armslab_anims\idle2"
	fps 5
	loop
}
$sequence "dummy_researching" {
	"b_armslab_anims\dummy_researching"
	fps 1
}
$sequence "active" {
	"b_armslab_anims\active"
	fps 10
	loop
}
$sequence "dummy_fire" {
	"b_armslab_anims\dummy_fire"
	fps 1
}
$sequence "take_damage_dummy" {
	"b_armslab_anims\take_damage_dummy"
	fps 1
}
$sequence "die_forward" {
	"b_armslab_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"b_armslab_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"b_armslab_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"b_armslab_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"b_armslab_anims\dummy_special"
	fps 1
}
