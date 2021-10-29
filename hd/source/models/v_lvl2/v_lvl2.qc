
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\lvl2\v_lvl2.mdl"
$modelname "v_lvl2.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "lvl2_ref"
}

$flags 0


$hbox 0 "Bone01" -226.860001 -238.789993 -67.529999 231.020004 0 202.929993
$hbox 0 "Bone06" -155.690002 -109.169998 -107.720001 0 75.190002 68.07
$hbox 0 "Bone07" -74.040001 -178.779999 -49.470001 33.009998 0 50.060001
$hbox 0 "Bone03" -155.559998 -110.441917 -68.029999 0 75.139999 107.769997
$hbox 0 "Bone04" -76.389999 -177.220001 -50.02 34.689999 0 49.52

$sequence "idle1" {
	"v_lvl2_anims\idle1"
	fps 15
}
$sequence "idle2" {
	"v_lvl2_anims\idle2"
	fps 10
}
$sequence "idle_web" {
	"v_lvl2_anims\idle_web"
	fps 10
}
$sequence "spit_attack" {
	"v_lvl2_anims\spit_attack"
	fps 25
}
$sequence "spin_web" {
	"v_lvl2_anims\spin_web"
	fps 25
}
$sequence "spit->web" {
	"v_lvl2_anims\spit-web"
	fps 25
}
$sequence "spit->build" {
	"v_lvl2_anims\spit-build"
	fps 15
}
$sequence "web->spit" {
	"v_lvl2_anims\web-spit"
	fps 25
}
$sequence "web->build" {
	"v_lvl2_anims\web-build"
	fps 15
}
$sequence "build->spit" {
	"v_lvl2_anims\build-spit"
	fps 15
}
$sequence "build->web" {
	"v_lvl2_anims\build-web"
	fps 15
}
$sequence "draw" {
	"v_lvl2_anims\draw"
	fps 15
}
