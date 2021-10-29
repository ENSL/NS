
// Stored modelname (without quotes): "H:\SDK2\SDK\Weapon Models\NS\b_sentry.mdl"
$modelname "b_sentry.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "sentry01_reference"
}

$flags 0

$externaltextures


$attachment 0 "Dummy04" 0 0 9
$attachment 1 "Dummy04" 0 0 0

$hbox 0 "Dummy01" -8.84 -14.610001 0 8.86 8.33 42.061226
$hbox 0 "Dummy02" -9.47 -0.94 -9.47 9.53 16.219999 3.09
$hbox 0 "D_ammochain01" -1.88 -1.308814 -3.82927 1.94 0.75 0.16
$hbox 0 "D_ammochain02" -1.88 -0.49 -4.24774 1.94 0.55 0.18
$hbox 0 "D_ammochain03" -1.88 -0.56 -2.938813 1.94 2.914068 0.09
$hbox 0 "D_ammochain04" -1.88 -0.24 -1.182028 1.94 4.369955 0.49
$hbox 0 "D_ammochain05" -1.88 -0.24 -0.5 1.94 4.397474 1.540223
$hbox 0 "D_ammochain06" -1.88 -0.49 -0.37 1.94 2.199258 3.237689
$hbox 0 "D_ammochain07" -1.88 -0.48 -0.02 1.94 0.65 4.508045
$hbox 0 "Dummy04" -5.47 -5.11 -8.93 4.94 6.79 11.963336
$hbox 0 "D_sentry_barrel" -3.06 -3.14 -4.4 3.23 3.14 0

$controller 0 "Dummy02" ZR 0 360
$controller 1 "Dummy04" XR -60 60

$sequence "idle_off" {
	"b_sentry_anims\idle_off"
	fps 1
}
$sequence "fire" {
	"b_sentry_anims\fire"
	{ event 5001 0 "21" }
	{ event 5001 2 "21" }
	{ event 5001 4 "31" }
	{ event 5001 6 "21" }
	{ event 5001 8 "21" }
	{ event 5001 10 "31" }
	{ event 5001 12 "21" }
	{ event 5001 14 "21" }
	fps 20
	loop
}
$sequence "spin" {
	"b_sentry_anims\spin"
	fps 10
	loop
}
$sequence "deploy" {
	"b_sentry_anims\deploy"
	fps 20
}
$sequence "retire" {
	"b_sentry_anims\retire"
	fps 10
}
$sequence "die" {
	"b_sentry_anims\die"
	fps 15
}
