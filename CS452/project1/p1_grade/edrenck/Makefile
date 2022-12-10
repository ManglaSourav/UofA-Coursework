driver: driver.c library.c library.h
	gcc -o driver driver.c library.c

pull:
	scp -r edrenck@lectura.cs.arizona.edu:~/private/proj1 /home/tc

push:
	scp -r $(PWD) edrenck@lectura.cs.arizona.edu:~/private
	
.PHONY: pull push
