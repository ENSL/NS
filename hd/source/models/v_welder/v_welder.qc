
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\welder\v_welder.mdl"
$modelname "v_welder.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "welder_ref"
}

$flags 0


$attachment 0 "Bone52" 0 -5 0


$hbox 0 "Bone26" -2.214566 -3.690814 -1.07 1.82 1.35 8.493554
$hbox 0 "Bone27" -1.986841 -1.07 0 0.16 0.57 1.25
$hbox 0 "Bone28" -0.506463 -1.4 -0.92 0.58 0.07 0.46
$hbox 0 "Bone25" -2.2 -0.03 -1.98 2.06 10.52 1.44
$hbox 0 "Bone02" -9.506847 -13.94 -2.82 2.2 6.58 2.57
$hbox 0 "Bone51" -0.98 -12.1 -1.11 0.92 0.08 0.72
$hbox 0 "Bone52" -0.63 -3.44 -0.91 0.83 0 0.53
$hbox 0 "Bone49" -0.42 -10.34 -2.79 3.76 2.72 2.38
$hbox 0 "Bone03" -1.55 -2.14 -1 1.82 1.35 1.06
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0.83 0.16
$hbox 0 "Bone47" -2.2 -0.03 -1.44 2.05 10.52 1.97

$sequence "idle" {
	"v_welder_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_welder_anims\idle2"
	fps 8
}
$sequence "weld" {
	"v_welder_anims\weld"
	{ event 5002 0 "2" }
	fps 20
}
$sequence "draw" {
	"v_welder_anims\draw"
	fps 20
}
