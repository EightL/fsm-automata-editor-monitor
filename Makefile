all:
	cmake -B build && $(MAKE) -C build
    
run:
	./build/src/fsm_gui

clean:
	rm -rf build