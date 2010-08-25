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

#include "structureImage.hpp"
#include <fstream>
#include <boost/foreach.hpp>
#include "../../extcode/xmlwriter.hpp"
#include "../../dynamics/include.hpp"
#include "../../base/is_exception.hpp"
#include "../../base/is_simdata.hpp"
#include "../../base/is_colormap.hpp"
#include "../../dynamics/liouvillean/liouvillean.hpp"
#include "../../dynamics/interactions/squarebond.hpp"
#include "../../dynamics/ranges/2RList.hpp"
#include "radiusGyration.hpp"
#include "../../dynamics/topology/chain.hpp"
#include <boost/algorithm/string.hpp>
#include "../../datatypes/vector.xml.hpp"

OPStructureImaging::OPStructureImaging(const DYNAMO::SimData* tmp, const XMLNode& XML):
  OPTicker(tmp,"StructureImaging"),
  id(0),
  imageCount(500)
{
  operator<<(XML);
}

void 
OPStructureImaging::operator<<(const XMLNode& XML)
{
  try 
    {
      if (!(XML.isAttributeSet("Structure")))
	D_throw() << "You must specify the name of the structure to monitor for StructureImaging";
      
      structureName = XML.getAttribute("Structure");
    }
  catch (boost::bad_lexical_cast &)
    {
      D_throw() << "Failed a lexical cast in OPVACF";
    }
}


void 
OPStructureImaging::initialise()
{
  id = Sim->dynamics.getTopology().size();
  
  BOOST_FOREACH(const smrtPlugPtr<CTopology>& ptr, Sim->dynamics.getTopology())
    if (boost::iequals(structureName, ptr->getName()))
      id = ptr->getID();
  
  if (id == Sim->dynamics.getTopology().size())
    D_throw() << "Could not find a structure named " << structureName << " in the simulation";
  
  imagelist.clear();
  ticker();
}

void 
OPStructureImaging::changeSystem(OutputPlugin* nplug) 
{
  std::swap(Sim, static_cast<OPStructureImaging*>(nplug)->Sim);
}

void 
OPStructureImaging::ticker()
{
  if (imageCount != 0)
    {
      --imageCount;
      printImage();
    }
}

void
OPStructureImaging::printImage()
{
  BOOST_FOREACH(const smrtPlugPtr<CRange>& prange, Sim->dynamics.getTopology()[id]->getMolecules())
    {
      std::vector<Vector  > atomDescription;

      Vector  lastpos(Sim->vParticleList[*prange->begin()].getPosition());
      
      Vector  masspos(0,0,0);

      Iflt sysMass(0.0);

      Vector  sumrij(0,0,0);
      
      BOOST_FOREACH(const size_t& pid, *prange)
	{
	  //This is all to make sure we walk along the structure
	  const Particle& part(Sim->vParticleList[pid]);
	  Vector  rij = part.getPosition() - lastpos;
	  lastpos = part.getPosition();
	  Sim->dynamics.BCs().applyBC(rij);
	  
	  sumrij += rij;
	  
	  Iflt pmass = Sim->dynamics.getSpecies(part).getMass();
	  sysMass += pmass;
	  masspos += sumrij * pmass;
	  
	  atomDescription.push_back(sumrij);
	}
      
      masspos /= sysMass;

      BOOST_FOREACH(Vector & rijpos, atomDescription)
	rijpos -= masspos;

      //We use a trick to not copy the vector, just swap it over
      imagelist.push_back(std::vector<Vector  >());
      std::swap(imagelist.back(), atomDescription);
    }
}

void 
OPStructureImaging::output(xmlw::XmlStream& XML)
{
  XML << xmlw::tag("StructureImages")
      << xmlw::attr("version") << 2;
  
  BOOST_FOREACH(const std::vector<Vector  >& vec, imagelist)
    {
      XML << xmlw::tag("Image");

      size_t id(0);

      BOOST_FOREACH(const Vector & vec2, vec)
	{
	  XML << xmlw::tag("Atom")
	      << xmlw::attr("ID")
	      << id++
	      << (vec2 / Sim->dynamics.units().unitLength())
	      << xmlw::endtag("Atom");
	}
	  
      XML << xmlw::endtag("Image");
    }
    
  XML << xmlw::endtag("StructureImages");
}
