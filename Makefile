all:
	gcc -ggdb3 -Wall midi_input.c cheap_utils.c -o midi_input
 
clean:
	rm -f midi_input
 
