
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\welder\v_welder_hv.mdl"
$modelname "v_welder_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "welder_ref_hv"
}

$flags 0


$attachment 0 "Bone52" 0 -5 0


$hbox 0 "Bone26" -14.244369 -2.14 -4.632631 1.82 1.173586 1.17
$hbox 0 "Bone27" -2.14 -1.72 -0.4 0 0 1.27
$hbox 0 "Bone25" -2.97 -4.59 -2.4 3.32 10.49 2.6
$hbox 0 "Bone02" -9.506847 -13.94 -2.82 2.2 6.58 2.57
$hbox 0 "Bone51" -0.98 -12.1 -1.11 0.92 0.08 0.72
$hbox 0 "Bone52" -0.63 -3.44 -0.91 0.83 0 0.53
$hbox 0 "Bone49" -0.42 -10.34 -2.79 3.76 2.72 2.38
$hbox 0 "Bone50" 0 0 -15.946157 5.350159 10.759996 0
$hbox 0 "Bone03" -1.8 -2.14 -1.18 1.83 1.174229 0.46
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.16
$hbox 0 "Bone47" -2.98 -4.59 -2.61 3.32 10.49 2.4

$sequence "idle" {
	"v_welder_hv_anims\idle"
	fps 20
}
$sequence "idle2" {
	"v_welder_hv_anims\idle2"
	fps 8
}
$sequence "weld" {
	"v_welder_hv_anims\weld"
	{ event 5002 0 "2" }
	fps 20
}
$sequence "draw" {
	"v_welder_hv_anims\draw"
	fps 25
}
