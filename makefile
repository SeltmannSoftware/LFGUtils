DUMP_OBJS =  EXPLODE.O READ_LFG.O LFGDUMP.O
MAKE_OBJS = IMPLODE.O PACK_LFG.O LFGMAKE.O

LFGDUMP: $(DUMP_OBJS)
	$(CC) $^ -o $@

LFGMAKE: $(MAKE_OBJS)
	gcc $^ -o $@

%.O: %.C
	gcc -x c -c $< -o $@

clean:
	-rm *.O
