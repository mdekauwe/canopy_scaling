##############################################################################
USER    := $(shell whoami)
HOME     = /Users/$(USER)
#CFLAGS   = -g
#CFLAGS   = -g -Wall -Wextra -Wpedantic \
           -Wformat=2 -Wno-unused-parameter -Wshadow \
           -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
           -Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		   -fsanitize=bounds -fsanitize-undefined-trap-on-error
CFLAGS   = -O3 #-g
ARCH     =  x86_64
INCLS    = -I./include #-I/opt/local/include
LIBS     = -lm #-L/opt/local/lib -lgsl -lgslcblas
CC       =  gcc
PROGRAM  =  canopy_scaling


SOURCES  =  \
$(PROGRAM).c version.c read_param_file.c read_met_file.c \
utilities.c photosynthesis.c initialise_model.c canopy.c \
radiation.c

OBJECTS = $(SOURCES:.c=.o)
RM       =  rm -f
##############################################################################

# top level create the program...
all: 		$(PROGRAM)


version.c:
		git rev-parse HEAD | awk ' BEGIN {print "#include \"version.h\""} \
		                    {print "const char *build_git_sha = \"" $$0"\";"}\
		                     END {}' > version.c
		date | awk 'BEGIN {} {print "const char *build_git_time = \""$$0"\";"}\
		                     END {} ' >> version.c

# Compile the src file...
$(OBJECTS):	$(SOURCES)
		$(CC) ${INCLS} $(CFLAGS) -c $(SOURCES)

# Linking the program...
$(PROGRAM):	$(OBJECTS)
		$(CC) $(OBJECTS) $(LIBS) ${INCLS} $(CFLAGS) -o $(PROGRAM)

clean:
		$(RM) $(OBJECTS) $(PROGRAM) version.c

install:
		cp $(PROGRAM) $(HOME)/bin/$(ARCH)/.
		$(RM) $(OBJECTS)
##############################################################################
