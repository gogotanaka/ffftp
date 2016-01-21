SERV_TARGET=dist/myftpd
CLI_TARGET=dist/myftpc

all: $(SERV_TARGET) $(CLI_TARGET)

$(SERV_TARGET): src/myftpd.c src/server_cmd.c src/sock_util.c
	gcc -Wall -o $@ $^

$(CLI_TARGET): src/myftpc.c src/client_cmd.c src/sock_util.c
	gcc -Wall -o $@ $^

clean:
	rm -rf $(CLI_TARGET) $(SERV_TARGET) *.o *~ $(CLI_TARGET).dSYM $(SERV_TARGET).dSYM .*~
