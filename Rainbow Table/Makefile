INC=/usr/local/ssl/include/
LIB=/usr/local/ssl/lib/
all:
	gcc -I$(INC) -L$(LIB) -o GenTable GenTable.c -lcrypto -ldl
	gcc -I$(INC) -L$(LIB) -o Crack Crack.c -lcrypto -ldl
