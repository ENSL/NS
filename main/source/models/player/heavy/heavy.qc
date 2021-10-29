
// Stored modelname (without quotes): "C:\Documents and Settings\Joe\Desktop\heavy\heavy.mdl"
$modelname "player/heavy/heavy.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "body"
{
	studio "reference"
}

$flags 0


$attachment 0 "Bip01 R Hand" 11.4 1.6 6.4
$attachment 1 "Bip01 R Hand" 23 2 7.35
$attachment 2 "Bip01 R Hand" 30 2 9.5


$hbox 0 "Bip01 Pelvis" -8.22 -13.03 -13.15 8.29 10.04 14.22
$hbox 0 "Bip01 Spine" 0 -14.98 -9.73 13.95 8.41 10.3
$hbox 0 "Bip01 Spine1" 0 -15.52 -19.540001 20.780001 10.69 20.120001
$hbox 0 "Bip01 Neck" -2.962026 -8 -5.710172 18 6 5.710153
$hbox 0 "Bip01 L UpperArm" 0 -6.61 -4.65 14.73 4.81 5.38
$hbox 0 "Bip01 L Forearm" 0 -5.71 -4 24.25 5.09 4.54
$hbox 0 "Bip01 L Hand" 0 -1.88 -2.837552 6.406904 1.41 2.2
$hbox 0 "Bip01 L Finger0" 0 -1.17 -0.87 2.80696 0.74 0.55
$hbox 0 "Bip01 L Finger1" -0.81 -1.48 -2.6 2.77 0.73 2.81
$hbox 0 "Bip01 L Finger11" 0 -1.15 -2.56 3.53 1.05 2.77
$hbox 0 "Bip01 R UpperArm" 0 -6.52 -5.41 14.15 4.89 4.63
$hbox 0 "Bip01 R Forearm" -0.28 -5.67 -4.56 23.67 5.13 3.98
$hbox 0 "Bip01 R Hand" 0 -1.86 -2.23 6.406905 1.43 2.83755
$hbox 0 "Bip01 R Finger0" 0 -0.91 -0.45 2.80696 1 0.97
$hbox 0 "Bip01 R Finger1" -0.19 -0.57 -2.89 2.541756 0.99 2.54
$hbox 0 "Bip01 R Finger11" -2.1 -0.77 -2.84 3.1 1.44 2.49
$hbox 0 "Bip01 L Thigh" 0 -11.87 -7.82 25.222631 7.42 6.8
$hbox 0 "Bip01 L Calf" -6.66 -6.36 -7.96 35.049999 8.94 7.13
$hbox 0 "Bip01 L Foot" 0 0 -4.7 5.976402 9.13 4.45
$hbox 0 "Bip01 L Toe0" 0 0 -3.82 5 2.95 3.9
$hbox 0 "Bip01 R Thigh" 0 -11.89 -7.37 25.222633 7.42 7.24
$hbox 0 "Bip01 R Calf" -6.67 -6.36 -7.71 35.029999 8.93 7.39
$hbox 0 "Bip01 R Foot" 0 0 -5.02 5.976402 9.13 4.08
$hbox 0 "Bip01 R Toe0" 0 0 -4.47 5 2.95 4.12

$controller 0 "Bip01 Spine" XR -60 60
$controller 1 "Bip01 Spine1" XR -60 60

$sequence "look_idle" {
	"heavy_anims\look_idle_blend01"
	"heavy_anims\look_idle_blend02"
	ACT_IDLE 1
	blend XR -90 90
	fps 30
	loop
}
$sequence "idle1" {
	"heavy_anims\idle1_blend01"
	"heavy_anims\idle1_blend02"
	ACT_IDLE 2
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_idle" {
	"heavy_anims\crouch_idle"
	ACT_CROUCHIDLE 1
	fps 1
	loop
}
$sequence "walk" {
	"heavy_anims\walk"
	ACT_WALK 1
	fps 25
	loop
	LX
}
$sequence "run" {
	"heavy_anims\run"
	ACT_RUN 1
	fps 35
	loop
	LX
}
$sequence "crawl" {
	"heavy_anims\crawl"
	ACT_CROUCH 1
	fps 20
	loop
	LX
}
$sequence "jump" {
	"heavy_anims\jump"
	ACT_HOP 1
	fps 30
}
$sequence "swim" {
	"heavy_anims\swim"
	ACT_SWIM 1
	fps 14
	loop
}
$sequence "treadwater" {
	"heavy_anims\treadwater"
	ACT_HOVER 1
	fps 14
	loop
}
$sequence "communicate" {
	"heavy_anims\communicate"
	fps 30
	loop
}
$sequence "voicecomm" {
	"heavy_anims\voicecomm"
	fps 30
	loop
}
$sequence "lmg_look" {
	"heavy_anims\lmg_look_blend01"
	"heavy_anims\lmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "pistol_look" {
	"heavy_anims\pistol_look_blend01"
	"heavy_anims\pistol_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "knife_look" {
	"heavy_anims\knife_look_blend01"
	"heavy_anims\knife_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "shotgun_look" {
	"heavy_anims\shotgun_look_blend01"
	"heavy_anims\shotgun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "grenade_look" {
	"heavy_anims\grenade_look_blend01"
	"heavy_anims\grenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "hmg_look" {
	"heavy_anims\hmg_look_blend01"
	"heavy_anims\hmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "welder_look" {
	"heavy_anims\welder_look_blend01"
	"heavy_anims\welder_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "grenadegun_look" {
	"heavy_anims\grenadegun_look_blend01"
	"heavy_anims\grenadegun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "tripmine_look" {
	"heavy_anims\tripmine_look_blend01"
	"heavy_anims\tripmine_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "alien_look" {
	"heavy_anims\alien_look_blend01"
	"heavy_anims\alien_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "lmg_fire" {
	"heavy_anims\lmg_fire_blend01"
	"heavy_anims\lmg_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
	loop
}
$sequence "pistol_fire" {
	"heavy_anims\pistol_fire_blend01"
	"heavy_anims\pistol_fire_blend02"
	blend XR -90 90
	{ event 5001 1 "30" }
	fps 30
	loop
}
$sequence "knife_fire" {
	"heavy_anims\knife_fire_blend01"
	"heavy_anims\knife_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "shotgun_fire" {
	"heavy_anims\shotgun_fire_blend01"
	"heavy_anims\shotgun_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
	loop
}
$sequence "grenade_fire" {
	"heavy_anims\grenade_fire_blend01"
	"heavy_anims\grenade_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "hmg_fire" {
	"heavy_anims\hmg_fire_blend01"
	"heavy_anims\hmg_fire_blend02"
	blend XR -90 90
	{ event 5021 1 "40" }
	fps 30
	loop
}
$sequence "welder_fire" {
	"heavy_anims\welder_fire_blend01"
	"heavy_anims\welder_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "grenadegun_fire" {
	"heavy_anims\grenadegun_fire_blend01"
	"heavy_anims\grenadegun_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "tripmine_fire" {
	"heavy_anims\tripmine_fire_blend01"
	"heavy_anims\tripmine_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_lmg_look" {
	"heavy_anims\crouch_lmg_look_blend01"
	"heavy_anims\crouch_lmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_pistol_look" {
	"heavy_anims\crouch_pistol_look_blend01"
	"heavy_anims\crouch_pistol_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_knife_look" {
	"heavy_anims\crouch_knife_look_blend01"
	"heavy_anims\crouch_knife_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_shotgun_look" {
	"heavy_anims\crouch_shotgun_look_blend01"
	"heavy_anims\crouch_shotgun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_grenade_look" {
	"heavy_anims\crouch_grenade_look_blend01"
	"heavy_anims\crouch_grenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_hmg_look" {
	"heavy_anims\crouch_hmg_look_blend01"
	"heavy_anims\crouch_hmg_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_welder_look" {
	"heavy_anims\crouch_welder_look_blend01"
	"heavy_anims\crouch_welder_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_grenadegun_look" {
	"heavy_anims\crouch_grenadegun_look_blend01"
	"heavy_anims\crouch_grenadegun_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_tripmine_look" {
	"heavy_anims\crouch_tripmine_look_blend01"
	"heavy_anims\crouch_tripmine_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_lmg_fire" {
	"heavy_anims\crouch_lmg_fire_blend01"
	"heavy_anims\crouch_lmg_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
	loop
}
$sequence "crouch_pistol_fire" {
	"heavy_anims\crouch_pistol_fire_blend01"
	"heavy_anims\crouch_pistol_fire_blend02"
	blend XR -90 90
	{ event 5001 1 "30" }
	fps 30
	loop
}
$sequence "crouch_knife_fire" {
	"heavy_anims\crouch_knife_fire_blend01"
	"heavy_anims\crouch_knife_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_shotgun_fire" {
	"heavy_anims\crouch_shotgun_fire_blend01"
	"heavy_anims\crouch_shotgun_fire_blend02"
	blend XR -90 90
	{ event 5011 1 "30" }
	fps 30
	loop
}
$sequence "crouch_grenade_fire" {
	"heavy_anims\crouch_grenade_fire_blend01"
	"heavy_anims\crouch_grenade_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_hmg_fire" {
	"heavy_anims\crouch_hmg_fire_blend01"
	"heavy_anims\crouch_hmg_fire_blend02"
	blend XR -90 90
	{ event 5021 1 "40" }
	fps 30
	loop
}
$sequence "crouch_welder_fire" {
	"heavy_anims\crouch_welder_fire_blend01"
	"heavy_anims\crouch_welder_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_grenadegun_fire" {
	"heavy_anims\crouch_grenadegun_fire_blend01"
	"heavy_anims\crouch_grenadegun_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_tripmine_fire" {
	"heavy_anims\crouch_tripmine_fire_blend01"
	"heavy_anims\crouch_tripmine_fire_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "ability1_alien" {
	"heavy_anims\ability1_alien"
	fps 20
}
$sequence "ability2_alien" {
	"heavy_anims\ability2_alien"
	fps 20
}
$sequence "ability3_alien" {
	"heavy_anims\ability3_alien"
	fps 20
}
$sequence "ability4_alien" {
	"heavy_anims\ability4_alien"
	fps 20
}
$sequence "ability5_alien" {
	"heavy_anims\ability5_alien"
	fps 20
}
$sequence "ability6_alien" {
	"heavy_anims\ability6_alien"
	fps 20
}
$sequence "head_flinch" {
	"heavy_anims\head_flinch"
	fps 20
}
$sequence "body_flinch" {
	"heavy_anims\body_flinch"
	fps 20
}
$sequence "death1_die" {
	"heavy_anims\death1_die"
	ACT_DIESIMPLE 1
	{ event 2001 42 }
	fps 30
}
$sequence "death2_die" {
	"heavy_anims\death2_die"
	ACT_DIEBACKWARD 1
	{ event 2001 42 }
	fps 30
}
$sequence "death3_die" {
	"heavy_anims\death3_die"
	ACT_DIE_HEADSHOT 1
	{ event 2001 43 }
	fps 30
}
$sequence "head_die" {
	"heavy_anims\head_die"
	ACT_DIE_HEADSHOT 4
	{ event 2001 42 }
	fps 30
}
$sequence "gut_die" {
	"heavy_anims\gut_die"
	ACT_DIE_GUTSHOT 1
	{ event 2001 44 }
	fps 30
}
$sequence "left_die" {
	"heavy_anims\left_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 30
}
$sequence "back_die" {
	"heavy_anims\back_die"
	ACT_DIEBACKWARD 4
	{ event 2001 44 }
	fps 30
}
$sequence "right_die" {
	"heavy_anims\right_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 30
}
$sequence "forward_die" {
	"heavy_anims\forward_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 30
}
$sequence "crouch_die" {
	"heavy_anims\crouch_die"
	ACT_DIESIMPLE 1
	{ event 2001 44 }
	fps 30
}
$sequence "lmg_reload" {
	"heavy_anims\lmg_reload"
	fps 20
}
$sequence "pistol_reload" {
	"heavy_anims\pistol_reload"
	fps 33
}
$sequence "shotgun_reload_start" {
	"heavy_anims\shotgun_reload_start"
	fps 20
}
$sequence "shotgun_reload_insert" {
	"heavy_anims\shotgun_reload_insert"
	fps 28
}
$sequence "shotgun_reload_end" {
	"heavy_anims\shotgun_reload_end"
	fps 30
}
$sequence "hmg_reload" {
	"heavy_anims\hmg_reload"
	fps 20
}
$sequence "grenadegun_reload" {
	"heavy_anims\grenadegun_reload"
	fps 20
}
$sequence "handgrenade_look" {
	"heavy_anims\handgrenade_look_blend01"
	"heavy_anims\handgrenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "handgrenade_prime" {
	"heavy_anims\handgrenade_prime_blend01"
	"heavy_anims\handgrenade_prime_blend02"
	blend XR -90 90
	fps 20
}
$sequence "handgrenade_fire" {
	"heavy_anims\handgrenade_fire_blend01"
	"heavy_anims\handgrenade_fire_blend02"
	blend XR -90 90
	fps 20
}
$sequence "crouch_handgrenade_look" {
	"heavy_anims\crouch_handgrenade_look_blend01"
	"heavy_anims\crouch_handgrenade_look_blend02"
	blend XR -90 90
	fps 30
	loop
}
$sequence "crouch_handgrenade_prime" {
	"heavy_anims\crouch_handgrenade_prime_blend01"
	"heavy_anims\crouch_handgrenade_prime_blend02"
	blend XR -90 90
	fps 20
}
$sequence "crouch_handgrenade_fire" {
	"heavy_anims\crouch_handgrenade_fire_blend01"
	"heavy_anims\crouch_handgrenade_fire_blend02"
	blend XR -90 90
	fps 20
}
$sequence "grenadegun_reload_start" {
	"heavy_anims\grenadegun_reload_start"
	fps 20
}
$sequence "grenadegun_reload_insert" {
	"heavy_anims\grenadegun_reload_insert"
	fps 17
}
$sequence "grenadegun_reload_end" {
	"heavy_anims\grenadegun_reload_end"
	fps 20
}