
$modelname "shell.mdl"
$cd "."
$cdtexture "."
$cliptotextures
$scale 1.0

$bodygroup "studio"
{
	studio "shellcase2"
}

$flags 0


$hbox 0 "Cylinder01" -0.864896 -1.01591 0 1.057096 1.01591 4.118555

$sequence "shell" {
	"shell_anims\shell"
	fps 30
}
