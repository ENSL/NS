
// Stored modelname (without quotes): "E:\Projects\Natural Selection\3.2\models\pg\b_phasegate\b_phaseg"
$modelname "b_phasegate.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "ref_phasegate"
}

$flags 0


$attachment 0 "Bone01" 0 0 0

$hbox 0 "PHASEGATE" -62.990002 -58.02 -5 57.209999 55.93 13
$hbox 0 "PHASEGATE" -24 -43 13 5 -57 29

$sequence "idle_phasegate" {
	"b_phasegate_anims\idle_phasegate"
	ACT_IDLE 2
	{ event 7000 0 "PhaseGateIdle" }
	fps 1
	loop
}
