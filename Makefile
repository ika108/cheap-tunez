all:
	gcc -ggdb3 -Wall midi_input.c cheap_shm.c cheap_utils.c -o midi_input -lpthread
	gcc -ggdb3 -Wall cheap-tunez.c cheap_shm.c cheap_utils.c -o cheap-tunez -lpthread
 
clean:
	rm -f midi_input cheap-tunez
 
