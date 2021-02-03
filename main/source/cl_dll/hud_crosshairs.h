#pragma once
#include <vector>

// Crosshair system based on OpenAG
// https://github.com/YaLTeR/OpenAG

class CHudCrosshairs : public CHudBase
{
	cvar_t* cl_cross;
	cvar_t* cl_cross_color;
	cvar_t* cl_cross_alpha;
	cvar_t* cl_cross_thickness;
	cvar_t* cl_cross_size;
	cvar_t* cl_cross_gap;
	cvar_t* cl_cross_outline;
	cvar_t* cl_cross_outline_alpha;
	cvar_t* cl_cross_outline_inner;
	cvar_t* cl_cross_circle_radius;
	cvar_t* cl_cross_circle_thickness;
	cvar_t* cl_cross_dot_size;
	cvar_t* cl_cross_dot_color;
	cvar_t* cl_cross_dot_outline;
	cvar_t* cl_cross_line_top;
	cvar_t* cl_cross_line_bottom;
	cvar_t* cl_cross_line_left;
	cvar_t* cl_cross_line_right;

#ifdef __APPLE__
	//Remove when OSX builds with c++11
#else
	float old_circle_radius;
	std::vector<Vector2D> circle_points;
#endif

public:
	virtual int Init();
	virtual int VidInit();
	virtual int Draw(float time);
};
