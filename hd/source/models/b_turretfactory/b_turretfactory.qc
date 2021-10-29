
$modelname "b_turretfactory.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "turret_factory_reference"
}

$flags 0

$externaltextures


$attachment 0 "Dummy02" 0 -33 25

$hbox 0 "Dummy01" -30.919998 22.290001 26 30.880001 48.549999 37.720001
$hbox 0 "Dummy02" -4.09 5.748449 14.197174 4.486324 38.59 30.198435
$hbox 0 "D_piston06" -3.27 -3 -21.540001 3.29 2.97 23.129999
$hbox 0 "D_piston02" -3.23 -3 -20.27 3.32 2.97 23.129999
$hbox 0 "D_smoke_spray01" 9.7 -12.65 -29.5 -57.549999 14 11.66
$hbox 0 "D_smoke_spray06" 6.8 -15.65 -6.73 40.5 28.5 48.200001
$hbox 0 "D_smoke_spray02" -7 -23.85 -17.5 -41.549999 14 20.66
$hbox 0 "Dummy03" -17.17 -19.209999 -24.219999 16.49 9.29 24.83

$sequence "spawn" {
	"b_turretfactory_anims\spawn"
	fps 1
}
$sequence "deploy" {
	"b_turretfactory_anims\deploy"
	fps 8
}
$sequence "idle1" {
	"b_turretfactory_anims\idle1"
	fps 8
	loop
}
$sequence "idle2" {
	"b_turretfactory_anims\idle2"
	fps 6
	loop
}
$sequence "dummy_researching" {
	"b_turretfactory_anims\dummy_researching"
	fps 14
	loop
}
$sequence "active" {
	"b_turretfactory_anims\active"
	fps 14
	loop
}
$sequence "dummy_fire" {
	"b_turretfactory_anims\dummy_fire"
	fps 1
}
$sequence "dummy_take_damage" {
	"b_turretfactory_anims\dummy_take_damage"
	fps 30
}
$sequence "die_forward" {
	"b_turretfactory_anims\die_forward"
	fps 15
}
$sequence "die_left" {
	"b_turretfactory_anims\die_left"
	fps 15
}
$sequence "die_backward" {
	"b_turretfactory_anims\die_backward"
	fps 15
}
$sequence "die_right" {
	"b_turretfactory_anims\die_right"
	fps 15
}
$sequence "special_createturret" {
	"b_turretfactory_anims\special_createturret"
	fps 8
}
