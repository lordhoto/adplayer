SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs`
CXXFLAGS:=-pedantic-errors \
		  -g \
		  -Wall \
		  -Wextra \
		  -Wpointer-arith \
		  -Wcast-qual \
		  -Wcast-align \
		  -Wshadow \
		  -Wnon-virtual-dtor \
		  -Wformat=2 \
		  -Winit-self \
		  -Wfloat-equal \
		  -Wconversion \
		  $(SDL_CFLAGS)
CPPFLAGS:=-I.
LDFLAGS:=-g $(SDL_LIBS)
CXX:=g++
DEPDIR:=.deps

OBJS := \
		adplayer.o \
		dbopl.o \
		music.o

DEPDIRS = $(addsuffix $(DEPDIR),$(sort $(dir $(OBJS))))

adplayer: $(OBJS)
	$(CXX) -o adplayer $(OBJS) $(LDFLAGS)

-include $(wildcard $(addsuffix /*.d,$(DEPDIRS)))

# common rule for .cpp files
%.o: %.cpp
	mkdir -p $(*D)/$(DEPDIR)
	$(CXX) -Wp,-MMD,"$(*D)/$(DEPDIR)/$(*F).d",-MQ,"$@",-MP $(CXXFLAGS) $(CPPFLAGS) -c $(<) -o $*.o

clean:
	rm -f $(OBJS)
	rm -fR $(DEPDIRS)
	rm -f adplayer
