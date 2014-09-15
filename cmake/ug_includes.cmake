###############################################################################
# created by Sebastian Reiter, Andreas Vogel, Martin Rupp, Michael Hoffer, ...
# s.b.reiter@googlemail.com
#
# This file handles all options, with which ug can be compiled. Depending on
# those options, it then resolves all dependencies by adding required
# include-paths and by linking to required libraries.
# It also sets associated defines which can be queried in C and C++ Code.
#
# If you're creating an executable or a library which mainly depends on ug4,
# including this file in your CMakeLists.txt may be a good idea.
################################################################################

################################################################################
# Make sure code is only executed once.
# (no indent here, since it affects the whole file)
if(UG_CMAKE_INCLUDES_INCLUDED)
	return()
endif()
set(UG_CMAKE_INCLUDES_INCLUDED on)

################################################################################
# include ug header and library path
get_filename_component(UG_ROOT_CMAKE_PATH ${CMAKE_CURRENT_LIST_FILE} PATH)
set(UG_ROOT_PATH ${UG_ROOT_CMAKE_PATH}/../)
include_directories(${UG_ROOT_PATH}/ugbase)
include_directories(${CMAKE_BINARY_DIR})
link_directories(${UG_ROOT_PATH}/lib)
################################################################################
# include cmake functions
set(CMAKE_MODULE_PATH ${UG_ROOT_PATH}/cmake/modules)
include(${UG_ROOT_PATH}/cmake/compiler_flags.cmake)
# reset flags, because of maybe switched DEBUG option
reset_cxx_flags()

################################################################################
# We want the code to be built into one library (except the plugins, of course)
set(BUILD_ONE_LIB ON)


# Those variables control the build process. They are later modified depending
# on the value of TARGET.
set(buildUGShell OFF)
set(buildForVRL OFF)
set(buildForLUA OFF)
set(buildAlgebra OFF)
set(buildPlugins OFF)
set(buildEmbeddedPlugins OFF)
set(buildDynamicLib OFF)
set(buildGrid OFF)
set(buildDisc OFF)
set(buildBridge OFF)
set(buildBindings OFF)
set(buildRegistry OFF)
set(buildCompileInfo ON)
set(buildMetis OFF)
set(buildParmetis OFF)

# Here we'll store libs, if we find and need them
set(linkLibraries)

################################################################################
# In this section we'll define default variables
	
# Values for the TARGET option
set(targetOptions "ugshell, vrl, libug4, libgrid, ugplugin, gridshell, amg")
set(targetDefault "ugshell")
set(targetExecutableName ugshell)
set(targetLibraryName ug4)

# Values for the DIM option
set(dimOptions "1, 2, 3, ALL, \"1\;2\", \"1\;3\", \"2\;3\"")
set(dimDefault "ALL")

# Values for the CPU option
set(cpuOptions "1, 2, 3, 4, 5, VAR, ALL, \"2\;4\", \"1\;3\;4\" , ..." )
set(cpuDefault "ALL")

# Values for the blas / lapack option
set(blasDefault ON)
set(lapackDefault ON)

# If enabled, the boost version contained in the externals folder will be used
set(internalBoostDefault ON)

# Option to add svn head revision, compile date and build host into ugshell's initial output
set(svnDefault ON)

# Precision of the number type
set(precisionDefault "double")
set(precisionOptions "single, double")

# Values for the PROFILER option
set(profilerOptions "None, Shiny, Scalasca, Vampir, ScoreP")
set(profilerDefault "None")

# If we run the script the first time, search for MPI to determine the default value
if(LOCAL_OPENMPI)
	message("local openmpi")
	set(MPI_C_INCLUDE_PATH "~/local/openmpi/used/include")
	set(MPI_CXX_INCLUDE_PATH "~/local/openmpi/used/include")
	set(MPI_C_LIBRARIES "mpi")
	set(MPI_CXX_LIBRARIES "mpi;mpi_cxx")
endif(LOCAL_OPENMPI)
if(BUILTIN_MPI)
	set(MPI_FOUND YES)
else(BUILTIN_MPI)
	if(NOT mpiHasBeenSearched)
		find_package(MPI)
		set(mpiHasBeenSearched ON CACHE BOOL "This var is set to true after mpi has been searched the first time.")
	endif(NOT mpiHasBeenSearched)
endif(BUILTIN_MPI)


set(posixDefault OFF)
if(WIN32)
	add_definitions(-DUG_WIN32)
	if(CYGWIN)
		add_definitions(-DUG_CYGWIN)
	endif(CYGWIN)
else(WIN32)
	if(UNIX)
		set(posixDefault ON)
	endif(UNIX)
endif(WIN32)

add_definitions(-DUG_PROFILER_SHINY_CHECK_CONSISTENCY)

################################################################################
# All available options should be defined here:
# cmake-options can either be on or off.
# note: none-on/off-variables are set below, and the docstring is set at the end of this file


# the following options are real cmake-options
option(STATIC_BUILD "Enables static linking. Valid options are: ON, OFF" OFF)
option(DEBUG "Enables debugging. Valid options are: ON, OFF" OFF)
option(DEBUG_LOGS "Enables debug output. Valid options are: ON, OFF" OFF)
option(PARALLEL "Enables parallel compilation. Valid options are: ON, OFF" ${MPI_FOUND})
option(PROFILE_PCL "Enables profiling of the pcl-library. Valid options are ON, OFF" OFF)
option(SHINY_CALL_LOGGING "Enables Call Logging for Shiny. Valid options are ON, OFF" OFF)
option(PROFILE_BRIDGE "Enables profiling of bridge objects. Valid options are ON, OFF" OFF)
option(PCL_DEBUG_BARRIER "Enables debug barriers in the pcl-library. Valid options are ON, OFF" OFF)
option(LAPACK "Lapack won't be used, even if available. Valid options are ON, OFF" ${lapackDefault})
option(BLAS "Blas won't be used, even if available. Valid options are ON, OFF" ${blasDefault})
option(METIS "Embeds the Metis library into UG. Valid options are ON, OFF" OFF)
option(PARMETIS "Embeds the Parmetis library for non commercial use. Valid options are ON, OFF" OFF)
option(INTERNAL_BOOST "If enabled, the boost version found in the externals directory will be used. Valid options are ON, OFF" ${internalBoostDefault})
option(BUILTIN_BLAS "BLAS is built into compiler" OFF)
option(BUILTIN_LAPACK "LAPACK is built into compiler" OFF)
option(BUILTIN_MPI "MPI is built into compiler" OFF)
option(OPENMP "Enables use of OpenMP. Valid options are ON, OFF" OFF)
option(CXX11 "Enables compilation with C++11 standard. Valid options are ON, OFF" OFF)
option(EMBEDDED_PLUGINS "Plugin sources are directly included in libug4. No dynamic loading required. Valid options are ON, OFF " OFF)
option(COMPILE_INFO "Embeds information on compile revision and date. Requires relinking of all involved libraries. Valid options are ON, OFF " ${buildCompileInfo})
option(POSIX "If enabled and available, some additional functionality may be available. Valid options are ON, OFF " ${posixDefault})
option(BUILD_UGDOCU "If enabled, every build builds a new completion file for ugIDE" OFF)
option(CRS_ALGEBRA "Use the CRS Sparse Matrix" OFF)
option(CPU_ALGEBRA "Use the old CPU Sparse Matrix" ON)
option(INTERNAL_MEMTRACKER "Internal Memory Tracker" OFF)

if(APPLE)
	option(USE_LUA2C "Use LUA2C" ON)
else(APPLE)
	option(USE_LUA2C "Use LUA2C" OFF)
endif(APPLE)

################################################################################
# set default values for pseudo-options
if(NOT TARGET)
	if(BUILDING_PLUGIN)
		set(TARGET ugplugin)
	else(BUILDING_PLUGIN)
		set(TARGET ${targetDefault})
	endif(BUILDING_PLUGIN)
endif(NOT TARGET)

if(NOT DIM)
	set(DIM ${dimDefault})
endif(NOT DIM)

if(NOT CPU)
	set(CPU ${cpuDefault})
endif(NOT CPU)

if(NOT PRECISION)
	set(PRECISION ${precisionDefault})
endif(NOT PRECISION)

if(NOT PROFILER)
	set(PROFILER ${profilerDefault})
endif(NOT PROFILER)

# convert the DIM and CPU sets to readable sets
# otherwise "2;3" gets "23" .
# todo: make this a function/macro
set(DIMReadable "")
foreach(d ${DIM})
	if("${DIMReadable}" STREQUAL "")
		set(DIMReadable ${d})
	else("${DIMReadable}" STREQUAL "")
		set(DIMReadable "${DIMReadable}" "\;" ${d})
	endif("${DIMReadable}" STREQUAL "")
endforeach(d)
set(CPUReadable "")
foreach(d ${CPU})
	if("${CPUReadable}" STREQUAL "")
		set(CPUReadable ${d})
	else("${CPUReadable}" STREQUAL "")
		set(CPUReadable "${CPUReadable}" "\;" ${d})
	endif("${CPUReadable}" STREQUAL "")
endforeach(d)



################################################################################
# We'll output the current options-setting in this section
message(STATUS "")
message(STATUS "Info: Current options:")
message(STATUS "Info: TARGET:            ${TARGET} (options are: ${targetOptions})")
message(STATUS "Info: DIM:               " ${DIMReadable} " (options are: " ${dimOptions} ")")
message(STATUS "Info: CPU:               " ${CPUReadable} " (options are: " ${cpuOptions} ")")
message(STATUS "Info: PRECISION:         ${PRECISION} (options are: ${precisionOptions})")
message(STATUS "Info: STATIC_BUILD:      ${STATIC_BUILD} (options are: ON, OFF)")
message(STATUS "Info: DEBUG:             ${DEBUG} (options are: ON, OFF)")
message(STATUS "Info: DEBUG_LOGS:        ${DEBUG_LOGS} (options are: ON, OFF)")
message(STATUS "Info: PARALLEL:          ${PARALLEL} (options are: ON, OFF)")
message(STATUS "Info: PCL_DEBUG_BARRIER: ${PCL_DEBUG_BARRIER} (options are: ON, OFF)")
message(STATUS "Info: PROFILER:          ${PROFILER} (options are: ${profilerOptions})")
message(STATUS "Info: PROFILE_PCL:       ${PROFILE_PCL} (options are: ON, OFF)")
message(STATUS "Info: PROFILE_BRIDGE:    ${PROFILE_BRIDGE} (options are: ON, OFF)")
message(STATUS "Info: LAPACK:            ${LAPACK} (options are: ON, OFF)")
message(STATUS "Info: BLAS:              ${BLAS} (options are: ON, OFF)")
message(STATUS "Info: METIS:             ${METIS} (options are: ON, OFF)")
message(STATUS "Info: PARMETIS:          ${PARMETIS} (options are: ON, OFF)")
message(STATUS "Info: INTERNAL_BOOST:    ${INTERNAL_BOOST} (options are: ON, OFF)")
message(STATUS "Info: EMBEDDED_PLUGINS   ${EMBEDDED_PLUGINS} (options are: ON, OFF)")
message(STATUS "Info: COMPILE_INFO       ${COMPILE_INFO} (options are: ON, OFF)")
message(STATUS "")
message(STATUS "Info: External libraries (path which contains the library or ON if you used uginstall):")
message(STATUS "Info: TETGEN:   ${TETGEN}")
message(STATUS "Info: HLIBPRO:  ${HLIBPRO}")
message(STATUS "")
message(STATUS "Info: C Compiler ID: ${CMAKE_C_COMPILER_ID}, C++ Compiler ID: ${CMAKE_CXX_COMPILER_ID}") 
message(STATUS "")

if(INTERNAL_MEMTRACKER)
    message(STATUS "Info: Using Internal Memtracker (INTERNAL_MEMTRACKER=ON)")
endif(INTERNAL_MEMTRACKER)


################################################################################
# Options are processed in this section.

########################################
# TARGET
include(${UG_ROOT_PATH}/cmake/ug/target.cmake)

########################################
# PRECISION
if("${PRECISION}" STREQUAL "single")
	add_definitions(-DUG_SINGLE_PRECISION)
elseif("${PRECISION}" STREQUAL "double")
	# Nothing to do here. double-precision is the default
else("${PRECISION}" STREQUAL "single")
	message(FATAL_ERROR "Unsupported PRECISION: ${PRECISION}. Options are: ${precisionOptions}")
endif("${PRECISION}" STREQUAL "single")

########################################
# STATIC
# If STATIC is enabled, then a static ug-lib will be built. This will add a
# _s suffix to the resulting lib name.
# Note that no plugins are built if STATIC is enabled.


set(buildEmbeddedPlugins ${EMBEDDED_PLUGINS})

if(STATIC_BUILD)
	set(buildDynamicLib OFF)
	set(buildEmbeddedPlugins ON)
	set(targetLibraryName ${targetLibraryName}_s)
	add_definitions(-DUG_STATIC)
else(STATIC_BUILD)
	set(buildDynamicLib ON)
	set(UG_SHARED ON)
endif(STATIC_BUILD)

if(buildEmbeddedPlugins)
	add_definitions(-DUG_EMBEDDED_PLUGINS)
endif(buildEmbeddedPlugins)


########################################
# DIM
# The dim option sets defines for C and C++
if("${DIM}" STREQUAL "ALL")
	add_definitions(-DUG_DIM_1 -DUG_DIM_2 -DUG_DIM_3)
	
else("${DIM}" STREQUAL "ALL")
	# DIM is a string of dimensions (e.g. "1;2")
	# loop dims
	foreach(d ${DIM})
		# check if dim is valid
		if(d GREATER 3 OR d LESS 1)
			message(FATAL_ERROR "ERROR: Cannot build world dimension ${d}. "
													"Valid options are: ${dimOptions}")
		endif(d GREATER 3 OR d LESS 1)
	
		add_definitions(-DUG_DIM_${d})
	endforeach(d)
endif("${DIM}" STREQUAL "ALL")

########################################
# Algebra Selection (CPU=)
# The cpu option sets defines for C and C++
include(${UG_ROOT_PATH}/cmake/ug/algebra_selection.cmake)

########################################
# COMPILER specific flags
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Cray")
	# Cray Compiler: add support for gnu extensions
	add_cxx_flag("-h gnu")
	# remove warning "The controlling expression is constant"
	add_cxx_flag("-hnomessage=236")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "XL")
    # currently no flags for IBM xl compiler
    # however, the -Wall option is not supported
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions("/EHsc")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	add_cxx_flag("-Wall")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	add_cxx_flag("-Wall")
    # for some reason -Wsign-compare is not in -Wall for Clang 
	add_cxx_flag("-Wsign-compare")
	#set(CMAKE_CPP_FLAGS	"${CMAKE_CPP_FLAGS} -Wno-overloaded-virtual -Wno-autological-compare" CACHE STRING "overriden flags!" FORCE)
endif()

########################################
# DEBUG
########################################
include(${UG_ROOT_PATH}/cmake/ug/debug.cmake)

# if build type is set print own cflags and flags from build type 
if(CMAKE_BUILD_TYPE)
	string(TOUPPER ${CMAKE_BUILD_TYPE} bt_upper)
	message(STATUS "Info: compiling with cxx flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${bt_upper}}")
	message(STATUS "Info: compiling with c flags: ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${bt_upper}}")
else()
	message(STATUS "Info: compiling with cxx flags: ${CMAKE_CXX_FLAGS}")
	message(STATUS "Info: compiling with c flags: ${CMAKE_C_FLAGS}")
endif()

########################################
# PROFILER
include(${UG_ROOT_PATH}/cmake/ug/profiler.cmake)


########################################
# PCL_DEBUG_BARRIER
if(PCL_DEBUG_BARRIER)
	add_definitions(-DPCL_DEBUG_BARRIER_ENABLED)
endif(PCL_DEBUG_BARRIER)


########################################
# OPENMP
include(${UG_ROOT_PATH}/cmake/ug/openmp.cmake)
# C++11
include(${UG_ROOT_PATH}/cmake/ug/cpp11.cmake)
# CUDA
include(${UG_ROOT_PATH}/cmake/ug/cuda.cmake)
# LUA2C
include(${UG_ROOT_PATH}/cmake/ug/lua2c.cmake)

########################################
# buildAlgebra
include(${UG_ROOT_PATH}/cmake/ug/build_algebra.cmake)


########################################
# METIS
include(${UG_ROOT_PATH}/cmake/ug/metis.cmake)
# PARMETIS
include(${UG_ROOT_PATH}/cmake/ug/parmetis.cmake)    
# TETGEN
include(${UG_ROOT_PATH}/cmake/ug/tetgen.cmake)
# HLIBPRO
include(${UG_ROOT_PATH}/cmake/ug/hlibpro.cmake)
# OpenCL
include(${UG_ROOT_PATH}/cmake/ug/opencl.cmake)


################################################################################
# find and collect required libraries

########################################
# Boost (required)
#  If INTERNAL_BOOST is enabled, the files in externals/boost_... are used.
#  If it is disabled, the system-installation of boost is used instead.
#  Note: If INTERNAL_BOOST is enabled and system installations are available,
#        the internal one has precedence.
if(INTERNAL_BOOST)
	set(INTERNAL_BOOST_PATH ${UG_ROOT_PATH}/externals/boost_1_56_0/)
	set(BOOST_ROOT ${INTERNAL_BOOST_PATH})
	message(STATUS "Info: Try using internal Boost from externals/boost_1_56_0")
endif(INTERNAL_BOOST)
find_package(Boost 1.40 REQUIRED)
message(STATUS "Info: Including Boost from ${Boost_INCLUDE_DIRS}")
include_directories(${Boost_INCLUDE_DIRS})


########################################
# dynamic linking
if(UNIX)
	set(linkLibraries ${linkLibraries} dl)
# for cekon pthread bug
#    set(linkLibraries ${linkLibraries} pthread)
elseif(WIN32)
	set(linkLibraries ${linkLibraries} Kernel32)
endif(UNIX)




########################################
# MPI
include(${UG_ROOT_PATH}/cmake/ug/mpi.cmake)


########################################
set(buildCompileInfo ${COMPILE_INFO})
if(buildCompileInfo)
	message(STATUS "Info: COMPILE_INFO enabled. Causes relinking on each run of make.")
endif(buildCompileInfo)


########################################
if(POSIX)
	add_definitions(-DUG_POSIX)
endif(POSIX)

########################################
if(HERMIT_EXPERIMENTAL)
	add_definitions(-DUG_PARALLEL)
	add_definitions(-DBLAS_AVAILABLE)
	add_definitions(-DLAPACK_AVAILABLE)
	message(STATUS "Info: Using experimental stuff on Hermit.")
endif(HERMIT_EXPERIMENTAL)

########################################
# JNI
if(buildForVRL)
	find_package(JNI REQUIRED)
    include_directories(${JNI_INCLUDE_DIRS})
	add_definitions(-DUG_FOR_VRL)
endif(buildForVRL)

########################################
# LUA
if(buildForLUA)
	add_definitions(-DUG_FOR_LUA)
endif(buildForLUA)


################################################################################
# This is a hidden option, which is currently only required for builds on jugene.
# Note that the associated option is enabled in /cmake/toolchain/jugene.cmake
if(enableDynamicOption)
	add_definitions(-dynamic)
endif(enableDynamicOption)


################################################################################
# Those options are temporary and should be removed in future builds.
# They are left from the old build script.
if(buildPlugins)
	add_definitions(-DUG_PLUGINS)
endif(buildPlugins)
if(buildBridge)
	add_definitions(-DUG_BRIDGE)
endif(buildBridge)
set(UG_DEBUG ${DEBUG})


################################################################################
# link against required libraries
link_libraries(${linkLibraries})


################################################################################
# Declare a method that allows all sub-cmake-files to add their sources
# to a common library.
include(${UG_ROOT_PATH}/cmake/ug/export_sources.cmake)

################################################################################
# Declare a method that allows all sub-cmake-files to add their dependencies
# to a common library.
include(${UG_ROOT_PATH}/cmake/ug/export_dependencies.cmake)

################################################################################
# Declare a method that allows all sub-cmake-files to add their includes paths
# to a common library.
include(${UG_ROOT_PATH}/cmake/ug/export_includes.cmake)

################################################################################
# Declare a method that allows all sub-cmake-files to add their definitions to
# to the main project P_UG4
include(${UG_ROOT_PATH}/cmake/ug/export_definitions.cmake)

######################################################################################################################
# the following options are pseudo cmake-options (normal options only support ON and OFF).
# Their default values are defined in the section above.
# we need to put this here so we only have to set the doc string and the type once.
# note: do not change the variables after this

set(TARGET ${TARGET} CACHE STRING "Set the target of this build process. Valid options are: ${targetOptions}")
set(DIM ${DIM} CACHE STRING "Set the dimension for which ug4 is build. Valid options are: ${dimOptions}")
set(CPU ${CPU} CACHE STRING "Set block sizes for which ug4 is build. Valid options are: ${cpuOptions}")
set(PRECISION ${PRECISION} CACHE STRING "Set the precision of the number type. Valid options are: ${precisionOptions}")
set(PROFILER ${PROFILER} CACHE STRING "Set the a profiler. Valid options are: ${profilerOptions}")

# the following options too are pseudo cmake-options. However, they should
# contains pathes, if set.
set(TETGEN ${TETGEN} CACHE PATH "Sets the path in which tetgen shall be searched.")
set(HLIBPRO ${HLIBPRO} CACHE PATH "Sets the path in which hlibpro shall be searched.")

set(DEBUG_FORMAT ${DEBUG_FORMAT} CACHE STRING "Debug format options like -g, -gstabs, -ggbd. If not set, debug format is -g.")


# mark some stuff as advanced, so it won't show up in ccmake / CMakeSetup.exe
mark_as_advanced(CMAKE_INSTALL_PREFIX)
IF(APPLE)
	mark_as_advanced(CMAKE_OSX_ARCHITECTURES)
	mark_as_advanced(CMAKE_OSX_DEPLOYMENT_TARGET)
	mark_as_advanced(CMAKE_OSX_SYSROOT)
ENDIF(APPLE)
mark_as_advanced(LAPACK_INCLUDE_PATH)
mark_as_advanced(LAPACK_LIBRARIES)
mark_as_advanced(libReadline)
mark_as_advanced(mpiHasBeenSearched)
mark_as_advanced(BLAS_INCLUDE_PATH)
mark_as_advanced(BLAS_LIBRARIES)
#mark_as_advanced(CMAKE_BUILD_TYPE)
mark_as_advanced(CMAKE_CXX_FLAGS)
mark_as_advanced(CMAKE_C_FLAGS)

mark_as_advanced(BUILTIN_LAPACK)
mark_as_advanced(BUILTIN_BLAS)
mark_as_advanced(BUILTIN_MPI)

