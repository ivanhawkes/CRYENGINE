#include "StdAfx.h"

#include "SampleRigidbodyActorComponent.h"
#include <CryPhysics/IPhysics.h>

namespace Cry
{
namespace DefaultComponents
{

void CSampleActorComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::SetMoveVel, "{5DA08668-046F-4ced-B2A4-FC53E6F33F77}"_cry_guid, "SetMoveVel");
		pFunction->SetDescription("Sets the desired movement velocity");
		pFunction->BindInput(1, 'vel', "Velocity", nullptr, Vec3(ZERO));
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::GetMoveVel, "{D7716491-E3E0-4257-9810-FA56E0332363}"_cry_guid, "GetMoveVel");
		pFunction->SetDescription("Get the desired movement velocity");
		pFunction->BindOutput(0, 'vel', "velocity");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::Jump, "{D2CF8D00-19FD-4e89-B792-85010B1A3530}"_cry_guid, "Jump");
		pFunction->SetDescription("Requests jump to the specified height");
		pFunction->BindInput(1, 'h', "Height", nullptr, 1.0f);
		componentScope.Register(pFunction);
	}
}

void CSampleActorComponent::ReflectType(Schematyc::CTypeDesc<CSampleActorComponent>& desc)
{
	desc.SetGUID(CSampleActorComponent::IID());
	desc.SetEditorCategory("Physics");
	desc.SetLabel("Sample Rigidbody Actor");
	desc.SetDescription("Very basic sample support for the new actor (walking rigid) entity in physics");
	//desc.SetIcon("icons:ObjectTypes/object.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	desc.AddMember(&CSampleActorComponent::m_friction, 'fric', "Friction", "Friction", "Ground friction when standing still", 1.0f);
	desc.AddMember(&CSampleActorComponent::m_minMass, 'gmas', "MinGroundMass", "MinGroundMass", "Only check ground colliders with this or higher masses", 1.0f);
	desc.AddMember(&CSampleActorComponent::m_legStiffness, 'stif', "LegStiffness", "LegStiffness", "Leg stiffness", 10.0f);
}

int CSampleActorComponent::g_numActors = 0;

/**
 * Handle PhysicalWorld Event: OnPostStep
 *
 * This method will retrieve the Entity/Component that matches our EventPhysPostStep Foreign Data. If the retrieved
 * Entity is of type `PE_WALKING_RIGID`, this method will invoke its `OnPostStep` method.
 *
 * A "step" in Physics is the moment the Engine moves all Entities that requested movement since the last step.
 *
 * This method is invoked automatically by the PhysicalWorld Event System after adding an Event Listener.
 * @example gEnv->pPhysicalWorld->AddEventClient(EventPhysPostStep::id, (int(*)(const EventPhys*))OnPostStepWalking, 0);
 *
 * @param epps | EventPhysPostStep | A `EventPhysPostStep` Pointer
 */
int OnPostStepWalking(const EventPhysPostStep *epps)
{
	if (epps->iForeignData == PHYS_FOREIGN_ID_ENTITY && epps->pEntity->GetType() == PE_WALKINGRIGID)
		if (CSampleActorComponent *actor = ((IEntity*)epps->pForeignData)->GetComponent<CSampleActorComponent>())
			return actor->OnPostStep(epps);
	return 1;
}

/**
 * Construct a CSampleActorComponent
 *
 * This method will add a PhysicalWorld Event Listener for `EventPhysPostStep`, using `OnPostStepWalking` as a callback
 */
CSampleActorComponent::CSampleActorComponent()
{
	if (!g_numActors++)
		gEnv->pPhysicalWorld->AddEventClient(EventPhysPostStep::id, (int(*)(const EventPhys*))OnPostStepWalking, 0);
}

/**
 * Deconstruct a CSampleActorComponent
 *
 * This method will De-Physicalize the Entity and remove the PhysicalWorld Event Listener for `EventPhysPostStep`
 */
CSampleActorComponent::~CSampleActorComponent()
{
	SEntityPhysicalizeParams pp;
	pp.type = PE_NONE;
	m_pEntity->Physicalize(pp);
	if (!--g_numActors)
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, (int(*)(const EventPhys*))OnPostStepWalking, 0);
}

/**
 * Handle PhysicalWorld Event: OnPostStepWalkingRigid
 *
 * This method uses two user-defined velocities, `m_velMove` and `m_velJump`, to modify the velocity of the Entity,
 * causing it to move in response to keyboard inputs. The user-defined velocities are modified according to the Ground
 * Slope and will inherit from Ground Velocity.
 *
 * @note This method is invoked by `CSampleActorComponent::OnPostStepWalking`
 *
 * @param epps | EventPhysPostStep | A `EventPhysPostStep` Pointer
 */
int CSampleActorComponent::OnPostStep(const EventPhysPostStep *epps)
{
	// immediate post step; is called directly from the physics thread
	if (epps->dt <= 0)
		return 1;
	pe_params_walking_rigid pwr;
	pe_action_set_velocity asv;
	pe_simulation_params sp;
	pe_status_living sl;
	pe_status_dynamics sd;
	pe_status_sensors ss;
	Vec3 pt; ss.pPoints = &pt;
	epps->pEntity->GetStatus(&sl);
	epps->pEntity->GetStatus(&sd);
	epps->pEntity->GetStatus(&ss);
	if (sl.pGroundCollider)
	{
		Vec3 velReq = m_velMove - sl.groundSlope*(sl.groundSlope*m_velMove); // project velMove to the ground
		velReq += sl.velGround;	// if we stand on something, inherit its velocity
		Vec3 dv = (velReq-sl.vel)*(epps->dt*10.0f);	// default inertial movement: exponentially approach velReq with strength 10
		m_timeFly = 0;
		pwr.velLegStick = 0.5f;
		if (m_velJump.len2())
		{
			// if jump is requested, immediately set velocity to velJump + velMove
			dv = m_velMove + m_velJump - sl.vel;
			m_timeFly = 0.1f;
			pwr.velLegStick = -1e6f; // make sure we don't stick to the ground until m_timeFly expires
			SetupLegs(true);
			m_velJump.zero();
		}
		asv.v = sl.vel + dv;
		if (m_velMove.len2())
		{
			// if we are accelerating, subtract the momentum we gain from the object we stand on
			// this ensures physical consistency, but can be disabled if it messes too much with the ground objects
			pe_action_impulse ai;
			ai.point = ss.pPoints[0];
			ai.impulse = dv*-sd.mass;
			sl.pGroundCollider->Action(&ai);
			pwr.legFriction = 0;
			sp.minEnergy = 0;	// never sleep when accelerating
		}
		else
		{
			// when not accelerating, also let the natural friction stop the movement
			pwr.legFriction = m_friction;
			sp.minEnergy = sqr(0.07f);
		}
	}
	else
		m_timeFly -= epps->dt;
	if (m_timeFly <= 0)
		pwr.velLegStick = 0.5f;
	epps->pEntity->SetParams(&sp,1);
	epps->pEntity->SetParams(&pwr,1);
	epps->pEntity->Action(&asv,1);
	return 1;
}

/**
 * Physicalize the Entity as WalkingRigid
 *
 * This method will Physicalize this Entity by creating/attaching a WalkingRigid PhysicalEntity.
 *
 * This Component assumes that Geometries have been added by other means. An example of adding Geometry would be
 * adding a Mesh Collider Component to this Entity via the Sandbox. Characters can also have pre-defined Geometries.
 *
 * If you would prefer to not use Geometries and instead use a `pe_living` Entity Style, you can specify these
 * additional parameters to your Physical Entity to apply a general collider:
 *
 * @example Set `pe_player_dimensions`
    pe_player_dimensions ppdim;
    ppdim.bUseCapsule = 1;
    ppdim.sizeCollider = Vec3(0.225f, 1.f, 0.4675f);
    ppdim.groundContactEps = 0.004f;
    ppdim.heightPivot = 0;
    ppdim.heightCollider = 0.4675f;
    pent->SetParams(&ppdim, 1);
 */
void CSampleActorComponent::Physicalize()
{
	SEntityPhysicalizeParams epp;
	epp.type = PE_NONE;
	m_pEntity->Physicalize(epp);

	IPhysicalEntity *pent = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_WALKINGRIGID);
	m_pEntity->AssignPhysicalEntity(pent);
	SEntityEvent event(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
	m_pEntity->SendEvent(event);
	pe_params_flags pf;
	pf.flagsOR = pef_monitor_poststep;
	pent->SetParams(&pf);

	pe_params_walking_rigid pwr;
	pwr.legFriction = m_friction;
	pwr.minLegTestMass = m_minMass;
	pwr.legStiffness = m_legStiffness;
	pent->SetParams(&pwr);

	SetupLegs();
	m_velMove.zero();
	m_velJump.zero();
}

/**
 * Setup WalkingRigid Actor Legs
 *
 * This method configures and initializes the "legs" of our WalkingRigid Entity.
 *
 * The "legs" of this Sample WalkingRigid Component work by using `pe_params_sensors` to cast a ray directly down (-z)
 * from the Entity to find the ground under the "feet" of this Entity. This ray is used to determine which Object this
 * Entity is currently standing on (if any).
 *
 * Without invoking this method after Physicalization, the Entity will be stuck in an eternal "free-fall" state, even
 * when on the ground, which will not work as expected. If you have an issue where `pe_status_living::pGroundCollider`
 * is always `nullptr` in `OnPostStep`, be sure to verify that this method has been invoked after Physicalization and
 * that the `dir` variable in this method contains a negative `z` value (for a standard Actor with "legs").
 *
 * @note Ensure that the length of the ray (`dir`) is long enough, which means that the ray ends lower than the Geometry
 * and ideally starts inside the Geometry. This is so that the Geometry can protect it from going fully under ground.
 *
 * @param immediately | Determines if the operation should occur immediately
 */
void CSampleActorComponent::SetupLegs(bool immediately)
{
	if (m_pEntity->GetPhysics())
	{
		pe_params_sensors ps;
		auto trans = GetTransform();
		Vec3 org = trans->GetTranslation();
		Vec3 dir = Vec3(0,0,-org.z);
		ps.nSensors = 1;
		ps.pOrigins = &org;
		ps.pDirections = &dir;
		m_pEntity->GetPhysics()->SetParams(&ps, immediately);
	}
}

/**
 * Handle User Input
 *
 * This method will execute a `pe_action_awake` on this Entity, and is called whenever there is a change to User Inputs.
 */
void CSampleActorComponent::OnInput()
{
	if (m_pEntity->GetPhysics() && m_velMove.len2() + m_velJump.len2())
	{
		pe_action_awake aa;
		m_pEntity->GetPhysics()->Action(&aa);
	}
}

}
}