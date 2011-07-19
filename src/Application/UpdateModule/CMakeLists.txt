# Define target name and output directory
init_target (UpdateModule OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB XML_FILES *.xml)

# Qt4 Moc files to "CMake Moc" subgroup
# and ui_*.h generated .h files to "Generated UI" subgroup
MocFolder ()

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})
QT4_WRAP_CPP(MOC_SRCS ${H_FILES})

# Includes
use_core_modules (Core Foundation Interfaces Ui)

# note: Core, Foundation and Interfaces are removed from tundra2 branch, need to modify this and code to make this module compile!

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS})

# Linking
link_modules(Core Foundation Interfaces Ui)

SetupCompileFlagsWithPCH()


final_target ()
