
// Stored modelname (without quotes): "D:\SDK2\SDK\Weapon Models\NS\alien_resource_tower\alien_resource"
$modelname "ba_resource.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "alien_resource_tower_reference"
}

$flags 0

$externaltextures


$attachment 0 "Dummy10" -1 1 -2
$attachment 1 "Dummy08" 8 -8 0

$hbox 0 "Dummy03" -17.879999 -21.24 -17.959999 25.57 20.194477 20.5
$hbox 0 "Dummy02" -15.35 -7.729051 -16.73 15.19 6.6 14.14
$hbox 0 "Dummy04" -23.15 -2.99 -18.360001 17.93 3.72 20.280001
$hbox 0 "Dummy17" -15.33 -13.73 -4.32 0 0 4.16
$hbox 0 "Dummy18" -15.05 -11.66 -4.22 0 0 1.19
$hbox 0 "Dummy19" -12.13 -3.02 -2.01 0 2.57 2.11
$hbox 0 "Dummy12" -5.28 -15.58 0 3.12 0 12.79
$hbox 0 "Dummy13" -3.22 -6.33 0 2.61 0 11.919139
$hbox 0 "Dummy14" 0 -4.65 0 3.86 0 9.99
$hbox 0 "Dummy06" -13.53 -0.94 -27.549999 33.48 59.740002 24.32
$hbox 0 "Dummy09" -11.23 -2.21 -9.87 0 9.68 3.46
$hbox 0 "Dummy10" -8.03 -5.73 -8.41 3.6 8.3 5.17
$hbox 0 "Dummy07" -0.80625 -5.515213 -19.722242 37.602444 0 16.672321
$hbox 0 "Dummy22" 0 -13.31 -3.900901 17.299999 0 3.28
$hbox 0 "Dummy25" 0 0 -1.890498 25.309999 9.77 2.62
$hbox 0 "Dummy26" 0 0 -1.956202 16.809999 10.36 1.92
$hbox 0 "Dummy27" 0 0 -2.57 13.54 16.209999 2.24
$hbox 0 "Dummy23" -1.64 -28.987169 0 13.53 0 15.95
$hbox 0 "Dummy24" -2.59 -31.061268 -5.97 24.144669 0 4
$hbox 0 "Dummy30" -8.62 -8.22 -8.51 18.530001 2.5 11.6
$hbox 0 "Dummy32" 0 -18.707108 -6.18 19.136213 0 6.398983
$hbox 0 "Dummy33" 0 -6.07 -1.61 22.26 2.54 2.417311
$hbox 0 "Dummy34" 0 -9.73 -1.61 22.719999 0 1.2379
$hbox 0 "Dummy35" 0 -6.04 0 17.950001 0 4.05
$hbox 0 "Dummy37" 0 -24.317728 -10.59 21.200001 0 3
$hbox 0 "Dummy38" -5.6 -33.086845 -9.466918 15.735092 0 3.15
$hbox 0 "Dummy39" -6.81 -5.74 -10.15 24.139999 1.68 8.83
$hbox 0 "Dummy08" -18.858711 -38.305321 -39.275291 35.770271 35.787449 37.642731
$hbox 0 "Dummy41" -20.68 -26.190001 -0.13 19.040001 19.35 7.32
$hbox 0 "Dummy46" -14.19 0 0 15.21 15.05 13.3
$hbox 0 "Dummy42" -13.72 -8.21 0 0 20.08 13.12
$hbox 0 "Dummy48" -15.36 -12.07 0 18.48 0 11.04
$hbox 0 "Dummy44" 0 -13.63 0 17.18 15.38 13.08
$hbox 0 "Dummy53" -19.48 -19.370001 0 17.4 19.751888 7.011816
$hbox 0 "Dummy55" 0 -13.51 0 20.92 15.3 11.29
$hbox 0 "Dummy51" -8.33 -16.32 0 0 0 12.7
$hbox 0 "Dummy52" -16.030001 0 0 17.309999 15.83 13.9
$hbox 0 "Dummy57" -12.07 -15.99 0 21.98 0.47 12.48
$hbox 0 "Dummy59" -10.16 -4.74 -8.75 10.38 0 8.83
$hbox 0 "Dummy62" -9.99 -3.11 -8.79 10.71 0 8.71
$hbox 0 "Dummy64" -9.21 -5.9 -8.71 9.67 0 8.77
$hbox 0 "Dummy66" -11.81 -3.53 -8.65 8.98 0 8.97
$hbox 0 "Dummy68" -2.32 -4 -7.86 0 0 8.23

$sequence "spawn" {
	"ba_resource_anims\spawn"
	fps 12
}
$sequence "deploy" {
	"ba_resource_anims\deploy"
	fps 6
}
$sequence "idle1" {
	"ba_resource_anims\idle1"
	fps 5
	loop
}
$sequence "idle2" {
	"ba_resource_anims\idle2"
	fps 5
	loop
}
$sequence "dummy_researching" {
	"ba_resource_anims\dummy_researching"
	fps 1
}
$sequence "active" {
	"ba_resource_anims\active"
	fps 8
	loop
}
$sequence "dummy_fire" {
	"ba_resource_anims\dummy_fire"
	fps 1
}
$sequence "take_damage" {
	"ba_resource_anims\take_damage"
	fps 30
}
$sequence "die_forward" {
	"ba_resource_anims\die_forward"
	fps 17
}
$sequence "die_left" {
	"ba_resource_anims\die_left"
	fps 17
}
$sequence "die_backward" {
	"ba_resource_anims\die_backward"
	fps 17
}
$sequence "die_right" {
	"ba_resource_anims\die_right"
	fps 17
}
$sequence "dummy_special" {
	"ba_resource_anims\dummy_special"
	fps 1
}
