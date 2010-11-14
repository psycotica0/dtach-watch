.PHONY: clean

dtach-watch: dtach-watch.c
	$(CC) -o dtach-watch dtach-watch.c -levent

clean:
	$(RM) dtach-watch
