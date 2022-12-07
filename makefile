.phony all:
all:ACS

ACS: ACS
	gcc -pthread ACS.c -o ACS


.phony clean:
clean:
	-rm -rf *.o *.exe