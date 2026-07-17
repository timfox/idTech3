/* Engine Lua table registration.
 * Included by g_lua_bindings.c so binding functions stay file-local. */

/* ========== Registration ========== */

static void registerTable(lua_State *L, const char *name, const luaL_Reg *funcs) {
	lua_newtable(L);
	const luaL_Reg *f;
	for (f = funcs; f->name; f++) {
		lua_pushcfunction(L, f->func);
		lua_setfield(L, -2, f->name);
	}
	lua_setfield(L, -2, name);
}

void LuaBindings_RegisterAll(void *luaState) {
	lua_State *L = (lua_State *)luaState;
	if (!L) return;

	lua_newtable(L);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg directorFuncs[] = {
		{"init", l_director_init}, {"update", l_director_update}, {"getPhase", l_director_getPhase},
		{"forcePhase", l_director_forcePhase}, {"getIntensity", l_director_getIntensity},
		{"getPlayerIntensity", l_director_getPlayerIntensity}, {"getPlayerStress", l_director_getPlayerStress},
		{"triggerWave", l_director_triggerWave}, {"updatePlayer", l_director_updatePlayer},
		{"playerKill", l_director_playerKill}, {"playerDeath", l_director_playerDeath},
		{"playerDamage", l_director_playerDamage}, {"addSpawnType", l_director_addSpawnType},
		{"shouldSpawn", l_director_shouldSpawn}, {"pickSpawnType", l_director_pickSpawnType},
		{"addZone", l_director_addZone},
		{NULL, NULL}
	};
	registerTable(L, "Director", directorFuncs);
#endif

	static const luaL_Reg navFuncs[] = {
		{"init", l_nav_init}, {"buildFromBSP", l_nav_buildFromBSP},
		{"findPath", l_nav_findPath}, {"addAgent", l_nav_addAgent},
		{NULL, NULL}
	};
	registerTable(L, "Nav", navFuncs);

		static const luaL_Reg physicsFuncs[] = {
			{"init", l_phys_init}, {"step", l_phys_step}, {"createBody", l_phys_createBody},
			{"destroyBody", l_phys_destroyBody}, {"applyImpulse", l_phys_applyImpulse},
			{"applyImpulseRadius", l_phys_applyImpulseRadius},
			{"createBox", l_phys_createBox}, {"createSphere", l_phys_createSphere},
			{"createStatic", l_phys_createStatic}, {"createShadow", l_phys_createShadow},
			{"setShadowPose", l_phys_setShadowPose}, {"createBuoyancy", l_phys_createBuoyancy},
			{"getTransform", l_phys_getTransform}, {"setTransform", l_phys_setTransform},
			{"setGravity", l_phys_setGravity},
			{"rayCast", l_phys_rayCast}, {"createSensor", l_phys_createSensor},
			{"createConstraint", l_phys_createConstraint}, {"moverStep", l_phys_moverStep},
			{"pmoveCorrect", l_phys_pmoveCorrect}, {"addHeightField", l_phys_addHeightField},
			{"backend", l_phys_backend}, {"createRagdoll", l_phys_createRagdoll},
			{"loadRagdoll", l_phys_loadRagdoll}, {"setBoneAnimTarget", l_phys_setBoneAnimTarget},
		{"subscribe", l_phys_subscribe},
		{"setFriction", l_phys_setFriction}, {"setRestitution", l_phys_setRestitution},
		{"validateReplay", l_phys_validateReplay},
			{"setFilter", l_phys_setFilter}, {"attachShape", l_phys_attachShape},
			{"convexSweep", l_phys_convexSweep}, {"overlapSphere", l_phys_overlapSphere},
			{"overlapBox", l_phys_overlapBox},
			{"getContacts", l_phys_getContacts}, {"setConstraintSpring", l_phys_setConstraintSpring},
			{"setSphericalLimits", l_phys_setSphericalLimits}, {"setWheelSteering", l_phys_setWheelSteering},
			{"pollEvent", l_phys_pollEvent},
			{"forceAnimState", l_phys_forceAnimState}, {"hitRagdoll", l_phys_hitRagdoll},
			{"createDmm", l_phys_createDmm}, {"fractureDmm", l_phys_fractureDmm},
			{"dmmStatus", l_phys_dmmStatus}, {"spawnBoundAlive", l_phys_spawnBoundAlive},
			{"getClosestPoint", l_phys_getClosestPoint}, {"sphereTOI", l_phys_sphereTOI},
			{"setContinuous", l_phys_setContinuous}, {"setSleepEnabled", l_phys_setSleepEnabled},
			{"setSleepThreshold", l_phys_setSleepThreshold},
			{"setHingeTarget", l_phys_setHingeTarget}, {"setSliderTarget", l_phys_setSliderTarget},
			{"setDistanceLength", l_phys_setDistanceLength}, {"rebuildTree", l_phys_rebuildTree},
			{"setContactTuning", l_phys_setContactTuning}, {"setMaxLinearSpeed", l_phys_setMaxLinearSpeed},
			{"enableSpeculative", l_phys_enableSpeculative}, {"setDebugDrawFlags", l_phys_setDebugDrawFlags},
			{"stats", l_phys_stats},
			{"replayOpen", l_phys_replayOpen}, {"replayStep", l_phys_replayStep},
			{"replaySeek", l_phys_replaySeek}, {"replayClose", l_phys_replayClose},
			{"replayStatus", l_phys_replayStatus},
			{"setWheelSuspension", l_phys_setWheelSuspension}, {"setWheelSpin", l_phys_setWheelSpin},
			{"setMotorVelocities", l_phys_setMotorVelocities}, {"setSphericalTarget", l_phys_setSphericalTarget},
			{"setBodyDamping", l_phys_setBodyDamping}, {"setBodyType", l_phys_setBodyType},
			{"applyWind", l_phys_applyWind}, {"explode", l_phys_explode}, {"rayCastAll", l_phys_rayCastAll},
			{"setGravityScale", l_phys_setGravityScale}, {"setMotionLocks", l_phys_setMotionLocks},
			{"setVelocity", l_phys_setVelocity}, {"applyForce", l_phys_applyForce},
			{"applyTorque", l_phys_applyTorque}, {"destroyConstraint", l_phys_destroyConstraint},
			{"setTargetTransform", l_phys_setTargetTransform},
		{NULL, NULL}
	};
	registerTable(L, "Physics", physicsFuncs);

	static const luaL_Reg particleFuncs[] = {
		{"init", l_particles_init}, {"clear", l_particles_clear},
		{"emitSmoke", l_particles_emitSmoke}, {"emitSparks", l_particles_emitSparks},
		{"count", l_particles_count},
		{NULL, NULL}
	};
	registerTable(L, "Particles", particleFuncs);

	static const luaL_Reg musicFuncs[] = {
		{"init", l_music_init}, {"addLayer", l_music_addLayer},
		{"setIntensity", l_music_setIntensity}, {"addStinger", l_music_addStinger},
		{"fadeToSilence", l_music_fadeToSilence},
		{NULL, NULL}
	};
	registerTable(L, "Music", musicFuncs);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg faceFuncs[] = {
		{"create", l_face_create}, {"destroy", l_face_destroy},
		{"setExpression", l_face_setExpression}, {"setFlex", l_face_setFlex},
		{"setPhoneme", l_face_setPhoneme}, {"setBlinkRate", l_face_setBlinkRate},
		{"setAU", l_face_setAU}, {"setAUSide", l_face_setAUSide},
		{"getAU", l_face_getAU}, {"clearAUs", l_face_clearAUs},
		{"auName", l_face_auName},
		{NULL, NULL}
	};
	registerTable(L, "Face", faceFuncs);
	/* FACS AU enum constants on Engine.Face (AU12 = index, etc.) */
	lua_getglobal(L, "Engine");
	lua_getfield(L, -1, "Face");
	{
		int ai;
		for (ai = 0; ai < FACS_AU_COUNT; ai++) {
			lua_pushinteger(L, ai);
			lua_setfield(L, -2, Face_AUName((facsActionUnit_t)ai));
		}
		lua_pushinteger(L, FACS_SIDE_BOTH); lua_setfield(L, -2, "SIDE_BOTH");
		lua_pushinteger(L, FACS_SIDE_LEFT); lua_setfield(L, -2, "SIDE_LEFT");
		lua_pushinteger(L, FACS_SIDE_RIGHT); lua_setfield(L, -2, "SIDE_RIGHT");
	}
	lua_pop(L, 2);

	static const luaL_Reg hordeFuncs[] = {
		{"init", l_horde_init}, {"spawn", l_horde_spawn}, {"kill", l_horde_kill},
		{"setTarget", l_horde_setTarget}, {"getState", l_horde_getState},
		{"getCount", l_horde_getCount}, {"createGroup", l_horde_createGroup},
		{NULL, NULL}
	};
	registerTable(L, "Horde", hordeFuncs);

	static const luaL_Reg dismemberFuncs[] = {
		{"create", l_dismember_create}, {"damage", l_dismember_damage},
		{"sever", l_dismember_sever}, {"explode", l_dismember_explode},
		{"isAttached", l_dismember_isAttached},
		{NULL, NULL}
	};
	registerTable(L, "Dismember", dismemberFuncs);

	static const luaL_Reg choreoFuncs[] = {
		{"create", l_choreo_create}, {"addActor", l_choreo_addActor},
		{"addEvent", l_choreo_addEvent}, {"play", l_choreo_play},
		{"stop", l_choreo_stop}, {"isPlaying", l_choreo_isPlaying},
		{NULL, NULL}
	};
	registerTable(L, "Choreo", choreoFuncs);

	static const luaL_Reg responseFuncs[] = {
		{"addRule", l_response_addRule}, {"addCriteria", l_response_addCriteria},
		{"addResponse", l_response_addResponse}, {"trigger", l_response_trigger},
		{NULL, NULL}
	};
	registerTable(L, "Response", responseFuncs);
#endif

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg goapFuncs[] = {
		/* Actions */
		{"registerAction", l_goap_registerAction},
		{"setActionPrecondition", l_goap_setActionPrecondition},
		{"setActionEffect", l_goap_setActionEffect},
		{"setActionDuration", l_goap_setActionDuration},
		{"setActionRange", l_goap_setActionRange},
		{"setActionActive", l_goap_setActionActive},
		{"getActionName", l_goap_getActionName},
		/* Goals */
		{"registerGoal", l_goap_registerGoal},
		{"setGoalState", l_goap_setGoalState},
		{"setGoalActive", l_goap_setGoalActive},
		{"getGoalName", l_goap_getGoalName},
		/* Properties */
		{"defineProperty", l_goap_defineProperty},
		{"findProperty", l_goap_findProperty},
		{"getPropertyName", l_goap_getPropertyName},
		/* Agents */
		{"createAgent", l_goap_createAgent},
		{"destroyAgent", l_goap_destroyAgent},
		{"setWorldState", l_goap_setWorldState},
		{"getWorldState", l_goap_getWorldState},
		{"addAgentAction", l_goap_addAgentAction},
		{"setReplanInterval", l_goap_setReplanInterval},
		/* Planning */
		{"plan", l_goap_plan},
		{"autoPlan", l_goap_autoPlan},
		{"getCurrentAction", l_goap_getCurrentAction},
		{"advancePlan", l_goap_advancePlan},
		{"abortPlan", l_goap_abortPlan},
		{"isPlanComplete", l_goap_isPlanComplete},
		/* Blackboard */
		{"bbSetFloat", l_goap_bbSetFloat},
		{"bbGetFloat", l_goap_bbGetFloat},
		{"bbSetInt", l_goap_bbSetInt},
		{"bbGetInt", l_goap_bbGetInt},
		{"bbClear", l_goap_bbClear},
		/* Defaults + debug */
		{"registerDefaults", l_goap_registerDefaults},
		{"debugAgent", l_goap_debugAgent},
		{"debugActions", l_goap_debugActions},
		{"debugGoals", l_goap_debugGoals},
		{"getActionCount", l_goap_getActionCount},
		{"getGoalCount", l_goap_getGoalCount},
		{"getAgentCount", l_goap_getAgentCount},
		{"setMaxPlanIterations", l_goap_setMaxPlanIterations},
		{"getMaxPlanIterations", l_goap_getMaxPlanIterations},
		{"getLastPlanIterations", l_goap_getLastPlanIterations},
		{"forceReplan", l_goap_forceReplan},
		{NULL, NULL}
	};
	registerTable(L, "GOAP", goapFuncs);

	static const luaL_Reg btFuncs[] = {
		{"createTree", l_bt_createTree}, {"addNode", l_bt_addNode}, {"setRoot", l_bt_setRoot},
		{"createAgent", l_bt_createAgent}, {"destroyAgent", l_bt_destroyAgent},
		{"setAgentEntity", l_bt_setAgentEntity}, {"setAgentTarget", l_bt_setAgentTarget},
		{"setAgentContext", l_bt_setAgentContext}, {"setAnimOutput", l_bt_setAnimOutput},
		{"bbSetFloat", l_bt_bbSetFloat}, {"bbGetFloat", l_bt_bbGetFloat},
		{"bbSetInt", l_bt_bbSetInt}, {"bbGetInt", l_bt_bbGetInt},
		{"linkHordeAgent", l_bt_linkHordeAgent}, {"linkGOAPAgent", l_bt_linkGOAPAgent},
		{"getActiveCount", l_bt_getActiveCount}, {"debugAgent", l_bt_debugAgent},
		{NULL, NULL}
	};
	registerTable(L, "BT", btFuncs);
#endif

	static const luaL_Reg ecsFuncs[] = {
		{"create", l_ecs_create}, {"destroy", l_ecs_destroy},
		{"valid", l_ecs_valid}, {"count", l_ecs_count}, {"countWith", l_ecs_countWith},
		{"has", l_ecs_has}, {"add", l_ecs_add}, {"remove", l_ecs_remove},
		{"setPosition", l_ecs_setPosition}, {"getPosition", l_ecs_getPosition},
		{"setRotation", l_ecs_setRotation}, {"getRotation", l_ecs_getRotation},
		{"setScale", l_ecs_setScale}, {"getScale", l_ecs_getScale},
		{"setVelocity", l_ecs_setVelocity}, {"getVelocity", l_ecs_getVelocity},
		{"setHealth", l_ecs_setHealth}, {"getHealth", l_ecs_getHealth},
		{"setTag", l_ecs_setTag}, {"getTag", l_ecs_getTag},
		{"setGentityLink", l_ecs_setGentityLink}, {"getGentityLink", l_ecs_getGentityLink},
		{"componentFromName", l_ecs_componentFromName}, {"componentName", l_ecs_componentName},
		{"stepMotion", l_ecs_stepMotion},
		{NULL, NULL}
	};
	registerTable(L, "ECS", ecsFuncs);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg aimlFuncs[] = {
		{"createBot", l_aiml_createBot},
		{"destroyBot", l_aiml_destroyBot},
		{"loadFile", l_aiml_loadFile},
		{"setProperty", l_aiml_setProperty},
		{"getProperty", l_aiml_getProperty},
		{"getResponse", l_aiml_getResponse},
		{"setUserVar", l_aiml_setUserVar},
		{"getUserVar", l_aiml_getUserVar},
		{"resetUser", l_aiml_resetUser},
		{"getCategoryCount", l_aiml_getCategoryCount},
		{NULL, NULL}
	};
	registerTable(L, "AIML", aimlFuncs);

	static const luaL_Reg eventFuncs[] = {
		{"publish", l_eda_publish},
		{"pop", l_eda_pop},
		{"peek", l_eda_peek},
		{"isEnabled", l_eda_isEnabled},
		{"queueDepth", l_eda_queueDepth},
		{"drain", l_eda_drain},
		{"clear", l_eda_clear},
		{NULL, NULL}
	};
	registerTable(L, "Events", eventFuncs);
#endif

	static const luaL_Reg telemetryFuncs[] = {
		{"record", l_telem_record},
		{"get", l_telem_get},
		{"clear", l_telem_clear},
		{NULL, NULL}
	};
	registerTable(L, "Telemetry", telemetryFuncs);

	static const luaL_Reg cvarFuncs[] = {
		{"getString", l_cvars_getString},
		{"getNumber", l_cvars_getNumber},
		{"getInteger", l_cvars_getInteger},
		{"getBoolean", l_cvars_getBoolean},
		{"exists", l_cvars_exists},
		{"flags", l_cvars_flags},
		{"set", l_cvars_set},
		{"setNumber", l_cvars_setNumber},
		{"setInteger", l_cvars_setInteger},
		{"setBoolean", l_cvars_setBoolean},
		{"reset", l_cvars_reset},
		{NULL, NULL}
	};
	registerTable(L, "Cvars", cvarFuncs);

	static const luaL_Reg consoleFuncs[] = {
		{"exec", l_console_exec},
		{"addText", l_console_addText},
		{NULL, NULL}
	};
	registerTable(L, "Console", consoleFuncs);

	static const luaL_Reg p2pFuncs[] = {
		{"getSession", l_p2p_getSession},
		{"isBackupHostEligible", l_p2p_isBackupHostEligible},
		{NULL, NULL}
	};
	registerTable(L, "P2P", p2pFuncs);

	static const luaL_Reg replayFuncs[] = {
		{"frameIndex", l_replay_frame},
		{"baseTime", l_replay_baseTime},
		{NULL, NULL}
	};
	registerTable(L, "Replay", replayFuncs);

	static const luaL_Reg saveFuncs[] = {
		{"write", l_save_write},
		{"read", l_save_read},
		{"writeSlot", l_save_write},
		{"readSlot", l_save_read},
		{"lastSlot", l_save_lastSlot},
		{NULL, NULL}
	};
	registerTable(L, "Save", saveFuncs);

	static const luaL_Reg dbFuncs[] = {
		{"available", l_db_available},
		{"path", l_db_path},
		{"exec", l_db_exec},
		{"queryOne", l_db_queryOne},
		{"profileSet", l_db_profileSet},
		{"profileGet", l_db_profileGet},
		{"profileDelete", l_db_profileDelete},
		{NULL, NULL}
	};
	registerTable(L, "DB", dbFuncs);

	static const luaL_Reg questFuncs[] = {
		{"add", l_quest_add},
		{"setStage", l_quest_setStage},
		{"getStage", l_quest_getStage},
		{"count", l_quest_count},
		{NULL, NULL}
	};
	registerTable(L, "Quest", questFuncs);

	static const luaL_Reg dialogueFuncs[] = {
		{"start", l_dialogue_start},
		{"clear", l_dialogue_clear},
		{"count", l_dialogue_count},
		{NULL, NULL}
	};
	registerTable(L, "Dialogue", dialogueFuncs);

	static const luaL_Reg vdbFuncs[] = {
		{"load", l_vdb_load},
		{"free", l_vdb_free},
		{"sample", l_vdb_sample},
		{"getInfo", l_vdb_getInfo},
		{"upload", l_vdb_upload},
		{"bindAsFog", l_vdb_bindAsFog},
		{"getGridCount", l_vdb_getGridCount},
		{NULL, NULL}
	};
	registerTable(L, "VDB", vdbFuncs);

	static const luaL_Reg fogBioFuncs[] = {
		{"enabled", l_fogBio_enabled},
		{"getPhase", l_fogBio_getPhase},
		{"getMarineInfluence", l_fogBio_getMarineInfluence},
		{"getCoastKm", l_fogBio_getCoastKm},
		{"getPathogenRisk", l_fogBio_getPathogenRisk},
		{"getCommunity", l_fogBio_getCommunity},
		{"setSite", l_fogBio_setSite},
		{"setCoastKm", l_fogBio_setCoastKm},
		{"setMarineWind", l_fogBio_setMarineWind},
		{"setFogActive", l_fogBio_setFogActive},
		{"poll", l_fogBio_poll},
		{NULL, NULL}
	};
	registerTable(L, "FogBiology", fogBioFuncs);

#ifdef USE_ARC_BLANC
	static const luaL_Reg arcBlancFuncs[] = {
		{"enabled", l_arcBlanc_enabled},
		{"sampleHeight", l_arcBlanc_sampleHeight},
		{"sampleVelocity", l_arcBlanc_sampleVelocity},
		{"registerHull", l_arcBlanc_registerHull},
		{"unregisterHull", l_arcBlanc_unregisterHull},
		{"reseed", l_arcBlanc_reseed},
		{NULL, NULL}
	};
	registerTable(L, "ArcBlanc", arcBlancFuncs);
#endif

	static const luaL_Reg genomeFuncs[] = {
		{"enabled", l_genome_enabled},
		{"getDim", l_genome_getDim},
		{"create", l_genome_create},
		{"breed", l_genome_breed},
		{"mutate", l_genome_mutate},
		{"setFitness", l_genome_setFitness},
		{"getFitness", l_genome_getFitness},
		{"getGene", l_genome_getGene},
		{"setGene", l_genome_setGene},
		{"selectBest", l_genome_selectBest},
		{"selectTournament", l_genome_selectTournament},
		{"count", l_genome_count},
		{"getPhenotype", l_genome_getPhenotype},
		{"jobStatus", l_genome_jobStatus},
		{"decode", l_genome_decode},
		{NULL, NULL}
	};
	registerTable(L, "Genome", genomeFuncs);

	static const luaL_Reg spriteFuncs[] = {
		{"spawnLocal", l_sprite_spawnLocal},
		{"spawnServer", l_sprite_spawnServer},
		{NULL, NULL}
	};
	registerTable(L, "Sprites", spriteFuncs);

	static const luaL_Reg decalFuncs[] = {
		{"spawnLocal", l_decal_spawnLocal},
		{"spawnServer", l_decal_spawnServer},
		{NULL, NULL}
	};
	registerTable(L, "Decals", decalFuncs);

	static const luaL_Reg charFuncs[] = {
		{"create", l_char_create},
		{"move", l_char_move},
		{"destroy", l_char_destroy},
		{"getState", l_char_getState},
		{NULL, NULL}
	};
	registerTable(L, "Character", charFuncs);

	static const luaL_Reg animFuncs[] = {
		{"load", l_animgraph_load},
		{"setState", l_animgraph_setState},
		{"update", l_animgraph_update},
		{NULL, NULL}
	};
	registerTable(L, "AnimGraph", animFuncs);

	lua_setglobal(L, "Engine");
	Com_Printf("Lua bindings registered: Engine.{...,Cvars,Console,P2P,Sprites,Decals,Character,AnimGraph}\n");
}
