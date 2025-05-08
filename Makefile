# -----------------------------------------------------------------------------
# @file   Makefile
# @brief  Generic Makefile for the FSM editor project.
#
# Provides build targets for compiling the project, running the GUI application,
# executing tests, generating documentation, and creating submission packages.
# Configures CMake build with proper options and provides convenient shortcuts.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

# ---------- configurable knobs ---------------------------------------------
BUILD_DIR   ?= build
BIN_DIR     := $(BUILD_DIR)/bin
DOC_DIR     := doc
PACK_NAME   ?= xsevcim00-xlucnyj00
CMAKE       ?= cmake
BUILD_TYPE  ?= Release               # switch to Debug with:  make BUILD_TYPE=Debug
JOBS        ?= $(shell nproc)        # parallelism for the native make inside CMake

# ————————————————————————————————————————————————————————————————
# ** Your Qt install root (must contain lib/cmake/Qt5/… ) 
CMAKE_PREFIX_PATH ?= /usr/local/share/Qt-5.9.2/5.9.2/gcc_64

# ** Qt “platforms” plugin dir (must contain libqxcb.so)  
QT_QPA_PLATFORM_PLUGIN_PATH ?= $(CMAKE_PREFIX_PATH)/plugins/platforms
# ————————————————————————————————————————————————————————————————

.PHONY: all clean doxygen pack
all: build

# ---------------------------------------------------------------------------
#  Default target: compile everything
# ---------------------------------------------------------------------------
build:
	@echo "\n==[ CMake configure ]================================================"
	CMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH) \
	$(CMAKE) -S . -B $(BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH)
	@echo "\n==[ Build ]==========================================================="
	$(CMAKE) --build $(BUILD_DIR) -- -j$(JOBS)

# ---------------------------------------------------------------------------
#  Run the demo – spec requires a 'make run' convenience target
# ---------------------------------------------------------------------------
run: build
	@echo "\n==[ Launch GUI client ]=============================================="
	QT_QPA_PLATFORM_PLUGIN_PATH=$(QT_QPA_PLATFORM_PLUGIN_PATH) \
	$(BIN_DIR)/gui_qt_client_exec

# ---------------------------------------------------------------------------
#  Doxygen documentation
# ---------------------------------------------------------------------------
doxygen:
	@echo "\n==[ Doxygen ]========================================================="
	doxygen Doxyfile
	@echo "HTML documentation generated in $(DOC_DIR)/html"

# ---------------------------------------------------------------------------
#  Clean every generated file
# ---------------------------------------------------------------------------
clean:
	@echo "\n==[ Clean ]==========================================================="
	rm -rf $(BUILD_DIR) $(DOC_DIR)/html $(PACK_NAME).zip doxygen_warnings.log

# ---------------------------------------------------------------------------
#  Pack everything for IS submission
# ---------------------------------------------------------------------------
pack: clean
	@echo "\n==[ Creating submission archive ]===================================="
	zip -qr $(PACK_NAME).zip \
		. -x "$(BUILD_DIR)/*" "$(DOC_DIR)/html/*" "*.zip" "*.tar.gz" "./.*"
	@echo "Wrote $(PACK_NAME).zip"
