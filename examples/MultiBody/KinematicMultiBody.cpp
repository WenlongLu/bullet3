#include "KinematicMultiBody.h"
#include "../OpenGLWindow/SimpleOpenGL3App.h"
#include "btBulletDynamicsCommon.h"

#include "BulletDynamics/MLCPSolvers/btDantzigSolver.h"
#include "BulletDynamics/MLCPSolvers/btLemkeSolver.h"
#include "BulletDynamics/MLCPSolvers/btSolveProjectedGaussSeidel.h"

#include "BulletDynamics/Featherstone/btMultiBody.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBodyMLCPConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletDynamics/Featherstone/btMultiBodyLink.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointLimitConstraint.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointMotor.h"
#include "BulletDynamics/Featherstone/btMultiBodyPoint2Point.h"
#include "BulletDynamics/Featherstone/btMultiBodyFixedConstraint.h"
#include "BulletDynamics/Featherstone/btMultiBodySliderConstraint.h"

#include "../OpenGLWindow/GLInstancingRenderer.h"
#include "BulletCollision/CollisionShapes/btShapeHull.h"

#include "../CommonInterfaces/CommonMultiBodyBase.h"

#include <math.h>
#include <iostream>

class KinematicMultiBody : public CommonMultiBodyBase
{
public:
	KinematicMultiBody(GUIHelperInterface* helper);
	virtual ~KinematicMultiBody();

	virtual void initPhysics();

	virtual void stepSimulation(float deltaTime);

	virtual void resetCamera()
	{
		float dist = 1;
		float pitch = -35;
		float yaw = 50;
		float targetPos[3] = {-3, 2.8, -2.5};
		m_guiHelper->resetCamera(dist, yaw, pitch, targetPos[0], targetPos[1], targetPos[2]);
	}

	btMultiBody* createFeatherstoneMultiBody(class btMultiBodyDynamicsWorld* world, int numLinks, const btVector3& basePosition, const btVector3& linkHalfExtents, bool floating = false);
	void addColliders(btMultiBody* pMultiBody, btMultiBodyDynamicsWorld* pWorld, const btVector3& linkHalfExtents);

	void animate(float deltaTime);

  btMultiBody* mbC;
};

static bool g_floatingBase = false;
static bool g_firstInit = true;
static float scaling = 0.4f;
static float friction = 1.;
static int g_constraintSolverType = 0;
static int g_numLinks = 5;

KinematicMultiBody::KinematicMultiBody(GUIHelperInterface* helper)
	: CommonMultiBodyBase(helper)
{
	m_guiHelper->setUpAxis(1);
}
KinematicMultiBody::~KinematicMultiBody()
{
}

void KinematicMultiBody::stepSimulation(float deltaTime)
{
	//use a smaller internal timestep, there are stability issues
	float internalTimeStep = 1. / 240.f;
	animate(internalTimeStep);
	m_dynamicsWorld->stepSimulation(deltaTime, 1, internalTimeStep);
}

void KinematicMultiBody::initPhysics()
{
	m_guiHelper->setUpAxis(1);

	if (g_firstInit)
	{
		m_guiHelper->getRenderInterface()->getActiveCamera()->setCameraDistance(btScalar(10. * scaling));
		m_guiHelper->getRenderInterface()->getActiveCamera()->setCameraPitch(50);
		g_firstInit = false;
	}
	///collision configuration contains default setup for memory, collision setup
	m_collisionConfiguration = new btDefaultCollisionConfiguration();

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);

	m_broadphase = new btDbvtBroadphase();

	if (g_constraintSolverType == 4)
	{
		g_constraintSolverType = 0;
		g_floatingBase = !g_floatingBase;
	}

	btMultiBodyConstraintSolver* sol;
	btMLCPSolverInterface* mlcp;
	switch (g_constraintSolverType++)
	{
		case 0:
			sol = new btMultiBodyConstraintSolver;
			b3Printf("Constraint Solver: Sequential Impulse");
			break;
		case 1:
			mlcp = new btSolveProjectedGaussSeidel();
			sol = new btMultiBodyMLCPConstraintSolver(mlcp);
			b3Printf("Constraint Solver: MLCP + PGS");
			break;
		case 2:
			mlcp = new btDantzigSolver();
			sol = new btMultiBodyMLCPConstraintSolver(mlcp);
			b3Printf("Constraint Solver: MLCP + Dantzig");
			break;
		default:
			mlcp = new btLemkeSolver();
			sol = new btMultiBodyMLCPConstraintSolver(mlcp);
			b3Printf("Constraint Solver: MLCP + Lemke");
			break;
	}

	m_solver = sol;

	//use btMultiBodyDynamicsWorld for Featherstone btMultiBody support
	m_dynamicsWorld = new btMultiBodyDynamicsWorld(m_dispatcher, m_broadphase, sol, m_collisionConfiguration);
	//	m_dynamicsWorld->setDebugDrawer(&gDebugDraw);
	m_guiHelper->createPhysicsDebugDrawer(m_dynamicsWorld);
	m_dynamicsWorld->setGravity(btVector3(0, -9.81, 0));
	m_dynamicsWorld->getSolverInfo().m_globalCfm = 1e-3;

	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////

	bool damping = true;
	bool gyro = true;
	bool spherical = false;  //set it ot false -to use 1DoF hinges instead of 3DoF sphericals
	bool multibodyOnly = false;
	bool canSleep = false;
	bool selfCollide = true;
	btVector3 linkHalfExtents(0.05, 0.37, 0.1);

	mbC = createFeatherstoneMultiBody(m_dynamicsWorld, g_numLinks, btVector3(-0.4f, 2.f, 0.f), linkHalfExtents, g_floatingBase);
	//mbC->forceMultiDof();							//if !spherical, you can comment this line to check the 1DoF algorithm

	mbC->setCanSleep(canSleep);
	mbC->setHasSelfCollision(selfCollide);
	mbC->setUseGyroTerm(gyro);
	//
	if (!damping)
	{
		mbC->setLinearDamping(0.f);
		mbC->setAngularDamping(0.f);
	}
	else
	{
		mbC->setLinearDamping(0.1f);
		mbC->setAngularDamping(0.9f);
	}
	//////////////////////////////////////////////
	if (g_numLinks > 0)
	{
		btScalar q0 = 45.f * SIMD_PI / 180.f;
		if (!spherical)
		{
			mbC->setJointPosMultiDof(0, &q0);
		}
		else
		{
			btQuaternion quat0(btVector3(1, 1, 0).normalized(), q0);
			quat0.normalize();
			mbC->setJointPosMultiDof(0, quat0);
		}
	}
	///
	addColliders(mbC, m_dynamicsWorld, linkHalfExtents);

	///create a few basic rigid bodies
	/////////////////////////////////////////////////////////////////
	if (!multibodyOnly)
	{
	btScalar groundHeight = -51.55;
		btVector3 groundHalfExtents(50, 50, 50);
		btCollisionShape* groundShape = new btBoxShape(groundHalfExtents);

		m_collisionShapes.push_back(groundShape);

		btScalar mass(0.);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0, 0, 0);
		if (isDynamic)
			groundShape->calculateLocalInertia(mass, localInertia);

		//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		btTransform groundTransform;
		groundTransform.setIdentity();
		groundTransform.setOrigin(btVector3(0, groundHeight, 0));
		btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
		btRigidBody* body = new btRigidBody(rbInfo);

		//add the body to the dynamics world
		m_dynamicsWorld->addRigidBody(body, 1, 1 + 2);  //,1,1+2);
	}
	/////////////////////////////////////////////////////////////////
	if (!multibodyOnly)
	{
		btVector3 halfExtents(.5, .5, .5);
		btBoxShape* colShape = new btBoxShape(halfExtents);
		m_collisionShapes.push_back(colShape);

		/// Create Dynamic Objects
		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0, 0, 0);
		if (isDynamic)
			colShape->calculateLocalInertia(mass, localInertia);

		startTransform.setOrigin(btVector3(
			btScalar(0.0),
			0.0,
			btScalar(0.0)));

		//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
		btRigidBody* body = new btRigidBody(rbInfo);

		m_dynamicsWorld->addRigidBody(body);  //,1,1+2);
	}

	m_guiHelper->autogenerateGraphicsObjects(m_dynamicsWorld);

	/////////////////////////////////////////////////////////////////
}

btMultiBody* KinematicMultiBody::createFeatherstoneMultiBody(btMultiBodyDynamicsWorld* pWorld, int numLinks, const btVector3& basePosition, const btVector3& linkHalfExtents, bool floating)
{
	float linkMass = 1.f;
	btVector3 linkInertiaDiag(0.f, 0.f, 0.f);

	btCollisionShape* pTempBox = new btBoxShape(btVector3(linkHalfExtents[0], linkHalfExtents[1], linkHalfExtents[2]));
	pTempBox->calculateLocalInertia(linkMass, linkInertiaDiag);
	delete pTempBox;

	//init the base
	bool canSleep = false;

	btMultiBody* pMultiBody = new btMultiBody(numLinks, linkMass, linkInertiaDiag, !floating, canSleep);

	btQuaternion baseOriQuat(0.f, 0.f, 0.f, 1.f);
	pMultiBody->setBasePos(basePosition);
	pMultiBody->setWorldToBaseRot(baseOriQuat);
	btVector3 vel(0, 0, 0);

	//init the links
	btVector3 hingeJointAxis(1, 0, 0);

	//y-axis assumed up
	btVector3 parentComToCurrentCom(0, -linkHalfExtents[1] * 2.f, 0);                      //par body's COM to cur body's COM offset
	btVector3 currentPivotToCurrentCom(0, -linkHalfExtents[1], 0);                         //cur body's COM to cur body's PIV offset
	btVector3 parentComToCurrentPivot = parentComToCurrentCom - currentPivotToCurrentCom;  //par body's COM to cur body's PIV offset

	//////
	btScalar q0 = 0.f * SIMD_PI / 180.f;
	btQuaternion quat0(btVector3(0, 1, 0).normalized(), q0);
	quat0.normalize();
	/////

	for (int i = 0; i < numLinks; ++i)
	{
			pMultiBody->setupRevolute(i, linkMass, linkInertiaDiag, i - 1, btQuaternion(0.f, 0.f, 0.f, 1.f), hingeJointAxis, parentComToCurrentPivot, currentPivotToCurrentCom, true);
	}

	pMultiBody->finalizeMultiDof();

	///
	pWorld->addMultiBody(pMultiBody);
	///
	return pMultiBody;
}

void KinematicMultiBody::addColliders(btMultiBody* pMultiBody, btMultiBodyDynamicsWorld* pWorld, const btVector3& linkHalfExtents)
{
	btAlignedObjectArray<btQuaternion> world_to_local;
	world_to_local.resize(pMultiBody->getNumLinks() + 1);

	btAlignedObjectArray<btVector3> local_origin;
	local_origin.resize(pMultiBody->getNumLinks() + 1);
	world_to_local[0] = pMultiBody->getWorldToBaseRot();
	local_origin[0] = pMultiBody->getBasePos();

	btScalar quat[4] = {-world_to_local[0].x(), -world_to_local[0].y(), -world_to_local[0].z(), world_to_local[0].w()};

	btCollisionShape* box = new btBoxShape(linkHalfExtents);
	btMultiBodyLinkCollider* col = new btMultiBodyLinkCollider(pMultiBody, -1);
	col->setCollisionShape(box);

	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(local_origin[0]);
	tr.setRotation(btQuaternion(quat[0], quat[1], quat[2], quat[3]));
	col->setWorldTransform(tr);

	pWorld->addCollisionObject(col, 2, 1 + 2);

	col->setFriction(friction);
	pMultiBody->setBaseCollider(col);

	for (int i = 0; i < pMultiBody->getNumLinks(); ++i)
	{
		const int parent = pMultiBody->getParent(i);
		world_to_local[i + 1] = pMultiBody->getParentToLocalRot(i) * world_to_local[parent + 1];
		local_origin[i + 1] = local_origin[parent + 1] + (quatRotate(world_to_local[i + 1].inverse(), pMultiBody->getRVector(i)));
	}

	for (int i = 0; i < pMultiBody->getNumLinks(); ++i)
	{
		btVector3 posr = local_origin[i + 1];

		btScalar quat[4] = {-world_to_local[i + 1].x(), -world_to_local[i + 1].y(), -world_to_local[i + 1].z(), world_to_local[i + 1].w()};

		btCollisionShape* box = new btBoxShape(linkHalfExtents);
		btMultiBodyLinkCollider* col = new btMultiBodyLinkCollider(pMultiBody, i);

		col->setCollisionShape(box);
		btTransform tr;
		tr.setIdentity();
		tr.setOrigin(posr);
		tr.setRotation(btQuaternion(quat[0], quat[1], quat[2], quat[3]));
		col->setWorldTransform(tr);
		col->setFriction(friction);
		pWorld->addCollisionObject(col, 2, 1 + 2);

		pMultiBody->getLink(i).m_collider = col;
	}
}

void KinematicMultiBody::animate(float deltaTime)
{
	static float time = 0.0;
	time += deltaTime;
	for (int kinematic_id = 0; kinematic_id < g_numLinks - 2; kinematic_id++) {
	  double old_joint_pos = mbC->getJointPos(kinematic_id);
	  //double old_joint_vel = mbC->getJointVel(kinematic_id);
		//std::cout << "current: " << old_joint_pos << " " << old_joint_vel << std::endl;
		double joint_pos = 1.0 * sin(time * 3.0 - 0.3);
		double joint_vel = (joint_pos - old_joint_pos) / deltaTime;
		//std::cout << "set to: " << joint_pos << " " << joint_vel << std::endl;
		mbC->setLinkDynamicType(kinematic_id, KINEMATIC_OBJECT);
		mbC->setJointPosMultiDof(kinematic_id, &joint_pos);
		mbC->setJointVelMultiDof(kinematic_id, &joint_vel);
	}
}

class CommonExampleInterface* KinematicMultiBodyCreateFunc(struct CommonExampleOptions& options)
{
	return new KinematicMultiBody(options.m_guiHelper);
}
