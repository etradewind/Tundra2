# Define target name and output directory
init_target(Math)

MocFolder ()

# Define source files
file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
# file(GLOB MOC_FILES )

AddSourceFolder(Algorithm/Random)
AddSourceFolder(Geometry)
AddSourceFolder(Math)
AddSourceFolder(Time)

#AddSourceFolder(MathGeoLib/src/Algorithm/Random)
#AddSourceFolder(MathGeoLib/src/Geometry)
#AddSourceFolder(MathGeoLib/src/Math)
#AddSourceFolder(MathGeoLib/src/Time)

set(SOURCE_FILES ${CPP_FILES} ${H_FILES})

#QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})

use_core_modules(Framework)

build_library(${TARGET_NAME} STATIC ${SOURCE_FILES} ${MOC_SRCS})

link_package(BOOST)
link_package(QT4)

link_modules(Framework)

SetupCompileFlags()

final_target()
