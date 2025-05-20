TREX_CSRC := trex_exec.c trex_verify.c
TREX_CXXSRC := trex_tests.cpp

CFLAGS=-g -std=c99
CXXFLAGS=-g -std=c++20

# Enable verbose compilation with "make V=1"
ifdef V
 Q :=
 E := @:
else
 Q := @
 E := @echo
endif

# De-dupe the list of C source files
CSRC := $(sort $(TREX_CSRC))
CXXSRC := $(sort $(TREX_CXXSRC))

OBJDIR := obj
DEPDIR := .dep

# Define all object files.
OBJ := $(patsubst %,$(OBJDIR)/%,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o))

# Generate list of obj dirs
OBJDIRS := $(sort $(dir $(OBJ)))

# Compiler flags to generate dependency files.
GENDEPFLAGS = -MMD -MP -MF $(DEPDIR)/$(@F).d
ALL_CFLAGS = $(CFLAGS) $(GENDEPFLAGS)
ALL_CXXFLAGS = $(CXXFLAGS) $(GENDEPFLAGS)

check: trex_tests
	./trex_tests

trex_tests: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o trex_tests

$(OBJDIR)/%.o : %.c | $(OBJDIRS)
	$(E) "  CC     $<"
	$(Q)$(CC) -c $(ALL_CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp | $(OBJDIRS)
	$(E) "  CXX    $<"
	$(Q)$(CXX) -c $(ALL_CXXFLAGS) $< -o $@

# Create the output directory
$(OBJDIRS) :
	$(E) "  MKDIR  $(OBJDIRS)"
	$(Q)mkdir -p $(OBJDIRS)

clean:
	$(RM) $(DEPDIR)/*.d
	$(RM) $(OBJDIR)/*.o
	$(RM) trex_tests

# Include the dependency files.
-include $(info $(DEPDIR)) $(shell mkdir $(DEPDIR) 2>/dev/null) $(wildcard $(DEPDIR)/*)

.PHONY: all check distcheck clean
