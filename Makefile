sort:
	@echo "Compile sort_main ..."
	gcc -g -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ \
	    ./examples/sort_main.c \
	    ./src/record.c \
	    ./src/sort.c \
	    ./src/merge.c \
	    ./src/chunk.c \
	    -lbf -lhp_file -o ./build/sort_main -O2

run: sort
	@echo "Running sort_main ..."
	./build/sort_main

clean:
	@echo "Cleaning..."
	rm -f ./build/sort_main
	#Deleting all .db files in the current directory and subdirectories.
	find . -name '*.db' -exec rm -f {} +
