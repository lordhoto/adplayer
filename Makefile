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
		  -Wconversion
CPPFLAGS:=-I.
LDFLAGS:=-g
CXX:=g++
DEPDIR:=.deps

OBJS := \
		adplayer.o

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
