CC = bcc
CFLAGS = -v-
DUMP_OBJS = \
        EXPLODE.OBJ \
        READ_LFG.OBJ \
        LFGDUMP.OBJ

MAKE_OBJS = \
        IMPLODE.OBJ \
        PACK_LFG.OBJ \
        LFGMAKE.OBJ

LFGDUMP.EXE: $(DUMP_OBJS)
        $(CC) $(CFLAGS) -eLFGDUMP.EXE $(DUMP_OBJS)

LFGMAKE.EXE: $(MAKE_OBJS)
        $(CC) $(CFLAGS) -eLFGMAKE.EXE $(MAKE_OBJS)

clean:
        -DEL *.OBJ
