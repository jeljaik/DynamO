/* wiimote head tracking for linux
   Originally by Steven Thomas Snyder, stsnyder@ucla.edu

   (C) copyright 2008, Steven Snyder, All Rights Reserved
   (C) copyright 2011, Marcus Bannerman, All Rights Reserved

   LICENSING INFORMATION:
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifdef COIL_wiimote
#include <magnet/exception.hpp>
#include <cwiid.h> /* cwiid wii remote library */

/*! \brief A class to facilitate head tracking using the cwiid
 * library.
 *
 */
class TrackWiimote {
public:
  TrackWiimote(bool wiimoteAboveScreen = true);

  bool updateState();

  bool connect(bdaddr_t* bt_address = BDADDR_ANY);

  void calibrate();

  inline const double& getHeadPosition(const size_t dim) const { return eye_pos[dim]; }

  inline bool connected() const { return m_wiimote; }

  inline double getBatteryLevel() const { return double(m_state.battery) / CWIID_BATTERY_MAX; }

  const cwiid_ir_src& getIRState(size_t IRDotID) const 
  { 
    if (IRDotID >= CWIID_IR_SRC_COUNT)
      M_throw() << "Out of bounds access on wii IR data.";
    return m_state.ir_src[IRDotID]; 
  }

private:
  size_t updateIRPositions();

  void updateHeadPos();

  cwiid_wiimote_t* m_wiimote; /* wiimote connection through cwiid library */

  struct cwiid_state m_state; /* wiimote state (updated every frame) using cwiid library */

  // calculated viewing position
  double eye_pos[3]; 
  double v_angle;
  bool _wiimoteAboveScreen;  
  size_t _valid_ir_points;
};
#endif
