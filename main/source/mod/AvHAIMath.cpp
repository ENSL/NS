//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_math.cpp
// 
// Contains all useful math functions for bot stuff
//

#include "AvHAIMath.h"
#include "../dlls/util.h"

#include <time.h>

#include <string>
#include <sstream>
#include <cfloat>

extern enginefuncs_t g_engfuncs;

bool isNumber(const char* line)
{
	char* p;
	strtol(line, &p, 10);
	return *p == 0;
}

bool isFloat(const char* line)
{
	std::string myString = line;
	std::istringstream iss(myString);
	float f;
	iss >> std::noskipws >> f; // noskipws considers leading whitespace invalid
	// Check the entire string was consumed and if either failbit or badbit is set
	return iss.eof() && !iss.fail();
}

// Returns the normalized surface normal of a triangle defined by v1,v2,v3. Assumes clockwise indices.
Vector UTIL_GetSurfaceNormal(const Vector v1, const Vector v2, const Vector v3)
{

	Vector normal(((v2.y - v1.y) * (v3.z - v1.z)) - ((v2.z - v1.z) * (v3.y - v1.y)),
		((v2.z - v1.z) * (v3.x - v1.x)) - ((v2.x - v1.x) * (v3.z - v1.z)),
		((v2.x - v1.x) * (v3.y - v1.y)) - ((v2.y - v1.y) * (v3.x - v1.x)));

	UTIL_NormalizeVector(&normal);

	return normal;
}

bool vPointOverlaps3D(const Vector Point, const Vector MinBB, const Vector MaxBB)
{
	return (Point.x >= MinBB.x && Point.x <= MaxBB.x
		&& Point.y >= MinBB.y && Point.y <= MaxBB.y
		&& Point.z >= MinBB.z && Point.z <= MaxBB.z);
}

bool vPointOverlaps2D(const Vector Point, const Vector MinBB, const Vector MaxBB)
{
	return (Point.x >= MinBB.x && Point.x <= MaxBB.x
		&& Point.y >= MinBB.y && Point.y <= MaxBB.y);
}

bool vBBOverlaps2D(const Vector MinBBA, const Vector MaxBBA, const Vector MinBBB, const Vector MaxBBB)
{
	return ( (MinBBA.x < MaxBBB.x && MaxBBA.x > MinBBB.x)
		&&	 (MinBBA.y < MaxBBB.y && MaxBBA.y > MinBBB.y));
}

// Given three collinear points p, q, r, the function checks if 
// point q lies on line segment 'pr' 
bool onSegment(Vector p, Vector q, Vector r)
{
	if (q.x <= fmaxf(p.x, r.x) && q.x >= fminf(p.x, r.x) &&
		q.y <= fmaxf(p.y, r.y) && q.y >= fminf(p.y, r.y))
		return true;

	return false;
}

// To find orientation of ordered triplet (p, q, r). 
// The function returns following values 
// 0 --> p, q and r are collinear 
// 1 --> Clockwise 
// 2 --> Counterclockwise 
int orientation(Vector p, Vector q, Vector r)
{
	// See https://www.geeksforgeeks.org/orientation-3-ordered-points/ 
	// for details of below formula. 
	int val = (q.y - p.y) * (r.x - q.x) -
		(q.x - p.x) * (r.y - q.y);

	if (val == 0) return 0;  // collinear 

	return (val > 0) ? 1 : 2; // clock or counterclock wise 
}

bool vIntersects2D(const Vector LineAStart, const Vector LineAEnd, const Vector LineBStart, const Vector LineBEnd)
{
	int o1 = orientation(LineAStart, LineAEnd, LineBStart);
	int o2 = orientation(LineAStart, LineAEnd, LineBEnd);
	int o3 = orientation(LineBStart, LineBEnd, LineAStart);
	int o4 = orientation(LineBStart, LineBEnd, LineAEnd);

	// General case 
	if (o1 != o2 && o3 != o4)
		return true;

	// Special Cases 
	// p1, q1 and p2 are collinear and p2 lies on segment p1q1 
	if (o1 == 0 && onSegment(LineAStart, LineBStart, LineAEnd)) return true;

	// p1, q1 and q2 are collinear and q2 lies on segment p1q1 
	if (o2 == 0 && onSegment(LineAStart, LineBEnd, LineAEnd)) return true;

	// p2, q2 and p1 are collinear and p1 lies on segment p2q2 
	if (o3 == 0 && onSegment(LineBStart, LineAStart, LineBEnd)) return true;

	// p2, q2 and q1 are collinear and q1 lies on segment p2q2 
	if (o4 == 0 && onSegment(LineBStart, LineAEnd, LineBEnd)) return true;

	return false; // Doesn't fall in any of the above cases 
}

Vector vClosestPointOnBB(const Vector Point, const Vector MinBB, const Vector MaxBB)
{
	return Vector(clampf(Point.x, MinBB.x, MaxBB.x), clampf(Point.y, MinBB.y, MaxBB.y), clampf(Point.z, MinBB.z, MaxBB.z));
}

void vScaleBB(Vector& MinBB, Vector& MaxBB, const float Scale)
{
	Vector Centre = MinBB + ((MaxBB - MinBB) * 0.5f);

	float SizeX = MaxBB.x - MinBB.x;
	float SizeY = MaxBB.y - MinBB.y;
	float SizeZ = MaxBB.z - MinBB.z;

	MinBB.x = Centre.x - (SizeX * Scale);
	MinBB.y = Centre.y - (SizeY * Scale);
	MinBB.z = Centre.z - (SizeZ * Scale);

	MaxBB.x = Centre.x + (SizeX * Scale);
	MaxBB.y = Centre.y + (SizeY * Scale);
	MaxBB.z = Centre.z + (SizeZ * Scale);
}

// Returns the 3D distance of point from a line defined between lineFrom and lineTo
float vDistanceFromLine3D(const Vector lineFrom, const Vector lineTo, const Vector CheckPoint)
{
	Vector nearestToLine = vClosestPointOnLine(lineFrom, lineTo, CheckPoint);
	return vDist3D(CheckPoint, nearestToLine);
}

// Returns the 2D distance (Z axis ignored) of point from a line defined between lineFrom and lineTo
float vDistanceFromLine2D(const Vector lineFrom, const Vector lineTo, const Vector CheckPoint)
{
	Vector nearestToLine = vClosestPointOnLine2D(lineFrom, lineTo, CheckPoint);
	return vDist2D(CheckPoint, nearestToLine);
}

// Returns the 2D distance (Z axis ignored) of point from a line defined between lineFrom and lineTo
float vDistanceFromLine2DSq(const Vector lineFrom, const Vector lineTo, const Vector CheckPoint)
{
	Vector nearestToLine = vClosestPointOnLine2D(lineFrom, lineTo, CheckPoint);
	return vDist2DSq(CheckPoint, nearestToLine);
}

Vector vClosestPointOnInfiniteLine3D(const Vector PointOnLine, const Vector NormalisedLineDir, const Vector TestPoint)
{
	Vector DirectionToPoint = UTIL_GetVectorNormal(PointOnLine - TestPoint);

	float DirectionDot = UTIL_GetDotProduct(DirectionToPoint, NormalisedLineDir);

	return TestPoint + (NormalisedLineDir * DirectionDot);
}

Vector vClosestPointOnInfiniteLine2D(const Vector PointOnLine, const Vector NormalisedLineDir, const Vector TestPoint)
{
	Vector NormalisedLineDir2D = UTIL_GetVectorNormal2D(NormalisedLineDir);
	Vector DirectionToPoint = UTIL_GetVectorNormal2D(PointOnLine - TestPoint);

	float DirectionDot = UTIL_GetDotProduct2D(DirectionToPoint, NormalisedLineDir2D);

	return TestPoint + (NormalisedLineDir * DirectionDot);
}

// Returns 0 if point sits right on the line defined by lineFrom and lineTo, -1 if it sits to the left, 1 if it sits to the right. Ignores Z axis
int vPointOnLine(const Vector lineFrom, const Vector lineTo, const Vector point)
{
	float value = ((lineTo.x - lineFrom.x) * (point.y - lineFrom.y)) - ((point.x - lineFrom.x) * (lineTo.y - lineFrom.y));

	if (value > 1.0f)
		return 1;

	if (value < -1.0f)
		return -1;

	return 0;
}

// For given line lineFrom -> lineTo, returns the point along that line closest to point
Vector vClosestPointOnLine(const Vector lineFrom, const Vector lineTo, const Vector point)
{
	Vector vVector1 = point - lineFrom;
	Vector vVector2 = UTIL_GetVectorNormal(lineTo - lineFrom);

	float d = vDist3D(lineFrom, lineTo);
	float t = UTIL_GetDotProduct(vVector2, vVector1);

	if (t <= 0)
		return lineFrom;

	if (t >= d)
		return lineTo;

	Vector vVector3 = vVector2 * t;

	Vector vClosestPoint = lineFrom + vVector3;

	return vClosestPoint;
}

// For given line lineFrom -> lineTo, returns the 2D point (Z axis ignored) along that line closest to point
Vector vClosestPointOnLine2D(const Vector lineFrom, const Vector lineTo, const Vector point)
{
	Vector lineFrom2D = Vector(lineFrom.x, lineFrom.y, 0.0f);
	Vector lineTo2D = Vector(lineTo.x, lineTo.y, 0.0f);
	Vector point2D = Vector(point.x, point.y, 0.0f);

	Vector vVector1 = point2D - lineFrom2D;
	Vector vVector2 = UTIL_GetVectorNormal2D(lineTo2D - lineFrom2D);

	float d = vDist2D(lineFrom2D, lineTo2D);
	float t = UTIL_GetDotProduct2D(vVector2, vVector1);

	if (t <= 0)
		return lineFrom2D;

	if (t >= d)
		return lineTo2D;

	Vector vVector3 = vVector2 * t;

	Vector vClosestPoint = lineFrom2D + vVector3;

	return vClosestPoint;
}

// Normalizes the supplied vector, overwriting it with the normalized value in the process
void UTIL_NormalizeVector(Vector* vec)
{
	float len = sqrt((vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z));

	float div = 1.0f / len;

	vec->x *= div;
	vec->y *= div;
	vec->z *= div;
}

void UTIL_NormalizeVector2D(Vector* vec)
{
	float len = sqrt((vec->x * vec->x) + (vec->y * vec->y));

	float div = 1.0f / len;

	vec->x *= div;
	vec->y *= div;
	vec->z = 0.0f;
}

// Returns a normalized copy of the supplied Vector. Original value is unmodified
Vector UTIL_GetVectorNormal(const Vector vec)
{
	return vec.Normalize();
}

// Returns a 2D (Z axis is 0) normalized copy of the supplied Vector. Original value is unmodified
Vector UTIL_GetVectorNormal2D(const Vector vec)
{
	if (vec.x == 0.0f && vec.y == 0.0f) { return ZERO_VECTOR; }

	Vector result;
	float len = sqrt((vec.x * vec.x) + (vec.y * vec.y));

	float div = 1.0f / len;

	result.x = vec.x * div;
	result.y = vec.y * div;
	result.z = 0.0f;

	return result;
}

// Returns the cross product of v1 and v2.
Vector UTIL_GetCrossProduct(const Vector v1, const Vector v2)
{
	Vector result;

	result.x = v1.y * v2.z - v1.z * v2.y;
	result.y = v1.z * v2.x - v1.x * v2.z;
	result.z = v1.x * v2.y - v1.y * v2.x;

	return result;
}

// Returns the 2D (ignoring Z axis) distance between the two vectors
float vDist2D(const Vector v1, const Vector v2)
{
	return (float)sqrt((v2.x - v1.x) * (v2.x - v1.x)
		+ (v2.y - v1.y) * (v2.y - v1.y));
}

// Returns the 3D distance between the two vectors
float vDist3D(const Vector v1, const Vector v2)
{
	return (float)sqrt((v2.x - v1.x) * (v2.x - v1.x)
		+ (v2.y - v1.y) * (v2.y - v1.y)
		+ (v2.z - v1.z) * (v2.z - v1.z));
}

// Returns the 2D (ignoring Z axis) squared distance between the two vectors
float vDist2DSq(const Vector v1, const Vector v2)
{
	return ((v2.x - v1.x) * (v2.x - v1.x)
		+ (v2.y - v1.y) * (v2.y - v1.y));
}

// Returns the 3D squared distance between the two vectors
float vDist3DSq(const Vector v1, const Vector v2)
{
	return ((v2.x - v1.x) * (v2.x - v1.x)
		+ (v2.y - v1.y) * (v2.y - v1.y)
		+ (v2.z - v1.z) * (v2.z - v1.z));
}

float vSize3DSq(const Vector V)
{
	return (V.x * V.x) + (V.y * V.y) + (V.z * V.z);
}

float vSize2DSq(const Vector V)
{
	return (V.x * V.x) + (V.y * V.y);
}

float vSize3D(const Vector V)
{
	return sqrtf((V.x * V.x) + (V.y * V.y) + (V.z * V.z));
}

float vSize2D(const Vector V)
{
	return sqrtf((V.x * V.x) + (V.y * V.y));
}

// Returns true if the two vectors are the same (all components are within 0.0001f of each other)
bool vEquals(const Vector v1, const Vector v2)
{
	return fabsf(v1.x - v2.x) <= 0.0001f && fabsf(v1.y - v2.y) <= 0.0001f && fabsf(v1.z - v2.z) <= 0.0001f;
}

bool vEquals2D(const Vector v1, const Vector v2)
{
	return fabsf(v1.x - v2.x) <= 0.01f && fabsf(v1.y - v2.y) <= 0.01f;
}

// Returns true if the two vectors are the same (all components are within epsilon of each other)
bool vEquals(const Vector v1, const Vector v2, const float epsilon)
{
	return fabsf(v1.x - v2.x) <= epsilon && fabsf(v1.y - v2.y) <= epsilon && fabsf(v1.z - v2.z) <= epsilon;
}

bool vEquals2D(const Vector v1, const Vector v2, const float epsilon)
{
	return fabsf(v1.x - v2.x) <= epsilon && fabsf(v1.y - v2.y) <= epsilon;
}

bool vIsZero(const Vector v1)
{
	return (fabsf(v1.x) < 0.0001f && fabsf(v1.y) < 0.0001f && fabsf(v1.z) < 0.0001f);
}

bool fNearlyEqual(const float f1, const float f2)
{
	return fabsf(f1 - f2) < 0.001f;
}


// Returns the dot product of two vectors (1.0f if both vectors pointing exactly the same direction, -1.0f if opposites, 0.0f if perpendicular)
//float UTIL_GetDotProduct(const Vector v1, const Vector v2)
//{
//	return ((v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z));
//}

// Returns the 2D dot product (Z axis ignored) of two vectors (1.0f if both vectors pointing exactly the same direction, -1.0f if opposites, 0.0f if perpendicular)
float UTIL_GetDotProduct2D(const Vector v1, const Vector v2)
{
	return ((v1.x * v2.x) + (v1.y * v2.y));
}

// Returns a random point along the circumference of a circle defined with the supplied origin and radius
Vector UTIL_RandomPointOnCircle(const Vector origin, const float radius)
{
	Vector result;

	float random = ((float)rand()) / (float)RAND_MAX;

	float a = random * (MATH_PI * 2);
	result.x = origin.x + (radius * cos(a));
	result.y = origin.y + (radius * sin(a));
	result.z = origin.z;

	return result;
}

// For given plane, determine if the given point sits within the plane or not
bool UTIL_PointInsidePlane(const frustum_plane_t* plane, const Vector point)
{
	float distance = plane->d + (plane->normal.x * point.x + plane->normal.y * point.y + plane->normal.z * point.z);
	return distance >= 0.0f;
}

bool UTIL_CylinderInsidePlane(const frustum_plane_t* plane, const Vector centre, float height, float radius)
{
	Vector testNormal = plane->normal;
	testNormal.z = 0;

	Vector topPoint = centre + Vector(0.0f, 0.0f, height * 0.5f) + (testNormal * radius);
	Vector bottomPoint = centre - Vector(0.0f, 0.0f, height * 0.5f) + (testNormal * radius);

	return (UTIL_PointInsidePlane(plane, topPoint) || UTIL_PointInsidePlane(plane, bottomPoint));
}

void UTIL_SetFrustumPlane(frustum_plane_t* plane, Vector v1, Vector v2, Vector v3)
{

	Vector normal = UTIL_GetSurfaceNormal(v1, v2, v3);

	plane->normal.x = normal.x;
	plane->normal.y = normal.y;
	plane->normal.z = normal.z;

	plane->point.x = v2.x;
	plane->point.y = v2.y;
	plane->point.z = v2.z;

	plane->d = -(plane->normal.x * plane->point.x + plane->normal.y * plane->point.y + plane->normal.z * plane->point.z);
}

float UTIL_MetresToGoldSrcUnits(const float Metres)
{
	return Metres * 52.4934f;
}

float UTIL_GoldSrcUnitsToMetres(const float GoldSrcUnits)
{
	return GoldSrcUnits * 0.01905f;
}

float sqrf(float input)
{
	return (input * input);
}

int imaxi(const int a, const int b)
{
	return (a > b) ? a : b;
}

int imini(const int a, const int b)
{
	return (a < b) ? a : b;
}

float clampf(float input, float inMin, float inMax)
{
	return fmaxf(fminf(input, inMax), inMin);
}

float clampi(int input, int inMin, int inMax)
{
	return imaxi(imini(input, inMax), inMin);
}

Vector GetPitchForProjectile(Vector LaunchPoint, Vector TargetPoint, const float ProjectileSpeed, const float Gravity)
{
	double start_x = LaunchPoint.x;
	double start_y = LaunchPoint.y;
	double start_z = LaunchPoint.z;
	double target_x = TargetPoint.x;
	double target_y = TargetPoint.y;
	double target_z = TargetPoint.z;

	double x = target_x - start_x;
	double y = target_y - start_y;
	double z = target_z - start_z;

	double range = sqrt(x * x + y * y);

	double discriminant = pow(ProjectileSpeed, 4) - Gravity * (Gravity * pow(range, 2) + 2 * z * pow(ProjectileSpeed, 2));

	if (discriminant < 0)
	{
		return ZERO_VECTOR;
	}

	double launch_angle = atan((pow(ProjectileSpeed, 2) - sqrt(discriminant)) / (Gravity * range));

	// Calculate the components of the unit vector
	double unit_vector_x = cos(launch_angle) * cos(atan2(y, x));
	double unit_vector_y = cos(launch_angle) * sin(atan2(y, x));
	double unit_vector_z = sin(launch_angle);

	Vector LaunchVector = Vector(unit_vector_x, unit_vector_y, unit_vector_z);
	return LaunchVector;

}

void UTIL_AnglesToVector(const Vector angles, Vector* fwd, Vector* right, Vector* up)
{
	g_engfuncs.pfnAngleVectors(angles, (float*)fwd, (float*)right, (float*)up);
}

float UTIL_WrapAngle(float angle)
{
	// check for wraparound of angle
	if (angle > 180)
		angle -= 360;
	else if (angle < -180)
		angle += 360;

	return (angle);
}

Vector UTIL_WrapAngles(Vector angles)
{
	// check for wraparound of angles
	if (angles.x > 180)
		angles.x -= 360;
	else if (angles.x < -180)
		angles.x += 360;
	
	if (angles.y > 180)
		angles.y -= 360;
	else if (angles.y < -180)
		angles.y += 360;
	
	if (angles.z > 180)
		angles.z -= 360;
	else if (angles.z < -180)
		angles.z += 360;

	return (angles);
}

float signf(float input)
{
	return (input == 0.0f) ? 0.0f : ((input > 0.0f) ? 1.0f : -1.0f);
}

Vector ViewInterpTo(const Vector CurrentViewAngles, const Vector& TargetDirection, const float DeltaTime, const float InterpSpeed)
{
	if (DeltaTime == 0.f)
	{
		return CurrentViewAngles;
	}

	// If no interp speed, jump to target value
	if (InterpSpeed <= 0.f)
	{
		return UTIL_VecToAngles(TargetDirection);
	}

	Vector TargetAngles = UTIL_VecToAngles(TargetDirection);

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;

	const Vector Delta = UTIL_GetVectorNormal(TargetAngles - CurrentViewAngles);

	// If steps are too small, just return Target and assume we have reached our destination.
	if (vEquals(CurrentViewAngles, TargetAngles))
	{
		return TargetAngles;
	}

	// Delta Move, Clamp so we do not over shoot.
	const Vector DeltaMove = Delta * clampf(DeltaInterpSpeed, 0.f, 1.f);
	Vector NewAngle = (CurrentViewAngles + DeltaMove);

	if (NewAngle.y > 180)
		NewAngle.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (NewAngle.x > 180)
		NewAngle.x -= 360;

	NewAngle.z = 0.0f;

	return NewAngle;
}

float fInterpTo(float start, float end, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value
	if (InterpSpeed <= 0.f)
	{
		return end;
	}

	// Distance to reach
	float Dist = end - start;

	// If distance is too small, just set the desired location
	if (fabsf(sqrf(Dist)) < 0.0001f)
	{
		return end;
	}

	// Delta Move, Clamp so we do not over shoot.
	float DeltaMove = Dist * clampf((DeltaTime * InterpSpeed), 0.f, 1.f);

	float NewVal = start + DeltaMove;

	return NewVal;
}

float fInterpConstantTo(float start, float end, float DeltaTime, float InterpSpeed)
{
	float Dist = end - start;

	// If distance is too small, just set the desired location
	if (fabsf(sqrf(Dist)) < 0.0001f)
	{
		return end;
	}

	const float Step = InterpSpeed * DeltaTime;
	float NewVal = start + clampf(Dist, -Step, Step);

	return NewVal;
}

float frandrange(float MinValue, float MaxValue)
{
	return ((float(rand()) / float(RAND_MAX)) * (MaxValue - MinValue)) + MinValue;
}

int irandrange(int MinValue, int MaxValue)
{
	if (MinValue == MaxValue) { return MinValue; }

	return MinValue + rand() / (RAND_MAX / (MaxValue - MinValue + 1) + 1);
}

bool randbool()
{
	return (rand() & 1);
}

Vector random_unit_vector_within_cone(const Vector Direction, double cone_half_angle)
{
	Vector Result = ZERO_VECTOR;

	double u = ((double)rand() / RAND_MAX) * 2 - 1; // random number in [-1, 1]
	double phi = ((double)rand() / RAND_MAX) * 2 * M_PI; // random number in [0, 2*pi]
	double r = sqrt(1 - u * u);
	double x = r * cos(phi);
	double y = r * sin(phi);
	double z = u;

	// Rotate the random point to the cone direction using a quaternion.
	// First, compute the quaternion that rotates the z-axis to the cone direction.
	double cos_half_angle = cos(cone_half_angle / 2);
	double sin_half_angle = sin(cone_half_angle / 2);
	double qx = Direction.x * sin_half_angle;
	double qy = Direction.y * sin_half_angle;
	double qz = Direction.z * sin_half_angle;
	double qw = cos_half_angle;
	// Then, apply the quaternion rotation to the random point.
	double t = qw * z - qx * x - qy * y - qz * z;
	Result.x = qx * z + qw * x - qz * y + qy * t;
	Result.y = qy * z + qz * x + qw * y - qx * t;
	Result.z = qz * z - qy * x + qx * y + qw * t;

	return UTIL_GetVectorNormal(Result);
}

Vector UTIL_GetRandomUnitVectorInCone(const Vector ConeDirection, const float HalfAngleRadians)
{
	Vector P = UTIL_GetVectorNormal(UTIL_GetCrossProduct(ConeDirection, UP_VECTOR));
	Vector Q = UTIL_GetVectorNormal(UTIL_GetCrossProduct(ConeDirection, P));

	float RMax = tanf(HalfAngleRadians);
	float Theta = frandrange(0.0f, 2.0f) * MATH_PI;
	float u = frandrange(cos(HalfAngleRadians), 1.0f);
	float r = RMax * sqrt(1.0f - sqrf(u));
	return UTIL_GetVectorNormal((r * (P * cos(Theta) + Q * sin(Theta))));
}

float fDegreesToRadians(const float Degrees)
{
	return Degrees * DEGREES_RADIANS_CONV;
}

Vector UTIL_GetForwardVector(const Vector angles)
{
	Vector fwd, right, up;

	UTIL_AnglesToVector(angles, &fwd, &right, &up);
	return fwd;
}

Vector UTIL_GetForwardVector2D(const Vector angles)
{
	Vector fwd, right, up;

	UTIL_AnglesToVector(angles, &fwd, &right, &up);
	return UTIL_GetVectorNormal2D(fwd);
}

float UTIL_GetDistanceToPolygon2DSq(const Vector TestPoint, const Vector* Points, const int NumPoints)
{
	float minDist = -1.0f;

	for (int i = 0; i < NumPoints; i++)
	{
		Vector destPoint = (i <= (NumPoints - 2)) ? Points[i + 1] : Points[0];
		float thisDist = vDistanceFromLine2DSq(Points[i], destPoint, TestPoint);

		if (minDist < 0.0f)
		{
			minDist = thisDist;
		}
		else
		{
			minDist = fminf(minDist, thisDist);
		}
	}

	return minDist;
}

Vector UTIL_GetAimLocationToLeadTarget(const Vector ShooterLocation, const Vector TargetLocation, const Vector TargetVelocity, const float ProjectileVelocity)
{
	// We interpret a speed of 0.0f to mean hitscan, i.e. infinitely fast
	if (ProjectileVelocity == 0.0f) { return TargetLocation; }


	Vector totarget = TargetLocation - ShooterLocation;


	float a = UTIL_GetDotProduct(TargetVelocity, TargetVelocity) - (ProjectileVelocity * ProjectileVelocity);
	float b = 2.0f * UTIL_GetDotProduct(TargetVelocity, totarget);
	float c = UTIL_GetDotProduct(totarget, totarget);

	float p = -b / (2.0f * a);
	float q = (float)sqrt((b * b) - 4.0f * a * c) / (2.0f * a);

	float t1 = p - q;
	float t2 = p + q;
	float t;

	if (t1 > t2 && t2 > 0)
	{
		t = t2;
	}
	else
	{
		t = t1;
	}

	return (TargetLocation + TargetVelocity * t);
}

float UTIL_GetVelocityRequiredToReachTarget(const Vector StartLocation, const Vector TargetLocation, float Gravity)
{
	// Calculate the distance between the start and target positions
	double distance = vDist3D(StartLocation, TargetLocation);

	// Calculate the initial velocity required to reach the target with no air resistance
	double velocity = sqrt(2.0f * Gravity * distance);

	return velocity;
}

Vector UTIL_GetRandomPointInBoundingBox(const Vector BoxMin, const Vector BoxMax)
{
	float RandX = frandrange(BoxMin.x, BoxMax.x);
	float RandY = frandrange(BoxMin.y, BoxMax.y);
	float RandZ = frandrange(BoxMin.z, BoxMax.z);

	return Vector(RandX, RandY, RandZ);
}

/* Function to get no of set bits in binary
   representation of positive integer n */
unsigned int UTIL_CountSetBitsInInteger(unsigned int n)
{
	unsigned int count = 0;
	while (n)
	{
		count += n & 1;
		n >>= 1;
	}
	return count;
}

float UTIL_CalculateSlopeAngleBetweenPoints(const Vector StartPoint, const Vector EndPoint)
{
	float Run = vDist2DSq(StartPoint, EndPoint);
	float Rise = fabsf(StartPoint.z - EndPoint.z);

	return atanf(Rise / Run);
}

// Function to check if a finite line intersects with an AABB
bool vlineIntersectsAABB(Vector lineStart, Vector lineEnd, Vector BoxMinPosition, Vector BoxMaxPosition)
{
	if (vPointOverlaps3D(lineStart, BoxMinPosition, BoxMaxPosition) || vPointOverlaps3D(lineEnd, BoxMinPosition, BoxMaxPosition)) { return true; }

	Vector RayDir = UTIL_GetVectorNormal(lineEnd - lineStart);
	float LineLength = vDist3D(lineStart, lineEnd);
	Vector dirfrac;

	float t = FLT_MAX;

	// r.dir is unit direction vector of ray
	dirfrac.x = 1.0f / RayDir.x;
	dirfrac.y = 1.0f / RayDir.y;
	dirfrac.z = 1.0f / RayDir.z;
	// lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
	// r.org is origin of ray
	float t1 = (BoxMinPosition.x - lineStart.x) * dirfrac.x;
	float t2 = (BoxMaxPosition.x - lineStart.x) * dirfrac.x;
	float t3 = (BoxMinPosition.y - lineStart.y) * dirfrac.y;
	float t4 = (BoxMaxPosition.y - lineStart.y) * dirfrac.y;
	float t5 = (BoxMinPosition.z - lineStart.z) * dirfrac.z;
	float t6 = (BoxMaxPosition.z - lineStart.z) * dirfrac.z;

	float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
	float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

	// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
	if (tmax < 0)
	{
		t = tmax;
		return false;
	}

	// if tmin > tmax, ray doesn't intersect AABB
	if (tmin > tmax)
	{
		t = tmax;
		return false;
	}

	t = tmin;
	return t <= LineLength;
}