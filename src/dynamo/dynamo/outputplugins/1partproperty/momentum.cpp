/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.dynamomd.org
    Copyright (C) 2011  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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

#include <dynamo/outputplugins/1partproperty/momentum.hpp>
#include <dynamo/interactions/captures.hpp>
#include <dynamo/include.hpp>
#include <dynamo/interactions/intEvent.hpp>
#include <dynamo/simulation.hpp>
#include <boost/foreach.hpp>
#include <cmath>

namespace dynamo {
  OPMomentum::OPMomentum(const dynamo::Simulation* tmp, const magnet::xml::Node&):
    OP1PP(tmp,"Momentum", 250),
    accMom(0,0,0), accMomsq(0,0,0), sysMom(0,0,0)
  {}

  void
  OPMomentum::initialise()
  {  
    accMom = Vector (0,0,0);
    accMomsq = Vector (0,0,0);
    sysMom = Vector (0,0,0);

    BOOST_FOREACH(const shared_ptr<Species>& spec, Sim->species)
      BOOST_FOREACH(const size_t& ID, *spec->getRange())
      sysMom += spec->getMass(ID) * Sim->particles[ID].getVelocity();
  }

  void 
  OPMomentum::A1ParticleChange(const ParticleEventData& PDat)
  {
    const Particle& p1 = Sim->particles[PDat.getParticleID()];
    const Species& sp1 = *Sim->species[PDat.getSpeciesID()];
    
    Vector dP = sp1.getMass(p1.getID()) * (p1.getVelocity() - PDat.getOldVel());

    sysMom += dP;
  }

  void 
  OPMomentum::stream(const double& dt)
  {
    Vector  tmp(sysMom * dt);
    accMom += tmp;
    for (size_t i(0); i < NDIM; ++i)
      accMomsq[i] += sysMom[i] * tmp[i];
  }

  void
  OPMomentum::output(magnet::xml::XmlStream &XML)
  {
    XML << magnet::xml::tag("Momentum")
	<< magnet::xml::tag("Current")
	<< sysMom / Sim->units.unitMomentum()
	<< magnet::xml::endtag("Current")
	<< magnet::xml::tag("Avg") 
	<< accMom / (Sim->dSysTime * Sim->units.unitMomentum())
	<< magnet::xml::endtag("Avg")    
	<< magnet::xml::tag("SqAvg") 
	<< accMomsq / (Sim->dSysTime * Sim->units.unitMomentum() * Sim->units.unitMomentum())
	<< magnet::xml::endtag("SqAvg")
	<< magnet::xml::endtag("Momentum");
  }
}
