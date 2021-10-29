
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\lvl1\v_lvl1.mdl"
$modelname "v_lvl1.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "lvl1_ref"
}

$flags 0


$hbox 0 "Bone01" -103.068939 -45.662361 -3.234768 105.367386 0 121.113564
$hbox 0 "Bone03" -52.060001 -90.739059 -11.14 51.23 2.09 2.920136
$hbox 0 "Bone04" -52.060001 -45.25 -64.830002 51.23 28.66 1.99
$hbox 0 "Bone06" -47.57 -95.1772 -23.355648 46.099998 4.23 10.53
$hbox 0 "Bone07" -46.799999 -31.15 -1.23 46.880001 35.400002 40.700001
$hbox 0 "Bone13" -30.200001 -77.56147 -73.139999 32.470001 0.62 17.52
$hbox 0 "Bone14" -22.370001 -77.160004 -46.130001 17.25 0 65.269997
$hbox 0 "Bone15" -20.700001 -64.389999 -110.099998 19.030001 1.91 1.85
$hbox 0 "Bone09" -39.360001 -75.400002 -64.160004 25.18 0.75 40.630001
$hbox 0 "Bone10" -20.59 -79.220001 -39.380001 26.08 0 84.482712
$hbox 0 "Bone11" -23.85 -100.07 -76.43 18.1 0.34 0.99

$sequence "idle1_bite" {
	"v_lvl1_anims\idle1_bite"
	fps 10
}
$sequence "idle2_bite" {
	"v_lvl1_anims\idle2_bite"
	fps 9
}
$sequence "pick_teeth" {
	"v_lvl1_anims\pick_teeth"
	fps 9
}
$sequence "leap_attack" {
	"v_lvl1_anims\leap_attack"
	fps 25
}
$sequence "bite_attack" {
	"v_lvl1_anims\bite_attack"
	fps 40
}
$sequence "draw" {
	"v_lvl1_anims\draw"
	fps 20
}
