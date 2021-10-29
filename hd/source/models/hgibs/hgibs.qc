
// Stored modelname (without quotes): "valve/models/Hgibs.mdl"
$modelname "hgibs.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "parts"
{
	studio "HGIB_Skull1"
	studio "HGIB_HMeat1"
	studio "HGIB_Guts1"
	studio "HGIB_Lung1"
	studio "HGIB_Legbone1"
	studio "HGIB_B_Gib1"
	studio "HGIB_B_Bone1"
	studio "Flesh1"
	studio "Flesh2"
	studio "Flesh3"
	studio "Flesh4"
}

$flags 0


$hbox 0 "Dummy01" -6.99694 -1.476532 -2.9109 3.109554 4.272732 1.216496
$hbox 0 "Skull1" 0 -3.01 -6.11 11.23 8.98 6.11
$hbox 0 "Object13" -6.84 -10.14 -4.81 6.84 10.14 4.81
$hbox 0 "Object11" -3.27 -14.97 -3.23 3.27 14.97 3.23
$hbox 0 "Lung01" -6.69 -13.08 -5.11 7.02 11.97 5.23
$hbox 0 "Leg_Bone01" -5.31 -19.299999 -3.12 5.31 19.299999 3.12
$hbox 0 "Big_Gib01" -5.74 -13.66 -5.82 5.74 13.66 5.81
$hbox 0 "Spinal01" -8.77 -18.469999 -7.48 4.92 12.39 5.18
$hbox 0 "Object03" -6.19 -12.76 -1.67 6.19 12.76 1.67
$hbox 0 "Object12" -7.62 -16.9 -3.33 7.62 16.9 3.33
$hbox 0 "Object07" -3.86 -7.33 -4.18 3.86 7.33 4.18
$hbox 0 "Object16" -4.68 -17.389999 -1.44 4.68 17.389999 1.44

$sequence "idle1" {
	"hgibs_anims\idle1"
	fps 30
}
