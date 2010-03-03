/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2010  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef Liouvillean_H
#define Liouvillean_H

#include "../../base/is_base.hpp"
#include "../eventtypes.hpp"
#include "../../simulation/particle.hpp"
#include "../../base/is_simdata.hpp"
#include "datastruct.hpp"

class XMLNode;
namespace xmlw
{
  class XmlStream;
}
class CParticle;
class C2ParticleData;
class C1ParticleData;
class CIntEvent;
class intPart;

template <class T>
class CVector;

/*! \brief Implements the time operators for the system.
 *
 * This class provides the fundamentals of classes like CInteraction,
 * CGlobal, CLocal and CSystem. This describes when objects collide or
 * when an event occurs. Coupled with that are functions to perform
 * the events such flipping the velocity of a particle when it hits a wall.
 * 
 * This is a distinct class from the interactions etc. to prevent
 * repetition and allow the easy implementation of "other" dynamics
 * while still having complex classes like square wells!
 *
 * This class also has the delayed states algorithm implemented by
 * default. This can be overridden if required.
 *
 * The bulk of the code is implemented in the LNewtonian class.
 *
 * Before using any functions in this class you must updateParticle
 * first with the exception of the getSquareCell events! This is to
 * make sure the Delayed States algorithm is up to date for the
 * particles being tested.
 */

class Liouvillean: public DYNAMO::SimBase
{
public:  
  Liouvillean(DYNAMO::SimData* tmp):
    SimBase(tmp,"FreeStream", IC_blue),
    partPecTime(0.0),
    streamCount(0),
    streamFreq(1)
  {};

  virtual ~Liouvillean() {}
  
  virtual void initialise() 
  {
    streamFreq = 10 * Sim->lN;
  }

  /*! \brief Parses the XML data to see if it can load XML particle
   * data or if it needs to decode the binary data. Then loads the particle data.
   */
  virtual void loadParticleXMLData(const XMLNode&);
  
  /*! \brief Writes the XML particle data, either the base64 header or
   * the entire XML form.
   */
  void outputParticleXMLData(xmlw::XmlStream&) const;

  /*! \brief Returns the degrees of freedom per particle.
   */
  virtual size_t getParticleDOF() const { return NDIM; }
  
  /*! \brief Calculates the kinetic energy of a single particle
   */
  virtual Iflt getParticleKineticEnergy(const CParticle&) const;

  /*! \brief Calculates the kinetic energy of the system
   */
  Iflt getSystemKineticEnergy() const;

  /*! \brief Calculates the thermal unit of the system
   */
  Iflt getkT() const;

  /*! \brief Rescales the kinetic energy of the system by a factor
   */
  virtual void rescaleSystemKineticEnergy(const Iflt&);

  /*! \brief Rescales the translational kinetic energy vector of the system by a vector of factors
   */
  virtual void rescaleSystemKineticEnergy(const Vector&);

  /*! \brief Calculates the translational kinetic energy vector of the system
   */
  Vector  getVectorParticleKineticEnergy(const CParticle&) const;

  /*! \brief Calculates the translational kinetic energy vector of the system
   */
  Vector  getVectorSystemKineticEnergy() const;

  /*! \brief Performs an elastic multibody collision between to ranges of particles.
   * 
   * Also works for bounce (it will collide receeding structures).
   *
   * \param r1 The first structure defining CRange.
   * \param r2 The second structure defining CRange.
   * \param d2 The square of the interaction distance.
   * \param eType A way of setting the collision type from CORE to BOUNCE etc.
   * \return The collision data.
   */  
  virtual CNParticleData multibdyCollision(const CRange& r1, const CRange& r2,
					   const Iflt& d2,
					   const EEventType& eType) const = 0;

  virtual CNParticleData multibdyWellEvent(const CRange&, const CRange&, 
					   const Iflt&, const Iflt&, 
					   EEventType&) const = 0;

  /*! \brief Determines if and when two spheres will intersect.
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class
   *
   * \param d2 Square of the interaction distance
   *
   * \return Whether the event will occur
   */
  virtual bool SphereSphereInRoot(CPDData& pd, const Iflt& d2) const = 0;

  /*! \brief Determines if and when two spheres will stop intersecting.
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class
   *
   * \param d2 Square of the interaction distance
   *
   * \return Whether the event will occur (Always false for LNewtonian
   * but not for LCompression!)
   */
  virtual bool SphereSphereOutRoot(CPDData&, const Iflt&) const = 0;  

  /*! \brief Determines if two spheres are overlapping
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d2 Square of the interaction distance.
   *
   * \return True if the spheres are overlapping.
   */
  virtual bool sphereOverlap(const CPDData& PD, const Iflt& d2) const = 0;

  /*! \brief Determines if and when two parallel cube will intersect.
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d Rhe interaction distance.
   *
   * \return Whether the event will occur.
   */
  virtual bool CubeCubeInRoot(CPDData& pd, const Iflt& d) const { D_throw() << "Not Implemented"; }

  /*! \brief Determines if and when two rotated parallel cube will
      intersect.
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d Rhe interaction distance.
   *
   * \param rot The rotation such that the cubes are parallel
   *
   * \return Whether the event will occur.
   */
  virtual bool CubeCubeInRoot(CPDData& pd, const Iflt& d, 
			      const Matrix& rot) const 
  { D_throw() << "Not Implemented"; }

  /*! \brief Determines if and when two parallel cubes will stop intersecting.
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d The interaction distance.
   *
   * \return Whether the event will occur (Almost always true for
   * LNewtonian but not for LCompression).
   */
  virtual bool CubeCubeOutRoot(CPDData&, const Iflt& d) const { D_throw() << "Not Implemented"; }

  /*! \brief Determines if two parallel cubes are overlapping
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d The interaction distance.
   *
   * \return True if the cubes are overlapping.
   */
  virtual bool cubeOverlap(const CPDData& PD, const Iflt& d) const 
  { D_throw() << "Not Implemented"; }

  /*! \brief Determines if two rotated parallel cubes are overlapping
   *
   * \param pd Some precomputed data about the event that is cached by
   * the interaction/calling class.
   *
   * \param d The interaction distance.
   *
   * \param rot Rotation matrix.
   *
   * \return True if the cubes are overlapping.
   */
  virtual bool cubeOverlap(const CPDData& PD, const Iflt& d,
			   const Matrix& rot) const 
  { D_throw() << "Not Implemented"; }

  /*! \brief Determines when the particle center will hit a bounding box.
   *
   * For speed this does a little extra math with the event time to
   * avoid having to use the delayed states update.
   *
   * DO NOT do updateParticle before calling this function, there's no need.
   * 
   * \param part The particle to test.
   * \param origin The lowest corner of the bounding cell box.
   * \param width The width of the bounding cell box.
   * \return The time till collision.
   */    
  virtual Iflt getSquareCellCollision2(const CParticle& part, 
				       const Vector & origin, 
				       const Vector & width) const = 0;
  
  /*! \brief Determines which dimension of the cell the particle will
   * leave first.
   *
   * This is used to determine which face of the cell the particle has
   * left.
   *
   * DO NOT do updateParticle before calling this function, there's no need.
   * 
   * For speed this does a little extra math with the event time to
   * avoid having to use the delayed states update.
   *
   * \param part The particle to test.
   * \param origin The lowest corner of the bounding cell box.
   * \param width The width of the bounding cell box.
   * \return The time till collision.
   */    
  virtual int getSquareCellCollision3(const CParticle& part, 
				      const Vector & origin, 
				      const Vector & width) const = 0;

  /*! \brief Tests if and when two lines will collide.
   *
   * \param PD Some precalculated data on the lines.
   * \param length The length of the lines, or interaction length.
   * \param p1 First particle.
   * \param p2 Second particle.
   * \param twindow Maximum time to check till.
   * \return Wether the event will occur or not.
   */    
  virtual bool getLineLineCollision(CPDData& PD, const Iflt& length, 
				    const CParticle& p1, const CParticle& p2
				    ) const;

  virtual Iflt getPointPlateCollision(const CParticle& np1, const Vector& nrw0,
				      const Vector& nhat, const Iflt& Delta,
				      const Iflt& Omega, const Iflt& Sigma,
				      const Iflt& t, bool lastPart) const;

  virtual C1ParticleData runOscilatingPlate
  (const CParticle& part, const Vector& rw0, const Vector& nhat, Iflt& delta, 
   const Iflt& omega0, const Iflt& sigma, const Iflt& mass, const Iflt& e, 
   Iflt& t, bool strongPlate = false) const;

  /*! \brief Calculates when a particle has travelled far enough to
   *   change its nearest-images. 
   *
   *   This is required in low density configurations. The time
   *   returned in a normal system is 
   *   \f[ 
   *   t=Min\left(\left[L_\alpha/2 - \sigma\right]/\left|v\right|_\alpha\right) 
   *   \f]
   *
   *   where \f$\alpha\f$ is the dimension, \f$\sigma\f$ is the maximum
   *   interaction distance and \f${\bf L}\f$ is the sim box dimensions.
   *
   * \param part Particle tested
   * \param maxl The maximum range of the particles interactions.
   * \return Time of the event.
   */    
  virtual Iflt getPBCSentinelTime(const CParticle& p1, const Iflt& maxl) const;

  /*! \brief Calculates when a particle has peaked in its parabola to
   *   allow cell lists to not stream the system.
   *
   *   This is required in gravity with cell neighbour lists. 
   * \param part Particle tested.
   * \param passed A bit to set if the parabola is already over.
   * \return Time of the event.
   */    
  virtual Iflt getParabolaSentinelTime(const CParticle& p1, unsigned char& passed) const
  { 
    D_throw() << "This is not needed for this type of Liouvillean";
  }

  /*! \brief Calculates when a particle has peaked in its parabola to
   *   allow cell lists to not stream the system.
   *
   *   This is required in gravity with cell neighbour lists. 
   * \param part Particle tested.
   * \param passed A bit to set if the parabola is already over.
   * \return Time of the event.
   */    
  virtual void enforceParabola(const CParticle&) const
  { 
    D_throw() << "This is not needed for this type of Liouvillean";
  }

  /*! \brief Runs a line line collision event
   *
   * \param eevent Description of the scheduled event
   * \return Collision data
   */    
  virtual C2ParticleData runLineLineCollision(const CIntEvent& eevent,
					      const Iflt& elasticity, const Iflt& length) const;

  /*! \brief Determines when the particle center will hit a wall.
   *
   *
   * \param part The particle to test.
   * \param origin A point the wall passes through.
   * \param norm The normal vector to the wall surface.
   * \return The time till collision.
   */    
  virtual Iflt getWallCollision(const CParticle& part, 
				const Vector & origin, 
				const Vector & norm
				) const = 0;

  /*! \brief Determines when the particle center will hit a cylindrical wall.
   *
   *
   * \param part The particle to test.
   * \param origin A point on the axis of the cylinder
   * \param norm The direction of the cylinder axis.
   * \param radius The radius of the cylinder
   * \return The time till collision.
   */    
  virtual Iflt getCylinderWallCollision(const CParticle& part, 
					const Vector & origin, 
					const Vector & norm,
					const Iflt& radius
					) const;

  /*! \brief Collides a particle with a cylindrical wall.
   *
   * \param part The particle that is colliding with the wall.
   * \param origin A point on the axis of the cylinder
   * \param norm The direction of the cylinder axis.
   * \param e Elasticity of wall.
   * \return The data for the collision.
   */
  virtual C1ParticleData runCylinderWallCollision(const CParticle& part, 
						  const Vector & origin,
						  const Vector & norm,
						  const Iflt& e
						  ) const;

  /*! \brief Collides a particle with a containing spherical wall.
   *
   * \param part The particle that is colliding with the wall.
   * \param origin The origin of the sphere
   * \param e Elasticity of wall.
   * \return The data for the collision.
   */
  virtual C1ParticleData runSphereWallCollision(const CParticle& part, 
						const Vector & origin,
						const Iflt& e
						) const;

  /*! \brief Collides a particle with a wall.
   *
   * \param part The particle that is colliding with the wall.
   * \param e Elasticity of wall.
   * \param vNorm Normal of the wall (\f$ vNorm \cdot v_1\f$ must be negative).
   * \return The data for the collision.
   */
  virtual C1ParticleData runWallCollision(const CParticle& part, 
					  const Vector & vNorm,
					  const Iflt& e
					  ) const = 0;

  /*! \brief Collides a particle with an Andersen thermostat wall.
   * 
   * This wall reassigns the velocity components of the particle on
   * collision. Care was taken to ensure this gives a \f$ p \propto
   * v_{norm} \exp(v_{norm}^2) \f$ distribution and gaussian tangent
   * vectors.
   *
   * \param part Particle colliding the wall.
   * \param sqrtT Square root of the Temperature of wall.
   * \param vNorm Normal of the wall (\f$ vNorm \cdot v_1 \f$ must be negative).
   */    
  virtual C1ParticleData runAndersenWallCollision(const CParticle& part, 
						  const Vector & vNorm,
						  const Iflt& sqrtT
						  ) const = 0;
  
  /*! \brief Performs a hard sphere collision between the two particles.
   * 
   * Also works for bounce collisions inside wells/outside squareshoulders
   * (it will collide receeding particles).
   *
   * \param e Elasticity.
   * \param event The event containing the data on the two particles.
   * \param d2 Square of the interaction distance
   * \param eType A way of setting the collision type from CORE to BOUNCE etc.
   * \return The collision data.
   */  
  virtual C2ParticleData SmoothSpheresColl(const CIntEvent& event, 
					   const Iflt& e, 
					   const Iflt& d2, 
					   const EEventType& eType = CORE
					   ) const = 0;

  /*! \brief Performs a hard sphere collision between the two
   *   particles where one may have infinite mass.
   * 
   * Also works for bounce collisions inside wells/outside squareshoulders
   * (it will collide receeding particles).
   *
   * \param e Elasticity.
   * \param event The event containing the data on the two particles.
   * \param d2 Square of the interaction distance
   * \param eType A way of setting the collision type from CORE to BOUNCE etc.
   * \return The collision data.
   */  
  virtual C2ParticleData SmoothSpheresCollInfMassSafe(const CIntEvent& event, const Iflt& e, 
						      const Iflt& d2,
						      const EEventType& eType = CORE) const;

  /*! \brief Performs a parallel cube collision between the two particles.
   * 
   * Also works for bounce collisions inside wells/outside squareshoulders
   * (it will collide receeding particles).
   *
   * \param e Elasticity.
   * \param event The event containing the data on the two particles.
   * \param d2 The interaction distance
   * \param eType A way of setting the collision type from CORE to BOUNCE etc.
   * \return The collision data.
   */  
  virtual C2ParticleData parallelCubeColl(const CIntEvent& event, 
					  const Iflt& e, 
					  const Iflt& d, 
					  const EEventType& eType = CORE) const;

  /*! \brief Performs a hard sphere collision between the two particles.
   * 
   * Also works for bounce collisions inside wells/outside squareshoulders
   * (it will collide receeding particles).
   * 
   * \param e Elasticity
   * \param event The event containing the data on the two particles
   * \param d2 The interaction distance
   * \param rot Rotation matrix
   * \param eType A way of setting the collision type from CORE to BOUNCE etc.
   * \return The collision data
   */
  virtual C2ParticleData parallelCubeColl(const CIntEvent& event,
					  const Iflt& e, const Iflt& d,
					  const Matrix& rot,
					  const EEventType& eType = CORE) const;

  /*! \brief Tests for a collision between spherical particles
   * according to the ESMC (Enskog DSMC)
   * 
   *
   * \param p1 First particle to test
   * \param p1 Second particle to test
   * \param maxprob The current maximum of the collision radius
   * \param pdat Some cached calc data
   * \return Whether the collision occurs
   */  
  virtual bool DSMCSpheresTest(const CParticle& p1,
			       const CParticle& p2,
			       Iflt& maxprob,
			       const Iflt& factor,
			       CPDData& pdat
			       ) const = 0;
  
  /*! \brief Performs a hard sphere collision between the two
   * particles according to the ESMC (Enskog DSMC)
   * 
   *
   * \param p1 First particle to test
   * \param p1 Second particle to test
   * \param e Inelasticity
   * \param pdat Some cached calc data
   * \return Data on the collision
   */  
  virtual C2ParticleData DSMCSpheresRun(const CParticle& p1,
					const CParticle& p2,
					const Iflt& e,
					CPDData& pdat
					) const = 0;

  /*! \brief Executes a well/shoulder event
   *
   * This is a workhorse of the square well/square shoulder/core
   * softend interactions. This calculates if the particle will
   * escape/enter a well/shoulder and runs the interaction.
   *
   * \param event The interaction event containing the particle data.
   * \param deltaKE The kinetic energy change of the particles if the
   * well/shoulder is transitioned.
   * \param d2 The square of the interaction distance.
   * \return The event data.
   */  
  virtual C2ParticleData SphereWellEvent(const CIntEvent& event, 
					 const Iflt& deltaKE, 
					 const Iflt& d2) const = 0;

  /* \brief Reassigns the velocity componets of a particle from a
   * Gaussian.
   *
   * Used to thermostat particles.
   *
   * \param part The particle to reassign the velocities of.
   * \param sqrtT The square root of the temperature.
   * \return The event data
   *
   * \bug Does this work for arbitrary mass particles.
   */
  virtual C1ParticleData randomGaussianEvent(const CParticle& part, 
					     const Iflt& sqrtT) const = 0;

  /*! \brief A method to allow polymorphic classes to be copied
   */
  virtual Liouvillean* Clone() const = 0;

  /*! \brief An XML output operator for the class. Calls the virtual
   * OutputXML member function.
   */
  friend xmlw::XmlStream& operator<<(xmlw::XmlStream&, const Liouvillean&);

  /*! \brief This is called by each particle to request any extra data
   *   the liouvillean is holding and wants to store in the particles XML.
   */

  /*! \brief Instantiates and loads CLiovillean classes from an XML
   * entry.
   */
  static Liouvillean* loadClass(const XMLNode& ,DYNAMO::SimData*);
    
  /*! \brief Free streams all particles up to the current time.
   * 
   * This synchronises all the delayed states of the particles
   */
  void updateAllParticles() const
  {
    //May as well take this opportunity to reset the streaming
    //Note: the Replexing coordinator RELIES on this behaviour!
    BOOST_FOREACH(const CParticle& part, Sim->vParticleList)
      {
	streamParticle(const_cast<CParticle&>(part), 
		       part.getPecTime() + partPecTime);
	
	const_cast<CParticle&>(part).getPecTime() = 0;
      }

    const_cast<Iflt&>(partPecTime) = 0;
    const_cast<size_t&>(streamCount) = 0;
  }

  /*! \brief Free streams a particle up to the current time.
   * 
   * This synchronises the delayed states of the particle.
   * \param part Particle to syncronise.
   */
  inline void updateParticle(const CParticle& part) const
  {
    streamParticle(const_cast<CParticle&>(part), 
		   part.getPecTime() + partPecTime);

    const_cast<CParticle&>(part).getPecTime() = -partPecTime;
  }

  inline bool isUpToDate(const CParticle& part) const
  {
    return part.getPecTime() == -partPecTime;
  }

  /*! \brief Free streams two particles up to the current time.
   * 
   * This is here incase an optimisation or overload is performed later.
   *
   * This synchronises the delayed states of the two particle.
   * \param p1 A CParticle to syncronise.
   * \param p2 A CParticle to syncronise.
   */
  inline void updateParticlePair(const CParticle& p1, const CParticle& p2) const
  {
    //This is slow but sure, other stuff like reverse streaming, and
    //partial streaming are faster but work only for some collision
    //detections, not compression

    updateParticle(p1);
    updateParticle(p2);
  }

  inline Iflt getParticleDelay(const CParticle& part) const
  {
    return partPecTime + part.getPecTime();
  }

  /*! \brief Called when the system is moved forward in time to update
   * the delayed states state.
   */
  inline void stream(const Iflt& dt)
  {
    partPecTime += dt;

    //Keep the magnitude of the partPecTime boundedx
    if (++streamCount == streamFreq)
      {
	BOOST_FOREACH(CParticle& part, Sim->vParticleList)
	  part.getPecTime() += partPecTime;

	partPecTime = 0;
	streamCount = 0;
      }
  }

  //This requires access to the streamParticle function and the
  //advanceUpdateParticle function
  friend class CSMultListShear;
  
protected:
  friend class CGCellsShearing;
  /*! \brief A dangerous function to predictavly move a particle forward.
   *
   * See CGCellsShearing, this just over advances the particle to find
   * its future position in boundary changes.
   */
  inline void advanceUpdateParticle(const CParticle& part, const Iflt& dt) const
  {
    streamParticle(const_cast<CParticle&>(part), 
		   dt + partPecTime + part.getPecTime());

    const_cast<CParticle&>(part).getPecTime() = - dt - partPecTime;
  }
  
  /*! \brief The time by which the delayed state differs from the actual.*/
  Iflt partPecTime;

  /*! \brief How many time increments have occured since the last
      system syncronise.*/
  size_t streamCount;

  /*! \brief How often the system peculiar times should be syncronised.*/
  size_t streamFreq;
  
  /*! \brief Writes out the liouvilleans data to XML. */
  virtual void outputXML(xmlw::XmlStream&) const = 0;

  virtual void extraXMLParticleData(xmlw::XmlStream&, const size_t) const {}

  virtual void extraXMLData(xmlw::XmlStream&) const {}

  /*! \brief Moves the particles data along in time. */
  virtual void streamParticle(CParticle& part, const Iflt& dt) const = 0;

};
#endif
