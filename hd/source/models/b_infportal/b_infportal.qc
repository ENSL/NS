
// Stored modelname (without quotes): "c:\backup\SDK2\SDK\Weapon Models\NS\b_infportal.mdl"
$modelname "b_infportal.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "infantry_gate_reference"
}

$flags 0

$externaltextures


$attachment 0 "D_infantry_gate01" 0 0.1 0


$hbox 0 "D_infantry_gate02" -30.940001 -29.98 -1.04 30.940001 31.26 13.45
$hbox 0 "D_infantry_gate05" -4.78 -7.46 -4.05 5.03 2.2 7.12
$hbox 0 "D_infantry_gate03" -1.19 -4.96 -4.34 8.4 4.92 6.82

$sequence "dummy_spawn" {
	"b_infportal_anims\dummy_spawn"
	fps 12
}
$sequence "dummy_deploy" {
	"b_infportal_anims\dummy_deploy"
	fps 6
}
$sequence "idle1" {
	"b_infportal_anims\idle1"
	fps 1
}
$sequence "dummy_idle2" {
	"b_infportal_anims\dummy_idle2"
	fps 5
	loop
}
$sequence "dummy_researching" {
	"b_infportal_anims\dummy_researching"
	fps 1
}
$sequence "dummy_active" {
	"b_infportal_anims\dummy_active"
	fps 8
	loop
}
$sequence "dummy_fire" {
	"b_infportal_anims\dummy_fire"
	fps 1
}
$sequence "dummy_take_damage" {
	"b_infportal_anims\dummy_take_damage"
	fps 30
}
$sequence "dummy_die_forward" {
	"b_infportal_anims\dummy_die_forward"
	fps 17
}
$sequence "dummy_die_left" {
	"b_infportal_anims\dummy_die_left"
	fps 17
}
$sequence "dummy_die_backward" {
	"b_infportal_anims\dummy_die_backward"
	fps 17
}
$sequence "dummy_die_right" {
	"b_infportal_anims\dummy_die_right"
	fps 17
}
$sequence "dummy_special" {
	"b_infportal_anims\dummy_special"
	fps 1
}
