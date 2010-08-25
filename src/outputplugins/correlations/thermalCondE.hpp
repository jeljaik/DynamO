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

#ifndef OPThermalConductivityE_H
#define OPThermalConductivityE_H

#include "../outputplugin.hpp"
#include "../../datatypes/vector.hpp"
#include <boost/circular_buffer.hpp>

/*! \brief The Correlator class for the Thermal Conductivity.*/
class OPThermalConductivityE: public OutputPlugin
{
public:
  OPThermalConductivityE(const DYNAMO::SimData*, const XMLNode&);

  virtual void initialise();

  virtual void output(xmlw::XmlStream&);

  virtual OutputPlugin* Clone() const 
  { return new OPThermalConductivityE(*this); }
  
  virtual void eventUpdate(const CGlobEvent&, const CNParticleData&);

  virtual void eventUpdate(const CLocalEvent&, const CNParticleData&);

  virtual void eventUpdate(const CSystem&, const CNParticleData&, const Iflt&); 
  
  virtual void eventUpdate(const IntEvent&, const C2ParticleData&);

  virtual void operator<<(const XMLNode&);

protected:
  boost::circular_buffer<Vector  > G;
  std::vector<Vector  > accG2;
  size_t count;
  Iflt dt, currentdt;
  Vector  constDelG, delG;
  size_t currlen;
  bool notReady;

  size_t CorrelatorLength;

  void stream(const Iflt&);

  void newG();

  void accPass();

  Iflt rescaleFactor();
  
  Vector  impulseDelG(const C2ParticleData&);
  Vector  impulseDelG(const CNParticleData&);

  void updateConstDelG(const CNParticleData&);
  void updateConstDelG(const C2ParticleData&);
  void updateConstDelG(const C1ParticleData&);
};

#endif

