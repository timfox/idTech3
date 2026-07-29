/* Engine.Physics Lua bindings and physics-table parsing helpers.
 * Included by g_lua_bindings.c so binding symbols remain file-local. */

/* ========== Physics bindings ========== */

static qboolean Lua_GetVec3Field( lua_State *L, int index, const char *field, vec3_t out )
{
	int absIndex;

	if ( !lua_istable( L, index ) ) {
		return qfalse;
	}

	absIndex = ID3_LUA_ABSINDEX( L, index );
	lua_getfield( L, absIndex, field );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return qfalse;
	}

	lua_rawgeti( L, -1, 1 );
	out[0] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );
	lua_rawgeti( L, -1, 2 );
	out[1] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );
	lua_rawgeti( L, -1, 3 );
	out[2] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );

	lua_pop( L, 1 );
	return qtrue;
}

static float Lua_GetNumberField( lua_State *L, int index, const char *field, float defaultValue )
{
	float value;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	value = (float)luaL_optnumber( L, -1, defaultValue );
	lua_pop( L, 1 );
	return value;
}

static int Lua_GetIntegerField( lua_State *L, int index, const char *field, int defaultValue )
{
	int value;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	value = (int)luaL_optinteger( L, -1, defaultValue );
	lua_pop( L, 1 );
	return value;
}

static qboolean Lua_GetBooleanField( lua_State *L, int index, const char *field, qboolean defaultValue )
{
	qboolean value = defaultValue;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	if ( !lua_isnoneornil( L, -1 ) ) {
		value = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	}
	lua_pop( L, 1 );
	return value;
}

static physShape_t Lua_ParsePhysShape( const char *shapeName )
{
	if ( !shapeName || !shapeName[0] || !Q_stricmp( shapeName, "box" ) ) {
		return PHYS_SHAPE_BOX;
	}
	if ( !Q_stricmp( shapeName, "sphere" ) ) {
		return PHYS_SHAPE_SPHERE;
	}
	if ( !Q_stricmp( shapeName, "capsule" ) ) {
		return PHYS_SHAPE_CAPSULE;
	}
	if ( !Q_stricmp( shapeName, "cylinder" ) ) {
		return PHYS_SHAPE_CYLINDER;
	}
	if ( !Q_stricmp( shapeName, "hull" ) || !Q_stricmp( shapeName, "convexHull" ) || !Q_stricmp( shapeName, "convex_hull" ) ) {
		return PHYS_SHAPE_CONVEX_HULL;
	}
	return PHYS_SHAPE_BOX;
}

static physBodyType_t Lua_ParsePhysBodyType( const char *typeName )
{
	if ( !typeName || !typeName[0] || !Q_stricmp( typeName, "dynamic" ) ) {
		return PHYS_BODY_DYNAMIC;
	}
	if ( !Q_stricmp( typeName, "static" ) ) {
		return PHYS_BODY_STATIC;
	}
	if ( !Q_stricmp( typeName, "kinematic" ) ) {
		return PHYS_BODY_KINEMATIC;
	}
	return PHYS_BODY_DYNAMIC;
}

static void Lua_ParsePhysBodyDefTable( lua_State *L, int index, physBodyDef_t *def, float **tempHullPointsOut )
{
	vec3_t v;
	const char *shapeName;
	const char *typeName;
	int absIndex;

	absIndex = ID3_LUA_ABSINDEX( L, index );
	shapeName = NULL;
	typeName = NULL;

	if ( tempHullPointsOut ) {
		*tempHullPointsOut = NULL;
	}

	lua_getfield( L, absIndex, "shape" );
	shapeName = lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : NULL;
	def->shape = Lua_ParsePhysShape( shapeName );
	lua_pop( L, 1 );

	lua_getfield( L, absIndex, "type" );
	typeName = lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : NULL;
	def->type = Lua_ParsePhysBodyType( typeName );
	lua_pop( L, 1 );

	if ( Lua_GetVec3Field( L, absIndex, "position", v ) ) {
		VectorCopy( v, def->position );
	} else {
		def->position[0] = Lua_GetNumberField( L, absIndex, "x", 0.0f );
		def->position[1] = Lua_GetNumberField( L, absIndex, "y", 0.0f );
		def->position[2] = Lua_GetNumberField( L, absIndex, "z", 0.0f );
	}

	if ( Lua_GetVec3Field( L, absIndex, "rotation", v ) || Lua_GetVec3Field( L, absIndex, "angles", v ) ) {
		VectorCopy( v, def->rotation );
	} else {
		def->rotation[0] = Lua_GetNumberField( L, absIndex, "pitch", 0.0f );
		def->rotation[1] = Lua_GetNumberField( L, absIndex, "yaw", 0.0f );
		def->rotation[2] = Lua_GetNumberField( L, absIndex, "roll", 0.0f );
	}

	if ( Lua_GetVec3Field( L, absIndex, "halfExtents", v ) || Lua_GetVec3Field( L, absIndex, "extents", v ) ) {
		VectorCopy( v, def->halfExtents );
	} else {
		float size = Lua_GetNumberField( L, absIndex, "size", 8.0f );
		def->halfExtents[0] = Lua_GetNumberField( L, absIndex, "hx", size );
		def->halfExtents[1] = Lua_GetNumberField( L, absIndex, "hy", size );
		def->halfExtents[2] = Lua_GetNumberField( L, absIndex, "hz", size );
	}

	def->radius = Lua_GetNumberField( L, absIndex, "radius", 8.0f );
	def->height = Lua_GetNumberField( L, absIndex, "height", 32.0f );
	def->mass = Lua_GetNumberField( L, absIndex, "mass", def->type == PHYS_BODY_DYNAMIC ? 10.0f : 0.0f );
	def->friction = Lua_GetNumberField( L, absIndex, "friction", 0.5f );
	def->restitution = Lua_GetNumberField( L, absIndex, "restitution", 0.3f );
	def->linearDamping = Lua_GetNumberField( L, absIndex, "linearDamping", 0.0f );
	def->angularDamping = Lua_GetNumberField( L, absIndex, "angularDamping", 0.0f );
	def->gravityScale = Lua_GetNumberField( L, absIndex, "gravityScale", 1.0f );
	def->motionLocks = Lua_GetIntegerField( L, absIndex, "motionLocks", 0 );
	def->isSensor = Lua_GetBooleanField( L, absIndex, "isSensor",
		Lua_GetBooleanField( L, absIndex, "sensor", qfalse ) );
	def->collisionGroup = Lua_GetIntegerField( L, absIndex, "collisionGroup", 1 );
	def->collisionMask = Lua_GetIntegerField( L, absIndex, "collisionMask", -1 );
	def->materialId = Lua_GetIntegerField( L, absIndex, "materialId", PHYS_MAT_DEFAULT );

	lua_getfield( L, absIndex, "material" );
	if ( lua_isstring( L, -1 ) ) {
		def->materialId = PhysMat_FindByName( lua_tostring( L, -1 ) );
	} else if ( lua_isnumber( L, -1 ) ) {
		def->materialId = (int)lua_tointeger( L, -1 );
	}
	lua_pop( L, 1 );

	if ( def->materialId >= 0 && def->materialId < PHYS_MAT_COUNT ) {
		PhysMat_ApplyToBodyDef( def, def->materialId );
	}

	lua_getfield( L, absIndex, "hullPoints" );
	if ( lua_istable( L, -1 ) ) {
		size_t rawCount = ID3_LUA_RAWLEN( L, -1 );
		int floatCount = (int)rawCount;
		int pointCount = floatCount / 3;
		float *pts;
		int i;

		if ( pointCount >= 4 && floatCount == pointCount * 3 ) {
			pts = (float *)Z_Malloc( (size_t)floatCount * sizeof( float ) );
			for ( i = 0; i < floatCount; i++ ) {
				lua_rawgeti( L, -1, i + 1 );
				pts[i] = (float)luaL_optnumber( L, -1, 0.0 );
				lua_pop( L, 1 );
			}
			def->shape = PHYS_SHAPE_CONVEX_HULL;
			def->hullPoints = pts;
			def->hullPointCount = pointCount;
			if ( tempHullPointsOut ) {
				*tempHullPointsOut = pts;
			}
		}
	}
	lua_pop( L, 1 );
}

static int l_phys_init(lua_State *L) { lua_pushboolean(L, Phys_Init()); return 1; }
static int l_phys_step(lua_State *L) { Phys_StepSimulation((float)luaL_checknumber(L,1)); return 0; }
static int l_phys_createBody(lua_State *L) {
	physBodyDef_t def;
	physBodyHandle_t handle;
	float *tempHullPoints = NULL;

	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_DYNAMIC;
	def.mass = 10.0f;
	def.radius = 8.0f;
	def.height = 32.0f;
	def.gravityScale = 1.0f;
	def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = 8.0f;
	def.friction = 0.5f;
	def.restitution = 0.3f;
	def.collisionGroup = 1;
	def.collisionMask = -1;
	def.materialId = PHYS_MAT_DEFAULT;

	if ( lua_istable( L, 1 ) ) {
		Lua_ParsePhysBodyDefTable( L, 1, &def, &tempHullPoints );
	} else {
		def.position[0]=(float)luaL_checknumber(L,1); def.position[1]=(float)luaL_checknumber(L,2); def.position[2]=(float)luaL_checknumber(L,3);
		def.mass=(float)luaL_optnumber(L,4,10); def.halfExtents[0]=def.halfExtents[1]=def.halfExtents[2]=(float)luaL_optnumber(L,5,8);
	}

	handle = Phys_CreateBody( &def );
	if ( tempHullPoints ) {
		Z_Free( tempHullPoints );
	}
	lua_pushinteger( L, handle );
	return 1;
}
static int l_phys_destroyBody(lua_State *L) { Phys_DestroyBody((int)luaL_checkinteger(L,1)); return 0; }
static int l_phys_applyImpulse(lua_State *L) {
	vec3_t imp,pt;
	imp[0]=(float)luaL_checknumber(L,2); imp[1]=(float)luaL_checknumber(L,3); imp[2]=(float)luaL_checknumber(L,4);
	pt[0]=(float)luaL_optnumber(L,5,0); pt[1]=(float)luaL_optnumber(L,6,0); pt[2]=(float)luaL_optnumber(L,7,0);
	Phys_ApplyImpulse((int)luaL_checkinteger(L,1), imp, pt);
	return 0;
}
static int l_phys_applyImpulseRadius(lua_State *L) {
	vec3_t center;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, Phys_ApplyImpulseRadius(center, (float)luaL_checknumber(L,4), (float)luaL_checknumber(L,5),
		(float)luaL_optnumber(L,6,1.0)));
	return 1;
}
static int l_phys_createBox(lua_State *L) {
	vec3_t origin, half;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	half[0]=half[1]=half[2]=(float)luaL_optnumber(L,4,8);
	lua_pushinteger(L, PhysProp_CreateBox(origin, half, PHYS_BODY_DYNAMIC, (float)luaL_optnumber(L,5,20), (int)luaL_optinteger(L,6,0)));
	return 1;
}
static int l_phys_createSphere(lua_State *L) {
	vec3_t origin;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, PhysProp_CreateSphere(origin, (float)luaL_optnumber(L,4,10), PHYS_BODY_DYNAMIC,
		(float)luaL_optnumber(L,5,15), (int)luaL_optinteger(L,6,0)));
	return 1;
}
static int l_phys_createStatic(lua_State *L) {
	vec3_t origin, half;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	half[0]=(float)luaL_optnumber(L,4,32); half[1]=(float)luaL_optnumber(L,5,32); half[2]=(float)luaL_optnumber(L,6,4);
	lua_pushinteger(L, PhysProp_CreateBox(origin, half, PHYS_BODY_STATIC, 0.0f, (int)luaL_optinteger(L,7,0)));
	return 1;
}
static int l_phys_createShadow(lua_State *L) {
	physShadowDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.origin[0]=(float)luaL_checknumber(L,1); def.origin[1]=(float)luaL_checknumber(L,2); def.origin[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=(float)luaL_optnumber(L,4,16); def.halfExtents[1]=(float)luaL_optnumber(L,5,16); def.halfExtents[2]=(float)luaL_optnumber(L,6,32);
	def.shape = PHYS_SHAPE_BOX;
	def.entityNum = (int)luaL_optinteger(L,7,-1);
	def.allowMovement = qtrue;
	def.allowRotation = qtrue;
	lua_pushinteger(L, PhysProp_CreateShadow(&def));
	return 1;
}
static int l_phys_setShadowPose(lua_State *L) {
	vec3_t origin, angles;
	origin[0]=(float)luaL_checknumber(L,2); origin[1]=(float)luaL_checknumber(L,3); origin[2]=(float)luaL_checknumber(L,4);
	angles[0]=(float)luaL_optnumber(L,5,0); angles[1]=(float)luaL_optnumber(L,6,0); angles[2]=(float)luaL_optnumber(L,7,0);
	PhysProp_SetShadowPose((int)luaL_checkinteger(L,1), origin, angles);
	return 0;
}
static int l_phys_createBuoyancy(lua_State *L) {
	physVolumeDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.type = PHYS_VOLUME_BUOYANCY;
	def.center[0]=(float)luaL_checknumber(L,1); def.center[1]=(float)luaL_checknumber(L,2); def.center[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=(float)luaL_optnumber(L,4,128); def.halfExtents[1]=(float)luaL_optnumber(L,5,128); def.halfExtents[2]=(float)luaL_optnumber(L,6,48);
	def.density=(float)luaL_optnumber(L,7,1.0); def.linearDrag=(float)luaL_optnumber(L,8,0.4); def.angularDrag=(float)luaL_optnumber(L,9,0.2);
	lua_pushinteger(L, PhysVolume_Create(&def));
	return 1;
}
static int l_phys_getTransform(lua_State *L) {
	physTransform_t t; Phys_GetBodyTransform((int)luaL_checkinteger(L,1), &t);
	lua_pushnumber(L,t.position[0]); lua_pushnumber(L,t.position[1]); lua_pushnumber(L,t.position[2]);
	lua_pushnumber(L,t.rotation[0]); lua_pushnumber(L,t.rotation[1]); lua_pushnumber(L,t.rotation[2]);
	return 6;
}
static int l_phys_setGravity(lua_State *L) {
	vec3_t gravity;
	gravity[0] = (float)luaL_checknumber( L, 1 );
	gravity[1] = (float)luaL_checknumber( L, 2 );
	gravity[2] = (float)luaL_checknumber( L, 3 );
	Phys_SetGravity( gravity );
	return 0;
}
static int l_phys_setTransform(lua_State *L) {
	vec3_t pos, rot;
	pos[0]=(float)luaL_checknumber(L,2); pos[1]=(float)luaL_checknumber(L,3); pos[2]=(float)luaL_checknumber(L,4);
	rot[0]=(float)luaL_optnumber(L,5,0); rot[1]=(float)luaL_optnumber(L,6,0); rot[2]=(float)luaL_optnumber(L,7,0);
	Phys_SetBodyTransform((int)luaL_checkinteger(L,1), pos, rot);
	return 0;
}
static int l_phys_rayCast(lua_State *L) {
	vec3_t from, to;
	physRayResult_t hit;
	physQueryFilter_t filter;
	const physQueryFilter_t *filterPtr = NULL;
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	if ( lua_gettop( L ) >= 8 ) {
		filter.categoryBits = (unsigned)luaL_checkinteger( L, 7 );
		filter.maskBits = (unsigned)luaL_checkinteger( L, 8 );
		filterPtr = &filter;
	}
	if ( !Phys_RayCastFiltered( from, to, &hit, filterPtr ) || !hit.hit ) {
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, 1);
	lua_pushnumber(L, hit.hitPoint[0]); lua_pushnumber(L, hit.hitPoint[1]); lua_pushnumber(L, hit.hitPoint[2]);
	lua_pushnumber(L, hit.fraction);
	lua_pushinteger(L, hit.body);
	lua_pushinteger(L, (lua_Integer)hit.userMaterialId);
	lua_pushinteger(L, hit.triangleIndex);
	return 7;
}
static int l_phys_createSensor(lua_State *L) {
	physBodyDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_STATIC;
	def.isSensor = qtrue;
	def.position[0]=(float)luaL_checknumber(L,1); def.position[1]=(float)luaL_checknumber(L,2); def.position[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=def.halfExtents[1]=def.halfExtents[2]=(float)luaL_optnumber(L,4,32);
	lua_pushinteger(L, Phys_CreateBody(&def));
	return 1;
}
static int l_phys_createConstraint(lua_State *L) {
	physConstraintDef_t def;
	const char *typeName;
	Com_Memset(&def, 0, sizeof(def));
	typeName = luaL_checkstring(L, 1);
	if ( !Q_stricmp( typeName, "hinge" ) ) {
		def.type = PHYS_CONSTRAINT_HINGE;
	} else if ( !Q_stricmp( typeName, "slider" ) ) {
		def.type = PHYS_CONSTRAINT_SLIDER;
	} else if ( !Q_stricmp( typeName, "distance" ) ) {
		def.type = PHYS_CONSTRAINT_DISTANCE;
	} else if ( !Q_stricmp( typeName, "wheel" ) ) {
		def.type = PHYS_CONSTRAINT_WHEEL;
	} else if ( !Q_stricmp( typeName, "motor" ) ) {
		def.type = PHYS_CONSTRAINT_MOTOR;
	} else if ( !Q_stricmp( typeName, "fixed" ) ) {
		def.type = PHYS_CONSTRAINT_FIXED;
	} else if ( !Q_stricmp( typeName, "filter" ) ) {
		def.type = PHYS_CONSTRAINT_FILTER;
	} else if ( !Q_stricmp( typeName, "parallel" ) ) {
		def.type = PHYS_CONSTRAINT_PARALLEL;
	} else if ( !Q_stricmp( typeName, "cone" ) || !Q_stricmp( typeName, "spherical" ) ) {
		def.type = PHYS_CONSTRAINT_CONE_TWIST;
	} else {
		def.type = PHYS_CONSTRAINT_POINT;
	}
	def.bodyA = (physBodyHandle_t)luaL_checkinteger(L, 2);
	def.bodyB = (physBodyHandle_t)luaL_checkinteger(L, 3);
	def.lowerLimit = (float)luaL_optnumber(L, 4, 0);
	def.upperLimit = (float)luaL_optnumber(L, 5, 0);
	def.coneAngle = (float)luaL_optnumber(L, 6, 0);
	def.springHertz = (float)luaL_optnumber(L, 7, 0);
	def.springDamping = (float)luaL_optnumber(L, 8, 0);
	def.axisA[2] = 1.0f;
	def.disableCollision = qtrue;
	lua_pushinteger(L, Phys_CreateConstraint(&def));
	return 1;
}
static int l_phys_moverStep(lua_State *L) {
	vec3_t origin, velocity, wish;
	qboolean grounded = qfalse;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	velocity[0]=(float)luaL_checknumber(L,4); velocity[1]=(float)luaL_checknumber(L,5); velocity[2]=(float)luaL_checknumber(L,6);
	wish[0]=(float)luaL_optnumber(L,7,0); wish[1]=(float)luaL_optnumber(L,8,0); wish[2]=(float)luaL_optnumber(L,9,0);
	lua_pushboolean(L, Phys_MoverStep(origin, velocity, (float)luaL_optnumber(L,10,15), (float)luaL_optnumber(L,11,56),
		wish, (float)luaL_optnumber(L,12,320), (float)luaL_optnumber(L,13,0.016), lua_toboolean(L,14), &grounded));
	lua_pushnumber(L, origin[0]); lua_pushnumber(L, origin[1]); lua_pushnumber(L, origin[2]);
	lua_pushnumber(L, velocity[0]); lua_pushnumber(L, velocity[1]); lua_pushnumber(L, velocity[2]);
	lua_pushboolean(L, grounded);
	return 8;
}
static int l_phys_pmoveCorrect(lua_State *L) {
	vec3_t origin, velocity;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	velocity[0]=(float)luaL_checknumber(L,4); velocity[1]=(float)luaL_checknumber(L,5); velocity[2]=(float)luaL_checknumber(L,6);
	lua_pushinteger(L, Phys_PmoveCorrect(origin, velocity, (float)luaL_optnumber(L,7,15),
		(float)luaL_optnumber(L,8,56), (float)luaL_optnumber(L,9,0.016)));
	lua_pushnumber(L, origin[0]); lua_pushnumber(L, origin[1]); lua_pushnumber(L, origin[2]);
	lua_pushnumber(L, velocity[0]); lua_pushnumber(L, velocity[1]); lua_pushnumber(L, velocity[2]);
	return 7;
}
static int l_phys_addHeightField(lua_State *L) {
	/* Lua: heights table flat row-major, countX, countY, cellSize, origin */
	float heights[256];
	vec3_t origin;
	int countX, countY, n, i;
	countX = (int)luaL_checkinteger(L, 2);
	countY = (int)luaL_checkinteger(L, 3);
	n = countX * countY;
	if ( n < 4 || n > 256 || !lua_istable(L, 1) ) {
		lua_pushinteger(L, -1);
		return 1;
	}
	for ( i = 0; i < n; i++ ) {
		lua_rawgeti(L, 1, i + 1);
		heights[i] = (float)luaL_optnumber(L, -1, 0);
		lua_pop(L, 1);
	}
	origin[0]=(float)luaL_optnumber(L,5,0); origin[1]=(float)luaL_optnumber(L,6,0); origin[2]=(float)luaL_optnumber(L,7,0);
	lua_pushinteger(L, Phys_AddStaticHeightField(heights, countX, countY, (float)luaL_optnumber(L,4,32), 1.0f, origin));
	return 1;
}
static int l_phys_backend(lua_State *L) {
	lua_pushstring(L, Phys_GetBackendName());
	return 1;
}
static int l_phys_createRagdoll(lua_State *L) {
	physBoundRagdoll_t bound;
	vec3_t origin;
	const char *path = luaL_optstring(L, 1, NULL);
	origin[0] = (float)luaL_optnumber(L, 2, 0);
	origin[1] = (float)luaL_optnumber(L, 3, 0);
	origin[2] = (float)luaL_optnumber(L, 4, 64);
	if ( !Phys_RagdollSpawnBound( path, origin, &bound ) ) {
		lua_pushinteger(L, -1);
		return 1;
	}
	lua_pushinteger(L, bound.ragdoll);
	lua_pushinteger(L, bound.anim);
	lua_pushinteger(L, bound.motor);
	return 3;
}
static int l_phys_loadRagdoll(lua_State *L) {
	physRagdollDef_t def;
	lua_pushboolean(L, Phys_RagdollLoadDef(luaL_checkstring(L, 1), &def));
	lua_pushinteger(L, def.numBones);
	return 2;
}
static int l_phys_setBoneAnimTarget(lua_State *L) {
	vec3_t pos, rot;
	pos[0]=(float)luaL_checknumber(L,3); pos[1]=(float)luaL_checknumber(L,4); pos[2]=(float)luaL_checknumber(L,5);
	rot[0]=(float)luaL_optnumber(L,6,0); rot[1]=(float)luaL_optnumber(L,7,0); rot[2]=(float)luaL_optnumber(L,8,0);
	Phys_RagdollSetBoneAnimTarget((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), pos, rot);
	return 0;
}
static int l_phys_subscribe(lua_State *L) {
	const char *typeName = luaL_optstring(L, 1, "impact");
	Com_Printf( "[physics] Engine.Physics.subscribe(%s) — use pollEvent() for Soft Step bus\n", typeName );
	lua_pushboolean(L, 1);
	return 1;
}
static int l_phys_setFriction(lua_State *L) {
	Phys_SetBodyFriction( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setRestitution(lua_State *L) {
	Phys_SetBodyRestitution( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_validateReplay(lua_State *L) {
	lua_pushboolean( L, Phys_ValidateReplay( luaL_optstring( L, 1, "phys_recording.bin" ) ) );
	return 1;
}
static int l_phys_setFilter(lua_State *L) {
	Phys_SetBodyFilterEx( (physBodyHandle_t)luaL_checkinteger( L, 1 ),
		(int)luaL_checkinteger( L, 2 ), (int)luaL_checkinteger( L, 3 ),
		(int)luaL_optinteger( L, 4, 0 ) );
	return 0;
}
static int l_phys_attachShape(lua_State *L) {
	physBodyDef_t def;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_BOX;
	def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = (float)luaL_optnumber( L, 2, 8 );
	if ( lua_gettop( L ) >= 4 ) {
		def.halfExtents[0] = (float)luaL_checknumber( L, 2 );
		def.halfExtents[1] = (float)luaL_checknumber( L, 3 );
		def.halfExtents[2] = (float)luaL_checknumber( L, 4 );
	}
	lua_pushinteger( L, Phys_AttachShape( (physBodyHandle_t)luaL_checkinteger( L, 1 ), &def ) );
	return 1;
}
static int l_phys_convexSweep(lua_State *L) {
	physBodyDef_t def;
	physRayResult_t hit;
	vec3_t from, to, rot;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_SPHERE;
	def.radius = (float)luaL_optnumber( L, 7, 8 );
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	VectorClear( rot );
	if ( !Phys_ConvexSweep( &def, from, to, rot, &hit ) || !hit.hit ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, hit.fraction );
	lua_pushinteger( L, hit.body );
	lua_pushnumber( L, hit.hitPoint[0] ); lua_pushnumber( L, hit.hitPoint[1] ); lua_pushnumber( L, hit.hitPoint[2] );
	return 6;
}
static int l_phys_overlapSphere(lua_State *L) {
	vec3_t center;
	physBodyHandle_t hits[32];
	int n, i;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	n = Phys_OverlapSphere( center, (float)luaL_optnumber( L, 4, 32 ), hits, 32 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_pushinteger( L, hits[i] );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_overlapBox(lua_State *L) {
	vec3_t center, halfExtents;
	physBodyHandle_t hits[32];
	int n, i;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	halfExtents[0]=(float)luaL_optnumber(L,4,32); halfExtents[1]=(float)luaL_optnumber(L,5,32); halfExtents[2]=(float)luaL_optnumber(L,6,32);
	n = Phys_OverlapBox( center, halfExtents, hits, 32 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_pushinteger( L, hits[i] );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_getContacts(lua_State *L) {
	physContact_t contacts[16];
	int n, i;
	n = Phys_GetBodyContacts( (physBodyHandle_t)luaL_checkinteger( L, 1 ), contacts, 16 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_createtable( L, 0, 5 );
		lua_pushinteger( L, contacts[i].otherBody ); lua_setfield( L, -2, "other" );
		lua_pushnumber( L, contacts[i].point[0] ); lua_setfield( L, -2, "x" );
		lua_pushnumber( L, contacts[i].point[1] ); lua_setfield( L, -2, "y" );
		lua_pushnumber( L, contacts[i].point[2] ); lua_setfield( L, -2, "z" );
		lua_pushnumber( L, contacts[i].normalImpulse ); lua_setfield( L, -2, "impulse" );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_setConstraintSpring(lua_State *L) {
	Phys_SetConstraintSpring( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		lua_toboolean( L, 2 ) ? qtrue : qfalse,
		(float)luaL_optnumber( L, 3, 8 ), (float)luaL_optnumber( L, 4, 0.7 ) );
	return 0;
}
static int l_phys_setSphericalLimits(lua_State *L) {
	Phys_SetSphericalLimits( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, -1 ), (float)luaL_optnumber( L, 4, 1 ) );
	return 0;
}
static int l_phys_setWheelSteering(lua_State *L) {
	Phys_SetWheelSteering( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, 500 ) );
	return 0;
}
static int l_phys_pollEvent(lua_State *L) {
	phys_event_t ev;
	if ( !PhysEvent_Poll( &ev ) ) {
		lua_pushnil( L );
		return 1;
	}
	lua_createtable( L, 0, 10 );
	lua_pushinteger( L, (int)ev.type ); lua_setfield( L, -2, "type" );
	lua_pushinteger( L, ev.bodyA ); lua_setfield( L, -2, "bodyA" );
	lua_pushinteger( L, ev.bodyB ); lua_setfield( L, -2, "bodyB" );
	lua_pushinteger( L, ev.ragdoll ); lua_setfield( L, -2, "ragdoll" );
	lua_pushinteger( L, ev.bone ); lua_setfield( L, -2, "bone" );
	lua_pushnumber( L, ev.point[0] ); lua_setfield( L, -2, "x" );
	lua_pushnumber( L, ev.point[1] ); lua_setfield( L, -2, "y" );
	lua_pushnumber( L, ev.point[2] ); lua_setfield( L, -2, "z" );
	lua_pushnumber( L, ev.magnitude ); lua_setfield( L, -2, "magnitude" );
	return 1;
}
static int l_phys_forceAnimState(lua_State *L) {
	ProcAnim_ForceState( (procAnimHandle_t)luaL_checkinteger( L, 1 ),
		(procAnimState_t)luaL_checkinteger( L, 2 ) );
	return 0;
}
static int l_phys_hitRagdoll(lua_State *L) {
	vec3_t point, impulse;
	procAnimHandle_t anim = (procAnimHandle_t)luaL_checkinteger( L, 1 );
	physMotorHandle_t motor = (physMotorHandle_t)luaL_optinteger( L, 2, -1 );
	int bone = (int)luaL_optinteger( L, 3, PROCANIM_BONE_SPINE );
	impulse[0] = (float)luaL_optnumber( L, 4, 200 );
	impulse[1] = (float)luaL_optnumber( L, 5, 0 );
	impulse[2] = (float)luaL_optnumber( L, 6, 100 );
	point[0] = (float)luaL_optnumber( L, 7, 0 );
	point[1] = (float)luaL_optnumber( L, 8, 0 );
	point[2] = (float)luaL_optnumber( L, 9, 0 );
	PhysMiddleware_DispatchHit( -1, anim, motor, bone, 0, point, impulse );
	return 0;
}
static int l_phys_createDmm(lua_State *L) {
	dmmObjectDef_t def;
	dmmFracturePattern_t pattern;
	dmmObjectHandle_t h;
	float edge;
	Com_Memset( &def, 0, sizeof( def ) );
	def.position[0] = (float)luaL_checknumber( L, 1 );
	def.position[1] = (float)luaL_checknumber( L, 2 );
	def.position[2] = (float)luaL_checknumber( L, 3 );
	edge = (float)luaL_optnumber( L, 4, 48 );
	VectorSet( def.dimensions, edge, edge, edge );
	def.material = DMM_CONCRETE;
	def.density = (float)luaL_optnumber( L, 5, 2.4 );
	def.gridResolution = (int)luaL_optinteger( L, 6, 6 );
	def.deformability = 1.0f;
	Dmm_GenerateVoronoiPattern( def.position, edge * 0.5f, 8, &pattern );
	h = Dmm_CreateEnhanced( &def, &pattern );
	lua_pushinteger( L, h );
	return 1;
}
static int l_phys_fractureDmm(lua_State *L) {
	vec3_t point;
	int n;
	point[0] = (float)luaL_optnumber( L, 2, 0 );
	point[1] = (float)luaL_optnumber( L, 3, 0 );
	point[2] = (float)luaL_optnumber( L, 4, 0 );
	n = Dmm_Fracture( (dmmObjectHandle_t)luaL_checkinteger( L, 1 ), point,
		(float)luaL_optnumber( L, 5, 800 ) );
	lua_pushinteger( L, n );
	return 1;
}
static int l_phys_dmmStatus(lua_State *L) {
	lua_pushinteger( L, Dmm_GetActiveCount() );
	return 1;
}
static int l_phys_spawnBoundAlive(lua_State *L) {
	physBoundRagdoll_t bound;
	vec3_t origin;
	const char *path = luaL_optstring( L, 1, NULL );
	origin[0] = (float)luaL_optnumber( L, 2, 0 );
	origin[1] = (float)luaL_optnumber( L, 3, 0 );
	origin[2] = (float)luaL_optnumber( L, 4, 64 );
	if ( !Phys_RagdollSpawnBoundEx( path, origin, &bound, qfalse ) ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	lua_pushinteger( L, bound.ragdoll );
	lua_pushinteger( L, bound.anim );
	lua_pushinteger( L, bound.motor );
	return 3;
}
static int l_phys_getClosestPoint(lua_State *L) {
	vec3_t target, closest;
	float dist;
	target[0] = (float)luaL_checknumber( L, 2 );
	target[1] = (float)luaL_checknumber( L, 3 );
	target[2] = (float)luaL_checknumber( L, 4 );
	if ( !Phys_GetClosestPoint( (physBodyHandle_t)luaL_checkinteger( L, 1 ), target, closest, &dist ) ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, closest[0] ); lua_pushnumber( L, closest[1] ); lua_pushnumber( L, closest[2] );
	lua_pushnumber( L, dist );
	return 5;
}
static int l_phys_sphereTOI(lua_State *L) {
	vec3_t from, to;
	physRayResult_t hit;
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	if ( !Phys_SphereTimeOfImpact( from, to, (float)luaL_optnumber( L, 7, 8 ),
			(physBodyHandle_t)luaL_optinteger( L, 8, -1 ), &hit ) || !hit.hit ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, hit.fraction );
	lua_pushinteger( L, hit.body );
	lua_pushnumber( L, hit.hitPoint[0] ); lua_pushnumber( L, hit.hitPoint[1] ); lua_pushnumber( L, hit.hitPoint[2] );
	return 6;
}
static int l_phys_setContinuous(lua_State *L) {
	Phys_SetBodyContinuous( (physBodyHandle_t)luaL_checkinteger( L, 1 ), lua_toboolean( L, 2 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setSleepEnabled(lua_State *L) {
	Phys_SetBodySleepEnabled( (physBodyHandle_t)luaL_checkinteger( L, 1 ), lua_toboolean( L, 2 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setSleepThreshold(lua_State *L) {
	Phys_SetBodySleepThreshold( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setHingeTarget(lua_State *L) {
	Phys_SetHingeTargetAngle( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setSliderTarget(lua_State *L) {
	Phys_SetSliderTarget( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setDistanceLength(lua_State *L) {
	Phys_SetDistanceLength( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_rebuildTree(lua_State *L) {
	(void)L;
	Phys_RebuildStaticTree();
	return 0;
}
static int l_phys_setContactTuning(lua_State *L) {
	Phys_SetContactTuning( (float)luaL_checknumber( L, 1 ), (float)luaL_optnumber( L, 2, 0.7f ),
		(float)luaL_optnumber( L, 3, 0.0f ) );
	return 0;
}
static int l_phys_setMaxLinearSpeed(lua_State *L) {
	Phys_SetMaxLinearSpeed( (float)luaL_checknumber( L, 1 ) );
	return 0;
}
static int l_phys_enableSpeculative(lua_State *L) {
	Phys_EnableSpeculative( lua_toboolean( L, 1 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setDebugDrawFlags(lua_State *L) {
	Phys_SetDebugDrawFlags( (unsigned)luaL_checkinteger( L, 1 ) );
	return 0;
}
static int l_phys_stats(lua_State *L) {
	lua_createtable( L, 0, 4 );
	lua_pushstring( L, Phys_GetBackendName() ); lua_setfield( L, -2, "backend" );
	lua_pushinteger( L, Phys_GetWorkerCount() ); lua_setfield( L, -2, "workers" );
	lua_pushinteger( L, Phys_GetBodyCount() ); lua_setfield( L, -2, "bodies" );
	lua_pushinteger( L, Phys_GetConstraintCount() ); lua_setfield( L, -2, "constraints" );
	return 1;
}
static int l_phys_replayOpen(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayOpen( luaL_optstring( L, 1, "phys_recording.bin" ) ) );
	return 1;
}
static int l_phys_replayStep(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayStep() );
	return 1;
}
static int l_phys_replaySeek(lua_State *L) {
	Phys_ReplaySeek( (int)luaL_checkinteger( L, 1 ) );
	return 0;
}
static int l_phys_replayClose(lua_State *L) {
	(void)L;
	Phys_ReplayClose();
	return 0;
}
static int l_phys_replayStatus(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayIsOpen() );
	lua_pushinteger( L, Phys_ReplayGetFrame() );
	lua_pushinteger( L, Phys_ReplayGetFrameCount() );
	lua_pushboolean( L, Phys_ReplayHasDiverged() );
	return 4;
}
static int l_phys_setWheelSuspension(lua_State *L) {
	Phys_SetWheelSuspension( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_optnumber( L, 2, 8 ), (float)luaL_optnumber( L, 3, 0.7 ),
		(float)luaL_optnumber( L, 4, -8 ), (float)luaL_optnumber( L, 5, 8 ) );
	return 0;
}
static int l_phys_setWheelSpin(lua_State *L) {
	Phys_SetWheelSpin( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, 500 ) );
	return 0;
}
static int l_phys_setMotorVelocities(lua_State *L) {
	vec3_t lin, ang;
	lin[0]=(float)luaL_optnumber(L,2,0); lin[1]=(float)luaL_optnumber(L,3,0); lin[2]=(float)luaL_optnumber(L,4,0);
	ang[0]=(float)luaL_optnumber(L,5,0); ang[1]=(float)luaL_optnumber(L,6,0); ang[2]=(float)luaL_optnumber(L,7,0);
	Phys_SetMotorVelocities( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), lin, ang,
		(float)luaL_optnumber( L, 8, 0 ), (float)luaL_optnumber( L, 9, 0 ) );
	return 0;
}
static int l_phys_setSphericalTarget(lua_State *L) {
	vec3_t rot;
	rot[0]=(float)luaL_optnumber(L,2,0); rot[1]=(float)luaL_optnumber(L,3,0); rot[2]=(float)luaL_optnumber(L,4,0);
	Phys_SetSphericalTarget( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), rot );
	return 0;
}
static int l_phys_setBodyDamping(lua_State *L) {
	Phys_SetBodyDamping( (physBodyHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, -1 ) );
	return 0;
}
static int l_phys_setBodyType(lua_State *L) {
	physBodyType_t t = PHYS_BODY_DYNAMIC;
	const char *name = luaL_optstring( L, 2, "dynamic" );
	if ( !Q_stricmp( name, "static" ) ) {
		t = PHYS_BODY_STATIC;
	} else if ( !Q_stricmp( name, "kinematic" ) ) {
		t = PHYS_BODY_KINEMATIC;
	}
	Phys_SetBodyType( (physBodyHandle_t)luaL_checkinteger( L, 1 ), t );
	return 0;
}
static int l_phys_applyWind(lua_State *L) {
	vec3_t wind;
	wind[0]=(float)luaL_checknumber(L,2); wind[1]=(float)luaL_checknumber(L,3); wind[2]=(float)luaL_checknumber(L,4);
	Phys_ApplyWind( (physBodyHandle_t)luaL_checkinteger( L, 1 ), wind,
		(float)luaL_optnumber( L, 5, 0.5 ), (float)luaL_optnumber( L, 6, 0.2 ),
		(float)luaL_optnumber( L, 7, 400 ) );
	return 0;
}
static int l_phys_explode(lua_State *L) {
	vec3_t center;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger( L, Phys_Explode( center, (float)luaL_checknumber( L, 4 ),
		(float)luaL_checknumber( L, 5 ), (float)luaL_optnumber( L, 6, 1 ),
		(unsigned)luaL_optinteger( L, 7, 0 ) ) );
	return 1;
}
static int l_phys_rayCastAll(lua_State *L) {
	vec3_t from, to;
	physRayResult_t hits[32];
	physQueryFilter_t filter;
	const physQueryFilter_t *filterPtr = NULL;
	int n, i;
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	if ( lua_gettop( L ) >= 8 ) {
		filter.categoryBits = (unsigned)luaL_checkinteger( L, 7 );
		filter.maskBits = (unsigned)luaL_checkinteger( L, 8 );
		filterPtr = &filter;
	}
	n = Phys_RayCastAll( from, to, hits, 32, filterPtr );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_createtable( L, 0, 6 );
		lua_pushnumber( L, hits[i].fraction ); lua_setfield( L, -2, "fraction" );
		lua_pushinteger( L, hits[i].body ); lua_setfield( L, -2, "body" );
		lua_pushnumber( L, hits[i].hitPoint[0] ); lua_setfield( L, -2, "x" );
		lua_pushnumber( L, hits[i].hitPoint[1] ); lua_setfield( L, -2, "y" );
		lua_pushnumber( L, hits[i].hitPoint[2] ); lua_setfield( L, -2, "z" );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_setGravityScale(lua_State *L) {
	Phys_SetBodyGravityScale( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setMotionLocks(lua_State *L) {
	Phys_SetBodyMotionLocks( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (int)luaL_checkinteger( L, 2 ) );
	return 0;
}
static int l_phys_setVelocity(lua_State *L) {
	vec3_t lin, ang;
	lin[0]=(float)luaL_checknumber(L,2); lin[1]=(float)luaL_checknumber(L,3); lin[2]=(float)luaL_checknumber(L,4);
	ang[0]=(float)luaL_optnumber(L,5,0); ang[1]=(float)luaL_optnumber(L,6,0); ang[2]=(float)luaL_optnumber(L,7,0);
	Phys_SetBodyVelocity( (physBodyHandle_t)luaL_checkinteger( L, 1 ), lin, ang );
	return 0;
}
static int l_phys_applyForce(lua_State *L) {
	vec3_t force, point;
	force[0]=(float)luaL_checknumber(L,2); force[1]=(float)luaL_checknumber(L,3); force[2]=(float)luaL_checknumber(L,4);
	point[0]=(float)luaL_optnumber(L,5,0); point[1]=(float)luaL_optnumber(L,6,0); point[2]=(float)luaL_optnumber(L,7,0);
	Phys_ApplyForce( (physBodyHandle_t)luaL_checkinteger( L, 1 ), force, point );
	return 0;
}
static int l_phys_applyTorque(lua_State *L) {
	vec3_t torque;
	torque[0]=(float)luaL_checknumber(L,2); torque[1]=(float)luaL_checknumber(L,3); torque[2]=(float)luaL_checknumber(L,4);
	Phys_ApplyTorque( (physBodyHandle_t)luaL_checkinteger( L, 1 ), torque );
	return 0;
}
static int l_phys_destroyConstraint(lua_State *L) {
	Phys_DestroyConstraint( (physConstraintHandle_t)luaL_checkinteger( L, 1 ) );
	return 0;
}
static int l_phys_setTargetTransform(lua_State *L) {
	vec3_t pos, rot;
	pos[0]=(float)luaL_checknumber(L,2); pos[1]=(float)luaL_checknumber(L,3); pos[2]=(float)luaL_checknumber(L,4);
	rot[0]=(float)luaL_optnumber(L,5,0); rot[1]=(float)luaL_optnumber(L,6,0); rot[2]=(float)luaL_optnumber(L,7,0);
	Phys_SetBodyTargetTransform( (physBodyHandle_t)luaL_checkinteger( L, 1 ), pos, rot,
		(float)luaL_optnumber( L, 8, 0.016f ) );
	return 0;
}
