TOOLS += makedep
DESCRIPTION.makedep = The dependency building tool
TARGETS.makedep = makedep$E

SRC.makedep$E = $(wildcard tools/makedep/*.cpp)
