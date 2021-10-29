
$modelname "b_siege.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "siege_turret_reference"
}

$flags 0

$externaltextures


$attachment 0 "D_siege_barrel02" 0 0 29
$attachment 1 "Dummy04_new" -10 0 -1


$hbox 0 "Dummy01" -17.000000 -28.160000 0.000000 18.000000 27.459999 10.030000
$hbox 0 "Dummy01" -10.000000 -15.160000 9.000000 10.000000 22.459999 18.030001
$hbox 0 "Dummy03" -7.500000 -11.000000 -9.000000 7.500000 -4.500000 18.500000
$hbox 0 "Dummy02" -8.900000 6.100000 -24.520000 7.490000 17.000000 22.959999
$hbox 0 "Dummy04_new" -3.610000 -20.650000 3.370000 3.680000 -6.360000 13.900000
//$hbox 0 "D_siege_barrel02" -2.52 -2.56 -5.87 2.59 2.55 27.33

$controller 0 "Dummy02" YR 0 360
$controller 1 "Dummy04_new" XR 0 0

$sequence "idle_off" {
	"b_siege_anims\idle_off"
	fps 1
}
$sequence "fire" {
	"b_siege_anims\fire"
	{ event 5001 0 "21" }
	{ event 5001 2 "21" }
	{ event 5001 4 "31" }
	{ event 5001 6 "21" }
	{ event 5001 8 "21" }
	{ event 5001 10 "31" }
	{ event 5001 12 "21" }
	{ event 5001 14 "21" }
	fps 30
	loop
}
$sequence "spin" {
	"b_siege_anims\spin"
	fps 20
	loop
}
$sequence "deploy" {
	"b_siege_anims\deploy"
	fps 8
}
$sequence "retire" {
	"b_siege_anims\retire"
	fps 10
}
$sequence "die" {
	"b_siege_anims\die"
	fps 18
}
