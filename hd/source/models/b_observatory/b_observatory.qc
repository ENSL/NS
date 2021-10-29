
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\b_observatory.mdl"
$modelname "b_observatory.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "b_observatory_ref"
}

$flags 0

$externaltextures


$attachment 0 "Dummy10" 0 0 0

$hbox 0 "Dummy01" -6.513214 0 -4.28427 6.49351 42.910206 4.6
$hbox 0 "Dummy10" -1.287862 -21.179979 -6.832693 5.526575 0 0
$hbox 0 "RADAR FIN13" -4.78 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR FIN14" -4.78 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR FIN15" -4.78 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR FIN16" -4.78 -22.33 0 4.97 0 8.87
$hbox 0 "RADAR FIN01" -4.78 -22.33 0 4.97 0 8.88
$hbox 0 "RADAR FIN02" -4.78 -22.32 0 4.97 0 8.88
$hbox 0 "RADAR FIN03" -4.78 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN04" -4.78 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN05" -4.78 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN06" -4.78 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN07" -4.77 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN08" -4.77 -22.32 0 4.98 0 8.88
$hbox 0 "RADAR FIN09" -4.77 -22.33 0 4.98 0 8.88
$hbox 0 "RADAR FIN10" -4.77 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR FIN11" -4.77 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR FIN12" -4.78 -22.33 0 4.98 0 8.87
$hbox 0 "RADAR BASE" -4.55 -4.55 -4.69 4.55 4.55 2.04
$hbox 0 "POST02" -2.27 -2 -2.27 2.27 4.83 28.24
$hbox 0 "MAIN" -12.38 -21.629999 0 10.08 21.18 13.66
$hbox 0 "Dummy07" 0 -9.120917 -0.000001 6.660138 0 1.59484
$hbox 0 "Dummy09" -12.142116 0 0 0 16.770302 1.594842
$hbox 0 "leg2_02" -7.51 -21.07 -0.23 0 0 3.4
$hbox 0 "leg2_01" -4.62 -11.06 0 2.27 2.16 3.18
$hbox 0 "Dummy04" -6.590762 -9.23823 -1.590207 0 0 0
$hbox 0 "Dummy06" 0 0 -1.590205 12.450733 17.078957 0
$hbox 0 "leg1_02" -7.51 -21.07 -0.23 0 0 3.4
$hbox 0 "leg1_01" -4.62 -11.06 0 2.27 2.16 3.18
$hbox 0 "DOOR" -2.27 0 -5.46 0 25.959999 5.47

$sequence "spawn" {
	"b_observatory_anims\spawn"
	fps 1
}
$sequence "deploy" {
	"b_observatory_anims\deploy"
	fps 13
}
$sequence "idle1" {
	"b_observatory_anims\idle1"
	fps 2
	loop
}
$sequence "idle2" {
	"b_observatory_anims\idle2"
	fps 2
	loop
}
$sequence "researching" {
	"b_observatory_anims\researching"
	fps 14
	loop
}
$sequence "active" {
	"b_observatory_anims\active"
	fps 2
	loop
}
$sequence "dummy_fire" {
	"b_observatory_anims\dummy_fire"
	fps 1
}
$sequence "dummy_take_damage" {
	"b_observatory_anims\dummy_take_damage"
	fps 30
}
$sequence "die_forward" {
	"b_observatory_anims\die_forward"
	fps 15
}
$sequence "die_left" {
	"b_observatory_anims\die_left"
	fps 15
}
$sequence "die_backward" {
	"b_observatory_anims\die_backward"
	fps 15
}
$sequence "die_right" {
	"b_observatory_anims\die_right"
	fps 15
}
$sequence "dummy_special" {
	"b_observatory_anims\dummy_special"
	fps 8
}
