# Project names
PROJ_COLLECTOR = run_collector
PROJ_SIMULATION = adsb_simulation

# Source files
C_SOURCE = $(wildcard ./src/*.c)

# Header files
H_SOURCE = $(wildcard ./src/*.h)

# Object files for each executable
OBJ_COLLECTOR = $(patsubst ./src/%.c, ./objects/collector/%.o, $(filter-out ./src/adsb_simulation.c, $(C_SOURCE)))
OBJ_SIMULATION = $(patsubst ./src/%.c, ./objects/simulation/%.o, $(filter-out ./src/adsb_collector.c, $(C_SOURCE)))

# Compiler
#CC = gcc
CC_CROSS = arm-none-eabi-gcc
CC = $(CC_CROSS)

# Compiler flags
CC_FLAGS = -c        \
           -W        \
           -Wall     \
           -pedantic

# Library flags
# Incluindo librtlsdr
LDFLAGS = -lm -l sqlite3 -lrt -lrtlsdr


# Directories for object files
OBJ_DIR_COLLECTOR = objects/collector
OBJ_DIR_SIMULATION = objects/simulation

# Ensure the objects directories exist
$(shell mkdir -p $(OBJ_DIR_COLLECTOR))
$(shell mkdir -p $(OBJ_DIR_SIMULATION))

# Default target: build both executables
all: $(PROJ_COLLECTOR) $(PROJ_SIMULATION)

# Rule to build the collector executable
$(PROJ_COLLECTOR): $(OBJ_COLLECTOR)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule to build the simulation executable
$(PROJ_SIMULATION): $(OBJ_SIMULATION)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule to compile collector object files
$(OBJ_DIR_COLLECTOR)/%.o: ./src/%.c $(H_SOURCE)
	$(CC) $(CC_FLAGS) -o $@ $<

# Rule to compile simulation object files
$(OBJ_DIR_SIMULATION)/%.o: ./src/%.c $(H_SOURCE)
	$(CC) $(CC_FLAGS) -o $@ $<

# Clean up
clean:
	rm -rf $(OBJ_DIR_COLLECTOR) $(OBJ_DIR_SIMULATION) $(PROJ_COLLECTOR) $(PROJ_SIMULATION) *~