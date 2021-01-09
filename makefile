CC=gcc
OBJDIR=obj
CFLAGS=-lm
_OBJ = FileSystem.o fsdriver3.o fsLow.o
OBJ = $(patsubst %,$(OBJDIR)/%,$(_OBJ))

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS)
	
myfs: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean:
	rm $(OBJDIR)/*.o myfs