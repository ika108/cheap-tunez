all:
	gcc -ggdb3 -Wall midi_input.c cheap_utils.c -o midi_input
	gcc -ggdb3 -Wall cheap-tunez.c -o cheap-tunez
 
clean:
	rm -f midi_input cheap-tunez
 
