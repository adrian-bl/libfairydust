default: lib

lib:
	@echo "Building libfairydust"
	cd src ; make

testapp:
	cd test ; make
	
clean:
	cd src  ; make clean
	cd test ; make clean
