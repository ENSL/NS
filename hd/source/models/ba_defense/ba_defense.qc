
// Stored modelname (without quotes): "h:\SDK2\SDK\Weapon Models\NS\ba_defense.mdl"
$modelname "ba_defense.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "upgrade_alien_defense_reference"
}

$flags 0

$externaltextures


$attachment 0 "Bone03" 0 0 0

$hbox 0 "Bone01" -0.49 -25.969999 -26.690001 12.720094 24.42 24.75
$hbox 0 "Bone02" 0 -13.83 -16.139999 12.76 14.54 14.45
$hbox 0 "Bone03" 0 -10.61 -11.6 27.85 10.86 9.87
$hbox 0 "Bone06" -10.43 -5.05 -8.18 26.370001 4.09 9.7
$hbox 0 "Bone09" -10.28 -4.51 -8.1 26.23 4.78 9.76
$hbox 0 "Bone12" -10.27 -3.46 -9.45 26.27 5.02 8.38
$hbox 0 "Bone15" -10.42 -4.45 -10.52 26.92 4.76 7.35
$hbox 0 "Bone18" -10.36 -5.2 -6.93 26.51 4.16 10.94

$sequence "spawn" {
	"ba_defense_anims\spawn"
	fps 12
}
$sequence "deploy" {
	"ba_defense_anims\deploy"
	fps 6
}
$sequence "idle1" {
	"ba_defense_anims\idle1"
	fps 5
	loop
}
$sequence "idle2" {
	"ba_defense_anims\idle2"
	fps 5
	loop
}
$sequence "researching" {
	"ba_defense_anims\researching"
	fps 1
}
$sequence "dummy_active" {
	"ba_defense_anims\dummy_active"
	fps 8
	loop
}
$sequence "dummy_fire" {
	"ba_defense_anims\dummy_fire"
	fps 1
}
$sequence "take_damage" {
	"ba_defense_anims\take_damage"
	fps 30
}
$sequence "die_forward" {
	"ba_defense_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"ba_defense_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"ba_defense_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"ba_defense_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"ba_defense_anims\dummy_special"
	fps 1
}
