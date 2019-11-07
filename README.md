# supereight: a fast octree library for Dense SLAM
welcome to *supereight*: a high performance template octree library and a dense
volumetric SLAM pipeline implementation.

# Related publications
This software implements the octree library and dense SLAM system presented in
our paper
[Efficient Octree-Based Volumetric SLAM Supporting Signed-Distance and
Occupancy
Mapping.](https://spiral.imperial.ac.uk/bitstream/10044/1/55715/2/EVespaRAL_final.pdf)
If you publish work that relates to this software,
please cite our paper as:

`@ARTICLE{VespaRAL18,
author={E. Vespa and N. Nikolov and M. Grimm and L. Nardi and P. H. J. Kelly
and S. Leutenegger},
journal={IEEE Robotics and Automation Letters},
title={Efficient Octree-Based Volumetric SLAM Supporting Signed-Distance and
Occupancy Mapping}, year={2018}, volume={3}, number={2}, pages={1144-1151},
doi={10.1109/LRA.2018.2792537}, ISSN={}, month={April}}`

# Licence
The core library is released under the BSD 3-clause Licence. There are part of
the this software that are released under MIT licence, see individual headers
for which licence applies.

# Project structure
supereight is made of three main components:

* `se_core`: the main header only template octree library
* `se_denseslam`: the volumetric SLAM pipelines presented in [1], which can be
  compiled in a library and used in external projects. Notice that the pipeline
  API exposes the discrete octree map via a shared pointer. As the map is a
  template class, it needs to be instantiated correctly. You do this by
  defining a `SE_FIELD_TYPE` macro before including `DenseSLAMSystem.h`. The
  field type must be consistent with the library you are linking against. Have a
  look at `se_denseslam` and `se_apps` CMakeLists to see how it is done in our
  examples.
* `se_apps`: front-end applications which run the `se-denseslam` pipelines on
  given inputs or live camera.
* `se_shared`: third party libraries and code required throughout supereight
* `se_tools`: related tools and scripts (e.g. for converting datasets and
  evaluating the pipeline)

# Dependencies
The following packages are required to build the `se-denseslam` library:
* CMake >= 3.10
* Eigen3
* Sophus
* OpenMP (optional)
* googletest (for unit tests)

The benchmarking and GUI apps additionally require:
* GLut
* OpenGL
* OpenNI2
* PkgConfig/Qt5
* Python/Numpy for evaluation scripts

To install the dependencies run
```
./se_tools/install_dependencies.sh
```

# Build
From the project root:
`make`
This will create a build/ folder from which `cmake ..` is invoked.

# Installation
After building, supereight can be installed system-wide by running
```
sudo make install
```
This will also install the appropriate `supereightConfig.cmake` file so that
supereight can be used in a CMake project by adding `find_package(supereight)`.
Executables using supereight should be linked against all the libraries in
`SUPEREIGHT_CORE_LIBS` and, if the dense SLAM pipeline functionality is
desired, with ONE of the libraries in the `SUPEREIGHT_DENSESLAM_LIBS` CMake
variable.  More information about the variables defined by using
`find_package(supereight)` along with usage examples can be found in
[cmake/Config.cmake.in](cmake/Config.cmake.in).

# Running unit tests
The unit tests use googletest. It must be compiled with the same flags as
supereight and the environment variable `GTEST_ROOT` set to the path which
contains the googletest library. To compile and run the tests, assuming
googletest was compiled in `~/googletest/googletest`, run:
```
GTEST_ROOT=~/googletest/googletest make test
```

# Usage example
To run one of the apps in se_apps you need to first produce an input file. We
use SLAMBench 1.0 file format (https://github.com/pamela-project/slambench).
The tool scene2raw can be used to produce an input sequence from the ICL-NUIM
dataset:
```
mkdir living_room_traj2_loop
cd living_room_traj2_loop
wget http://www.doc.ic.ac.uk/~ahanda/living_room_traj2_loop.tgz
tar xzf living_room_traj2_loop.tgz
cd ..
build/se_tools/scene2raw living_room_traj2_loop living_room_traj2_loop/scene.raw
```
Then it can be used as input to one of the apps

```
./build/se_apps/se-denseslam-tsdf-main -i living_room_traj2_loop/scene.raw -s 4.8 -p 0.34,0.5,0.24 -z 4 -c 2 -r 1 -k 481.2,-480,320,240  > benchmark.log
```
