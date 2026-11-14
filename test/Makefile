# Tests de la logica del firmware en el PC (sin placa).
#   make        -> compila y ejecuta
#   make FILTER=evolve  -> solo los tests cuyo nombre contenga "evolve"
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter \
            -fno-omit-frame-pointer
# las capturas del acelerometro (test_steps.cpp / test_motion.cpp), con ruta
# absoluta para que el binario las encuentre se lance desde donde se lance
CXXFLAGS += -DTEST_IMU_DIR='"$(CURDIR)/data/imu"'
LDFLAGS  ?=
SRC_DIR  := ..
BUILD    := build
BIN      := $(BUILD)/tamapoke_tests

SOURCES := main.cpp shim/shim.cpp test_pet.cpp test_dex.cpp test_i18n.cpp test_blit.cpp test_batcurve.cpp test_power.cpp test_roundmask.cpp test_button.cpp \
           test_steps.cpp test_motion.cpp test_battle.cpp test_wind.cpp test_burnin.cpp test_fx.cpp test_ballgame.cpp test_zengame.cpp test_dashgame.cpp test_growth.cpp test_box.cpp test_eggs.cpp test_volume.cpp test_gifts.cpp \
           $(SRC_DIR)/pet.cpp $(SRC_DIR)/i18n.cpp

.PHONY: all run clean asan
all: run

$(BIN): $(SOURCES) framework.h shim/Arduino.h shim/Preferences.h ../roundmask.h ../ui_frame.h ../board_amoled_216.h ../button.h \
        ../steps.h ../motion.h imu_data.h ../battle.h ../dex.h ../wind.h ../burnin.h ../fx.h ../ballgame.h ../zengame.h ../dashgame.h ../growth.h ../box.h ../volume.h ../pet.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $@ $(SOURCES) $(LDFLAGS)

run: $(BIN)
	@./$(BIN) $(FILTER)

# misma suite bajo AddressSanitizer + UBSan: caza desbordes e indices malos
asan: CXXFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
asan: BIN := $(BUILD)/tamapoke_tests_asan
asan:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $(BUILD)/tamapoke_tests_asan $(SOURCES) $(LDFLAGS)
	@./$(BUILD)/tamapoke_tests_asan $(FILTER)

clean:
	rm -rf $(BUILD)
