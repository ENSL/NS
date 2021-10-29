
// Stored modelname (without quotes): "h:\SDK2\SDK\Weapon Models\NS\ba_movement.mdl"
$modelname "ba_movement.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "upgrade_alien_movement_reference"
}

$flags 0

$externaltextures


$attachment 0 "Bone03" 0 0 0


$hbox 0 "Bone01" -0.09 -29.690001 -24.65 13.328173 26.82 24.719999
$hbox 0 "Bone02" 0 -14.33 -14.16 12.667274 16.27 14.21
$hbox 0 "Bone03" 0 -12.82 -15.95 27.559999 13.4 14.91
$hbox 0 "Bone07" -6.44 -12.24 -9.36 24.110001 4.2 13.03
$hbox 0 "Bone11" -6.92 -4.39 -12.6 23.5 12.05 9.75

$sequence "spawn" {
	"ba_movement_anims\spawn"
	fps 12
}
$sequence "deploy" {
	"ba_movement_anims\deploy"
	fps 6
}
$sequence "idle1" {
	"ba_movement_anims\idle1"
	fps 2
	loop
}
$sequence "idle2" {
	"ba_movement_anims\idle2"
	fps 5
	loop
}
$sequence "researching" {
	"ba_movement_anims\researching"
	fps 2
}
$sequence "dummy_active" {
	"ba_movement_anims\dummy_active"
	fps 8
	loop
}
$sequence "dummy_fire" {
	"ba_movement_anims\dummy_fire"
	fps 1
}
$sequence "take_damage" {
	"ba_movement_anims\take_damage"
	fps 30
}
$sequence "die_forward" {
	"ba_movement_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"ba_movement_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"ba_movement_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"ba_movement_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"ba_movement_anims\dummy_special"
	fps 1
}
