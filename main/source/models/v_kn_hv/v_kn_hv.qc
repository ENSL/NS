
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\knife\v_knife_HV.mdl"
$modelname "v_kn_hv.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "knife_ref_HV"
}

$flags 0


$hbox 0 "Bone26" -1.8 -2.14 -0.47 1.82 1.173586 1.17
$hbox 0 "Bone27" -2.14 -1.72 -0.4 0 0 1.27
$hbox 0 "Bone25" -2.97 -4.59 -2.4 3.32 11.039249 2.6
$hbox 0 "Bone03" -1.8 -2.14 -1.18 1.83 1.174229 0.46
$hbox 0 "Bone04" -2.33 -1.36 -1.26 0 0 0.16
$hbox 0 "Bone47" -2.98 -4.59 -2.61 3.32 10.49 2.4

$sequence "idle1" {
	"v_kn_hv_anims\idle1"
	fps 10
}
$sequence "idle2" {
	"v_kn_hv_anims\idle2"
	fps 10
}
$sequence "attack1" {
	"v_kn_hv_anims\attack1"
	fps 25
}
$sequence "attack2" {
	"v_kn_hv_anims\attack2"
	fps 25
}
$sequence "draw" {
	"v_kn_hv_anims\draw"
	fps 18
}
