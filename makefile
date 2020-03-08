DUMP_OBJS = EXPLODE.O READ_LFG.O LFGDUMP.O
MAKE_OBJS = IMPLODE.O PACK_LFG.O LFGMAKE.O

lfgdump: $(DUMP_OBJS)
	gcc $^ -o $@

lfgmake: $(MAKE_OBJS)
	gcc $^ -o $@

%.O: %.C
	gcc -x c -c $< -o $@

clean:
	-rm *.O
