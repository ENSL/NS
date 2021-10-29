
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\lvl5\v_lvl5.mdl"
$modelname "v_lvl5.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "lvl5_ref"
}

$flags 0


$hbox 0 "Bone31" -84.209999 -64.529999 -59.709999 86.82 0 38.740002
$hbox 0 "Bone11" -9.15 -24.098656 -6.89 8.6 0 7.09
$hbox 0 "Bone12" -9.15 -21.93 -6.85 8.6 0 7.13
$hbox 0 "Bone13" -9.78 -19.406517 -7.14 9.24 0 7.83
$hbox 0 "Bone14" -10.39 -19.129999 -7.68 9.85 0 8.25
$hbox 0 "Bone15" -8.44 -19.351028 -6.37 7.89 0 6.49
$hbox 0 "Bone16" -11.18 -26.93 -9.18 11.79 0 8.45
$hbox 0 "Bone17" -8.35 -11.07 -7.78 7.8 0 7.69
$hbox 0 "Bone19" -8.35 -8.5 -29.24 7.8 12.3 8.42
$hbox 0 "Bone03" -8.81 -23.832542 -7.28 8.94 0 6.7
$hbox 0 "Bone04" -8.81 -21.98 -7.05 8.94 0 6.93
$hbox 0 "Bone05" -9.45 -19.431664 -7.32 9.57 0 7.66
$hbox 0 "Bone06" -10.06 -19.346481 -8.09 10.18 0 7.84
$hbox 0 "Bone07" -8.1 -19.129999 -6.5 8.23 0 6.36
$hbox 0 "Bone08" -10.85 -27.17 -9.49 12.13 0 8.14
$hbox 0 "Bone09" -8.01 -11.29 -7.58 8.14 0 7.89
$hbox 0 "Bone10" -2.698035 -15.791092 0 0 0 1.570702
$hbox 0 "Bone01" -5.32 -11.37 -29.16 10.84 9.44 8.51
$hbox 0 "Bone21" -9.17 -23.886158 -6.95 8.58 0 7.03
$hbox 0 "Bone22" -9.17 -22.05983 -7.03 8.58 0 6.95
$hbox 0 "Bone23" -9.81 -19.19986 -7.68 9.22 0 7.3
$hbox 0 "Bone24" -10.41 -19.155869 -8.18 9.82 0 7.75
$hbox 0 "Bone25" -8.46 -19.200001 -6.58 7.87 0 6.28
$hbox 0 "Bone26" -11.21 -27.049999 -9.7 11.77 0 7.93
$hbox 0 "Bone27" -8.37 -10.65 -7.96 7.78 0 7.51
$hbox 0 "Bone29" -8.37 -8.75 -29.26 7.78 12.06 8.41
$hbox 0 "Bone38" -46.099998 -149.589996 -31.360001 42.59 16.610001 67.720001
$hbox 0 "Bone32" -170.805023 -89.057571 -8.839775 175.365479 0 0
$hbox 0 "Bone34" -45.93 -214.080002 -32.220001 52.700001 19.42 145.410004
$hbox 0 "Bone33" -53.650002 -213.419998 -32.549999 42.75 20.08 165.699997

$sequence "gore_idle" {
	"v_lvl5_anims\gore_idle"
	fps 10
}
$sequence "charge_idle" {
	"v_lvl5_anims\charge_idle"
	fps 10
}
$sequence "paralysis_idle" {
	"v_lvl5_anims\paralysis_idle"
	fps 10
}
$sequence "stomp_idle" {
	"v_lvl5_anims\stomp_idle"
	fps 10
}
$sequence "devour_idle" {
	"v_lvl5_anims\devour_idle"
	fps 10
}
$sequence "gore->charge" {
	"v_lvl5_anims\gore-charge"
	fps 25
}
$sequence "gore->paralysis" {
	"v_lvl5_anims\gore-paralysis"
	fps 25
}
$sequence "gore->stomp" {
	"v_lvl5_anims\gore-stomp"
	fps 25
}
$sequence "gore->devour" {
	"v_lvl5_anims\gore-devour"
	fps 25
}
$sequence "charge->gore" {
	"v_lvl5_anims\charge-gore"
	fps 25
}
$sequence "charge->paralysis" {
	"v_lvl5_anims\charge-paralysis"
	fps 25
}
$sequence "charge->stomp" {
	"v_lvl5_anims\charge-stomp"
	fps 25
}
$sequence "charge->devour" {
	"v_lvl5_anims\charge-devour"
	fps 25
}
$sequence "paralysis->gore" {
	"v_lvl5_anims\paralysis-gore"
	fps 25
}
$sequence "paralysis->charge" {
	"v_lvl5_anims\paralysis-charge"
	fps 25
}
$sequence "stomp->gore" {
	"v_lvl5_anims\stomp-gore"
	fps 25
}
$sequence "stomp->charge" {
	"v_lvl5_anims\stomp-charge"
	fps 25
}
$sequence "stomp->devour" {
	"v_lvl5_anims\stomp-devour"
	fps 25
}
$sequence "devour->gore" {
	"v_lvl5_anims\devour-gore"
	fps 25
}
$sequence "devour->charge" {
	"v_lvl5_anims\devour-charge"
	fps 25
}
$sequence "devour->stomp" {
	"v_lvl5_anims\devour-stomp"
	fps 25
}
$sequence "gore_attack" {
	"v_lvl5_anims\gore_attack"
	fps 20
}
$sequence "charge_attack" {
	"v_lvl5_anims\charge_attack"
	fps 30
}
$sequence "paralysis_attack" {
	"v_lvl5_anims\paralysis_attack"
	fps 25
}
$sequence "stomp_attack" {
	"v_lvl5_anims\stomp_attack"
	fps 20
}
$sequence "devour_attack" {
	"v_lvl5_anims\devour_attack"
	fps 20
}
$sequence "devour_full" {
	"v_lvl5_anims\devour_full"
	fps 10
}
$sequence "draw" {
	"v_lvl5_anims\draw"
	fps 20
}
