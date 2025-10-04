all: writer

writer: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) writer.c -o writer $(LDFLAGS)

clean:
	rm -f writer  *.o
