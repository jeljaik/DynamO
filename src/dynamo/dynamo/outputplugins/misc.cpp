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

#include <dynamo/outputplugins/misc.hpp>
#include <dynamo/include.hpp>
#include <dynamo/simulation.hpp>
#include <magnet/memUsage.hpp>
#include <magnet/xmlwriter.hpp>
#include <dynamo/systems/tHalt.hpp>
#include <sys/time.h>
#include <ctime>

namespace dynamo {
  OPMisc::OPMisc(const dynamo::Simulation* tmp, const magnet::xml::Node&):
    OutputPlugin(tmp,"Misc",0),//ContactMap must be after this
    _dualEvents(0),
    _singleEvents(0),
    _virtualEvents(0),
    _reverseEvents(0)
  {}

  void
  OPMisc::changeSystem(OutputPlugin* misc2)
  {
    OPMisc& op = static_cast<OPMisc&>(*misc2);
    _KE.swapCurrentValues(op._KE);
    _internalE.swapCurrentValues(op._internalE);
    _kineticP.swapCurrentValues(op._kineticP);

    std::swap(Sim, op.Sim);
  }

  void
  OPMisc::temperatureRescale(const double& scale)
  { 
    _KE  = _KE.current() * scale;
  }

  double 
  OPMisc::getMeankT() const
  {
    return 2.0 * _KE.mean() / (Sim->N * Sim->dynamics->getParticleDOF());
  }

  double 
  OPMisc::getMeanSqrkT() const
  {
    return 4.0 * _KE.meanSqr()
      / (Sim->N * Sim->N * Sim->dynamics->getParticleDOF()
	 * Sim->dynamics->getParticleDOF());
  }

  double 
  OPMisc::getCurrentkT() const
  {
    return 2.0 * _KE.current() / (Sim->N * Sim->dynamics->getParticleDOF());
  }

  double 
  OPMisc::getMeanUConfigurational() const
  { 
    return _internalE.mean(); 
  }

  double 
  OPMisc::getMeanSqrUConfigurational() const
  { return _internalE.meanSqr(); }

  void
  OPMisc::initialise()
  {
    _KE.init(Sim->dynamics->getSystemKineticEnergy());
    _internalE.init(Sim->calcInternalEnergy());

    dout << "Particle Count " << Sim->N
	 << "\nSim Unit Length " << Sim->units.unitLength()
	 << "\nSim Unit Time " << Sim->units.unitTime()
	 << "\nDensity " << Sim->getNumberDensity() * Sim->units.unitVolume()
	 << "\nPacking Fraction " << Sim->getPackingFraction()
	 << "\nTemperature " << getCurrentkT() / Sim->units.unitEnergy()
	 << "\nNo. of Species " << Sim->species.size()
	 << "\nSimulation box length "
	 << Vector(Sim->primaryCellSize / Sim->units.unitLength()).toString()
	 << "\n";

    Matrix kineticP;
    Vector thermalConductivityFS(0,0,0);
    _speciesMomenta.clear();
    _speciesMomenta.resize(Sim->species.size());
    _speciesMasses.clear();
    _speciesMasses.resize(Sim->species.size());

    _internalEnergy.clear();
    _internalEnergy.resize(Sim->N, 0);

    for (auto ptr1 = Sim->particles.begin(); ptr1 != Sim->particles.end(); ++ptr1)
      for (auto ptr2 = ptr1 + 1; ptr2 != Sim->particles.end(); ++ptr2)
	{
	  double energy = 0.5 * Sim->getInteraction(*ptr1, *ptr2)->getInternalEnergy(*ptr1, *ptr2);
	  _internalEnergy[ptr1->getID()] += energy;
	  _internalEnergy[ptr2->getID()] += energy;
	}

    for (const Particle& part : Sim->particles)
      {
	const Species& sp = *(Sim->species[part]);
	const double mass = sp.getMass(part.getID());
	if (std::isinf(mass)) continue;
	kineticP += mass * Dyadic(part.getVelocity(), part.getVelocity());
	_speciesMasses[sp.getID()] += mass;
	_speciesMomenta[sp.getID()] += mass * part.getVelocity();
	thermalConductivityFS += part.getVelocity() * (Sim->dynamics->getParticleKineticEnergy(part) + _internalEnergy[part.getID()]);
      }

    Vector sysMomentum(0, 0, 0);
    _systemMass = 0;
    for (size_t i(0); i < Sim->species.size(); ++i)
      {
	sysMomentum += _speciesMomenta[i];
	_systemMass += _speciesMasses[i];
      }

    _kineticP.init(kineticP);
    _sysMomentum.init(sysMomentum);

    //Set up the correlators
    double correlator_dt = Sim->lastRunMFT / 8;
    if (correlator_dt == 0.0)
      correlator_dt = 1.0 / sqrt(getCurrentkT());

    _thermalConductivity.resize(correlator_dt, 10);
    _thermalConductivity.setFreeStreamValue(thermalConductivityFS);

    _viscosity.resize(correlator_dt, 10);
    _viscosity.setFreeStreamValue(kineticP);
    
    _thermalDiffusion.resize(Sim->species.size());
    _mutualDiffusion.resize(Sim->species.size() * Sim->species.size());
    for (size_t spid1(0); spid1 < Sim->species.size(); ++spid1)
      {
	_thermalDiffusion[spid1].resize(correlator_dt, 10);
	_thermalDiffusion[spid1].setFreeStreamValue(thermalConductivityFS, _speciesMomenta[spid1] - (_speciesMasses[spid1] / _systemMass) * _sysMomentum.current());
	
	for (size_t spid2(spid1); spid2 < Sim->species.size(); ++spid2)
	  {
	    _mutualDiffusion[spid1 * Sim->species.size() + spid2].resize(correlator_dt, 10);
	    _mutualDiffusion[spid1 * Sim->species.size() + spid2].setFreeStreamValue
	      (_speciesMomenta[spid1] - (_speciesMasses[spid1] / _systemMass) * _sysMomentum.current(),
	       _speciesMomenta[spid2] - (_speciesMasses[spid2] / _systemMass) * _sysMomentum.current());
	  }
      }

    dout << "Total momentum < ";
    for (size_t iDim = 0; iDim < NDIM; iDim++)
      dout  << _sysMomentum.current()[iDim] / Sim->units.unitMomentum() << " ";
    dout << ">\n";

    std::time(&tstartTime);

    clock_gettime(CLOCK_MONOTONIC, &acc_tstartTime);

    std::string sTime(std::ctime(&tstartTime));
    sTime[sTime.size()-1] = ' ';

    dout << "Started on " << sTime << std::endl;
  }

  void
  OPMisc::eventUpdate(const IntEvent& eevent, const PairEventData& PDat)
  {
    stream(eevent.getdt());
    eventUpdate(PDat);
    CounterData& counterdata = _counters[CounterKey(getClassKey(eevent), eevent.getType())];
    counterdata.count += 2;
  }

  void
  OPMisc::eventUpdate(const GlobalEvent& eevent, const NEventData& NDat)
  {
    stream(eevent.getdt());
    eventUpdate(NDat);
    CounterData& counterdata = _counters[CounterKey(getClassKey(eevent), eevent.getType())];
    counterdata.count += NDat.L1partChanges.size() + NDat.L2partChanges.size();
    for (const ParticleEventData& pData : NDat.L1partChanges)
      counterdata.netimpulse += Sim->species[pData.getSpeciesID()]->getMass(pData.getParticleID()) * (Sim->particles[pData.getParticleID()].getVelocity() -  pData.getOldVel());
  }

  void
  OPMisc::eventUpdate(const LocalEvent& eevent, const NEventData& NDat)
  {
    stream(eevent.getdt());
    eventUpdate(NDat);
    CounterData& counterdata = _counters[CounterKey(getClassKey(eevent), eevent.getType())];
    counterdata.count += NDat.L1partChanges.size() + NDat.L2partChanges.size();
    for (const ParticleEventData& pData : NDat.L1partChanges)
      counterdata.netimpulse += Sim->species[pData.getSpeciesID()]->getMass(pData.getParticleID()) * (Sim->particles[pData.getParticleID()].getVelocity() -  pData.getOldVel());
  }

  void
  OPMisc::eventUpdate(const System& eevent, const NEventData& NDat, const double& dt)
  {
    stream(dt);
    eventUpdate(NDat);
    CounterData& counterdata = _counters[CounterKey(getClassKey(eevent), eevent.getType())];
    counterdata.count += NDat.L1partChanges.size() + NDat.L2partChanges.size();
    for (const ParticleEventData& pData : NDat.L1partChanges)
      counterdata.netimpulse += Sim->species[pData.getSpeciesID()]->getMass(pData.getParticleID()) * (Sim->particles[pData.getParticleID()].getVelocity() -  pData.getOldVel());
  }

  void
  OPMisc::stream(double dt)
  {
    _reverseEvents += (dt < 0);
    _KE.stream(dt);
    _internalE.stream(dt);
    _kineticP.stream(dt);
    _sysMomentum.stream(dt);
    _thermalConductivity.freeStream(dt);
    _viscosity.freeStream(dt);
    for (size_t spid1(0); spid1 < Sim->species.size(); ++spid1)
      {
	_thermalDiffusion[spid1].freeStream(dt);
	for (size_t spid2(spid1); spid2 < Sim->species.size(); ++spid2)
	  _mutualDiffusion[spid1 * Sim->species.size() + spid2].freeStream(dt);
      }
  }

  void OPMisc::eventUpdate(const NEventData& NDat)
  {
    Vector thermalDel(0,0,0);

    for (const ParticleEventData& PDat : NDat.L1partChanges)
      {
	const Particle& part = Sim->particles[PDat.getParticleID()];
	const Species& species = *Sim->species[part];
	const double mass = species.getMass(part.getID());
	const double deltaKE = 0.5 * mass *  (part.getVelocity().nrm2() - PDat.getOldVel().nrm2());
	_KE += deltaKE;
	_internalE += PDat.getDeltaU();
	//This must be updated before p1E is calculated
	_internalEnergy[PDat.getParticleID()] += PDat.getDeltaU();
	const double p1E = Sim->dynamics->getParticleKineticEnergy(part) + _internalEnergy[PDat.getParticleID()];
	const double p1deltaE = deltaKE + PDat.getDeltaU();
	Vector delP1 = mass * (part.getVelocity() - PDat.getOldVel());

        _singleEvents += (PDat.getType() != VIRTUAL);
	_virtualEvents += (PDat.getType() == VIRTUAL);
	
	_kineticP += mass * (Dyadic(part.getVelocity(), part.getVelocity()) - Dyadic(PDat.getOldVel(), PDat.getOldVel()));
	_sysMomentum += delP1;
	_speciesMomenta[species.getID()] += delP1;
	thermalDel += part.getVelocity() * p1E - PDat.getOldVel() * (p1E - p1deltaE);
      }

    for (const PairEventData& PDat : NDat.L2partChanges)
      {
	const Particle& part1 = Sim->particles[PDat.particle1_.getParticleID()];
	const Particle& part2 = Sim->particles[PDat.particle2_.getParticleID()];
	const Species& sp1 = *Sim->species[PDat.particle1_.getSpeciesID()];
	const Species& sp2 = *Sim->species[PDat.particle2_.getSpeciesID()];
	const double p1E = Sim->dynamics->getParticleKineticEnergy(part1) + _internalEnergy[PDat.particle1_.getParticleID()];
	const double p2E = Sim->dynamics->getParticleKineticEnergy(part2) + _internalEnergy[PDat.particle2_.getParticleID()];
	const double mass1 = sp1.getMass(part1.getID());
	const double mass2 = sp2.getMass(part2.getID());
	const Vector delP = mass1 * (part1.getVelocity() - PDat.particle1_.getOldVel());
	const double deltaKE1 = 0.5 * mass1 * (part1.getVelocity().nrm2() - PDat.particle1_.getOldVel().nrm2());
	const double deltaKE2 = 0.5 * mass2 * (part2.getVelocity().nrm2() - PDat.particle2_.getOldVel().nrm2());
	const double p1deltaE = deltaKE1 + PDat.particle1_.getDeltaU();
	const double p2deltaE = deltaKE2 + PDat.particle2_.getDeltaU();

	_KE += deltaKE1 + deltaKE2;
	_internalE += PDat.particle1_.getDeltaU() + PDat.particle2_.getDeltaU();
	_internalEnergy[PDat.particle1_.getParticleID()] += PDat.particle1_.getDeltaU();
	_internalEnergy[PDat.particle2_.getParticleID()] += PDat.particle2_.getDeltaU();
	_dualEvents += (PDat.getType() != VIRTUAL);
	_virtualEvents += (PDat.getType() == VIRTUAL);

	collisionalP += magnet::math::Dyadic(PDat.rij, delP);

	_kineticP
	  += mass1 * (Dyadic(part1.getVelocity(), part1.getVelocity())
		      - Dyadic(PDat.particle1_.getOldVel(), PDat.particle1_.getOldVel()))
	  + mass2 * (Dyadic(part2.getVelocity(), part2.getVelocity())
		     - Dyadic(PDat.particle2_.getOldVel(), PDat.particle2_.getOldVel()));

	_viscosity.addImpulse(magnet::math::Dyadic(PDat.rij, delP));

	_speciesMomenta[sp1.getID()] += delP;
	_speciesMomenta[sp2.getID()] -= delP;

	const Vector thermalImpulse = PDat.rij * p1deltaE;

	_thermalConductivity.addImpulse(thermalImpulse);

	for (size_t spid1(0); spid1 < Sim->species.size(); ++spid1)
	  _thermalDiffusion[spid1].addImpulse(thermalImpulse, Vector(0,0,0));

	thermalDel += part1.getVelocity() * p1E + part2.getVelocity() * p2E
	  - PDat.particle1_.getOldVel() * (p1E - p1deltaE) - PDat.particle2_.getOldVel() * (p2E - p2deltaE);
      }

    _thermalConductivity.setFreeStreamValue
      (_thermalConductivity.getFreeStreamValue() + thermalDel);

    _viscosity.setFreeStreamValue(_kineticP.current());

    for (size_t spid1(0); spid1 < Sim->species.size(); ++spid1)
      {
	_thermalDiffusion[spid1]
	  .setFreeStreamValue(_thermalConductivity.getFreeStreamValue(),
			      _speciesMomenta[spid1] - _sysMomentum.current() * (_speciesMasses[spid1] / _systemMass));

	for (size_t spid2(spid1); spid2 < Sim->species.size(); ++spid2)
	  _mutualDiffusion[spid1 * Sim->species.size() + spid2].setFreeStreamValue
	    (_speciesMomenta[spid1] - (_speciesMasses[spid1] / _systemMass) * _sysMomentum.current(),
	     _speciesMomenta[spid2] - (_speciesMasses[spid2] / _systemMass) * _sysMomentum.current());
      }
  }

  double
  OPMisc::getMFT() const
  {
    return Sim->systemTime * static_cast<double>(Sim->N)
      /(Sim->units.unitTime()
	* ((2.0 * static_cast<double>(_dualEvents))
	   + static_cast<double>(_singleEvents)));
  }

  double 
  OPMisc::getEventsPerSecond() const
  {
    timespec acc_tendTime;
    clock_gettime(CLOCK_MONOTONIC, &acc_tendTime);
    
    double duration = double(acc_tendTime.tv_sec) - double(acc_tstartTime.tv_sec)
      + 1e-9 * (double(acc_tendTime.tv_nsec) - double(acc_tstartTime.tv_nsec));

    return Sim->eventCount / duration;
  }

  double 
  OPMisc::getSimTimePerSecond() const
  {
    timespec acc_tendTime;
    clock_gettime(CLOCK_MONOTONIC, &acc_tendTime);
    
    double duration = double(acc_tendTime.tv_sec) - double(acc_tstartTime.tv_sec)
      + 1e-9 * (double(acc_tendTime.tv_nsec) - double(acc_tstartTime.tv_nsec));

    return Sim->systemTime / (duration * Sim->units.unitTime());
  }

  void
  OPMisc::output(magnet::xml::XmlStream &XML)
  {
    using namespace magnet::xml;

    std::time_t tendTime;
    time(&tendTime);

    std::string sTime(std::ctime(&tstartTime));
    //A hack to remove the newline character at the end
    sTime[sTime.size()-1] = ' ';

    std::string eTime(std::ctime(&tendTime));
    //A hack to remove the newline character at the end
    eTime[eTime.size()-1] = ' ';

    dout << "Ended on " << eTime
	 << "\nTotal Collisions Executed " << Sim->eventCount
	 << "\nAvg Events/s " << getEventsPerSecond()
	 << "\nSim time per second " << getSimTimePerSecond()
	 << std::endl;

    const double V = Sim->getSimVolume();
    const Matrix collP = collisionalP / (V * Sim->systemTime);
    const Matrix P = (_kineticP.mean() / V) + collP;

    XML << tag("Misc")
	<< tag("Density")
	<< attr("val")
	<< Sim->getNumberDensity() * Sim->units.unitVolume()
	<< endtag("Density")

	<< tag("PackingFraction")
	<< attr("val") << Sim->getPackingFraction()
	<< endtag("PackingFraction")

	<< tag("SpeciesCount")
	<< attr("val") << Sim->species.size()
	<< endtag("SpeciesCount")

	<< tag("ParticleCount")
	<< attr("val") << Sim->N
	<< endtag("ParticleCount")

	<< tag("SystemMomentum")
	<< tag("Current")
	<< attr("x") << _sysMomentum.current()[0] / Sim->units.unitMomentum()
	<< attr("y") << _sysMomentum.current()[1] / Sim->units.unitMomentum()
	<< attr("z") << _sysMomentum.current()[2] / Sim->units.unitMomentum()
	<< endtag("Current")
	<< tag("Average")
	<< attr("x") << _sysMomentum.mean()[0] / Sim->units.unitMomentum()
	<< attr("y") << _sysMomentum.mean()[1] / Sim->units.unitMomentum()
	<< attr("z") << _sysMomentum.mean()[2] / Sim->units.unitMomentum()
	<< endtag("Average")
	<< endtag("SystemMomentum")

	<< tag("Temperature")
	<< attr("Mean") << getMeankT() / Sim->units.unitEnergy()
	<< attr("MeanSqr") << getMeanSqrkT() / (Sim->units.unitEnergy() * Sim->units.unitEnergy())
	<< attr("Current") << getCurrentkT() / Sim->units.unitEnergy()
	<< attr("Min") << 2.0 * _KE.min() / (Sim->N * Sim->dynamics->getParticleDOF() * Sim->units.unitEnergy())
	<< attr("Max") << 2.0 * _KE.max() / (Sim->N * Sim->dynamics->getParticleDOF() * Sim->units.unitEnergy())
	<< endtag("Temperature")

	<< tag("UConfigurational")
	<< attr("Mean") << getMeanUConfigurational() / Sim->units.unitEnergy()
	<< attr("MeanSqr") << getMeanSqrUConfigurational() / (Sim->units.unitEnergy() * Sim->units.unitEnergy())
	<< attr("Current") << _internalE.current() / Sim->units.unitEnergy()
	<< attr("Min") << _internalE.min() / Sim->units.unitEnergy()
	<< attr("Max") << _internalE.max() / Sim->units.unitEnergy()
	<< endtag("UConfigurational")

	<< tag("ResidualHeatCapacity")
	<< attr("Value") 
	<< (getMeanSqrUConfigurational() - getMeanUConfigurational() * getMeanUConfigurational())
      / (getMeankT() * getMeankT())
	<< endtag("ResidualHeatCapacity")
	<< tag("Pressure")
	<< attr("Avg") << P.tr() / (3.0 * Sim->units.unitPressure())
	<< tag("Tensor") << chardata()
      ;
    
    for (size_t iDim = 0; iDim < NDIM; ++iDim)
      {
	for (size_t jDim = 0; jDim < NDIM; ++jDim)
	  XML << P(iDim, jDim) / Sim->units.unitPressure() << " ";
	XML << "\n";
      }
    
    XML << endtag("Tensor")
	<< tag("InteractionContribution") << chardata()
      ;
    
    for (size_t iDim = 0; iDim < NDIM; ++iDim)
      {
	for (size_t jDim = 0; jDim < NDIM; ++jDim)
	  XML << collP(iDim, jDim) / Sim->units.unitPressure() << " ";
	XML << "\n";
      }
    
    XML << endtag("InteractionContribution")
	<< endtag("Pressure")
	<< tag("Duration")
	<< attr("Events") << Sim->eventCount
	<< attr("OneParticleEvents") << _singleEvents
	<< attr("TwoParticleEvents") << _dualEvents
	<< attr("VirtualEvents") << _virtualEvents
	<< attr("Time") << Sim->systemTime / Sim->units.unitTime()
	<< endtag("Duration")
	<< tag("EventCounters");
  
    typedef std::pair<CounterKey, CounterData> mappair;
    for (const mappair& mp1 : _counters)
      XML << tag("Entry")
	  << attr("Type") << getClass(mp1.first.first)
	  << attr("Name") << getName(mp1.first.first, Sim)
	  << attr("Event") << mp1.first.second
	  << attr("Count") << mp1.second.count
	  << tag("NetImpulse") 
	  << mp1.second.netimpulse / Sim->units.unitMomentum()
	  << endtag("NetImpulse") 
	  << endtag("Entry");
  
    XML << endtag("EventCounters")
	<< tag("Timing")
	<< attr("Start") << sTime
	<< attr("End") << eTime
	<< attr("EventsPerSec") << getEventsPerSecond()
	<< attr("SimTimePerSec") << getSimTimePerSecond()
	<< endtag("Timing")

	<< tag("PrimaryImageSimulationSize")
	<< Sim->primaryCellSize / Sim->units.unitLength()
	<< endtag("PrimaryImageSimulationSize")
	<< tag("totMeanFreeTime")
	<< attr("val")
	<< getMFT()
	<< endtag("totMeanFreeTime")
	<< tag("NegativeTimeEvents")
	<< attr("Count") << _reverseEvents
	<< endtag("NegativeTimeEvents")
	<< tag("Memusage")
	<< attr("MaxKiloBytes") << magnet::process_mem_usage()
	<< endtag("Memusage")
	<< tag("ThermalConductivity")
	<< tag("Correlator")
	<< chardata();

    {
      std::vector<magnet::math::LogarithmicTimeCorrelator<Vector>::Data>
	data = _thermalConductivity.getAveragedCorrelator();
    
      double inv_units = Sim->units.unitk()
	/ ( Sim->units.unitTime() * Sim->units.unitThermalCond() * 2.0 
	    * std::pow(getMeankT(), 2) * V);

      XML << "0 0 0 0 0\n";
      for (size_t i(0); i < data.size(); ++i)
	XML << data[i].time / Sim->units.unitTime() << " "
	    << data[i].sample_count << " "
	    << data[i].value[0] * inv_units << " "
	    << data[i].value[1] * inv_units << " "
	    << data[i].value[2] * inv_units << "\n";
    }

    XML << endtag("Correlator")
	<< endtag("ThermalConductivity")
	<< tag("Viscosity")
	<< tag("Correlator")
	<< chardata();

    {
      std::vector<magnet::math::LogarithmicTimeCorrelator<Matrix>::Data>
	data = _viscosity.getAveragedCorrelator();
      
      double inv_units = 1.0 / (Sim->units.unitTime() * Sim->units.unitViscosity() * 2.0 * getMeankT() * V);

      XML << "0 0 0 0 0 0 0 0 0 0 0\n";
      for (size_t i(0); i < data.size(); ++i)
	{
	  XML << data[i].time / Sim->units.unitTime() << " "
	      << data[i].sample_count << " ";
	  
	  for (size_t j(0); j < 3; ++j)
	    for (size_t k(0); k < 3; ++k)
	      XML << (data[i].value(j, k) - std::pow(data[i].time * P(j,k) * V, 2.0)) * inv_units << " ";
	  XML << "\n";
	}
    }

    XML << endtag("Correlator")
	<< endtag("Viscosity")
	<< tag("ThermalDiffusion");

    for (size_t i(0); i < Sim->species.size(); ++i)
      {
	XML << tag("Correlator")
	    << attr("Species") << Sim->species[i]->getName()
	    << chardata();
	
	std::vector<magnet::math::LogarithmicTimeCorrelator<Vector>::Data>
	  data = _thermalDiffusion[i].getAveragedCorrelator();
	
	double inv_units = 1.0
	  / (Sim->units.unitTime() * Sim->units.unitThermalDiffusion() * 2.0 * getMeankT() * V);
	
	XML << "0 0 0 0 0\n";
	for (size_t i(0); i < data.size(); ++i)
	  {
	    XML << data[i].time / Sim->units.unitTime() << " "
		<< data[i].sample_count << " ";
	    
	    for (size_t j(0); j < 3; ++j)
	      XML << data[i].value[j] * inv_units << " ";
	    XML << "\n";
	  }
	
	XML << endtag("Correlator");
      }

    XML << endtag("ThermalDiffusion")
	<< tag("MutualDiffusion");

    for (size_t i(0); i < Sim->species.size(); ++i)
      for (size_t j(i); j < Sim->species.size(); ++j)
      {
	XML << tag("Correlator")
	    << attr("Species1") << Sim->species[i]->getName()
	    << attr("Species2") << Sim->species[j]->getName()
	    << chardata();
	
	std::vector<magnet::math::LogarithmicTimeCorrelator<Vector>::Data>
	  data = _mutualDiffusion[i * Sim->species.size() + j].getAveragedCorrelator();
	
	double inv_units = 1.0
	  / (Sim->units.unitTime() * Sim->units.unitMutualDiffusion() * 2.0 * getMeankT() * V);
	
	XML << "0 0 0 0 0\n";
	for (size_t i(0); i < data.size(); ++i)
	  {
	    XML << data[i].time / Sim->units.unitTime() << " "
		<< data[i].sample_count << " ";
	    
	    for (size_t j(0); j < 3; ++j)
	      XML << data[i].value[j] * inv_units << " ";
	    XML << "\n";
	  }
	
	XML << endtag("Correlator");
      }

    XML << endtag("MutualDiffusion")
	<< endtag("Misc");
  }

  void
  OPMisc::periodicOutput()
  {
    time_t rawtime;
    time(&rawtime);

    tm timeInfo;
    localtime_r (&rawtime, &timeInfo);

    char dateString[12] = "";
    strftime(dateString, 12, "%a %H:%M", &timeInfo);

    //Output the date
    I_Pcout() << dateString;

    //Calculate the ETA of the simulation, and take care with overflows and the like
    double _earliest_end_time = HUGE_VAL;
    for (const auto& sysPtr : Sim->systems)
      if (std::dynamic_pointer_cast<SystHalt>(sysPtr))
	_earliest_end_time = std::min(_earliest_end_time, sysPtr->getdt());

    double time_seconds_remaining = _earliest_end_time / (getSimTimePerSecond() * Sim->units.unitTime());
    size_t seconds_remaining = time_seconds_remaining;
    
    if (time_seconds_remaining > std::numeric_limits<size_t>::max())
      seconds_remaining = std::numeric_limits<size_t>::max();

    if (Sim->endEventCount != std::numeric_limits<size_t>::max())
      {
	double event_seconds_remaining = (Sim->endEventCount - Sim->eventCount) / getEventsPerSecond() + 0.5;
	
	if (event_seconds_remaining < std::numeric_limits<size_t>::max())
	  seconds_remaining = std::min(seconds_remaining, size_t(event_seconds_remaining));

      }
  
    if (seconds_remaining != std::numeric_limits<size_t>::max())
      {
	size_t ETA_hours = seconds_remaining / 3600;
	size_t ETA_mins = (seconds_remaining / 60) % 60;
	size_t ETA_secs = seconds_remaining % 60;

	I_Pcout() << ", ETA ";
	if (ETA_hours)
	  I_Pcout() << ETA_hours << "hr ";
	  
	if (ETA_mins)
	  I_Pcout() << ETA_mins << "min ";

	I_Pcout() << ETA_secs << "s";
      }

    I_Pcout() << ", Events " << (Sim->eventCount+1)/1000 << "k, t "
	      << Sim->systemTime/Sim->units.unitTime() 
	      << ", <MFT> " <<  getMFT()
	      << ", T " << getCurrentkT() / Sim->units.unitEnergy()
	      << ", U " << _internalE.current() / (Sim->units.unitEnergy() * Sim->N);
  }
}
