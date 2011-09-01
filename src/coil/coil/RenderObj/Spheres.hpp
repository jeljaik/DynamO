/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
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

#pragma once
#include "Triangles.hpp"
#include <magnet/GL/objects/primitives/sphere.hpp>
#include <magnet/thread/mutex.hpp>
#include <magnet/CL/sort.hpp>

namespace coil {
  class RTSpheres : public RTriangles
  {
  public:
    struct SphereDetails
    {
      inline SphereDetails(magnet::GL::objects::primitives::Sphere::SphereType type, size_t order, size_t n):
	_type(type, order),
	_nSpheres(n)
      {}

      magnet::GL::objects::primitives::Sphere _type;
      cl_uint _nSpheres;

      void setupCLBuffers(magnet::GL::Context& context)
      {
	_primativeVertices = cl::Buffer(context.getCLContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(cl_float) * 3 * _type.getVertexCount(),
					_type.getVertices());
      }

      cl::Buffer _primativeVertices;
    };

    RTSpheres(size_t N, std::string name);

    virtual void clTick(const magnet::GL::Camera& cam);
    void sortTick(const magnet::GL::Camera& cam);

    virtual void init(const std::tr1::shared_ptr<magnet::thread::TaskQueue>& systemQueue);

    cl::Buffer& getSphereDataBuffer() { return _spherePositions; }
    cl::Buffer& getColorDataBuffer() { return _sphereColors; }

    virtual void pickingRender(magnet::GL::FBO& fbo, const magnet::GL::Camera& cam, uint32_t& offset);
    virtual void finishPicking(uint32_t& offset, const uint32_t val);
  
  protected:
    cl::Program _program;
    cl::Kernel _renderKernel;
    cl::Kernel _sortDataKernel;
    cl::Kernel _colorKernel;
    cl::Kernel _pickingKernel;

    cl::KernelFunctor _sortDataKernelFunc;
    cl::KernelFunctor _renderKernelFunc;
    cl::KernelFunctor _colorKernelFunc;
    cl::KernelFunctor _pickingKernelFunc;

    cl_uint _N;
    static const std::string kernelsrc;

    std::vector<SphereDetails> _renderDetailLevels;

    cl::Buffer _spherePositions; 
    cl::Buffer _sphereColors;
    cl::Buffer _sortKeys, _sortData;

    size_t _frameCount;
    size_t _sortFrequency;
    size_t _workgroupsize;
    size_t _globalsize;

    magnet::CL::sort<cl_uint> sortFunctor;

    void recolor();
  };
}
