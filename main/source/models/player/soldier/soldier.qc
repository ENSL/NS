
$modelname "player/soldier/soldier.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "marine"
{
	studio "marine_ref"
	studio "marine_jetpack"
}

$flags 0

$texturegroup "skinfamilies"
{
	{ "Marine_final.bmp"    }
	{ "Marine_black_01.bmp" }
}


$attachment 0 "Bip01 R Hand" 11.4 1.6 6.4
$attachment 1 "Bip01 R Hand" 23 2 7.35
$attachment 2 "Bip01 R Hand" 30 2 9.5


$hbox 0 "Bip01 Pelvis" -8 -5.56 -9.16 3.79 7.55 9.21
$hbox 0 "Bip01 L Thigh" 0 -4.24 -4.38 21.666876 5.42 4.54
$hbox 0 "Bip01 L Calf" -0.45 -4.86 -3.85 17.600178 4.25 4.11
$hbox 0 "Bip01 L Foot" 0 -3.17 -2.15 5.1 5.16 2.59
$hbox 0 "Bip01 L Toe0" 0 -0.17 -2.17 4.51 2.09 2.8
$hbox 0 "Bip01 R Thigh" 0 -4.16 -4.08 21.666874 5.5 4.95
$hbox 0 "Bip01 R Calf" -0.46 -4.77 -3.65 17.600176 4.34 4.3
$hbox 0 "Bip01 R Foot" 0 -3.11 -2.12 5.1 5.21 2.62
$hbox 0 "Bip01 R Toe0" 0 -0.17 -2.34 4.57 2.09 2.63
$hbox 0 "Bip01 Spine1" 0 -2.76 -7.3 3.831985 7.17 6.84
$hbox 0 "Bip01 Spine2" 0 -5.1 -9.76 3.832064 9.54 9.29
$hbox 0 "Bip01 Spine3" -11.06 -15.4 -9.31 8.87 7.77 8.95
$hbox 0 "Bip01 Neck" 0 -1.26 -2.64 4.837411 0.67 2.09
$hbox 0 "Bip01 Head" -1.67 -4.76 -3.96 8.64 5.02 4.48
$hbox 0 "Bip01 L Clavicle" 0 -6.39 -0.61 6.683292 2.48 4.14
$hbox 0 "Bip01 L UpperArm" 0 -5.21 -2.59 10.32 3.1 3.75
$hbox 0 "Bip01 L Forearm" 0 -4.44 -2.63 10.959037 3.1 2.85
$hbox 0 "Bip01 L Hand" -0.16 -1.98 -1.9 3.387688 1.44 1.54
$hbox 0 "Bip01 L Finger0" 0 -0.96 -0.84 2.325842 0.3 0.52
$hbox 0 "Bip01 L Finger01" -0.29 -0.82 -0.81 1.81 0.24 0.36
$hbox 0 "Bip01 L Finger1" -0.21 -0.79 -2.06 2.18939 0.93 1.82
$hbox 0 "Bip01 L Finger11" 0 -0.41 -1.87 2.29 0.93 2
$hbox 0 "Bip01 R Clavicle" 0 -6.47 -4.16 6.683292 2.41 0.58
$hbox 0 "Bip01 R UpperArm" 0 -5.34 -3.76 10.32 2.98 2.57
$hbox 0 "Bip01 R Forearm" 0 -4.6 -2.9 10.959036 3.05 2.64
$hbox 0 "Bip01 R Hand" 0 -1.88 -1.32 3.387687 1.16 2.11
$hbox 0 "Bip01 R Finger0" 0 -1.06 -0.72 2.325842 0.2 0.71
$hbox 0 "Bip01 R Finger01" 0 -1.02 -0.6 2.24 0.05 0.57
$hbox 0 "Bip01 R Finger1" -0.26 -0.99 -1.58 2.49 0.73 2.29
$hbox 0 "Bip01 R Finger11" 0 -0.65 -1.76 2.66 0.69 2.12

$controller 0 "Bip01 Spine" XR -22.5 22.5
$controller 1 "Bip01 Spine1" XR -22.5 22.5
$controller 2 "Bip01 Spine2" XR -22.5 22.5
$controller 3 "Bip01 Spine3" XR -22.5 22.5

$sequence "look_idle" {
	"soldier_anims\look_idle"
	ACT_IDLE 1
	fps 30
	loop
}
$sequence "idle1" {
	"soldier_anims\idle1"
	ACT_IDLE 2
	fps 30
	loop
}
$sequence "crouch_idle" {
	"soldier_anims\crouch_idle"
	ACT_CROUCHIDLE 1
	fps 1
	loop
}
$sequence "walk" {
	"soldier_anims\walk"
	ACT_WALK 1
	fps 28
	loop
	LX
}
$sequence "run" {
	"soldier_anims\run"
	ACT_RUN 1
	fps 25
	loop
	LX
}
$sequence "crawl" {
	"soldier_anims\crawl"
	ACT_CROUCH 1
	fps 23
	loop
	LX
}
$sequence "jump" {
	"soldier_anims\jump"
	ACT_HOP 1
	fps 20
}
$sequence "swim" {
	"soldier_anims\swim"
	ACT_SWIM 1
	fps 15
	loop
}
$sequence "treadwater" {
	"soldier_anims\treadwater"
	ACT_HOVER 1
	fps 15
	loop
}
$sequence "communicate" {
	"soldier_anims\communicate"
	fps 30
	loop
}
$sequence "voicecomm" {
	"soldier_anims\voicecomm"
	fps 30
	loop
}
$sequence "lmg_look" {
	"soldier_anims\lmg_look_blend01"
	"soldier_anims\lmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "pistol_look" {
	"soldier_anims\pistol_look_blend01"
	"soldier_anims\pistol_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "knife_look" {
	"soldier_anims\knife_look_blend01"
	"soldier_anims\knife_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "shotgun_look" {
	"soldier_anims\shotgun_look_blend01"
	"soldier_anims\shotgun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "grenade_look" {
	"soldier_anims\grenade_look_blend01"
	"soldier_anims\grenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "hmg_look" {
	"soldier_anims\hmg_look_blend01"
	"soldier_anims\hmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "welder_look" {
	"soldier_anims\welder_look_blend01"
	"soldier_anims\welder_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "grenadegun_look" {
	"soldier_anims\grenadegun_look_blend01"
	"soldier_anims\grenadegun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "tripmine_look" {
	"soldier_anims\tripmine_look_blend01"
	"soldier_anims\tripmine_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "alien_look" {
	"soldier_anims\alien_look"
	fps 20
	loop
}
$sequence "lmg_fire" {
	"soldier_anims\lmg_fire_blend01"
	"soldier_anims\lmg_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
}
$sequence "pistol_fire" {
	"soldier_anims\pistol_fire_blend01"
	"soldier_anims\pistol_fire_blend02"
	blend XR -90 90
	{ event 5001 1 "30" }
	fps 30
}
$sequence "knife_fire" {
	"soldier_anims\knife_fire_blend01"
	"soldier_anims\knife_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "shotgun_fire" {
	"soldier_anims\shotgun_fire_blend01"
	"soldier_anims\shotgun_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
}
$sequence "grenade_fire" {
	"soldier_anims\grenade_fire_blend01"
	"soldier_anims\grenade_fire_blend02"
	blend XR -90 90
	fps 17
}
$sequence "hmg_fire" {
	"soldier_anims\hmg_fire_blend01"
	"soldier_anims\hmg_fire_blend02"
	blend XR -90 90
	{ event 5021 1 "40" }
	fps 30
}
$sequence "welder_fire" {
	"soldier_anims\welder_fire_blend01"
	"soldier_anims\welder_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "grenadegun_fire" {
	"soldier_anims\grenadegun_fire_blend01"
	"soldier_anims\grenadegun_fire_blend02"
	blend XR -90 90
	fps 17
}
$sequence "tripmine_fire" {
	"soldier_anims\tripmine_fire_blend01"
	"soldier_anims\tripmine_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "crouch_lmg_look" {
	"soldier_anims\crouch_lmg_look_blend01"
	"soldier_anims\crouch_lmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_pistol_look" {
	"soldier_anims\crouch_pistol_look_blend01"
	"soldier_anims\crouch_pistol_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_knife_look" {
	"soldier_anims\crouch_knife_look_blend01"
	"soldier_anims\crouch_knife_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_shotgun_look" {
	"soldier_anims\crouch_shotgun_look_blend01"
	"soldier_anims\crouch_shotgun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_grenade_look" {
	"soldier_anims\crouch_grenade_look_blend01"
	"soldier_anims\crouch_grenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_hmg_look" {
	"soldier_anims\crouch_hmg_look_blend01"
	"soldier_anims\crouch_hmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_welder_look" {
	"soldier_anims\crouch_welder_look_blend01"
	"soldier_anims\crouch_welder_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_grenadegun_look" {
	"soldier_anims\crouch_grenadegun_look_blend01"
	"soldier_anims\crouch_grenadegun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_tripmine_look" {
	"soldier_anims\crouch_tripmine_look_blend01"
	"soldier_anims\crouch_tripmine_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_lmg_fire" {
	"soldier_anims\crouch_lmg_fire_blend01"
	"soldier_anims\crouch_lmg_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
}
$sequence "crouch_pistol_fire" {
	"soldier_anims\crouch_pistol_fire_blend01"
	"soldier_anims\crouch_pistol_fire_blend02"
	blend XR -90 90
	{ event 5001 1 "30" }
	fps 30
}
$sequence "crouch_knife_fire" {
	"soldier_anims\crouch_knife_fire_blend01"
	"soldier_anims\crouch_knife_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "crouch_shotgun_fire" {
	"soldier_anims\crouch_shotgun_fire_blend01"
	"soldier_anims\crouch_shotgun_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
}
$sequence "crouch_grenade_fire" {
	"soldier_anims\crouch_grenade_fire_blend01"
	"soldier_anims\crouch_grenade_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "crouch_hmg_fire" {
	"soldier_anims\crouch_hmg_fire_blend01"
	"soldier_anims\crouch_hmg_fire_blend02"
	blend XR -90 90
	{ event 5021 1 "40" }
	fps 30
}
$sequence "crouch_welder_fire" {
	"soldier_anims\crouch_welder_fire_blend01"
	"soldier_anims\crouch_welder_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "crouch_grenadegun_fire" {
	"soldier_anims\crouch_grenadegun_fire_blend01"
	"soldier_anims\crouch_grenadegun_fire_blend02"
	blend XR -90 90
	fps 17
}
$sequence "crouch_tripmine_fire" {
	"soldier_anims\crouch_tripmine_fire_blend01"
	"soldier_anims\crouch_tripmine_fire_blend02"
	blend XR -90 90
	fps 30
}
$sequence "ability1_alien" {
	"soldier_anims\ability1_alien"
	fps 20
}
$sequence "ability2_alien" {
	"soldier_anims\ability2_alien"
	fps 20
}
$sequence "ability3_alien" {
	"soldier_anims\ability3_alien"
	fps 20
}
$sequence "ability4_alien" {
	"soldier_anims\ability4_alien"
	fps 20
}
$sequence "ability5_alien" {
	"soldier_anims\ability5_alien"
	fps 20
}
$sequence "ability6_alien" {
	"soldier_anims\ability6_alien"
	fps 20
}
$sequence "head_flinch" {
	"soldier_anims\head_flinch_blend01"
	"soldier_anims\head_flinch_blend02"
	blend XR -90 90
	fps 30
}
$sequence "body_flinch" {
	"soldier_anims\body_flinch_blend01"
	"soldier_anims\body_flinch_blend02"
	blend XR -90 90
	fps 30
}
$sequence "death1_die" {
	"soldier_anims\death1_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 18
}
$sequence "death2_die" {
	"soldier_anims\death2_die"
	ACT_DIEBACKWARD 1
	{ event 2001 44 }
	fps 15
}
$sequence "death3_die" {
	"soldier_anims\death3_die"
	ACT_DIE_HEADSHOT 1
	{ event 2001 40 }
	fps 15
}
$sequence "head_die" {
	"soldier_anims\head_die"
	ACT_DIE_HEADSHOT 4
	{ event 2001 35 }
	fps 18
}
$sequence "gut_die" {
	"soldier_anims\gut_die"
	ACT_DIE_GUTSHOT 1
	{ event 2001 44 }
	fps 27
}
$sequence "left_die" {
	"soldier_anims\left_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 29
}
$sequence "back_die" {
	"soldier_anims\back_die"
	ACT_DIEBACKWARD 4
	{ event 2001 44 }
	fps 17
}
$sequence "right_die" {
	"soldier_anims\right_die"
	ACT_DIESIMPLE 1
	{ event 2001 40 }
	fps 15
}
$sequence "forward_die" {
	"soldier_anims\forward_die"
	ACT_DIESIMPLE 1
	{ event 2001 35 }
	fps 18
}
$sequence "crouch_die" {
	"soldier_anims\crouch_die"
	ACT_DIESIMPLE 1
	{ event 2001 30 }
	fps 23
}
$sequence "lmg_reload" {
	"soldier_anims\lmg_reload"
	fps 38
}
$sequence "pistol_reload" {
	"soldier_anims\pistol_reload"
	fps 26
}
$sequence "shotgun_reload_start" {
	"soldier_anims\shotgun_reload_start"
	fps 32
}
$sequence "shotgun_reload_insert" {
	"soldier_anims\shotgun_reload_insert"
	fps 30
}
$sequence "shotgun_reload_end" {
	"soldier_anims\shotgun_reload_end"
	fps 31
}
$sequence "hmg_reload" {
	"soldier_anims\hmg_reload"
	fps 23
}
$sequence "grenadegun_reload" {
	"soldier_anims\grenadegun_reload"
	fps 18
}
$sequence "handgrenade_look" {
	"soldier_anims\handgrenade_look_blend01"
	"soldier_anims\handgrenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "handgrenade_prime" {
	"soldier_anims\handgrenade_prime_blend01"
	"soldier_anims\handgrenade_prime_blend02"
	blend XR -90 90
	fps 20
}
$sequence "handgrenade_fire" {
	"soldier_anims\handgrenade_fire_blend01"
	"soldier_anims\handgrenade_fire_blend02"
	blend XR -90 90
	fps 20
}
$sequence "crouch_handgrenade_look" {
	"soldier_anims\crouch_handgrenade_look_blend01"
	"soldier_anims\crouch_handgrenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_handgrenade_prime" {
	"soldier_anims\crouch_handgrenade_prime_blend01"
	"soldier_anims\crouch_handgrenade_prime_blend02"
	blend XR -90 90
	fps 20
}
$sequence "crouch_handgrenade_fire" {
	"soldier_anims\crouch_handgrenade_fire_blend01"
	"soldier_anims\crouch_handgrenade_fire_blend02"
	blend XR -90 90
	fps 20
}
$sequence "grenadegun_reload_start" {
	"soldier_anims\grenadegun_reload_start"
	fps 18
}
$sequence "grenadegun_reload_insert" {
	"soldier_anims\grenadegun_reload_insert"
	fps 18
}
$sequence "grenadegun_reload_end" {
	"soldier_anims\grenadegun_reload_end"
	fps 18
}
