
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\prototype_lab.mdl"
$modelname "b_prototypelab.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "prototype_lab_ref"
}

$flags 0

$externaltextures


$attachment 0 "Dummy04" 0 0 0


$hbox 0 "Dummy01" -23.48 0 -21.370001 24.77 34.080002 21.48
$hbox 0 "Dummy15" -1.753298 0 0 0 14.017054 2.943781
$hbox 0 "Dummy16" -0.98 -0.12 -0.98 1.02 10.360385 0.76
$hbox 0 "Dummy02" -1.33 -2.26 -5.77 1.33 8.25578 5.97
$hbox 0 "Dummy05" -22.18 -8.33 -14.59 3.68 25.629999 14.7
$hbox 0 "Dummy14" -1.43 -7.49 -1.3 5.71 8.18 26.860001
$hbox 0 "Dummy08" -5.58 -7.42 -1.39 1.57 8.25 26.77
$hbox 0 "Dummy11" -1.55 -7.56 -1.3 5.59 8.11 26.860001
$hbox 0 "Dummy07" -5.09 -7.59 -1.3 2.05 8.08 26.85
$hbox 0 "Dummy04" -1.91 -13.08 -11.34 20.799999 5.99 11.56

$sequence "spawn" {
	"b_prototypelab_anims\spawn"
	fps 1
}
$sequence "deploy" {
	"b_prototypelab_anims\deploy"
	fps 12
}
$sequence "idle1" {
	"b_prototypelab_anims\idle1"
	fps 8
	loop
}
$sequence "idle2" {
	"b_prototypelab_anims\idle2"
	fps 6
	loop
}
$sequence "researching" {
	"b_prototypelab_anims\researching"
	fps 14
	loop
}
$sequence "active" {
	"b_prototypelab_anims\active"
	fps 14
	loop
}
$sequence "dummy_fire" {
	"b_prototypelab_anims\dummy_fire"
	fps 1
}
$sequence "dummy_take_damage" {
	"b_prototypelab_anims\dummy_take_damage"
	fps 30
}
$sequence "die_forward" {
	"b_prototypelab_anims\die_forward"
	fps 15
}
$sequence "die_left" {
	"b_prototypelab_anims\die_left"
	fps 15
}
$sequence "die_backward" {
	"b_prototypelab_anims\die_backward"
	fps 15
}
$sequence "die_right" {
	"b_prototypelab_anims\die_right"
	fps 15
}
$sequence "dummy_special" {
	"b_prototypelab_anims\dummy_special"
	fps 8
}
