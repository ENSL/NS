
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\ba_offense.mdl"
$modelname "ba_offense.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "upgrade_alien_offense_reference"
}

$flags 0

$externaltextures


$attachment 0 "Bone04" 0 0 0


$hbox 0 "Bone02" 0 -11.05 -10.29 12.83 12.2 11.27
$hbox 0 "Bone04" -8.5 -18.58 -18.549999 23.67 18.35 22.08
$hbox 0 "Bone07" 0 -15.94 -4.24 18.879999 0.000001 1.11
$hbox 0 "Bone08" -0.04 -14.2 -6.18 16.07 3.71 9.52
$hbox 0 "Bone09" 0 -16.77 -6.13 26.93 2.04 2.36
$hbox 0 "Bone11" -14.93 -24.08 -26 29.25 4.79 28.76
$hbox 0 "Bone12" 0 -14.69 -1.41 19.120001 0.000005 5.48
$hbox 0 "Bone13" -0.38 -14.65 -1.94 17.209999 4.2 3.94
$hbox 0 "Bone14" 0 -17.870001 -1.84 27.77 2.59 4.58
$hbox 0 "Bone16" -13.4 -7.86 -15.17 22.284824 7.95 16.02
$hbox 0 "Bone17" 0 -2.32 -2.32 21.200001 0.000004 2.51
$hbox 0 "Bone18" 0 -14.79 -2.59 18.52 3.02 5.49
$hbox 0 "Bone19" 0 -8.18 -3.05 30.08 3.34 2.88

$sequence "spawn" {
	"ba_offense_anims\spawn"
	fps 12
}
$sequence "deploy" {
	"ba_offense_anims\deploy"
	fps 6
}
$sequence "idle1" {
	"ba_offense_anims\idle1"
	fps 5
	loop
}
$sequence "idle2" {
	"ba_offense_anims\idle2"
	fps 2
	loop
}
$sequence "researching" {
	"ba_offense_anims\researching"
	fps 15
}
$sequence "dummy_active" {
	"ba_offense_anims\dummy_active"
	fps 8
	loop
}
$sequence "fire" {
	"ba_offense_anims\fire"
	fps 24
}
$sequence "take_damage" {
	"ba_offense_anims\take_damage"
	fps 30
}
$sequence "die_forward" {
	"ba_offense_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"ba_offense_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"ba_offense_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"ba_offense_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"ba_offense_anims\dummy_special"
	fps 1
}
