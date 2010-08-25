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

#include "nullInteraction.hpp"
#include "../../base/is_exception.hpp"
#include "../../extcode/xmlwriter.hpp"
#include "../../extcode/xmlParser.h"
#include "../../dynamics/interactions/intEvent.hpp"
#include "../2particleEventData.hpp"
#include <cstring>

INull::INull(DYNAMO::SimData* tmp, C2Range* nR):
  Interaction(tmp, nR) {}

INull::INull(const XMLNode& XML, DYNAMO::SimData* tmp):
  Interaction(tmp,NULL)
{
  operator<<(XML);
}

void 
INull::initialise(size_t nID)
{ ID=nID; }

void 
INull::operator<<(const XMLNode& XML)
{ 
  if (std::strcmp(XML.getAttribute("Type"),"Null"))
    D_throw() << "Attempting to load NullInteraction from " 
	      << XML.getAttribute("Type") <<" entry";
  
  range.set_ptr(C2Range::loadClass(XML,Sim));
  
  try 
    {
      intName = XML.getAttribute("Name");
    }
  catch (boost::bad_lexical_cast &)
    {
      D_throw() << "Failed a lexical cast in CINull";
    }
}

Iflt 
INull::maxIntDist() const 
{ return 0; }

Iflt 
INull::hardCoreDiam() const 
{ return 0; }

void 
INull::rescaleLengths(Iflt) 
{}

Interaction* 
INull::Clone() const 
{ return new INull(*this); }
  
IntEvent 
INull::getEvent(const Particle &p1, const Particle &p2) const 
{ 
  return IntEvent(p1, p2, HUGE_VAL, NONE, *this);
}

void
INull::runEvent(const Particle&, const Particle&, const IntEvent&) const
{ 
  D_throw() << "Null event trying to run a collision!"; 
}
   
void 
INull::outputXML(xmlw::XmlStream& XML) const
{
  XML << xmlw::attr("Type") << "Null"
      << xmlw::attr("Name") << intName
      << range;
}

void
INull::checkOverlaps(const Particle&, const Particle&) const
{}
