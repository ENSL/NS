
// Stored modelname (without quotes): "C:\3DMAX31\Scenes\natural_selection\lvl3\v_lvl3.mdl"
$modelname "v_lvl3.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "lvl3_ref"
}

$flags 0


$hbox 0 "Bone01" -78.884171 -63.030533 -9.028683 76.956375 57.093658 104.794922
$hbox 0 "Bone03" -34.720001 -50.580002 -6.692817 32.150002 1.58 6.05
$hbox 0 "Bone04" -38.529999 -17.700001 -36.869999 33.830002 5.87 1.31
$hbox 0 "Bone10" -16.110001 -67.067322 -14.52 0.05 0.14 12.58
$hbox 0 "Bone21" -11.309653 -24.077095 -12.148196 3.695606 0.267016 12.953183
$hbox 0 "Bone30" -14.335107 -27.98 -14.040939 10.9 0 14.962144
$hbox 0 "Bone92" -12.093898 -9.29 -14.007999 15.92 14.900893 13.613196
$hbox 0 "Bone88" -0.17 -3.73 -3.92 6.23 1.35 0
$hbox 0 "Bone93" -1.55 -3.73 -2.37 0.66 1.35 0
$hbox 0 "Bone95" 0 -3.85 -1.19 1.5 1.35 0.07
$hbox 0 "Bone99" 0 -8.58 -0.78 2.72 1.37 0.42
$hbox 0 "Bone101" -0.05 -9.04 -0.89 2.28 1.34 0.66
$hbox 0 "Bone103" -0.02 -9.03 -1.22 6.53 1.35 2.07
$hbox 0 "Bone41" -12.25 -84.269997 -11.25 10.6 0.18 11.34
$hbox 0 "Bone06" -34.259998 -54.797375 -2.52 30.42 1.09 5.85
$hbox 0 "Bone07" -33.119999 -11.11 -14.41 28.450001 2.6 34.93
$hbox 0 "Bone09" -0.08 -66.921028 -14.52 16.08 0.07 12.58
$hbox 0 "Bone57" -2.287431 -24.298706 -12.047101 12.665421 0.025398 13.043867
$hbox 0 "Bone72" -9.52 -29.09 -14.84 15.709197 0.018594 15.044899
$hbox 0 "Bone87" -14.58 -7.07 -14.252224 13.117925 14.517792 13.108405
$hbox 0 "Bone73" -6.17 -4.56 -3.24 0.23 2.58 0
$hbox 0 "Bone77" -1.43 -4.56 -1.27 0 2.58 0
$hbox 0 "Bone79" -1.55 -6.93 -1.12 0.01 2.58 0.01
$hbox 0 "Bone81" -1.56 -8.16 0 0 2.55 1.16
$hbox 0 "Bone83" -1.43 -4.58 0 0 2.55 1.26
$hbox 0 "Bone85" -6.53 -4.58 -0.81 0.01 2.55 1.78
$hbox 0 "Bone89" -9.92 -84.300003 -11.47 12.58 0.15 11.41

$sequence "idle_spike" {
	"v_lvl3_anims\idle_spike"
	fps 10
}
$sequence "idle_bite" {
	"v_lvl3_anims\idle_bite"
	fps 10
}
$sequence "idle_spore" {
	"v_lvl3_anims\idle_spore"
	fps 10
}
$sequence "spike_attack" {
	"v_lvl3_anims\spike_attack"
	fps 40
}
$sequence "bite_attack" {
	"v_lvl3_anims\bite_attack"
	fps 30
}
$sequence "spore_attack" {
	"v_lvl3_anims\spore_attack"
	fps 25
}
$sequence "scream_attack" {
	"v_lvl3_anims\scream_attack"
	fps 25
}
$sequence "spike->bite" {
	"v_lvl3_anims\spike-bite"
	fps 25
}
$sequence "spike->spore" {
	"v_lvl3_anims\spike-spore"
	fps 25
}
$sequence "bite->spore" {
	"v_lvl3_anims\bite-spore"
	fps 25
}
$sequence "bite->spike" {
	"v_lvl3_anims\bite-spike"
	fps 25
}
$sequence "spore->spike" {
	"v_lvl3_anims\spore-spike"
	fps 25
}
$sequence "spore->bite" {
	"v_lvl3_anims\spore-bite"
	fps 25
}
$sequence "draw" {
	"v_lvl3_anims\draw"
	fps 20
}
