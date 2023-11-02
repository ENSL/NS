// ============================== LADDER =======================================

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		0x00000001

class CBaseTrigger : public CBaseToggle
{
public:
	void EXPORT TeleportTouch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT MultiTouch( CBaseEntity *pOther );
	void EXPORT HurtTouch ( CBaseEntity *pOther );
	void EXPORT CDAudioTouch ( CBaseEntity *pOther );
	void ActivateMultiTrigger( CBaseEntity *pActivator );
	void EXPORT MultiWaitOver( void );
	void EXPORT CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void InitTrigger( void );
	
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

class CMultiManager : public CBaseToggle
{
public:
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);
	void EXPORT ManagerThink(void);
	void EXPORT ManagerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

#if _DEBUG
	void EXPORT ManagerReport(void);
#endif

	BOOL		HasTarget(string_t targetname);

	int ObjectCaps(void) { return CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save(CSave& save);
	virtual int		Restore(CRestore& restore);

	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cTargets;	// the total number of targets in this manager's fire list.
	int		m_index;	// Current target
	float	m_startTime;// Time we started firing
	int		m_iTargetName[MAX_MULTI_TARGETS];// list if indexes into global string array
	float	m_flTargetDelay[MAX_MULTI_TARGETS];// delay (in seconds) from time of manager fire to target fire
private:
	inline BOOL IsClone(void) { return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE; }
	inline BOOL ShouldClone(void)
	{
		if (IsClone())
			return FALSE;

		return (pev->spawnflags & SF_MULTIMAN_THREAD) ? TRUE : FALSE;
	}

	CMultiManager* Clone(void);
};

// this is a really bad idea.
class CTriggerChangeTarget : public CBaseDelay
{
public:
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	int GetNewTargetName() { return m_iszNewTarget; }

	int ObjectCaps(void) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save(CSave& save);
	virtual int		Restore(CRestore& restore);

	static	TYPEDESCRIPTION m_SaveData[];

private:
	int		m_iszNewTarget;
};


class CLadder : public CBaseTrigger
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
};
