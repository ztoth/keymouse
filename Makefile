# project
TARGET   = keymouse

# directories
BINDIR   = bin
SRCDIR   = src
LIBDIR   = lib
OBJDIR   = $(BINDIR)

# compiler and linker
CC       = g++
LIBS     = -lm -lpthread -lX11 -lXtst
INCLUDES = -I$(SRCDIR)

# check target
ifeq ($(lastword $(MAKECMDGOALS)), release)
FLAGS    = -Wall -Werror -std=c++11 -O2
LDFLAGS  = -L$(LIBDIR)
else ifeq ($(lastword $(MAKECMDGOALS)), profile)
FLAGS    = -Wall -Werror -std=c++11 -O0 -g -ggdb -rdynamic -DPROFILE -fprofile-arcs -ftest-coverage -fno-omit-frame-pointer
LDFLAGS  = -L$(LIBDIR) -fprofile-arcs
LIBS    := $(LIBS) -lgcov
else
FLAGS    = -Wall -Werror -std=c++11 -O0 -g -ggdb -rdynamic -DDEBUG
LDFLAGS  = -L$(LIBDIR)
endif

# main entry point
all release profile: $(BINDIR)/$(TARGET)

# build the application
$(BINDIR)/$(TARGET): $(OBJDIR)/keymouse.o $(OBJDIR)/framework.o
	$(CC) -o $@ $^ $(LDFLAGS) $(INCLUDES) $(LIBS)
$(OBJDIR)/keymouse.o: $(SRCDIR)/keymouse.cc $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/framework.o: $(SRCDIR)/framework.cc $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)

# clean up object files
.PHONEY: clean
clean:
	rm -f $(OBJDIR)/*.o

# generate doxygen
.PHONEY: doc
doc: $(SRCDIR) Doxyfile
	doxygen Doxyfile
