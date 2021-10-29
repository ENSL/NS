
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\ba_sensory.mdl"
$modelname "ba_sensory.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "upgrade_alien_sensory_reference"
}

$flags 0

$externaltextures


$attachment 0 "Bone03" 0 0 3


$hbox 0 "Bone01" -0.1 -24.66 -26.780001 12.972679 27.719999 29.75
$hbox 0 "Bone02" -0.05 -14.29 -16.219999 13.34 14.07 14.38
$hbox 0 "Bone03" 0 -9.57 -10.65 25.790001 9.17 7.94
$hbox 0 "Bone07" 0 -2.83 -2.72 12.04 3.29 3.48
$hbox 0 "Bone08" 0 -3.45 -3.56 13.21 2.64 2.8
$hbox 0 "Bone12" 0 -3.21 -2.38 13.42 2.91 3.73
$hbox 0 "Bone16" 0 -3.75 -3.16 13.24 3.29 3.38
$hbox 0 "Bone20" 0 -2.99 -3.23 13.01 2.95 2.81
$hbox 0 "Bone24" 0 -3.02 -2.82 13.33 3.04 3.75
$hbox 0 "Bone28" 0 -2.93 -2.84 10.98 4.64 3.23
$hbox 0 "Bone32" 0 -3.02 -3.69 10.81 3.5 2.85
$hbox 0 "Bone36" 0 -3.76 -3.17 12.55 3.72 3.36
$hbox 0 "Bone40" 0 -2.6 -2.53 11.45 3.8 4.02

$sequence "spawn" {
	"ba_sensory_anims\spawn"
	fps 12
}
$sequence "deploy" {
	"ba_sensory_anims\deploy"
	fps 6
}
$sequence "idle1" {
	"ba_sensory_anims\idle1"
	fps 3
	loop
}
$sequence "idle2" {
	"ba_sensory_anims\idle2"
	fps 5
	loop
}
$sequence "researching" {
	"ba_sensory_anims\researching"
	fps 10
}
$sequence "dummy_active" {
	"ba_sensory_anims\dummy_active"
	fps 8
	loop
}
$sequence "dummy_fire" {
	"ba_sensory_anims\dummy_fire"
	fps 1
}
$sequence "take_damage" {
	"ba_sensory_anims\take_damage"
	fps 30
}
$sequence "die_forward" {
	"ba_sensory_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"ba_sensory_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"ba_sensory_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"ba_sensory_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"ba_sensory_anims\dummy_special"
	fps 1
}
