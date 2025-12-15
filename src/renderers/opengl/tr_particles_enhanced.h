#ifndef __TR_PARTICLES_ENHANCED_H__
#define __TR_PARTICLES_ENHANCED_H__

#include "tr_local.h"

#define MAX_PARTICLE_EMITTERS		256
#define MAX_PARTICLE_TRAILS			128
#define MAX_TRAIL_POINTS			32
#define MAX_PARTICLE_RIBBONS		64
#define MAX_RIBBON_SEGMENTS		64

// Particle emitter types
typedef enum {
	EMITTER_POINT,			// Single point emission
	EMITTER_BOX,			// Box-shaped emission area
	EMITTER_SPHERE,			// Sphere-shaped emission area
	EMITTER_CYLINDER,		// Cylinder-shaped emission area
	EMITTER_MESH,			// Emit from mesh surface
	EMITTER_BURST			// Burst emission (all at once)
} emitterType_t;

// Particle physics flags
#define PARTICLE_PHYSICS_GRAVITY	0x0001
#define PARTICLE_PHYSICS_WIND		0x0002
#define PARTICLE_PHYSICS_TURBULENCE	0x0004
#define PARTICLE_PHYSICS_COLLISION	0x0008
#define PARTICLE_PHYSICS_ATTRACTION	0x0010

// Particle trail structure
typedef struct {
	vec3_t		points[MAX_TRAIL_POINTS];
	float		times[MAX_TRAIL_POINTS];
	int			numPoints;
	int			maxPoints;
	qboolean	active;
	float		width;
	vec3_t		color;
	qhandle_t	shader;
	int			spawnTime;
	int			lifeTime;
} particleTrail_t;

// Particle ribbon structure
typedef struct {
	vec3_t		segments[MAX_RIBBON_SEGMENTS][2]; // Start and end points
	vec3_t		colors[MAX_RIBBON_SEGMENTS];
	float		widths[MAX_RIBBON_SEGMENTS];
	int			numSegments;
	qboolean	active;
	qhandle_t	shader;
	int			spawnTime;
	int			lifeTime;
} particleRibbon_t;

// Enhanced particle structure
typedef struct {
	// Basic properties
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		color;
	vec3_t		colorEnd;		// Color interpolation
	float		size;
	float		sizeEnd;		// Size interpolation
	float		rotation;
	float		rotationSpeed;
	float		life;			// 0.0 to 1.0
	float		fade;
	qhandle_t	shader;
	int			spawnTime;
	int			lifeTime;
	qboolean	active;
	
	// Enhanced properties
	int			physicsFlags;
	float		mass;			// For physics calculations
	vec3_t		acceleration;	// Custom acceleration
	vec3_t		wind;			// Wind force
	float		turbulence;		// Turbulence strength
	float		drag;			// Air resistance (0.0 to 1.0)
	
	// Attachment
	int			attachEntity;	// Entity to attach to (-1 = none)
	vec3_t		attachOffset;	// Offset from entity origin
	
	// Trail/Ribbon
	int			trailId;		// Trail ID (-1 = none)
	int			ribbonId;		// Ribbon ID (-1 = none)
	
	// Scripting
	int			scriptId;		// Lua script ID (-1 = none)
} particleEnhanced_t;

// Particle emitter structure
typedef struct {
	emitterType_t	type;
	vec3_t			origin;
	vec3_t			mins;			// For box/cylinder
	vec3_t			maxs;			// For box/cylinder
	float			radius;			// For sphere/cylinder
	float			height;			// For cylinder
	
	// Emission properties
	float			rate;			// Particles per second
	float			burstCount;		// Particles per burst
	float			burstInterval;	// Time between bursts
	int				totalParticles;	// Total particles to emit (-1 = infinite)
	int				particlesEmitted;
	
	// Velocity properties
	vec3_t			velocityMin;
	vec3_t			velocityMax;
	float			velocitySpread;	// Random spread angle
	
	// Particle properties
	vec3_t			colorMin;
	vec3_t			colorMax;
	vec3_t			colorEndMin;
	vec3_t			colorEndMax;
	float			sizeMin;
	float			sizeMax;
	float			sizeEndMin;
	float			sizeEndMax;
	float			lifeMin;
	float			lifeMax;
	float			rotationSpeedMin;
	float			rotationSpeedMax;
	
	// Physics
	int				physicsFlags;
	float			gravity;
	vec3_t			wind;
	float			turbulence;
	float			drag;
	
	// Attachment
	int				attachEntity;
	vec3_t			attachOffset;
	
	// Shader
	qhandle_t		shader;
	
	// State
	qboolean		active;
	int				spawnTime;
	int				lastEmitTime;
	int				startTime;
	int				duration;		// Emitter duration (-1 = infinite)
	
	// Scripting
	int				scriptId;		// Lua script ID
} particleEmitter_t;

// Enhanced particle system
typedef struct {
	particleEnhanced_t	*particles;
	particleEmitter_t	emitters[MAX_PARTICLE_EMITTERS];
	particleTrail_t		trails[MAX_PARTICLE_TRAILS];
	particleRibbon_t	ribbons[MAX_PARTICLE_RIBBONS];
	
	int					maxParticles;
	int					numActive;
	int					nextFree;
	
	int					numEmitters;
	int					numTrails;
	int					numRibbons;
	
	// Global physics
	vec3_t				globalWind;
	float				globalGravity;
	
	// Performance
	int					particlesRendered;
	int					batchesRendered;
} particleSystemEnhanced_t;

// Enhanced particle system functions
void	R_InitParticleSystemEnhanced(void);
void	R_ShutdownParticleSystemEnhanced(void);
void	R_UpdateParticleSystemEnhanced(float deltaTime);
void	R_RenderParticleSystemEnhanced(void);
void	R_ClearParticleSystemEnhanced(void);

// Emitter functions
int		R_CreateParticleEmitter(emitterType_t type, const vec3_t origin, const vec3_t mins, const vec3_t maxs, float radius, float height);
void	R_DestroyParticleEmitter(int emitterId);
void	R_SetEmitterActive(int emitterId, qboolean active);
void	R_SetEmitterRate(int emitterId, float rate);
void	R_SetEmitterVelocity(int emitterId, const vec3_t min, const vec3_t max);
void	R_SetEmitterColor(int emitterId, const vec3_t min, const vec3_t max, const vec3_t endMin, const vec3_t endMax);
void	R_SetEmitterSize(int emitterId, float min, float max, float endMin, float endMax);
void	R_SetEmitterLife(int emitterId, float min, float max);
void	R_SetEmitterPhysics(int emitterId, int flags, float gravity, const vec3_t wind, float turbulence, float drag);
void	R_SetEmitterAttachment(int emitterId, int entityNum, const vec3_t offset);
void	R_SetEmitterShader(int emitterId, qhandle_t shader);
void	R_SetEmitterDuration(int emitterId, int duration);
void	R_SetEmitterBurst(int emitterId, float count, float interval);

// Trail functions
int		R_CreateParticleTrail(float width, const vec3_t color, qhandle_t shader, int maxPoints, int lifeTime);
void	R_DestroyParticleTrail(int trailId);
void	R_AddTrailPoint(int trailId, const vec3_t point);
void	R_SetTrailActive(int trailId, qboolean active);
void	R_SetTrailWidth(int trailId, float width);
void	R_SetTrailColor(int trailId, const vec3_t color);

// Ribbon functions
int		R_CreateParticleRibbon(qhandle_t shader, int maxSegments, int lifeTime);
void	R_DestroyParticleRibbon(int ribbonId);
void	R_AddRibbonSegment(int ribbonId, const vec3_t start, const vec3_t end, const vec3_t color, float width);
void	R_SetRibbonActive(int ribbonId, qboolean active);

// Direct particle functions
void	R_AddParticleEnhanced(const vec3_t origin, const vec3_t velocity, const vec3_t color, const vec3_t colorEnd, 
							  float size, float sizeEnd, float life, qhandle_t shaderHandle, int physicsFlags);
void	R_AddParticleWithTrail(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, 
								qhandle_t shaderHandle, int trailId);
void	R_AddParticleWithAttachment(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life,
									 qhandle_t shaderHandle, int entityNum, const vec3_t offset);

// Global physics
void	R_SetGlobalWind(const vec3_t wind);
void	R_SetGlobalGravity(float gravity);

// Utility
int		R_GetActiveParticleCount(void);
int		R_GetActiveEmitterCount(void);

// CVAR declarations (extern)
extern cvar_t *r_particlesEnhanced;

#endif // __TR_PARTICLES_ENHANCED_H__

