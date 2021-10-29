
// Stored modelname (without quotes): "D:\SDK2\SDK\Weapon Models\NS\turret_factory.mdl"
$modelname "b_chemlab.mdl"
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

$hbox 0 "Dummy01" -33.919998 -43.290001 0 33.880001 48.549999 55.720001
$hbox 0 "Dummy02" -26.09 -18.748449 -17.197174 25.486324 35.59 8.198435
$hbox 0 "D_piston06" -3.27 -3 -21.540001 3.29 2.97 23.129999
$hbox 0 "D_piston02" -3.23 -3 -20.27 3.32 2.97 23.129999
$hbox 0 "D_smoke_spray01" 0 -4.65 0 6.55 0 4.66
$hbox 0 "D_smoke_spray03" 0 -4.65 -6.73 4.1 0 0
$hbox 0 "D_smoke_spray04" -9.6 -4.65 0 0 0 11.2
$hbox 0 "D_smoke_spray05" 0 -4.65 0 5.49 0 9.65
$hbox 0 "D_smoke_spray06" -4.09 -4.65 -6.73 0 0 0
$hbox 0 "D_smoke_spray02" 0 -4.65 0 9.61 0 11.2
$hbox 0 "Dummy05" -0.84 -8.880445 -12.08 0.8 0 0
$hbox 0 "Dummy06" -0.84 -9.25 -3.15 0.8 0 0
$hbox 0 "Dummy03" -17.17 -9.21 -24.219999 16.49 9.29 22.83

$sequence "idle1" {
	"b_chemlab_anims\idle1"
	fps 1
}
$sequence "deploy" {
	"b_chemlab_anims\deploy"
	fps 8
}
$sequence "idle2" {
	"b_chemlab_anims\idle2"
	fps 1
}
$sequence "idle3" {
	"b_chemlab_anims\idle3"
	fps 8
}
$sequence "idle4" {
	"b_chemlab_anims\idle4"
	fps 6
}
$sequence "working" {
	"b_chemlab_anims\working"
	fps 8
	loop
}
$sequence "working2" {
	"b_chemlab_anims\working2"
	fps 14
	loop
}
$sequence "create" {
	"b_chemlab_anims\create"
	fps 8
}
$sequence "die" {
	"b_chemlab_anims\die"
	fps 15
}
